#if defined(RM_ENABLE_FFMPEG)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#endif

#include "H264AnnexB.h"
#include "H264Decoder.h"
#include <QDebug>

namespace RM {

namespace {

bool verboseVideoLogEnabled() {
  static const bool enabled = qEnvironmentVariableIsSet("RM_VERBOSE_VIDEO_LOG");
  return enabled;
}

} // namespace

#if defined(RM_ENABLE_FFMPEG)

H264Decoder::H264Decoder(QObject *parent) : QObject(parent) {}

H264Decoder::~H264Decoder() { cleanup(); }

bool H264Decoder::initialize() {
  QMutexLocker locker(&m_mutex);
  if (m_initialized)
    return true;

  m_codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (!m_codec) {
    emit decodingError("[herovideo] H264 decoder not found");
    return false;
  }

  m_parser = av_parser_init(AV_CODEC_ID_H264);
  if (!m_parser) {
    emit decodingError("[herovideo] Failed to init H264 parser");
    return false;
  }

  m_codecCtx = avcodec_alloc_context3(m_codec);
  if (!m_codecCtx) {
    emit decodingError("[herovideo] Failed to alloc codec context");
    return false;
  }

  // 实时视频优先低延迟；与 HevcDecoder 保持一致的调优参数
  m_codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
  m_codecCtx->flags2 |= AV_CODEC_FLAG2_FAST;
  m_codecCtx->skip_loop_filter = AVDISCARD_NONREF;

  if (avcodec_open2(m_codecCtx, m_codec, nullptr) < 0) {
    emit decodingError("[herovideo] Failed to open H264 codec");
    return false;
  }

  m_frame = av_frame_alloc();
  m_packet = av_packet_alloc();
  m_swsCtx = nullptr;
  m_rgbBuffer = nullptr;
  m_initialized = true;
  m_decodedFrameCount = 0;
  clearCachedState();
  return true;
}

QImage H264Decoder::decode(const QByteArray &data) {
  // 与 doorlock_decoder 的处理链一致：先用 av_parser_parse2 将 MQTT 分片
  // 拼成完整访问单元，再交给 avcodec_send_packet 解码。
  QMutexLocker locker(&m_mutex);
  if (!m_initialized || !m_parser || !m_codecCtx || !m_packet || !m_frame)
    return QImage();

  // 先把输入追加到内部缓存，让解析器能够跨多个 MQTT 小分片拼出完整访问单元。
  m_inputBuffer.append(data);
  // 防止解码器故障时缓冲区无限增长（1 MB 足够容纳数帧 H.264 数据）
  constexpr int kMaxInputBufferSize = 1 * 1024 * 1024;
  if (m_inputBuffer.size() > kMaxInputBufferSize) {
    qWarning() << "[herovideo] H264Decoder: input buffer exceeded"
               << kMaxInputBufferSize << "bytes, resetting to recover sync";
    m_inputBuffer.clear();
    avcodec_flush_buffers(m_codecCtx);
  }

  if (verboseVideoLogEnabled()) {
    qDebug() << "[herovideo] H264Decoder: Received data size=" << data.size()
             << " buffer_size=" << m_inputBuffer.size()
             << " prefix(hex)=" << m_inputBuffer.left(16).toHex();
  }
  QImage outImage;

  const uint8_t *buffer =
      reinterpret_cast<const uint8_t *>(m_inputBuffer.constData());
  int buf_size = m_inputBuffer.size();
  int consumed = 0;

  const uint8_t *input = buffer;
  int input_size = buf_size;

  while (input_size > 0) {
    uint8_t *out_data = nullptr;
    int out_size = 0;
    // av_parser_parse2 将输入字节切分为完整访问单元。len 是已消费字节数；
    // out_size 大于 0 时，说明已经得到可解码的数据包。
    int len =
        av_parser_parse2(m_parser, m_codecCtx, &out_data, &out_size, input,
                         input_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
    if (len <= 0)
      break; // 没有继续消费数据时停止，避免死循环。
    input += len;
    input_size -= len;
    consumed += len;

    if (out_size > 0) {
      // 一个访问单元可能同时包含 SPS/PPS/SEI/IDR，不能只检查第一个 NAL。
      // 分别提取参数集，并扫描整个访问单元的 VCL 类型。
      const QByteArray sps = h264AnnexBExtractNalUnit(out_data, out_size, 7);
      const QByteArray pps = h264AnnexBExtractNalUnit(out_data, out_size, 8);
      if (!sps.isEmpty()) {
        m_sps = sps;
        m_seenSPS = true;
      }
      if (!pps.isEmpty()) {
        m_pps = pps;
        m_seenPPS = true;
      }
      const bool containsIdr =
          h264AnnexBContainsNalType(out_data, out_size, 5);
      const bool containsNonIdrVcl =
          h264AnnexBContainsNalType(out_data, out_size, 1) ||
          h264AnnexBContainsNalType(out_data, out_size, 2) ||
          h264AnnexBContainsNalType(out_data, out_size, 3) ||
          h264AnnexBContainsNalType(out_data, out_size, 4);

      // 等待关键帧模式：复位后跳过非 IDR / 非参数集 NAL，直到收到 IDR 关键帧
      if (m_needKeyframe) {
        // 参数集/SEI/AUD 可先送入解码器；只有纯非 IDR VCL 才需要跳过。
        if (!containsIdr && containsNonIdrVcl) {
          if (verboseVideoLogEnabled()) {
            qDebug() << "[herovideo] H264Decoder: skipping non-IDR access unit "
                        "while waiting for keyframe";
          }
          continue;
        }
        if (containsIdr) {
          m_needKeyframe = false;
          qWarning() << "[herovideo] H264Decoder: 收到 IDR 关键帧，恢复解码";
        }
      }

      av_packet_unref(m_packet);
      if (av_new_packet(m_packet, out_size) < 0)
        continue;
      memcpy(m_packet->data, out_data, out_size);

      if (verboseVideoLogEnabled()) {
        const QByteArray pkt(reinterpret_cast<char *>(m_packet->data),
                             qMin(out_size, 32));
        qDebug() << "[herovideo] H264Decoder: send packet bytes=" << out_size
                 << "pkt_prefix(hex)=" << pkt.toHex();
      }

      int ret = avcodec_send_packet(m_codecCtx, m_packet);
      if (ret < 0) {
        char errbuf[128] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "[custombyte] H264Decoder: avcodec_send_packet failed ret="
                   << ret << "err=" << errbuf;
        av_packet_unref(m_packet);
        continue;
      }

      while (ret >= 0) {
        ret = avcodec_receive_frame(m_codecCtx, m_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
          break;
        if (ret < 0)
          break;

        outImage = convertToQImage(m_frame);
        if (verboseVideoLogEnabled()) {
          qDebug() << "[herovideo] H264Decoder: decoded frame #"
                   << (m_decodedFrameCount + 1) << "size=" << outImage.size();
        }
        m_decodedFrameCount++;
        emit frameDecoded(outImage);
        av_frame_unref(m_frame);
      }
      av_packet_unref(m_packet);
    }
  }

  // 只保留尚未被解析器消费的尾部数据。
  if (consumed > 0) {
    m_inputBuffer.remove(0, consumed);
  }

  return outImage;
}

void H264Decoder::flush() {
  QMutexLocker locker(&m_mutex);
  if (!m_initialized || !m_codecCtx || !m_frame)
    return;
  avcodec_send_packet(m_codecCtx, nullptr);
  while (avcodec_receive_frame(m_codecCtx, m_frame) == 0) {
    QImage img = convertToQImage(m_frame);
    emit frameDecoded(img);
    av_frame_unref(m_frame);
  }
}

QImage H264Decoder::convertToQImage(AVFrame *frame) {
  if (!frame)
    return QImage();
  updateSwsContext(frame->width, frame->height);

  // 使用 av_image_get_buffer_size 确保按 FFmpeg 对齐要求分配
  int rgbLinesize = 3 * frame->width;
  // 对齐到 4 字节：Linux X11 SHM 要求，未对齐会在某些驱动上导致画面损坏
  rgbLinesize = (rgbLinesize + 3) & ~3;
  int bufSize = av_image_get_buffer_size(AV_PIX_FMT_BGR24, frame->width,
                                          frame->height, 1);
  if (bufSize <= 0 || rgbLinesize * frame->height > bufSize) {
    bufSize = rgbLinesize * frame->height;
  }
  if (!m_rgbBuffer || m_rgbBufferSize < bufSize) {
    av_free(m_rgbBuffer);
    m_rgbBuffer = static_cast<uint8_t *>(av_malloc(bufSize));
    if (!m_rgbBuffer) {
      m_rgbBufferSize = 0;
      return QImage();
    }
    m_rgbBufferSize = bufSize;
  }

  uint8_t *dstData[1] = {m_rgbBuffer};
  int dstLinesize[1] = {rgbLinesize};

  sws_scale(m_swsCtx, frame->data, frame->linesize, 0, frame->height, dstData,
            dstLinesize);

  QImage img(m_rgbBuffer, frame->width, frame->height, rgbLinesize,
             QImage::Format_BGR888);
  // 解码层只负责像素格式转换。瞄准/校准网格属于 UI 叠加，不能烙入
  // 每一张视频帧，否则所有显示端都无法关闭且会增加热路径绘制开销。
  return img.copy();
}

void H264Decoder::updateSwsContext(int width, int height) {
  if (m_width == width && m_height == height && m_swsCtx)
    return;
  if (m_swsCtx) {
    sws_freeContext(m_swsCtx);
    m_swsCtx = nullptr;
  }
  m_width = width;
  m_height = height;
  m_swsCtx = sws_getContext(
      width, height, static_cast<AVPixelFormat>(m_frame->format), width, height,
      AV_PIX_FMT_BGR24, SWS_BILINEAR, nullptr, nullptr, nullptr);
}

void H264Decoder::cleanup() {
  if (m_swsCtx)
    sws_freeContext(m_swsCtx);
  if (m_frame)
    av_frame_free(&m_frame);
  if (m_packet)
    av_packet_free(&m_packet);
  if (m_parser)
    av_parser_close(m_parser);
  if (m_codecCtx)
    avcodec_free_context(&m_codecCtx);
  if (m_rgbBuffer) {
    av_free(m_rgbBuffer);
    m_rgbBuffer = nullptr;
  }
  m_rgbBufferSize = 0;
  m_initialized = false;
}

void H264Decoder::clearCachedState() {
  m_inputBuffer.clear();
  m_sps.clear();
  m_pps.clear();
  m_seenSPS = false;
  m_seenPPS = false;

  if (m_packet) {
    av_packet_unref(m_packet);
  }
  if (m_frame) {
    av_frame_unref(m_frame);
  }
}

void H264Decoder::resetParserAndCodec() {
  QMutexLocker locker(&m_mutex);
  qWarning() << "[custombyte] H264Decoder: resetParserAndCodec() called";
  // 重置期间保留最近一次参数集，使新的解码上下文不必等待发送端
  // 下一轮 SPS/PPS；clearCachedState() 会清空成员缓存，因此先保存副本。
  const QByteArray cachedSps = m_sps;
  const QByteArray cachedPps = m_pps;
  clearCachedState();
  // 关闭旧解析器。
  if (m_parser) {
    av_parser_close(m_parser);
    m_parser = nullptr;
  }

  // 释放旧解码上下文。
  if (m_codecCtx) {
    avcodec_free_context(&m_codecCtx);
    m_codecCtx = nullptr;
  }
  m_initialized = false;

  // 重新创建解析器和解码上下文。
  const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (codec) {
    m_parser = av_parser_init(AV_CODEC_ID_H264);
    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_parser || !m_codecCtx) {
      emit decodingError(
          "[custombyte] H264Decoder: failed to reinitialize parser/context");
      if (m_parser) {
        av_parser_close(m_parser);
        m_parser = nullptr;
      }
      if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
      }
    } else {
      m_codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
      m_codecCtx->flags2 |= AV_CODEC_FLAG2_FAST;
      m_codecCtx->skip_loop_filter = AVDISCARD_NONREF;
      if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        emit decodingError(
            "[custombyte] H264Decoder: failed to open codec during reset");
        av_parser_close(m_parser);
        avcodec_free_context(&m_codecCtx);
        m_parser = nullptr;
        m_codecCtx = nullptr;
      } else {
        m_codec = codec;
        m_initialized = true;
        if (verboseVideoLogEnabled()) {
          qDebug() << "[herovideo] H264Decoder: Reinitialized parser/context "
                      "successfully";
        }
        // 重新注入缓存的 SPS/PPS，避免解码器因缺少参数集而拒绝后续 IDR 帧。
        const QByteArray cachedParameterSets = cachedSps + cachedPps;
        const auto restoreParameterSets = [this](const QByteArray &nalData) {
          if (nalData.isEmpty())
            return;
          AVPacket *packet = av_packet_alloc();
          if (!packet)
            return;
          if (av_new_packet(packet, nalData.size()) >= 0) {
            memcpy(packet->data, nalData.constData(), nalData.size());
            const int ret = avcodec_send_packet(m_codecCtx, packet);
            if (ret < 0) {
              char errbuf[128] = {0};
              av_strerror(ret, errbuf, sizeof(errbuf));
              qWarning() << "[custombyte] H264Decoder: cached SPS/PPS restore "
                            "failed ret="
                         << ret << "err=" << errbuf;
            }
          }
          // 即使 av_new_packet 失败，也必须释放 av_packet_alloc 的对象。
          av_packet_free(&packet);
        };
        m_sps = cachedSps;
        m_pps = cachedPps;
        m_seenSPS = !m_sps.isEmpty();
        m_seenPPS = !m_pps.isEmpty();
        // SPS/PPS 必须作为连续参数集数据包回灌，避免解码器尚未取完上一批
        // 输出时，第二次 avcodec_send_packet 返回 EAGAIN。
        restoreParameterSets(cachedParameterSets);
      }
    }
  } else {
    emit decodingError("[custombyte] H264Decoder: codec not found during reset");
  }

  m_inputBuffer.clear();
  m_decodedFrameCount = 0;
  m_needKeyframe = true;
  if (m_rgbBuffer) {
    av_free(m_rgbBuffer);
    m_rgbBuffer = nullptr;
  }
  m_rgbBufferSize = 0;
}

#endif // RM_ENABLE_FFMPEG

} // namespace RM
