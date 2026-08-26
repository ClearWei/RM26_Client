// SPDX-License-Identifier: MIT
/**
 * @file TargetTracker.cpp
 * @brief 目标追踪器实现
 * @details 使用卡尔曼滤波进行平滑追踪
 * @author Clear
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#include "TargetTracker.h"

#include <QDateTime>
#include <QDebug>

#include <algorithm>
#include <cmath>

namespace RM {

// ============================================================================
// 构造函数和析构函数
// ============================================================================

TargetTracker::TargetTracker(QObject *parent) : QObject(parent) {
  m_frameTimer.start();
  m_lastUpdateTime = m_frameTimer.elapsed();
}

TargetTracker::~TargetTracker() = default;

// ============================================================================
// 公共方法
// ============================================================================

void TargetTracker::update(const QList<DetectionResult> &detections) {
  QMutexLocker locker(&m_mutex);

  // 计算时间差
  qint64 currentTime = m_frameTimer.elapsed();
  float dt = (currentTime - m_lastUpdateTime) / 1000.0f; // 秒
  m_lastUpdateTime = currentTime;

  // 限制 dt 范围，避免异常值
  dt = std::clamp(dt, 0.001f, 0.5f);

  // 1. 对所有现有目标进行预测
  for (auto &target : m_targets) {
    kalmanPredict(target, dt);
    target.framesSinceSeen++;
  }

  // 2. 匹配检测结果到现有目标
  QMap<int, int> matches = matchDetectionsToTargets(detections);

  // 3. 更新匹配的目标
  QSet<int> matchedRobotIds;
  for (auto it = matches.begin(); it != matches.end(); ++it) {
    int detectionIdx = it.key();
    int robotId = it.value();

    const DetectionResult &det = detections[detectionIdx];
    TrackedTarget &target = m_targets[robotId];

    // 卡尔曼更新
    kalmanUpdate(target, det.boundingBox);

    // 更新目标状态
    target.currentBox = det.boundingBox;
    target.confidence = det.confidence;
    target.framesSinceSeen = 0;
    target.totalFramesSeen++;
    target.lastSeenTimestamp = currentTime;
    target.isVisible = true;

    // 激活追踪
    if (!target.isActive && target.totalFramesSeen >= m_activationThreshold) {
      target.isActive = true;
      emit targetAppeared(robotId);
    }

    // 更新平滑边界框
    target.smoothedBox = calculateSmoothedBox(target);
    target.predictedBox = calculatePredictedBox(target);

    matchedRobotIds.insert(robotId);
  }

  // 4. 处理新检测 (创建新目标)
  for (int i = 0; i < detections.size(); ++i) {
    if (matches.contains(i)) {
      continue; // 已匹配
    }

    const DetectionResult &det = detections[i];

    // 检查是否已存在该 robotId 的目标
    if (m_targets.contains(det.robotId)) {
      // 可能是同一机器人的重复检测，跳过
      continue;
    }

    // 创建新目标
    TrackedTarget newTarget = createTarget(det);
    m_targets[det.robotId] = newTarget;
  }

  // 5. 处理丢失的目标
  QList<int> toRemove;
  for (auto it = m_targets.begin(); it != m_targets.end(); ++it) {
    TrackedTarget &target = it.value();

    if (!matchedRobotIds.contains(it.key())) {
      // 目标未被检测到
      target.isVisible = false;

      // 使用预测位置
      target.smoothedBox = target.predictedBox;

      // 检查是否超过最大丢失帧数
      if (target.framesSinceSeen > m_maxMissedFrames) {
        if (target.isActive) {
          emit targetDisappeared(it.key());
        }
        toRemove.append(it.key());
      }
    }
  }

  // 移除失效目标
  for (int robotId : toRemove) {
    m_targets.remove(robotId);
  }

  // 6. 发送更新信号
  emit trackingUpdated(getActiveTargets());
}

QList<TrackedTarget> TargetTracker::getActiveTargets() const {
  QMutexLocker locker(&m_mutex);
  QList<TrackedTarget> result;

  for (const auto &target : m_targets) {
    if (target.isActive) {
      result.append(target);
    }
  }

  return result;
}

QList<TrackedTarget>
TargetTracker::getVisibleTargets(bool includeRecent) const {
  QMutexLocker locker(&m_mutex);
  QList<TrackedTarget> result;

  for (const auto &target : m_targets) {
    if (target.isActive) {
      if (target.isVisible || (includeRecent && target.framesSinceSeen <= 5)) {
        result.append(target);
      }
    }
  }

  return result;
}

const TrackedTarget *TargetTracker::getTargetByRobotId(int robotId) const {
  QMutexLocker locker(&m_mutex);

  auto it = m_targets.find(robotId);
  if (it != m_targets.end()) {
    return &it.value();
  }

  return nullptr;
}

void TargetTracker::setMaxMissedFrames(int frames) {
  QMutexLocker locker(&m_mutex);
  m_maxMissedFrames = std::max(1, frames);
}

void TargetTracker::setSmoothingFactor(float factor) {
  QMutexLocker locker(&m_mutex);
  m_smoothingFactor = std::clamp(factor, 0.0f, 1.0f);
}

void TargetTracker::setActivationThreshold(int frames) {
  QMutexLocker locker(&m_mutex);
  m_activationThreshold = std::max(1, frames);
}

void TargetTracker::clear() {
  QMutexLocker locker(&m_mutex);
  m_targets.clear();
}

int TargetTracker::getActiveCount() const {
  QMutexLocker locker(&m_mutex);
  int count = 0;

  for (const auto &target : m_targets) {
    if (target.isActive) {
      count++;
    }
  }

  return count;
}

// ============================================================================
// 私有方法
// ============================================================================

void TargetTracker::kalmanPredict(TrackedTarget &target, float dt) {
  KalmanState &k = target.kalman;

  // 状态预测: x' = x + vx*dt
  k.x += k.vx * dt;
  k.y += k.vy * dt;

  // 协方差预测: P' = P + Q
  k.px += k.processNoise;
  k.py += k.processNoise;
  k.pvx += k.processNoise;
  k.pvy += k.processNoise;
  k.pw += k.processNoise * 0.5f;
  k.ph += k.processNoise * 0.5f;
}

void TargetTracker::kalmanUpdate(TrackedTarget &target,
                                 const QRectF &measurement) {
  KalmanState &k = target.kalman;

  // 测量值
  float mx = measurement.x() + measurement.width() / 2.0f;
  float my = measurement.y() + measurement.height() / 2.0f;
  float mw = measurement.width();
  float mh = measurement.height();

  // 卡尔曼增益: K = P / (P + R)
  float kx = k.px / (k.px + k.measurementNoise);
  float ky = k.py / (k.py + k.measurementNoise);
  float kw = k.pw / (k.pw + k.measurementNoise);
  float kh = k.ph / (k.ph + k.measurementNoise);

  // 计算速度 (通过位置变化)
  float newVx = (mx - k.x) / 0.033f; // 假设 30 FPS
  float newVy = (my - k.y) / 0.033f;

  // 速度平滑
  float kvx = k.pvx / (k.pvx + k.measurementNoise);
  float kvy = k.pvy / (k.pvy + k.measurementNoise);

  // 状态更新
  k.x = k.x + kx * (mx - k.x);
  k.y = k.y + ky * (my - k.y);
  k.vx = k.vx + kvx * (newVx - k.vx);
  k.vy = k.vy + kvy * (newVy - k.vy);
  k.w = k.w + kw * (mw - k.w);
  k.h = k.h + kh * (mh - k.h);

  // 协方差更新: P' = (1-K) * P
  k.px = (1 - kx) * k.px;
  k.py = (1 - ky) * k.py;
  k.pvx = (1 - kvx) * k.pvx;
  k.pvy = (1 - kvy) * k.pvy;
  k.pw = (1 - kw) * k.pw;
  k.ph = (1 - kh) * k.ph;
}

QRectF TargetTracker::calculatePredictedBox(const TrackedTarget &target) const {
  const KalmanState &k = target.kalman;

  // 预测下一帧位置 (假设 33ms 后)
  float predX = k.x + k.vx * 0.033f;
  float predY = k.y + k.vy * 0.033f;

  return QRectF(predX - k.w / 2, predY - k.h / 2, k.w, k.h);
}

QRectF TargetTracker::calculateSmoothedBox(const TrackedTarget &target) const {
  const KalmanState &k = target.kalman;

  // 使用卡尔曼滤波后的状态作为平滑位置
  return QRectF(k.x - k.w / 2, k.y - k.h / 2, k.w, k.h);
}

QMap<int, int> TargetTracker::matchDetectionsToTargets(
    const QList<DetectionResult> &detections) {
  QMap<int, int> matches; // 检测结果索引到机器人 ID 的映射。

  // 简单贪婪匹配，优先匹配 robotId 相同的
  QSet<int> usedTargets;

  for (int i = 0; i < detections.size(); ++i) {
    const DetectionResult &det = detections[i];
    int bestMatch = -1;
    float bestIoU = m_iouThreshold;

    // 首先尝试匹配相同 robotId
    if (m_targets.contains(det.robotId) && !usedTargets.contains(det.robotId)) {
      const TrackedTarget &target = m_targets[det.robotId];
      float iou = calculateIoU(det.boundingBox, target.predictedBox);

      if (iou >= m_iouThreshold * 0.5f) { // 对相同 ID 放宽阈值
        bestMatch = det.robotId;
        bestIoU = iou;
      }
    }

    // 如果没有相同 ID 匹配，尝试最近位置匹配
    if (bestMatch < 0) {
      for (auto it = m_targets.begin(); it != m_targets.end(); ++it) {
        if (usedTargets.contains(it.key())) {
          continue;
        }

        // 只匹配相同类别
        if (it->classId != det.classId) {
          continue;
        }

        float iou = calculateIoU(det.boundingBox, it->predictedBox);
        if (iou > bestIoU) {
          bestIoU = iou;
          bestMatch = it.key();
        }
      }
    }

    if (bestMatch >= 0) {
      matches[i] = bestMatch;
      usedTargets.insert(bestMatch);
    }
  }

  return matches;
}

float TargetTracker::calculateIoU(const QRectF &a, const QRectF &b) const {
  QRectF intersection = a.intersected(b);

  if (intersection.isEmpty()) {
    return 0.0f;
  }

  float intersectionArea = intersection.width() * intersection.height();
  float unionArea =
      a.width() * a.height() + b.width() * b.height() - intersectionArea;

  if (unionArea <= 0) {
    return 0.0f;
  }

  return intersectionArea / unionArea;
}

TrackedTarget TargetTracker::createTarget(const DetectionResult &detection) {
  TrackedTarget target;

  target.robotId = detection.robotId;
  target.classId = detection.classId;
  target.className = detection.className;
  target.confidence = detection.confidence;
  target.currentBox = detection.boundingBox;
  target.smoothedBox = detection.boundingBox;
  target.predictedBox = detection.boundingBox;
  target.framesSinceSeen = 0;
  target.totalFramesSeen = 1;
  target.lastSeenTimestamp = m_frameTimer.elapsed();
  target.isActive = false;
  target.isVisible = true;

  // 初始化卡尔曼状态
  KalmanState &k = target.kalman;
  k.x = detection.boundingBox.x() + detection.boundingBox.width() / 2.0f;
  k.y = detection.boundingBox.y() + detection.boundingBox.height() / 2.0f;
  k.vx = 0;
  k.vy = 0;
  k.w = detection.boundingBox.width();
  k.h = detection.boundingBox.height();

  return target;
}

} // namespace RM
