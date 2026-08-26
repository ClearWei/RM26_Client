/**
 * @file H264Decoder.h
 * @brief H.264 连续流解码器（基于 FFmpeg）
 */

#ifndef H264DECODER_H
#define H264DECODER_H

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QByteArray>

#if defined(RM_ENABLE_FFMPEG)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#endif

namespace RM {

#if defined(RM_ENABLE_FFMPEG)
class H264Decoder : public QObject {
  Q_OBJECT
public:
  explicit H264Decoder(QObject *parent = nullptr);
  ~H264Decoder();

  bool initialize();
  QImage decode(const QByteArray &data);
  void flush();
  void resetParserAndCodec();

  bool isInitialized() const { return m_initialized; }
  int getDecodedFrameCount() const { return m_decodedFrameCount; }

signals:
  void frameDecoded(const QImage &image);
  void decodingError(const QString &error);

private:
  void clearCachedState();
  const AVCodec *m_codec = nullptr;
  AVCodecContext *m_codecCtx = nullptr;
  AVCodecParserContext *m_parser = nullptr;
  AVFrame *m_frame = nullptr;
  AVPacket *m_packet = nullptr;
  SwsContext *m_swsCtx = nullptr;
  uint8_t *m_rgbBuffer = nullptr;
  int m_rgbBufferSize = 0;
  int m_width = 0;
  int m_height = 0;

  bool m_initialized = false;
  bool m_needKeyframe = false; // 重置后保持为真，直到收到下一帧 IDR。
  int m_decodedFrameCount = 0;
  QMutex m_mutex;
  QByteArray m_inputBuffer; // 累积收到的 H.264 分片。
  // 缓存 Annex-B 参数集，内容包含起始码。
  QByteArray m_sps;
  QByteArray m_pps;
  bool m_seenSPS = false;
  bool m_seenPPS = false;

  QImage convertToQImage(AVFrame *frame);
  void updateSwsContext(int width, int height);
  void cleanup();
};
#else
class H264Decoder : public QObject {
  Q_OBJECT
public:
  explicit H264Decoder(QObject *parent = nullptr) : QObject(parent) {}
  ~H264Decoder() {}
  bool initialize() { return false; }
  QImage decode(const QByteArray &) { return QImage(); }
  void flush() {}
  bool isInitialized() const { return false; }
  int getDecodedFrameCount() const { return 0; }
signals:
  void frameDecoded(const QImage &image);
  void decodingError(const QString &error);
};
#endif

} // namespace RM

#endif // H264DECODER_H
