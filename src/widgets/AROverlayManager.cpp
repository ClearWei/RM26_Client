// SPDX-License-Identifier: MIT
/**
 * @file AROverlayManager.cpp
 * @brief AR 叠加管理器实现
 * @author Clear
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#include "AROverlayManager.h"
#include "../core/GameData.h"
#include "AROverlayWidget.h"

#include <QDebug>

namespace RM {

// ============================================================================
// DetectionWorker 实现
// ============================================================================

DetectionWorker::DetectionWorker(QObject *parent) : QObject(parent) {}

DetectionWorker::~DetectionWorker() = default;

void DetectionWorker::setDetector(YoloDetector *detector) {
  m_detector = detector;
}

void DetectionWorker::setTracker(TargetTracker *tracker) {
  m_tracker = tracker;
}

void DetectionWorker::processFrame(const QImage &frame) {
  if (!m_running || !m_detector || !m_tracker) {
    return;
  }

  if (frame.isNull()) {
    return;
  }

  // 执行检测
  QList<DetectionResult> detections = m_detector->detect(frame);

  // 更新追踪
  m_tracker->update(detections);

  // 获取活跃目标
  QList<TrackedTarget> targets = m_tracker->getVisibleTargets();

  // 发送结果
  emit detectionCompleted(targets);
  emit processingTimeUpdated(m_detector->getLastInferenceTime());
}

void DetectionWorker::stop() { m_running = false; }

// ============================================================================
// AROverlayManager 实现
// ============================================================================

AROverlayManager::AROverlayManager(GameData *gameData, QObject *parent)
    : QObject(parent), m_gameData(gameData) {

  // 创建核心组件
  m_detector = new YoloDetector(this);
  m_tracker = new TargetTracker(this);

  // 创建帧处理定时器
  m_frameTimer = new QTimer(this);
  connect(m_frameTimer, &QTimer::timeout, this, [this]() {
    QMutexLocker locker(&m_frameMutex);
    if (m_frameReady && !m_currentFrame.isNull()) {
      QImage frameCopy = m_currentFrame.copy();
      m_frameReady = false;
      locker.unlock();

      // 发送到工作线程
      if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "processFrame",
                                  Qt::QueuedConnection,
                                  Q_ARG(QImage, frameCopy));
      }
    }
  });
}

AROverlayManager::~AROverlayManager() {
  stop();

  if (m_workerThread) {
    m_workerThread->quit();
    m_workerThread->wait();
    delete m_workerThread;
  }
}

bool AROverlayManager::initialize(const QString &modelPath) {
  qDebug() << "AROverlayManager: 初始化 AR 系统...";

  // 加载模型
  if (!m_detector->loadModel(modelPath)) {
    QString error = "加载 YOLO 模型失败: " + modelPath;
    qWarning() << "AROverlayManager:" << error;
    emit initializationCompleted(false, error);
    return false;
  }

  // 创建工作线程
  m_workerThread = new QThread(this);
  m_worker = new DetectionWorker();
  m_worker->setDetector(m_detector);
  m_worker->setTracker(m_tracker);
  m_worker->moveToThread(m_workerThread);

  // 连接信号
  connect(m_worker, &DetectionWorker::detectionCompleted, this,
          &AROverlayManager::onDetectionCompleted, Qt::QueuedConnection);
  connect(m_worker, &DetectionWorker::processingTimeUpdated, this,
          &AROverlayManager::onProcessingTimeUpdated, Qt::QueuedConnection);

  // 线程结束时清理
  connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

  m_workerThread->start();

  m_initialized = true;
  qDebug() << "AROverlayManager: AR 系统初始化成功";
  emit initializationCompleted(true, "AR 系统初始化成功");

  return true;
}

void AROverlayManager::start() {
  if (!m_initialized) {
    qWarning() << "AROverlayManager: 系统未初始化";
    return;
  }

  if (m_enabled) {
    return; // 已经启动
  }

  m_enabled = true;
  m_frameTimer->start(m_detectionInterval);

  qDebug() << "AROverlayManager: AR 系统已启动";
  emit enabledChanged(true);
}

void AROverlayManager::stop() {
  if (!m_enabled) {
    return;
  }

  m_enabled = false;
  m_frameTimer->stop();

  if (m_worker) {
    QMetaObject::invokeMethod(m_worker, "stop", Qt::QueuedConnection);
  }

  qDebug() << "AROverlayManager: AR 系统已停止";
  emit enabledChanged(false);
}

void AROverlayManager::setEnabled(bool enabled) {
  if (enabled) {
    start();
  } else {
    stop();
  }
}

void AROverlayManager::setOverlayWidget(AROverlayWidget *widget) {
  m_overlayWidget = widget;

  if (m_overlayWidget && m_gameData) {
    m_overlayWidget->setGameData(m_gameData);
  }
}

void AROverlayManager::setConfidenceThreshold(float threshold) {
  m_detector->setConfidenceThreshold(threshold);
}

void AROverlayManager::setNMSThreshold(float threshold) {
  m_detector->setNMSThreshold(threshold);
}

void AROverlayManager::setSmoothingFactor(float factor) {
  m_tracker->setSmoothingFactor(factor);
}

void AROverlayManager::setDetectionInterval(int ms) {
  m_detectionInterval = std::max(16, ms); // 最少 ~60 FPS

  if (m_frameTimer->isActive()) {
    m_frameTimer->setInterval(m_detectionInterval);
  }
}

void AROverlayManager::setMaxMissedFrames(int frames) {
  m_tracker->setMaxMissedFrames(frames);
}

bool AROverlayManager::isModelLoaded() const {
  return m_detector->isModelLoaded();
}

int AROverlayManager::getActiveTargetCount() const {
  return m_tracker->getActiveCount();
}

void AROverlayManager::processFrame(const QImage &frame) {
  if (!m_enabled || frame.isNull()) {
    return;
  }

  QMutexLocker locker(&m_frameMutex);
  m_currentFrame = frame;
  m_frameReady = true;
}

void AROverlayManager::onDetectionCompleted(
    const QList<TrackedTarget> &targets) {
  // 更新叠加控件
  if (m_overlayWidget) {
    m_overlayWidget->updateTargets(targets);
  }

  emit targetsUpdated(targets);
}

void AROverlayManager::onProcessingTimeUpdated(float timeMs) {
  m_lastProcessingTime = timeMs;
  emit processingTimeUpdated(timeMs);
}

} // namespace RM
