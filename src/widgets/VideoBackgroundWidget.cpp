#include "VideoBackgroundWidget.h"
#include <QAudioOutput>
#include <QDateTime>
#include <QDebug>
#include <QMetaObject>
#include <QPainter>
#include <QRandomGenerator>
#include <QThread>
#include <QUrl>
#include <chrono>
#ifdef RM_HAS_WEBENGINE
#include <QWebEngineView>
#endif

namespace RM {
namespace {

qint64 runtimeMonoMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

QString runtimeLogFields() {
  const QDateTime now = QDateTime::currentDateTimeUtc();
  return QStringLiteral("wall_ms=%1 wall_time=%2 mono_ms=%3")
      .arg(now.toMSecsSinceEpoch())
      .arg(now.toString(Qt::ISODateWithMs))
      .arg(runtimeMonoMs());
}

} // namespace

VideoBackgroundWidget::VideoBackgroundWidget(QWidget *parent)
    : QWidget(parent), m_updateTimer(nullptr), m_videoThread(new QThread(this)),
      m_videoReceiver(new VideoReceiver(this)), m_noiseLevel(0),
      m_signalQuality(100), m_interferenceEnabled(false), m_isPlaying(false),
      m_rotateVideo180(false), m_frameCount(0), m_scanLinePos(0),
      m_targetFps(60),
      m_lastReceivedFrameMs(0),
      m_lastPresentedFrameMs(0), m_lastRenderCompleteMs(0),
      m_lastRenderStallLogMs(0),
      m_pendingAssembledFrameMs(0), m_pendingPacketMs(0),
      m_pendingFirstPacketMs(0), m_pendingLastPacketMs(0),
      m_pendingAssembleStartMs(0), m_pendingDecodeStartMs(0),
      m_pendingDecodeDoneMs(0),
      m_lastPresentStatsMs(0), m_presentedFramesInLastSecond(0),
      m_lastPresentedFps(0.0), m_presentedFrameCount(0), m_droppedFrameCount(0),
      m_paintFrameCount(0), m_framePending(false), m_pendingFrameId(0),
      m_lastReceiveToPresentMs(0.0),
      m_totalReceiveToPresentMs(0.0), m_receiveToPresentSamples(0),
      m_lastAssembledToPresentMs(0.0), m_totalAssembledToPresentMs(0.0),
      m_assembledToPresentSamples(0), m_lastPacketToPresentMs(0.0),
      m_totalPacketToPresentMs(0.0), m_packetToPresentSamples(0) {

  // 视频控件优先走不透明、无系统背景重绘路径，减少额外擦除和合成
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setAttribute(Qt::WA_StaticContents, true);

  // 无视频时使用黑色背景
  QPalette pal = palette();
  pal.setColor(QPalette::Window, Qt::black);
  setAutoFillBackground(true);
  setPalette(pal);

#ifdef RM_HAS_WEBENGINE
  m_webView = nullptr;
#endif

  m_updateTimer = new QTimer(this);
  m_updateTimer->setSingleShot(true);
  connect(m_updateTimer, &QTimer::timeout, this, [this]() { update(); });

  m_videoReceiver->setParent(nullptr);
  m_videoThread->setObjectName(QStringLiteral("rm26-video-pipeline"));
  m_videoReceiver->moveToThread(m_videoThread);
  connect(m_videoThread, &QThread::finished, m_videoReceiver,
          &QObject::deleteLater);
  connect(m_videoReceiver, &VideoReceiver::imageReceivedTimed, this,
          &VideoBackgroundWidget::onTimedImageReceived, Qt::QueuedConnection);
  m_videoThread->start(QThread::HighPriority);

  generateNoise();
  m_lastPresentStatsMs = QDateTime::currentMSecsSinceEpoch();
}

VideoBackgroundWidget::~VideoBackgroundWidget() {
  stopVideo();
  if (m_videoThread) {
    m_videoThread->quit();
    if (!m_videoThread->wait(1500)) {
      qWarning() << "VideoBackgroundWidget: video thread did not stop in time";
    }
  }
}

void VideoBackgroundWidget::setNoiseLevel(int level) { m_noiseLevel = level; }

void VideoBackgroundWidget::setSignalQuality(int quality) {
  m_signalQuality = quality;
}

void VideoBackgroundWidget::setInterference(bool enabled) {
  m_interferenceEnabled = enabled;
}

void VideoBackgroundWidget::setTargetFps(int fps) {
  // 限制范围，避免异常值导致间隔计算错误
  m_targetFps = qBound(1, fps, 240);
}

bool VideoBackgroundWidget::shouldRotateVideo180ForRobotId(int robotId) {
  const int normalizedRobotId = robotId >= 100 ? (robotId - 100) : robotId;
  return normalizedRobotId == 6;
}

void VideoBackgroundWidget::setCurrentRobotId(int robotId) {
  const bool shouldRotate = shouldRotateVideo180ForRobotId(robotId);
  bool changed = false;
  {
    QMutexLocker locker(&m_imageMutex);
    if (m_rotateVideo180 == shouldRotate) {
      return;
    }
    m_rotateVideo180 = shouldRotate;
    m_scaledFrame = QImage();
    m_scaledFrameTargetSize = QSize();
    changed = true;
  }

  if (changed) {
    update();
  }
}

void VideoBackgroundWidget::playUrl(const QString &url) {
#ifdef RM_HAS_WEBENGINE
  if (m_webView) {
    m_webView->setVisible(false);
  }
#endif

  // 默认监听 3334；UDP URL 显式指定端口时以 URL 为准。
  quint16 port = 3334;
  QUrl qUrl(url);
  if (qUrl.scheme() == "udp" && qUrl.port() != -1) {
    port = qUrl.port();
  }

  qDebug() << "Starting video receiver on port" << port;
  bool started = false;
  if (m_videoReceiver && m_videoThread && m_videoThread->isRunning()) {
    QMetaObject::invokeMethod(
        m_videoReceiver,
        [this, port, &started]() {
          started = m_videoReceiver->startListening(port);
        },
        Qt::BlockingQueuedConnection);
  }
  if (started) {
    m_isPlaying = true;
  } else {
    qDebug() << "Failed to start video receiver";
  }
}

void VideoBackgroundWidget::stopVideo() {
  if (m_videoReceiver && m_videoThread && m_videoThread->isRunning()) {
    if (QThread::currentThread() == m_videoReceiver->thread()) {
      m_videoReceiver->stopListening();
    } else {
      QMetaObject::invokeMethod(m_videoReceiver,
                                [this]() { m_videoReceiver->stopListening(); },
                                Qt::BlockingQueuedConnection);
    }
  }
  m_isPlaying = false;
  {
    QMutexLocker locker(&m_imageMutex);
    m_currentFrame = QImage(); // 清空当前帧
    m_scaledFrame = QImage();
    m_scaledFrameTargetSize = QSize();
    m_framePending = false;
    m_pendingFrameId = 0;
    m_pendingFirstPacketMs = 0;
    m_pendingLastPacketMs = 0;
    m_pendingAssembleStartMs = 0;
    m_pendingAssembledFrameMs = 0;
    m_pendingPacketMs = 0;
    m_pendingDecodeStartMs = 0;
    m_pendingDecodeDoneMs = 0;
    m_lastRenderStallLogMs = 0;
  }
  update();
}


void VideoBackgroundWidget::onImageReceived(const QImage &image) {
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  qint64 assembledFrameMs = 0;
  qint64 packetMs = 0;
  if (m_videoReceiver) {
    assembledFrameMs = m_videoReceiver->lastAssembledFrameMs();
    packetMs = m_videoReceiver->lastPacketMs();
  }
  onTimedImageReceived(0, image, packetMs, packetMs, assembledFrameMs,
                       assembledFrameMs, nowMs, nowMs);
}

void VideoBackgroundWidget::onTimedImageReceived(
    quint16 frameId, const QImage &image, qint64 firstPacketMs,
    qint64 lastPacketMs, qint64 assembleStartMs, qint64 assembleDoneMs,
    qint64 decodeStartMs, qint64 decodeDoneMs) {
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  {
    QMutexLocker locker(&m_imageMutex);
    m_lastReceivedFrameMs = nowMs;
    m_pendingFrameId = frameId;
    m_pendingAssembledFrameMs = assembleDoneMs;
    m_pendingPacketMs = lastPacketMs;
    m_pendingFirstPacketMs = firstPacketMs;
    m_pendingLastPacketMs = lastPacketMs;
    m_pendingAssembleStartMs = assembleStartMs;
    m_pendingDecodeStartMs = decodeStartMs;
    m_pendingDecodeDoneMs = decodeDoneMs;
    if (m_framePending) {
      ++m_droppedFrameCount;
      if (nowMs - m_lastRenderStallLogMs >= 1000) {
        qWarning().noquote()
            << QStringLiteral(
                   "RM26_VIDEO_STALL %1 stage=render_backlog duration_ms=%2 "
                   "frame_id=%3 dropped_frames=%4 last_render_complete_ms=%5")
                   .arg(runtimeLogFields())
                   .arg(m_lastRenderCompleteMs > 0
                            ? nowMs - m_lastRenderCompleteMs
                            : 0)
                   .arg(frameId)
                   .arg(m_droppedFrameCount)
                   .arg(m_lastRenderCompleteMs);
        m_lastRenderStallLogMs = nowMs;
      }
    }
    m_currentFrame = image;
    m_scaledFrameTargetSize = QSize();
    m_framePending = true;
  }
  schedulePresent(nowMs);
}

void VideoBackgroundWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
#ifdef RM_HAS_WEBENGINE
  if (m_webView) {
    m_webView->resize(size());
  }
#endif
  // 重新生成噪点图
  generateNoise();
  QMutexLocker locker(&m_imageMutex);
  refreshScaledFrameLocked();
}

void VideoBackgroundWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter painter(this);

  QImage scaledFrame;
  QImage presentedFrame;
  bool hadPendingFrame = false;
  quint16 frameId = 0;
  qint64 receivedFrameMs = 0;
  qint64 firstPacketMs = 0;
  qint64 lastPacketMs = 0;
  qint64 assembleStartMs = 0;
  qint64 assembleDoneMs = 0;
  qint64 decodeStartMs = 0;
  qint64 decodeDoneMs = 0;
  {
    QMutexLocker locker(&m_imageMutex);
    refreshScaledFrameLocked();
    scaledFrame = m_scaledFrame;
    presentedFrame = m_currentFrame;
    hadPendingFrame = m_framePending && !scaledFrame.isNull();
    if (hadPendingFrame) {
      frameId = m_pendingFrameId;
      receivedFrameMs = m_lastReceivedFrameMs;
      firstPacketMs = m_pendingFirstPacketMs;
      lastPacketMs = m_pendingLastPacketMs;
      assembleStartMs = m_pendingAssembleStartMs;
      assembleDoneMs = m_pendingAssembledFrameMs;
      decodeStartMs = m_pendingDecodeStartMs;
      decodeDoneMs = m_pendingDecodeDoneMs;
      m_framePending = false;
    }
  }

  // 1. 播放状态下绘制视频帧
  if (m_isPlaying && !scaledFrame.isNull()) {
    const int x = (width() - scaledFrame.width()) / 2;
    const int y = (height() - scaledFrame.height()) / 2;
    painter.drawImage(x, y, scaledFrame);
    bool shouldEmitFrame = false;
    QString renderTimingLog;
    const qint64 paintDoneMs = QDateTime::currentMSecsSinceEpoch();
    if (hadPendingFrame) {
      QMutexLocker locker(&m_imageMutex);
      ++m_paintFrameCount;
      m_lastRenderCompleteMs = paintDoneMs;
      m_lastPresentedFrameMs = paintDoneMs;
      ++m_presentedFrameCount;
      ++m_presentedFramesInLastSecond;
      if (m_lastPresentStatsMs > 0 &&
          (paintDoneMs - m_lastPresentStatsMs) >= 1000) {
        m_lastPresentedFps = m_presentedFramesInLastSecond * 1000.0 /
                             (paintDoneMs - m_lastPresentStatsMs);
        m_lastPresentStatsMs = paintDoneMs;
        m_presentedFramesInLastSecond = 0;
      }
      if (receivedFrameMs > 0) {
        m_lastReceiveToPresentMs = paintDoneMs - receivedFrameMs;
        m_totalReceiveToPresentMs += m_lastReceiveToPresentMs;
        ++m_receiveToPresentSamples;
      }
      if (assembleDoneMs > 0) {
        m_lastAssembledToPresentMs = paintDoneMs - assembleDoneMs;
        m_totalAssembledToPresentMs += m_lastAssembledToPresentMs;
        ++m_assembledToPresentSamples;
      }
      if (lastPacketMs > 0) {
        m_lastPacketToPresentMs = paintDoneMs - lastPacketMs;
        m_totalPacketToPresentMs += m_lastPacketToPresentMs;
        ++m_packetToPresentSamples;
      }
      shouldEmitFrame = !presentedFrame.isNull();
      renderTimingLog =
          QStringLiteral(
              "RM26_VIDEO_FRAME_TIMING %1 event_version=2 stage=render "
              "frame_id=%2 first_packet_ms=%3 last_packet_ms=%4 "
              "assemble_start_ms=%5 assemble_done_ms=%6 decode_start_ms=%7 "
              "decode_done_ms=%8 render_done_ms=%9 receive_span_ms=%10 "
              "last_packet_to_assemble_ms=%11 "
              "assemble_to_decode_start_ms=%12 decode_to_render_ms=%13 "
              "first_packet_to_render_ms=%14 last_packet_to_render_ms=%15 "
              "paint_count=%16 dropped_frames=%17 assemble_to_render_ms=%18 "
              "render_presented=1")
              .arg(runtimeLogFields())
              .arg(frameId)
              .arg(firstPacketMs)
              .arg(lastPacketMs)
              .arg(assembleStartMs)
              .arg(assembleDoneMs)
              .arg(decodeStartMs)
              .arg(decodeDoneMs)
              .arg(paintDoneMs)
              .arg(firstPacketMs > 0 && lastPacketMs > 0
                       ? lastPacketMs - firstPacketMs
                       : -1)
              .arg(lastPacketMs > 0 ? assembleDoneMs - lastPacketMs : -1)
              .arg(assembleDoneMs > 0 ? decodeStartMs - assembleDoneMs : -1)
              .arg(decodeDoneMs > 0 ? paintDoneMs - decodeDoneMs : -1)
              .arg(firstPacketMs > 0 ? paintDoneMs - firstPacketMs : -1)
              .arg(lastPacketMs > 0 ? paintDoneMs - lastPacketMs : -1)
              .arg(m_paintFrameCount)
              .arg(m_droppedFrameCount)
              .arg(assembleDoneMs > 0 ? paintDoneMs - assembleDoneMs : -1);
    }
    if (!renderTimingLog.isEmpty()
        && qEnvironmentVariableIsSet("RM_VERBOSE_VIDEO_LOG")) {
      qInfo().noquote() << renderTimingLog;
    }
    if (shouldEmitFrame) {
      emit frameUpdated(presentedFrame);
    }
  } else {
    // 没有视频帧时填充黑色背景
    painter.fillRect(rect(), Qt::black);
  }

  // 2. 叠加噪点
  if (m_noiseLevel > 0) {
    painter.setOpacity(m_noiseLevel / 255.0); // 按噪点等级调整透明度
    // 平铺噪点图像
    painter.drawTiledPixmap(rect(), QPixmap::fromImage(m_noiseImage));
    painter.setOpacity(1.0);
    // 网格和干扰条纹在正式模式下保持禁用，只保留噪点叠加。
  }

  // 5. 绘制信号丢失效果 (如果信号质量很低)
  if (m_signalQuality < 20) {
    painter.fillRect(rect(), QColor(0, 0, 0, 255 - m_signalQuality * 10));
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 24));
    painter.drawText(rect(), Qt::AlignCenter, "SIGNAL LOST");
  }
}

void VideoBackgroundWidget::onUpdateTimer() {
}

void VideoBackgroundWidget::refreshScaledFrameLocked() {
  if (m_currentFrame.isNull() || size().isEmpty()) {
    m_scaledFrame = QImage();
    m_scaledFrameTargetSize = QSize();
    return;
  }

  if (m_scaledFrameTargetSize == size() &&
      !m_scaledFrame.isNull() &&
      m_scaledFrame.size() == size()) {
    return;
  }

  const QImage frameForDisplay =
      m_rotateVideo180 ? m_currentFrame.mirrored(true, true) : m_currentFrame;
  m_scaledFrame = frameForDisplay.scaled(size(), Qt::IgnoreAspectRatio,
                                         Qt::FastTransformation);
  m_scaledFrameTargetSize = size();
}

void VideoBackgroundWidget::schedulePresent(qint64 nowMs) {
  Q_UNUSED(nowMs);
  // Qt 自动合并多余 update() 为一次 paintEvent，无需手动节流。
  // 定时器延迟是 120Hz 下延迟增大的主要来源。
  update();
}

void VideoBackgroundWidget::generateNoise() {
  if (width() <= 0 || height() <= 0)
    return;

  // 降低分辨率以提高性能
  int w = width();
  int h = height();

  if (m_noiseImage.width() != w || m_noiseImage.height() != h) {
    m_noiseImage = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
  }

  // 填充透明背景
  m_noiseImage.fill(Qt::transparent);

  QPainter p(&m_noiseImage);

  // 绘制规则的点阵网格 (模拟图传背景)
  int gridSize = 40; // 网格间距
  p.setPen(QColor(255, 255, 255, 30));

  for (int y = 0; y < h; y += gridSize) {
    for (int x = 0; x < w; x += gridSize) {
      // 绘制小十字或点
      if ((x + y) % (gridSize * 2) == 0) {
        p.drawPoint(x, y);
      } else {
        p.fillRect(x - 1, y - 1, 2, 2, QColor(255, 255, 255, 20));
      }
    }
  }

  // 添加随机噪点
  if (m_noiseLevel > 0) {
    for (int i = 0; i < (w * h * m_noiseLevel / 5000); ++i) {
      int x = QRandomGenerator::global()->bounded(w);
      int y = QRandomGenerator::global()->bounded(h);
      int alpha = QRandomGenerator::global()->bounded(100) + 50;
      p.setPen(QColor(255, 255, 255, alpha));
      p.drawPoint(x, y);
    }
  }
}

void VideoBackgroundWidget::drawGrid(QPainter &painter) {
  painter.setPen(QPen(QColor(255, 255, 255, 30), 1));

  // 绘制十字准线
  int centerX = width() / 2;
  int centerY = height() / 2;

  painter.drawLine(centerX, 0, centerX, height());
  painter.drawLine(0, centerY, width(), centerY);

  // 绘制三分线
  int w3 = width() / 3;
  int h3 = height() / 3;

  painter.drawLine(w3, 0, w3, height());
  painter.drawLine(w3 * 2, 0, w3 * 2, height());
  painter.drawLine(0, h3, width(), h3);
  painter.drawLine(0, h3 * 2, width(), h3 * 2);
}

void VideoBackgroundWidget::drawInterference(QPainter &painter) {
  // 模拟扫描线干扰
  painter.setPen(Qt::NoPen);
  QLinearGradient gradient(0, m_scanLinePos, 0, m_scanLinePos + 50);
  gradient.setColorAt(0, QColor(0, 255, 255, 0));
  gradient.setColorAt(0.5, QColor(0, 255, 255, 20));
  gradient.setColorAt(1, QColor(0, 255, 255, 0));

  painter.setBrush(gradient);
  painter.drawRect(0, m_scanLinePos, width(), 50);

  // 随机水平干扰条
  if (QRandomGenerator::global()->bounded(100) < 5) {
    int y = QRandomGenerator::global()->bounded(height());
    int h = QRandomGenerator::global()->bounded(20) + 5;
    painter.fillRect(0, y, width(), h, QColor(255, 255, 255, 30));
  }
}

} // namespace RM
