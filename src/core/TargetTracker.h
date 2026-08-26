// SPDX-License-Identifier: MIT
/**
 * @file TargetTracker.h
 * @brief 目标追踪器
 * @details 使用卡尔曼滤波对检测到的机器人进行平滑追踪，
 *          处理检测抖动和短暂遮挡，提供稳定的位置输出。
 * @author Clear
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#ifndef TARGETTRACKER_H
#define TARGETTRACKER_H

#include "YoloDetector.h"

#include <QElapsedTimer>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPointF>
#include <QRectF>

namespace RM {

/**
 * @struct KalmanState
 * @brief 卡尔曼滤波器状态
 * @details 用于平滑追踪目标位置和速度
 */
struct KalmanState {
  // 状态向量 [x, y, vx, vy, w, h]
  float x = 0.0f;  // 中心 x 坐标
  float y = 0.0f;  // 中心 y 坐标
  float vx = 0.0f; // x 方向速度
  float vy = 0.0f; // y 方向速度
  float w = 0.0f;  // 宽度
  float h = 0.0f;  // 高度

  // 协方差矩阵对角元素 (简化)
  float px = 1.0f;  // x 不确定性
  float py = 1.0f;  // y 不确定性
  float pvx = 1.0f; // vx 不确定性
  float pvy = 1.0f; // vy 不确定性
  float pw = 1.0f;  // w 不确定性
  float ph = 1.0f;  // h 不确定性

  // 过程噪声和测量噪声
  float processNoise = 0.01f;
  float measurementNoise = 0.1f;
};

/**
 * @struct TrackedTarget
 * @brief 追踪目标结构
 * @details 存储单个追踪目标的完整状态
 */
struct TrackedTarget {
  int robotId;              // 机器人ID
  int classId;              // 类别ID
  QString className;        // 类别名称
  float confidence;         // 当前置信度
  QRectF currentBox;        // 当前边界框 (原始检测)
  QRectF smoothedBox;       // 平滑后的边界框 (用于显示)
  QRectF predictedBox;      // 预测的边界框 (用于丢失时)
  int framesSinceSeen;      // 未检测到的帧数
  int totalFramesSeen;      // 总共被检测到的帧数
  qint64 lastSeenTimestamp; // 最后检测到的时间戳 (毫秒)
  KalmanState kalman;       // 卡尔曼滤波状态
  bool isActive;            // 是否活跃 (正在追踪)
  bool isVisible;           // 是否可见 (最近被检测到)

  TrackedTarget()
      : robotId(0), classId(-1), confidence(0.0f), framesSinceSeen(0),
        totalFramesSeen(0), lastSeenTimestamp(0), isActive(false),
        isVisible(false) {}
};

/**
 * @class TargetTracker
 * @brief 目标追踪器
 * @details 对检测结果进行时序平滑和预测
 */
class TargetTracker : public QObject {
  Q_OBJECT

public:
  explicit TargetTracker(QObject *parent = nullptr);
  ~TargetTracker();

  /**
   * @brief 更新追踪状态
   * @param detections 当前帧检测结果
   */
  void update(const QList<DetectionResult> &detections);

  /**
   * @brief 获取所有活跃目标
   * @return 活跃目标列表
   */
  QList<TrackedTarget> getActiveTargets() const;

  /**
   * @brief 获取可见目标
   * @param includeRecent 是否包含最近丢失的目标
   * @return 可见目标列表
   */
  QList<TrackedTarget> getVisibleTargets(bool includeRecent = true) const;

  /**
   * @brief 根据机器人ID获取追踪目标
   * @param robotId 机器人ID
   * @return 追踪目标指针 (如果不存在返回 nullptr)
   */
  const TrackedTarget *getTargetByRobotId(int robotId) const;

  /**
   * @brief 设置最大丢失帧数
   * @param frames 帧数 (超过此值后目标被移除)
   */
  void setMaxMissedFrames(int frames);

  /**
   * @brief 设置平滑因子
   * @param factor 平滑因子 (0.0-1.0, 越小越平滑)
   */
  void setSmoothingFactor(float factor);

  /**
   * @brief 设置激活阈值
   * @param frames 需要连续检测到的帧数才激活追踪
   */
  void setActivationThreshold(int frames);

  /**
   * @brief 清除所有追踪目标
   */
  void clear();

  /**
   * @brief 获取追踪统计信息
   * @return 当前追踪目标数量
   */
  int getActiveCount() const;

signals:
  /**
   * @brief 新目标出现
   * @param robotId 机器人ID
   */
  void targetAppeared(int robotId);

  /**
   * @brief 目标消失
   * @param robotId 机器人ID
   */
  void targetDisappeared(int robotId);

  /**
   * @brief 追踪更新完成
   * @param targets 活跃目标列表
   */
  void trackingUpdated(const QList<TrackedTarget> &targets);

private:
  // 卡尔曼滤波预测
  void kalmanPredict(TrackedTarget &target, float dt);

  // 卡尔曼滤波更新
  void kalmanUpdate(TrackedTarget &target, const QRectF &measurement);

  // 计算预测边界框
  QRectF calculatePredictedBox(const TrackedTarget &target) const;

  // 计算平滑边界框
  QRectF calculateSmoothedBox(const TrackedTarget &target) const;

  // 匹配检测结果到现有目标
  QMap<int, int>
  matchDetectionsToTargets(const QList<DetectionResult> &detections);

  // 计算 IoU
  float calculateIoU(const QRectF &a, const QRectF &b) const;

  // 创建新目标
  TrackedTarget createTarget(const DetectionResult &detection);

private:
  // --- 追踪目标 ---
  QMap<int, TrackedTarget> m_targets; // 机器人 ID 到追踪目标的映射。

  // --- 追踪参数 ---
  int m_maxMissedFrames = 10;     // 最大丢失帧数
  float m_smoothingFactor = 0.3f; // 平滑因子
  int m_activationThreshold = 3;  // 激活阈值
  float m_iouThreshold = 0.3f;    // IoU 匹配阈值

  // --- 时间跟踪 ---
  QElapsedTimer m_frameTimer;  // 帧计时器
  qint64 m_lastUpdateTime = 0; // 上次更新时间

  // --- 线程安全 ---
  mutable QMutex m_mutex;
};

} // namespace RM

#endif // TARGETTRACKER_H
