// SPDX-License-Identifier: MIT
/**
 * @file AROverlayManager.h
 * @brief AR 叠加管理器
 * @details 协调目标检测、追踪和渲染，管理 AR 系统的生命周期。
 *          作为旁路系统独立运行，不影响主图传流程。
 * @author Clear
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#ifndef AROVERLAYMANAGER_H
#define AROVERLAYMANAGER_H

#include "../core/TargetTracker.h"
#include "../core/YoloDetector.h"

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QThread>
#include <QTimer>

#include "../core/GameData.h"

namespace RM {

class AROverlayWidget;

/**
 * @class DetectionWorker
 * @brief 检测工作线程
 * @details 在独立线程中运行 YOLO 检测，避免阻塞主线程
 */
class DetectionWorker : public QObject {
  Q_OBJECT

public:
  explicit DetectionWorker(QObject *parent = nullptr);
  ~DetectionWorker();

  void setDetector(YoloDetector *detector);
  void setTracker(TargetTracker *tracker);

public slots:
  void processFrame(const QImage &frame);
  void stop();

signals:
  void detectionCompleted(const QList<TrackedTarget> &targets);
  void processingTimeUpdated(float timeMs);

private:
  YoloDetector *m_detector = nullptr;
  TargetTracker *m_tracker = nullptr;
  bool m_running = true;
};

/**
 * @class AROverlayManager
 * @brief AR 叠加管理器
 * @details 管理整个 AR 系统的生命周期
 */
class AROverlayManager : public QObject {
  Q_OBJECT

public:
  explicit AROverlayManager(GameData *gameData, QObject *parent = nullptr);
  ~AROverlayManager();

  /**
   * @brief 初始化 AR 系统
   * @param modelPath YOLO 模型路径
   * @return 是否初始化成功
   */
  bool initialize(const QString &modelPath);

  /**
   * @brief 启动 AR 系统
   */
  void start();

  /**
   * @brief 停止 AR 系统
   */
  void stop();

  /**
   * @brief 是否已启用
   */
  bool isEnabled() const { return m_enabled; }

  /**
   * @brief 设置启用状态
   * @param enabled 是否启用
   */
  void setEnabled(bool enabled);

  /**
   * @brief 获取叠加控件
   * @return AR 叠加控件指针
   */
  AROverlayWidget *getOverlayWidget() const { return m_overlayWidget; }

  /**
   * @brief 设置叠加控件
   * @param widget AR 叠加控件
   */
  void setOverlayWidget(AROverlayWidget *widget);

  // 配置方法
  void setConfidenceThreshold(float threshold);
  void setNMSThreshold(float threshold);
  void setSmoothingFactor(float factor);
  void setMaxMissedFrames(int frames);
  void setDetectionInterval(int ms);

  // 状态查询
  bool isInitialized() const { return m_initialized; }
  bool isModelLoaded() const;
  float getLastProcessingTime() const { return m_lastProcessingTime; }
  int getActiveTargetCount() const;

public slots:
  /**
   * @brief 处理新视频帧
   * @param frame 视频帧
   */
  void processFrame(const QImage &frame);

signals:
  /**
   * @brief AR 系统状态变化
   * @param enabled 是否启用
   */
  void enabledChanged(bool enabled);

  /**
   * @brief 检测完成
   * @param targets 追踪目标列表
   */
  void targetsUpdated(const QList<TrackedTarget> &targets);

  /**
   * @brief 处理时间更新
   * @param timeMs 处理时间 (毫秒)
   */
  void processingTimeUpdated(float timeMs);

  /**
   * @brief 初始化完成
   * @param success 是否成功
   * @param message 消息
   */
  void initializationCompleted(bool success, const QString &message);

private slots:
  void onDetectionCompleted(const QList<TrackedTarget> &targets);
  void onProcessingTimeUpdated(float timeMs);

private:
  // --- 核心组件 ---
  GameData *m_gameData = nullptr;             // 比赛数据中心
  YoloDetector *m_detector = nullptr;         // YOLO 检测器
  TargetTracker *m_tracker = nullptr;         // 目标追踪器
  AROverlayWidget *m_overlayWidget = nullptr; // 叠加渲染控件

  // --- 工作线程 ---
  QThread *m_workerThread = nullptr;   // 工作线程
  DetectionWorker *m_worker = nullptr; // 检测工作者

  // --- 帧处理 ---
  QTimer *m_frameTimer = nullptr; // 帧处理定时器
  QImage m_currentFrame;          // 当前帧
  QMutex m_frameMutex;            // 帧互斥锁
  bool m_frameReady = false;      // 是否有新帧待处理

  // --- 状态 ---
  bool m_enabled = false;            // 是否启用
  bool m_initialized = false;        // 是否已初始化
  float m_lastProcessingTime = 0.0f; // 上次处理时间

  // --- 配置 ---
  int m_detectionInterval = 33; // 检测间隔 (毫秒, ~30 FPS)
};

} // namespace RM

#endif // AROVERLAYMANAGER_H
