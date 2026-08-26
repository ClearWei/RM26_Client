/**
 * @file HevcDecoder.cpp
 * @brief HEVC 连续流解码器实现
 * @details 使用 AVCodecParser 正确解析 NAL 单元边界，维护解码器状态。
 *
 *          解码流程：
 *          1. 通过 av_parser_parse2 解析输入数据
 *          2. 解析器输出完整的 NAL 单元
 *          3. 将 NAL 单元发送给解码器
 *          4. 从解码器获取解码帧
 *          5. 将 YUV 转为 RGB，再生成 QImage
 *
 * @author Clear
 * @date 2025-12-14
 */

#include "HevcDecoder.h"
#include <QElapsedTimer>
#include <QDebug>
#include <QThread>
#include <QtGlobal>

namespace RM {

#if defined(RM_ENABLE_FFMPEG)

namespace {

// 从带起始码的 Annex-B 数据中提取 HEVC NAL 单元类型。
// 找不到起始码或 NAL 头时返回 -1。
int extractHevcNalType(const uint8_t *data, int size) {
  if (!data || size < 3)
    return -1;
  int scLen = 0;
  if (size >= 4 && data[0] == 0x00 && data[1] == 0x00 &&
      data[2] == 0x00 && data[3] == 0x01) {
    scLen = 4;
  } else if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01) {
    scLen = 3;
  }
  if (scLen == 0 || size < scLen + 2)
    return -1;
  return (data[scLen] >> 1) & 0x3F;
}

} // namespace
// --- 构造与析构 ---

HevcDecoder::HevcDecoder(QObject *parent)
    : QObject(parent), m_codec(nullptr), m_codecContext(nullptr),
      m_parser(nullptr), m_frame(nullptr), m_frameRGB(nullptr),
      m_packet(nullptr), m_swsContext(nullptr), m_rgbBuffer(nullptr),
      m_initialized(false), m_decodedFrameCount(0), m_width(0), m_height(0),
      m_pixelFormat(AV_PIX_FMT_NONE), m_lastDecodeTimeMs(0.0),
      m_totalDecodeTimeMs(0.0), m_decodeSampleCount(0) {}

HevcDecoder::~HevcDecoder() { cleanup(); }

// --- 初始化 ---

bool HevcDecoder::initialize() {
  QMutexLocker locker(&m_mutex);

  if (m_initialized) {
    return true;
  }

  // -------------------------------------------------------------------------
  // 步骤 1: 查找 HEVC 解码器
  // -------------------------------------------------------------------------
  m_codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
  if (!m_codec) {
    qCritical() << "HevcDecoder: 未找到 HEVC 解码器";
    emit decodingError("HEVC decoder not found");
    return false;
  }
  qDebug() << "HevcDecoder: 找到解码器:" << m_codec->name;

  // -------------------------------------------------------------------------
  // 步骤 2: 创建解析器（关键！用于维护 NAL 边界）
  // -------------------------------------------------------------------------
  m_parser = av_parser_init(m_codec->id);
  if (!m_parser) {
    qCritical() << "HevcDecoder: 无法创建解析器";
    emit decodingError("Failed to create parser");
    return false;
  }
  qDebug() << "HevcDecoder: 创建解析器成功";

  // -------------------------------------------------------------------------
  // 步骤 3: 分配解码器上下文
  // -------------------------------------------------------------------------
  m_codecContext = avcodec_alloc_context3(m_codec);
  if (!m_codecContext) {
    qCritical() << "HevcDecoder: 无法分配解码器上下文";
    cleanup();
    return false;
  }

  // 实时图传优先低延迟和稳定帧率；slice threading 通常比 frame threading 少一帧排队延迟。
  const int idealThreads = qMax(1, QThread::idealThreadCount());
  m_codecContext->thread_count = qBound(1, idealThreads - 1, 6);
  m_codecContext->thread_type = FF_THREAD_SLICE;
  m_codecContext->flags |= AV_CODEC_FLAG_LOW_DELAY;
  m_codecContext->flags2 |= AV_CODEC_FLAG2_FAST;
  m_codecContext->skip_loop_filter = AVDISCARD_NONREF;

  // -------------------------------------------------------------------------
  // 步骤 4: 打开解码器
  // -------------------------------------------------------------------------
  int ret = avcodec_open2(m_codecContext, m_codec, nullptr);
  if (ret < 0) {
    char errBuf[256];
    av_strerror(ret, errBuf, sizeof(errBuf));
    qCritical() << "HevcDecoder: 无法打开解码器:" << errBuf;
    cleanup();
    return false;
  }

  // -------------------------------------------------------------------------
  // 步骤 5: 分配帧和数据包
  // -------------------------------------------------------------------------
  m_frame = av_frame_alloc();
  m_frameRGB = av_frame_alloc();
  m_packet = av_packet_alloc();

  if (!m_frame || !m_frameRGB || !m_packet) {
    qCritical() << "HevcDecoder: 无法分配帧或数据包";
    cleanup();
    return false;
  }

  m_initialized = true;
  qDebug() << "HevcDecoder: 初始化成功（连续流模式）";
  return true;
}

// --- 解码（连续流） ---

QImage HevcDecoder::decode(const QByteArray &data) {
  QMutexLocker locker(&m_mutex);

  if (!m_initialized || data.isEmpty()) {
    return QImage();
  }

  const uint8_t *inputData =
      reinterpret_cast<const uint8_t *>(data.constData());
  int inputSize = data.size();
  QImage resultImage;
  QElapsedTimer timer;
  timer.start();

  // -------------------------------------------------------------------------
  // 使用解析器处理输入数据
  // -------------------------------------------------------------------------
  while (inputSize > 0) {
    // av_parser_parse2 会解析 NAL 边界
    int parsedBytes = av_parser_parse2(
        m_parser, m_codecContext, &m_packet->data, &m_packet->size, inputData,
        inputSize, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

    if (parsedBytes < 0) {
      qDebug() << "HevcDecoder: 解析错误";
      break;
    }

    inputData += parsedBytes;
    inputSize -= parsedBytes;

    // 解析器输出完整 NAL 单元后再送入解码链。
    if (m_packet->size > 0) {
      // 遍历数据包中的全部 NAL 单元，缓存参数集并检测 IRAP 关键帧。
      bool hasIrap = false;
      bool hasParamSet = false;
      const uint8_t *p = m_packet->data;
        int remaining = m_packet->size;
        while (remaining >= 3) {
          int scLen = 0;
          if (remaining >= 4 && p[0] == 0x00 && p[1] == 0x00 &&
              p[2] == 0x00 && p[3] == 0x01) {
            scLen = 4;
          } else if (p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01) {
            scLen = 3;
          } else {
            break; // 找不到起始码时停止扫描。
          }
          // 用下一个起始码确定当前 NAL 的边界。
          int nalEnd = remaining;
          for (int i = scLen + 2; i < remaining - 2; ++i) {
            if ((p[i] == 0x00 && p[i+1] == 0x00 &&
                 (p[i+2] == 0x01 || (p[i+2] == 0x00 && i+3 < remaining && p[i+3] == 0x01)))) {
              nalEnd = i;
              break;
            }
          }
          int nalType = (p[scLen] >> 1) & 0x3F;
          int nalSize = nalEnd;

          if (nalType == 32) { // VPS
            m_vps = QByteArray(reinterpret_cast<const char *>(p), nalSize);
            hasParamSet = true;
          } else if (nalType == 33) { // SPS
            m_sps = QByteArray(reinterpret_cast<const char *>(p), nalSize);
            hasParamSet = true;
          } else if (nalType == 34) { // PPS
            m_pps = QByteArray(reinterpret_cast<const char *>(p), nalSize);
            hasParamSet = true;
          }
          if (nalType >= 16 && nalType <= 21)
            hasIrap = true;

          p += nalEnd;
          remaining -= nalEnd;
        }

      // 仅记录前 5 个 NAL 类型，供启动阶段诊断。
      static int nalLogCount = 0;
      if (nalLogCount < 5) {
        int firstNalType = extractHevcNalType(m_packet->data, m_packet->size);
        qWarning() << "HevcDecoder: NAL type=" << firstNalType
                 << "pkt_size=" << m_packet->size
                 << "needKeyframe=" << m_needKeyframe
                 << "vps=" << !m_vps.isEmpty()
                 << "sps=" << !m_sps.isEmpty()
                 << "pps=" << !m_pps.isEmpty();
        ++nalLogCount;
      }

      // 等待关键帧模式下，跳过非 IRAP / 非参数集 NAL，直到收到关键帧
      if (m_needKeyframe) {
        if (!hasIrap && !hasParamSet) {
          continue;
        }
        if (hasIrap) {
          m_needKeyframe = false;
          qWarning() << "HevcDecoder: 收到关键帧，恢复解码";
        }
      }
      QImage frame = decodePacket();
      if (!frame.isNull()) {
        resultImage = frame; // 返回最后一帧
      }
    }
  }

  if (!resultImage.isNull()) {
    m_lastDecodeTimeMs = timer.nsecsElapsed() / 1000000.0;
    m_totalDecodeTimeMs += m_lastDecodeTimeMs;
    ++m_decodeSampleCount;
  }

  return resultImage;
}

// --- 解码单个数据包 ---

QImage HevcDecoder::decodePacket() {
  // 发送数据包给解码器
  int ret = avcodec_send_packet(m_codecContext, m_packet);
  if (ret < 0) {
    if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
      // P/B 帧等待 I 帧时可能暂时失败，此处保持静默。
    }
    return QImage();
  }

  QImage resultImage;

  // 尝试获取所有可用的解码帧
  while (true) {
    ret = avcodec_receive_frame(m_codecContext, m_frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break; // 需要更多数据或已结束
    }
    if (ret < 0) {
      break; // 解码错误
    }

    // 解码成功！
    m_decodedFrameCount++;
    resultImage = convertToQImage(m_frame);

    if (!resultImage.isNull()) {
      emit frameDecoded(resultImage);
    }
  }

  return resultImage;
}

// --- 刷新解码器 ---

void HevcDecoder::flush() {
  QMutexLocker locker(&m_mutex);

  if (!m_initialized) {
    return;
  }

  // 发送空包刷新
  avcodec_send_packet(m_codecContext, nullptr);

  while (true) {
    int ret = avcodec_receive_frame(m_codecContext, m_frame);
    if (ret < 0) {
      break;
    }

    m_decodedFrameCount++;
    QImage image = convertToQImage(m_frame);
    if (!image.isNull()) {
      emit frameDecoded(image);
    }
  }
}

void HevcDecoder::resetStreamState() {
  QMutexLocker locker(&m_mutex);

  if (!m_initialized) {
    return;
  }

  avcodec_flush_buffers(m_codecContext);

  if (m_packet) {
    av_packet_unref(m_packet);
  }

  if (m_parser) {
    av_parser_close(m_parser);
    m_parser = av_parser_init(m_codec->id);
    if (!m_parser) {
      qCritical() << "HevcDecoder: 重建解析器失败";
      emit decodingError("Failed to recreate parser after transport discontinuity");
      return;
    }
  }

  m_needKeyframe = true;

  // 重新注入缓存的参数集，避免解码器因缺少 VPS/SPS/PPS 而报错。
  auto injectParamSet = [this](const QByteArray &nal) {
    if (nal.isEmpty())
      return;
    AVPacket *pkt = av_packet_alloc();
    if (pkt && av_new_packet(pkt, nal.size()) >= 0) {
      memcpy(pkt->data, nal.constData(), nal.size());
      avcodec_send_packet(m_codecContext, pkt);
      av_packet_free(&pkt);
    }
  };
  injectParamSet(m_vps);
  injectParamSet(m_sps);
  injectParamSet(m_pps);

  qWarning() << "HevcDecoder: 检测到上游断流/丢帧，已重置解码流状态，等待下一关键帧"
             << "vps_cached=" << !m_vps.isEmpty()
             << "sps_cached=" << !m_sps.isEmpty()
             << "pps_cached=" << !m_pps.isEmpty();
}

// 判断带起始码的 Annex-B NAL 单元是否为 IRAP 关键帧，或是否为等待
// 关键帧期间仍需放行的 VPS、SPS、PPS 参数集。
bool HevcDecoder::isHevcKeyframeOrParamSet(const uint8_t *data, int size) {
  int nalType = extractHevcNalType(data, size);
  if (nalType < 0)
    return false;
  // IRAP 类型范围为 BLA_W_LP(16) 到 CRA_NUT(21)。
  if (nalType >= 16 && nalType <= 21)
    return true;
  // 参数集和分隔单元必须放行，解码器初始化会用到它们。
  if (nalType == 32 || nalType == 33 || nalType == 34 || // VPS, SPS, PPS
      nalType == 35 || // AUD
      nalType == 39 || nalType == 40) // SEI
    return true;
  return false;
}

// --- 格式转换 ---

QImage HevcDecoder::convertToQImage(AVFrame *frame) {
  if (!frame || frame->width <= 0 || frame->height <= 0) {
    return QImage();
  }

  const AVPixelFormat sourceFormat = static_cast<AVPixelFormat>(frame->format);
  if (sourceFormat == AV_PIX_FMT_NONE) {
    qWarning() << "HevcDecoder: decoded frame has invalid pixel format";
    return QImage();
  }

  // 检查尺寸或像素格式是否变化。Linux/FFmpeg 下同一分辨率可能在
  // yuv420p/nv12/yuvj420p 等格式间变化，不能只按 codecContext->pix_fmt 建 sws。
  if (frame->width != m_width || frame->height != m_height ||
      sourceFormat != m_pixelFormat) {
    updateSwsContext(frame->width, frame->height, sourceFormat);
  }

  if (!m_swsContext) {
    return QImage();
  }

  // 直接把 sws_scale 输出写入 QImage 自有内存，避免 m_rgbBuffer -> image.copy() 二次拷贝
  QImage image(m_width, m_height, QImage::Format_RGB32);
  if (image.isNull()) {
    return QImage();
  }

  uint8_t *dstData[4] = {image.bits(), nullptr, nullptr, nullptr};
  int dstLinesize[4] = {static_cast<int>(image.bytesPerLine()), 0, 0, 0};

  sws_scale(m_swsContext, frame->data, frame->linesize, 0, frame->height,
            dstData, dstLinesize);

  return image;
}

void HevcDecoder::updateSwsContext(int width, int height,
                                   AVPixelFormat sourceFormat) {
  // 释放旧资源
  if (m_swsContext) {
    sws_freeContext(m_swsContext);
    m_swsContext = nullptr;
  }
  if (m_rgbBuffer) {
    av_free(m_rgbBuffer);
    m_rgbBuffer = nullptr;
  }

  m_width = width;
  m_height = height;
  m_pixelFormat = sourceFormat;

  // 创建新的转换上下文
  m_swsContext = sws_getContext(m_width, m_height, m_pixelFormat,
                                m_width, m_height, AV_PIX_FMT_BGRA,
                                SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

  if (!m_swsContext) {
    qCritical() << "HevcDecoder: 无法创建 SwsContext";
    return;
  }

  qDebug() << "HevcDecoder: 视频分辨率:" << m_width << "x" << m_height
           << "pixelFormat=" << m_pixelFormat;
}

// --- 清理 ---

void HevcDecoder::cleanup() {
  if (m_swsContext) {
    sws_freeContext(m_swsContext);
    m_swsContext = nullptr;
  }
  if (m_rgbBuffer) {
    av_free(m_rgbBuffer);
    m_rgbBuffer = nullptr;
  }
  if (m_frame) {
    av_frame_free(&m_frame);
    m_frame = nullptr;
  }
  if (m_frameRGB) {
    av_frame_free(&m_frameRGB);
    m_frameRGB = nullptr;
  }
  if (m_packet) {
    av_packet_free(&m_packet);
    m_packet = nullptr;
  }
  if (m_parser) {
    av_parser_close(m_parser);
    m_parser = nullptr;
  }
  if (m_codecContext) {
    avcodec_free_context(&m_codecContext);
    m_codecContext = nullptr;
  }

  m_vps.clear();
  m_sps.clear();
  m_pps.clear();
  m_width = 0;
  m_height = 0;
  m_pixelFormat = AV_PIX_FMT_NONE;
  m_initialized = false;
  qDebug() << "HevcDecoder: 资源已释放";
}

} // namespace RM
#else
HevcDecoder::HevcDecoder(QObject *parent) : QObject(parent) {}

HevcDecoder::~HevcDecoder() = default;

bool HevcDecoder::initialize() {
  emit decodingError("FFmpeg disabled");
  return false;
}

QImage HevcDecoder::decode(const QByteArray &) { return QImage(); }

void HevcDecoder::flush() {}

void HevcDecoder::resetStreamState() {}

bool HevcDecoder::isInitialized() const { return false; }

int HevcDecoder::getDecodedFrameCount() const { return 0; }

} // namespace RM
#endif
