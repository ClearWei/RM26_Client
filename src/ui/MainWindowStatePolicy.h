#ifndef MAINWINDOWSTATEPOLICY_H
#define MAINWINDOWSTATEPOLICY_H

#include "../core/GameData.h"

#include <algorithm>
#include <QString>

namespace RM::MainWindowStatePolicy {

inline QString resolvedActiveMqttClientId(const QString &currentClientId,
                                          const QString &switchClientId) {
  const QString normalizedSwitchClientId = switchClientId.trimmed();
  return normalizedSwitchClientId.isEmpty() ? currentClientId
                                             : normalizedSwitchClientId;
}

inline bool shouldScheduleMqttRetry(const QString &error) {
  const QString normalizedError = error.trimmed().toLower();
  if (normalizedError.isEmpty()) {
    return false;
  }

  return normalizedError.contains(QStringLiteral("transport error")) ||
         normalizedError.contains(QStringLiteral("server unavailable")) ||
         normalizedError.contains(QStringLiteral("connection refused")) ||
         normalizedError.contains(QStringLiteral("unreachable")) ||
         normalizedError.contains(QStringLiteral("timed out"));
}

inline bool shouldShowSiloPanel(int robotId, GameStage stage,
                                bool tacticalMode) {
  const int normalizedRobotId = robotId >= 100 ? robotId - 100 : robotId;
  return normalizedRobotId == 6 && stage != GameStage::SETTLEMENT &&
         !tacticalMode;
}

inline bool shouldRequestSiloOpen(int robotId, GameStage stage,
                                  bool tacticalMode, bool isUnmodifiedF,
                                  bool isAutoRepeat, bool panelReady) {
  return shouldShowSiloPanel(robotId, stage, tacticalMode) &&
         stage == GameStage::BATTLE && isUnmodifiedF && !isAutoRepeat &&
         panelReady;
}

inline bool isQuickPanelEffectivelyVisible(bool widgetVisible,
                                           bool rootVisible) {
  return widgetVisible && rootVisible;
}

inline bool shouldOpenQuickPanelOnToggle(bool widgetVisible,
                                         bool rootVisible) {
  return !isQuickPanelEffectivelyVisible(widgetVisible, rootVisible);
}

inline int remainingDartOcclusionSeconds(int hitRemainingTime,
                                         int occlusionDurationSec,
                                         int currentGameTime) {
  if (hitRemainingTime < 0 || occlusionDurationSec <= 0) {
    return 0;
  }
  return std::max(
      0, occlusionDurationSec - (hitRemainingTime - currentGameTime) + 1);
}

inline int accumulatedDartOcclusionSeconds(int hitRemainingTime,
                                           int occlusionDurationSec,
                                           int currentGameTime,
                                           int newOcclusionDurationSec) {
  return remainingDartOcclusionSeconds(hitRemainingTime, occlusionDurationSec,
                                       currentGameTime) +
         std::max(0, newOcclusionDurationSec);
}

} // namespace RM::MainWindowStatePolicy

#endif // MAINWINDOWSTATEPOLICY_H
