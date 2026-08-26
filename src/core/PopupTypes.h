/**
 * @file PopupTypes.h
 * @brief 弹窗类型定义
 */
#pragma once

#include <QVariantMap>

namespace Popup {

enum class PopupType {
  PrepPhase = 0,
  Countdown = 1,
  RobotRespawn = 2,
  Out = 3,
  BattlePause = 4,
};

enum class PopupPriority {
  Low = 0,
  Normal = 1,
  High = 2,
  Critical = 3,
};

enum class PopupIntent {
  Show = 0,
  Dismiss = 1,
};

struct PopupEntry {
  PopupType type;
  PopupPriority prio;
  PopupIntent intent;
  QVariantMap payload;
  qint64 seq = 0;
};

} // namespace Popup
