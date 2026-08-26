// SPDX-License-Identifier: MIT
/**
 * @file HevcDecoder.h
 * @brief HEVC (H.265) 视频解码器 - 基于 FFmpeg（连续流解码版本）
 * @details 本文件定义了 HevcDecoder 类，封装了 FFmpeg 的 HEVC 解码功能。
 *
 *          重要：HEVC 是时间压缩编码，P/B 帧依赖 I 帧作为参考。
 *          因此需要使用 AVCodecParser 维护解码器状态，实现连续流解码。
 *
 *          解码架构：
 *          ┌─────────────┐     ┌────────────────┐     ┌────────────────┐
 *          │ HEVC 码流   │ ──► │ AVCodecParser  │ ──► │ AVCodecContext │
 *          │ (NAL Units) │     │ (解析 NAL 边界) │     │   (解码帧)      │
 *          └─────────────┘     └────────────────┘     └────────┬───────┘
 *                                                              ▼
 *                                                     ┌────────────────┐
 *                                                     │ SwsContext     │
 *                                                     │ (YUV -> RGB)   │
 *                                                     └────────┬───────┘
 *                                                              ▼
 *                                                     ┌────────────────┐
 *                                                     │   QImage       │
 *                                                     └────────────────┘
 *
 * @author Clear
 * @date 2025-12-14
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#ifndef HEVCDECODER_H
#define HEVCDECODER_H

#include <QByteArray>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>

#if defined(RM_ENABLE_FFMPEG)
// FFmpeg 头文件需要使用 extern "C" 包裹
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#endif

namespace RM {

#if defined(RM_ENABLE_FFMPEG)
/**
 * @class HevcDecoder
 * @brief FFmpeg HEVC 连续流解码器
 * @details 使用 AVCodecParser 维护解码器状态，正确处理 P/B 帧的参考依赖。
 */
class HevcDecoder : public QObject {
  Q_OBJECT

public:
  explicit HevcDecoder(QObject *parent = nullptr);
  ~HevcDecoder();

  /**
   * @brief 初始化解码器
   * @return true 初始化成功
   */
  bool initialize();

  /**
   * @brief 解码 HEVC 数据（连续流方式）
   * @param data HEVC 码流数据
   * @return 解码后的 QImage，无帧输出时返回空 QImage
   *
   * @note 连续调用此方法喂入数据，解码器内部维护状态
   */
  QImage decode(const QByteArray &data);

  /**
   * @brief 刷新解码器，获取剩余帧
   */
  void flush();

  /**
   * @brief 重置连续流解码状态
   * @details 当上游出现丢帧或断流时，清空参考帧和解析器状态，
   *          等待下一张关键帧重新建立参考链。
   */
  void resetStreamState();

  bool isInitialized() const {
    QMutexLocker locker(&m_mutex);
    return m_initialized;
  }
  int getDecodedFrameCount() const {
    QMutexLocker locker(&m_mutex);
    return m_decodedFrameCount;
  }
  double getLastDecodeTimeMs() const {
    QMutexLocker locker(&m_mutex);
    return m_lastDecodeTimeMs;
  }
  double getAverageDecodeTimeMs() const {
    QMutexLocker locker(&m_mutex);
    return m_decodeSampleCount > 0 ? m_totalDecodeTimeMs / m_decodeSampleCount
                                   : 0.0;
  }

signals:
  void frameDecoded(const QImage &image);
  void decodingError(const QString &error);

private:
  // FFmpeg 组件
  const AVCodec *m_codec;         ///< HEVC 解码器
  AVCodecContext *m_codecContext; ///< 解码器上下文（维护状态）
  AVCodecParserContext *m_parser; ///< 码流解析器（解析 NAL 边界）
  AVFrame *m_frame;               ///< 解码后的帧
  AVFrame *m_frameRGB;            ///< RGB 帧
  AVPacket *m_packet;             ///< 解码数据包
  SwsContext *m_swsContext;       ///< YUV -> RGB 转换器
  uint8_t *m_rgbBuffer;           ///< RGB 缓冲区

  // 状态
  bool m_initialized;
  bool m_needKeyframe = false; // 重置后保持为真，直到收到下一帧 IRAP。
  int m_decodedFrameCount;
  int m_width;
  int m_height;
  AVPixelFormat m_pixelFormat;
  double m_lastDecodeTimeMs;
  double m_totalDecodeTimeMs;
  int m_decodeSampleCount;

  // 缓存 Annex-B 参数集（含起始码），供解码器重置后重新注入。
  QByteArray m_vps;
  QByteArray m_sps;
  QByteArray m_pps;

  // 线程安全
  mutable QMutex m_mutex;

  // 内部方法
  QImage decodePacket();
  QImage convertToQImage(AVFrame *frame);
  void updateSwsContext(int width, int height, AVPixelFormat sourceFormat);
  void cleanup();
  static bool isHevcKeyframeOrParamSet(const uint8_t *data, int size);
};
#else
class HevcDecoder : public QObject {
  Q_OBJECT

public:
  explicit HevcDecoder(QObject *parent = nullptr);
  ~HevcDecoder();

  bool initialize();
  QImage decode(const QByteArray &data);
  void flush();
  void resetStreamState();

  bool isInitialized() const;
  int getDecodedFrameCount() const;

signals:
  void frameDecoded(const QImage &image);
  void decodingError(const QString &error);
};
#endif

} // namespace RM

#endif // HEVCDECODER_H
