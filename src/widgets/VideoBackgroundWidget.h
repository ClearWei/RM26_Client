#ifndef VIDEOBACKGROUNDWIDGET_H
#define VIDEOBACKGROUNDWIDGET_H

#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QTimer>
#include <QWidget>


#include "../network/VideoReceiver.h"

class QPaintEvent;
class QThread;


#ifdef RM_HAS_WEBENGINE
class QWebEngineView;
#endif

namespace RM {

/**
 * @brief 视频背景模拟控件
 *
 * 模拟图传画面的背景，包含噪点、干扰条纹等效果
 * 支持播放视频流（模拟UDP图传）
 */
class VideoBackgroundWidget : public QWidget {
  Q_OBJECT

public:
  explicit VideoBackgroundWidget(QWidget *parent = nullptr);
  ~VideoBackgroundWidget();

  void setNoiseLevel(int level);      // 0-100
  void setSignalQuality(int quality); // 0-100
  void setInterference(bool enabled);
  void setTargetFps(int fps);         // 目标刷新率（上限）
  void setCurrentRobotId(int robotId);
  bool rotatesVideo180() const {
    QMutexLocker locker(&m_imageMutex);
    return m_rotateVideo180;
  }
  static bool shouldRotateVideo180ForRobotId(int robotId);

  // 设置视频流
  void playUrl(const QString &url);
  void stopVideo();

  VideoReceiver *getVideoReceiver() const { return m_videoReceiver; }
  qint64 lastPresentedFrameMs() const {
    QMutexLocker locker(&m_imageMutex);
    return m_lastPresentedFrameMs;
  }
  qint64 lastReceivedFrameMs() const {
    QMutexLocker locker(&m_imageMutex);
    return m_lastReceivedFrameMs;
  }
  qint64 lastRenderCompleteMs() const {
    QMutexLocker locker(&m_imageMutex);
    return m_lastRenderCompleteMs;
  }
  quint64 presentedFrameCount() const {
    QMutexLocker locker(&m_imageMutex);
    return m_presentedFrameCount;
  }
  quint64 droppedFrameCount() const {
    QMutexLocker locker(&m_imageMutex);
    return m_droppedFrameCount;
  }
  double presentedFps() const {
    QMutexLocker locker(&m_imageMutex);
    return m_lastPresentedFps;
  }
  double lastReceiveToPresentMs() const {
    QMutexLocker locker(&m_imageMutex);
    return m_lastReceiveToPresentMs;
  }
  double avgReceiveToPresentMs() const {
    QMutexLocker locker(&m_imageMutex);
    return m_receiveToPresentSamples > 0
               ? m_totalReceiveToPresentMs / m_receiveToPresentSamples
               : 0.0;
  }
  double lastAssembledToPresentMs() const {
    QMutexLocker locker(&m_imageMutex);
    return m_lastAssembledToPresentMs;
  }
  double avgAssembledToPresentMs() const {
    QMutexLocker locker(&m_imageMutex);
    return m_assembledToPresentSamples > 0
               ? m_totalAssembledToPresentMs / m_assembledToPresentSamples
               : 0.0;
  }
  double lastPacketToPresentMs() const {
    QMutexLocker locker(&m_imageMutex);
    return m_lastPacketToPresentMs;
  }
  double avgPacketToPresentMs() const {
    QMutexLocker locker(&m_imageMutex);
    return m_packetToPresentSamples > 0
               ? m_totalPacketToPresentMs / m_packetToPresentSamples
               : 0.0;
  }

signals:
  /**
   * @brief 视频帧更新信号
   * @details 当新的视频帧实际呈现后发出，用于 AR 系统处理
   */
  void frameUpdated(const QImage &frame);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void onUpdateTimer();
  void onImageReceived(const QImage &image);
  void onTimedImageReceived(quint16 frameId, const QImage &image,
                            qint64 firstPacketMs, qint64 lastPacketMs,
                            qint64 assembleStartMs, qint64 assembleDoneMs,
                            qint64 decodeStartMs, qint64 decodeDoneMs);

private:
  void generateNoise();
  void drawInterference(QPainter &painter);
  void drawGrid(QPainter &painter);
  void refreshScaledFrameLocked();
  void schedulePresent(qint64 nowMs);

  QTimer *m_updateTimer;
  QImage m_noiseImage;
  QImage m_currentFrame; // 当前视频帧
  QImage m_scaledFrame;  // 当前用于绘制的缩放后帧
  mutable QMutex m_imageMutex;
  QThread *m_videoThread;
  VideoReceiver *m_videoReceiver;

  int m_noiseLevel;
  int m_signalQuality;
  bool m_interferenceEnabled;
  bool m_isPlaying;
  bool m_rotateVideo180;
  int m_frameCount;
  float m_scanLinePos;
  int m_targetFps;
  qint64 m_lastReceivedFrameMs;
  qint64 m_lastPresentedFrameMs;
  qint64 m_lastRenderCompleteMs;
  qint64 m_lastRenderStallLogMs;
  qint64 m_pendingAssembledFrameMs;
  qint64 m_pendingPacketMs;
  qint64 m_pendingFirstPacketMs;
  qint64 m_pendingLastPacketMs;
  qint64 m_pendingAssembleStartMs;
  qint64 m_pendingDecodeStartMs;
  qint64 m_pendingDecodeDoneMs;
  qint64 m_lastPresentStatsMs;
  quint32 m_presentedFramesInLastSecond;
  double m_lastPresentedFps;
  quint64 m_presentedFrameCount;
  quint64 m_droppedFrameCount;
  quint64 m_paintFrameCount;
  bool m_framePending;
  quint16 m_pendingFrameId;
  double m_lastReceiveToPresentMs;
  double m_totalReceiveToPresentMs;
  quint64 m_receiveToPresentSamples;
  double m_lastAssembledToPresentMs;
  double m_totalAssembledToPresentMs;
  quint64 m_assembledToPresentSamples;
  double m_lastPacketToPresentMs;
  double m_totalPacketToPresentMs;
  quint64 m_packetToPresentSamples;
  QSize m_scaledFrameTargetSize;

#ifdef RM_HAS_WEBENGINE
  QWebEngineView *m_webView;
#endif
};

} // namespace RM

#endif // VIDEOBACKGROUNDWIDGET_H
