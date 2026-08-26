// SPDX-License-Identifier: MIT
/**
 * @file GameConstants.h
 * @brief 比赛常量定义
 * @details 集中定义所有比赛相关的常量值，避免在代码中硬编码。
 *          包括机器人血量、比赛时间、模块配置等。
 * @author Fudan EGA Team
 * @date 2025-12-07
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#ifndef GAMECONSTANTS_H
#define GAMECONSTANTS_H

#include <QString>
#include <QStringList>

namespace RM {

/**
 * @brief 比赛时间常量
 */
namespace GameTime {
constexpr int MATCH_DURATION_SECONDS = 420; // 7分钟 = 420秒
constexpr int PREPARATION_TIME = 30;        // 准备阶段时间
constexpr int COUNTDOWN_TIME = 5;           // 倒计时时间
constexpr int UPDATE_INTERVAL_MS = 100;     // 定时器更新间隔 (ms)

} // namespace GameTime

/**
 * @brief 基地相关常量
 */
namespace Base {
constexpr int DEFAULT_HP = 5000; // 基地默认血量
constexpr int MAX_HP = 5000;     // 基地最大血量
constexpr int SHIELD_HP = 2000;  // 虚拟护盾血量
} // namespace Base

/**
 * @brief 前哨站相关常量
 */
namespace Outpost {
constexpr int DEFAULT_HP = 1500; // 前哨站默认血量
constexpr int MAX_HP = 1500;     // 前哨站最大血量
constexpr int HEALTH_DROP_ALERT_WINDOW_MS = 5000;
constexpr int HEALTH_DROP_ALERT_SAMPLE_RETENTION_MS =
    HEALTH_DROP_ALERT_WINDOW_MS + 1000;
constexpr int HEALTH_DROP_ALERT_COOLDOWN_MS = 15000;
constexpr int HEALTH_DROP_ALERT_THRESHOLD = 200;
} // namespace Outpost

/**
 * @brief 哨兵相关常量
 */
namespace Sentry {
constexpr int DEFAULT_HP = 600; // 哨兵默认血量
constexpr int MAX_HP = 600;     // 哨兵最大血量
} // namespace Sentry

/**
 * @brief 机器人血量配置
 */
namespace RobotHP {
// 各类型机器人默认血量
constexpr int HERO = 200;     // 英雄
constexpr int ENGINEER = 250; // 工程
constexpr int INFANTRY = 200; // 步兵
constexpr int AERIAL = 250;   // 空中
constexpr int SENTRY = 600;   // 哨兵
constexpr int DEFAULT = 600;  // 默认血量
} // namespace RobotHP

/**
 * @brief 机器人属性配置
 */
namespace RobotStats {
constexpr int DEFAULT_LEVEL = 1;     // 默认等级
constexpr int MAX_EXPERIENCE = 1000; // 最大经验值
constexpr int MAX_POWER = 120;       // 最大功率 (W)
constexpr int HEAT_LIMIT = 240;      // 热量上限
constexpr int BULLET_LIMIT = 500;    // 弹丸上限
constexpr int HERO_BULLET_SPEED_LIMIT = 12;    // 英雄射速上限（m/s）
constexpr int ROBOT_BULLET_SPEED_LIMIT = 25;   // (除英雄外）机器人射速上限（m/s）
} // namespace RobotStats

/**
 * @brief 增益点配置
 */
namespace BuffPoint {
constexpr int COUNT = 3;         // 增益点数量
constexpr int MAX_ENERGY = 1000; // 最大能量值
} // namespace BuffPoint

/**
 * @brief 模块配置
 */
namespace Module {
constexpr int DEFAULT_TEMPERATURE = 25; // 默认温度 (°C)
constexpr int DEFAULT_VOLTAGE = 24;     // 默认电压 (V)

// 模块名称列表
inline QStringList getDefaultModuleNames() {
  return {"云台", "底盘", "发射机构", "超级电容"};
}
} // namespace Module

/**
 * @brief 机器人ID范围
 */
namespace RobotId {
// 红方机器人 ID: 1-7
constexpr int RED_HERO = 1;
constexpr int RED_ENGINEER = 2;
constexpr int RED_INFANTRY_3 = 3;
constexpr int RED_INFANTRY_4 = 4;
constexpr int RED_INFANTRY_5 = 5;
constexpr int RED_AERIAL = 6;
constexpr int RED_SENTRY = 7;

// 蓝方机器人 ID: 101-107
constexpr int BLUE_HERO = 101;
constexpr int BLUE_ENGINEER = 102;
constexpr int BLUE_INFANTRY_3 = 103;
constexpr int BLUE_INFANTRY_4 = 104;
constexpr int BLUE_INFANTRY_5 = 105;
constexpr int BLUE_AERIAL = 106;
constexpr int BLUE_SENTRY = 107;

// ID 范围
constexpr int RED_MIN = 1;
constexpr int RED_MAX = 7;
constexpr int BLUE_MIN = 101;
constexpr int BLUE_MAX = 107;
} // namespace RobotId

namespace Dart {
  constexpr int dartTotal = 4; // 总 Dart 数

  // 飞镖命中后图传界面遮挡时间（秒），根据 RoboMaster 2026 规则：
  // 前哨站/基地固定目标/基地随机固定目标按同队飞镖命中序号依次为 10/5/3/2 秒；
  // 基地随机移动目标/基地末端移动目标固定为 10 秒。
  constexpr int kOcclusionFixedHit1 = 10;
  constexpr int kOcclusionFixedHit2 = 5;
  constexpr int kOcclusionFixedHit3 = 3;
  constexpr int kOcclusionFixedHit4 = 2;
  constexpr int kOcclusionMovingTarget = 10;

  inline bool isFixedDurationTarget(int targetId) {
    switch (targetId) {
    case 1: // 前哨站
    case 2: // 基地固定目标
    case 3: // 基地随机固定目标
      return true;
    default:
      return false;
    }
  }

  inline int fixedTargetOcclusionDurationByHitCount(int teamDartHitCount) {
    switch (teamDartHitCount) {
    case 0:
    case 1: return kOcclusionFixedHit1;
    case 2: return kOcclusionFixedHit2;
    case 3: return kOcclusionFixedHit3;
    case 4: return kOcclusionFixedHit4;
    default: return kOcclusionFixedHit4;
    }
  }

  inline int dartOcclusionDurationSeconds(int targetId, int teamDartHitCount) {
    switch (targetId) {
    case 1:
    case 2:
    case 3:
      return fixedTargetOcclusionDurationByHitCount(teamDartHitCount);
    case 4: // 基地随机移动目标
    case 5: // 基地末端移动目标
      return kOcclusionMovingTarget;
    default:
      return kOcclusionMovingTarget;
    }
  }
} // namespace Dart



} // namespace RM

#endif // GAMECONSTANTS_H
