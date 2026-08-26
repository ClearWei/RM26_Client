// SPDX-License-Identifier: MIT
/**
 * @file GameData.h
 * @brief 比赛数据中心定义
 * @details 负责管理比赛中的所有核心数据，包括机器人状态、比赛进程、基地血量等。
 *          作为数据中心，它接收来自网络的更新，并向UI组件发送信号。
 * @author Clear
 * @date 2025-11-29
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#ifndef GAMEDATA_H
#define GAMEDATA_H

#include "../network/Protocol.h"
#include "TimedEventRules.h"
#include <QDateTime>
#include <QImage>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QtGlobal>

namespace robomaster {
class RobotStatus;
class GameInfo;
class GameStatus;
class Event;
class VideoControl;
class RoboMasterMessage;
class GlobalUnitStatus;
class GlobalLogisticsStatus;
class GlobalSpecialMechanism;
class RadarInfoToClient;
class RobotPathPlanInfo;
class RobotInjuryStat;
class RobotModuleStatus; // 模块状态 Protobuf 前向声明
class RobotStaticStatus;
class RobotDynamicStatus;
class TechCoreMotionStateSync;
class DeployModeStatusSync;
class AirSupportStatusSync;
class RuneStatusSync;
class RobotPosition;
class GroundRobotPosition;
class MapRobotData;
class RadarMarkData;
class Buff;
class PenaltyInfo;
class RefereeWarningData;
} // namespace robomaster

class PopupStateMachine;

/**
 * @enum RobotStatus
 * @brief 机器人状态枚举
 */
enum class RobotStatus {
  NORMAL = 0,     // 正常
  OFFLINE = 1,    // 离线
  DAMAGED = 2,    // 受损
  DESTROYED = 3,  // 被摧毁
  INVINCIBLE = 4, // 无敌状态
  REVIVING = 5,   // 复活中

  // 为了兼容其他文件中的使用，添加别名
  Normal = NORMAL,
  Dead = DESTROYED,
  Disconnected = OFFLINE,
  YellowCard = 6, // 黄牌
  RedCard = 7,    // 红牌
  Punished = 8    // 被罚下
};

/**
 * @enum GameStage
 * @brief 比赛阶段枚举
 * @details 按照 RoboMaster 2026 官方协议定义:
 *          0: 未开始比赛
 *          1: 准备阶段
 *          2: 十五秒裁判系统自检阶段
 *          3: 五秒倒计时
 *          4: 比赛中
 *          5: 比赛结算中
 */
enum class GameStage {
  NOT_STARTED = 0, // 未开始比赛
  PREPARATION = 1, // 准备阶段
  SELF_CHECK = 2,  // 十五秒裁判系统自检阶段
  COUNTDOWN = 3,   // 五秒倒计时
  BATTLE = 4,      // 比赛中
  SETTLEMENT = 5   // 比赛结算中
};

/**
 * @enum SpeedLockState
 * @brief 射速超限锁定状态
 */
enum class SpeedLockState {
    Normal = 0,
    Locked15s = 1,
    Locked20s = 2,
    PermanentLocked = 3
};

/**
 * @struct RobotData
 * @brief 机器人数据结构
 * @details 存储单个机器人的完整状态信息。
 */
#include "CustomDataTypes.h"

struct RobotData {
  /**
   * @struct BuffTimedData
   * @brief 单个 Buff 的完整状态
   */
  struct BuffTimedData {
    qint32 level;             // 协议上报 buff_level（可正可负）
    quint32 maxTime;          // 协议上报 buff_max_time（秒）
    quint32 leftTimeAtSync;   // 协议上报时刻 buff_left_time（秒）

    BuffTimedData()
        : level(0), maxTime(0), leftTimeAtSync(0) {}

    void clear() {
      level = 0;
      maxTime = 0;
      leftTimeAtSync = 0;
    }
  };

  /**
   * @struct BuffBundle
   * @brief 将机器人全部 Buff 状态打包，避免在 RobotData 扩散大量组合字段
   */
  struct BuffBundle {
    BuffTimedData attack;                   // 1: 攻击增益
    BuffTimedData defenseOrVulnerability;  // 2: 防御增益/负防增益（易伤）
    BuffTimedData cooling;                  // 3: 射击热量冷却增益
    BuffTimedData chassisPower;             // 4: 底盘功率增益
    BuffTimedData recovery;                 // 5: 回血增益
    BuffTimedData remoteAmmo;               // 6: 可兑换允许发弹量
    BuffTimedData terrainPrewarn;           // 7: 地形跨越增益（预警）

    //调用该方法可获取buff数据写入
    BuffTimedData *byType(quint8 buffType) {
      switch (buffType) {
      case 1:
        return &attack;
      case 2:
        return &defenseOrVulnerability;
      case 3:
        return &cooling;
      case 4:
        return &chassisPower;
      case 5:
        return &recovery;
      case 6:
        return &remoteAmmo;
      case 7:
        return &terrainPrewarn;
      default:
        return nullptr;
      }
    }

    //只读访问buff信息
    const BuffTimedData *byType(quint8 buffType) const {
      switch (buffType) {
      case 1:
        return &attack;
      case 2:
        return &defenseOrVulnerability;
      case 3:
        return &cooling;
      case 4:
        return &chassisPower;
      case 5:
        return &recovery;
      case 6:
        return &remoteAmmo;
      case 7:
        return &terrainPrewarn;
      default:
        return nullptr;
      }
    }

    //把所有buff相关数据重置
    void clear() {
      attack.clear();
      defenseOrVulnerability.clear();
      cooling.clear();
      chassisPower.clear();
      recovery.clear();
      remoteAmmo.clear();
      terrainPrewarn.clear();
    }
  };

  quint8 robotId;             // 机器人ID (1-7: 红方, 101-107: 蓝方)
  RobotType type;             // 机器人类型 (步兵/英雄/工程等)
  TeamColor team;             // 所属队伍颜色 (红/蓝)
  RobotStatus status;         // 当前状态 (正常/离线/复活中等)
  QString name;               // 机器人名称
  quint8 level;               // 当前等级 (1-3)

  quint16 currentHP;          // 当前血量
  quint16 maxHP;              // 最大血量
  quint16 currentRoundMaxHP;  // 当前局已记录的最大血量

  quint16 experience;         // 当前经验值
  quint16 maxExperience;      // 当前等级总经验需求（当前经验 + 距离升级仍需经验）

  quint8 heroRigMode;        //英雄机器人机构类型
  quint8 infantry17mmMode;    //步兵机器人机构类型
  quint8 shooterPerformanceSelection; // 性能体系-发射机构
  quint8 chassisPerformanceSelection; //性能体系-底盘类型

  quint16 launchingFrequency; // 当前射速 (发/秒)
  quint16 tabShootSpeedLimit; // Tab 面板射速上限显示值（m/s，工程为 0）
  quint16 firerate;           // 射速 (发/秒，兼容旧代码)
  bool isClientConnected;     // 客户端是否已连接
  bool isControllerConnected; // 遥控器是否已连接
  bool canRemoteAmmo;         // RobotDynamicStatus.can_remote_ammo：可远程兑换弹丸
  bool canRemoteHeal;         // RobotDynamicStatus.can_remote_heal：可远程兑换血量
  quint32 buffMask;           // 增益效果位掩码

  //底盘相关
  quint16 power;              // 当前底盘功率 (W)
  quint16 maxPower;           // 底盘功率上限 (W)
  quint16 bufferEnergy;       // 缓冲能量 (J)
  quint16 maxBufferEnergy;    // 缓冲能量上限 (J)
  quint16 currentChassisEnergy; // 当前底盘能量 (J)
  quint16 maxChassisEnergy;    // 底盘能量上限 (J)
  quint16 chassisOverPowerCutSeconds; // 协议下发的超功率断电剩余时间


  //弹丸射击相关
  float muzzleVelocity;       // 射击初速度 (m/s)
  float shootSpeedLimit;      // 射击初速度上限 (m/s)
  SpeedLockState speedLockState; // 射速超限锁定状态
  quint16 speedLockSeconds; // 射速超限剩余锁定时间

  // 弹量相关
  quint16 allowedAmmo17mm;    // 17mm 允许发弹量
  quint16 allowedAmmo42mm;    // 42mm 允许发弹量
  quint16 fortressBonusAmmo;  // 堡垒额外弹药 (测试/增益)


  bool isOutOfCombat;         // 是否脱战 (6s内未受到伤害且未发射弹丸)
  quint16 outOfCombatCountdown; // 脱战倒计时 (秒)
  quint8 yellowCardCount;     // 黄牌数量

  // Buff 新协议全量状态（含 level / maxTime / leftTimeAtSync）
  BuffBundle buffs;           // 新版 Buff 全量打包状态（含时间）

  QDateTime lastDamageTime;   // 上次受到伤害的时间
  QDateTime lastFireTime;     // 上次发射弹丸的时间
  bool chassisEnabled;        // 底盘是否使能 (0=禁用/锁定, 1=使能)


  // 热量相关
  quint16 currentHeat;        // 当前枪口热量
  quint16 heatLimit;          // 枪口热量上限
  quint16 coolingValue;       // 枪口热量冷却值
  bool isHeatOverLimit;     // 新增：热量超限标记 (用于驱动UI变红)
  quint16 q2HeatThreshold;  // 二级热量阈值 (Q2)
  bool shooterLocked;       // 射击机构是否被锁定 (裁判系统状态)
  bool shooterPermanentLocked; // 是否被永久锁定 (超过Q2)
  quint16 heatLockSeconds;  // 热量超限预计解锁倒计时

  // 模式与标志位
  float posX;                 // 机器人X坐标 (m)
  float posY;                 // 机器人Y坐标 (m)
  float angle;                // 机器人朝向角度 (度)
  quint8 isHighLight;         // 雷达特殊标识（保留协议原值，如 0/1/2）
  QDateTime lastUpdateTime;   // 最后更新时间
  bool hasTabGlobalSnapshot;  // Tab 面板是否收到过 GlobalUnitStatus 对应槽位快照
  quint16 tabGlobalCurrentHP; // Tab 面板使用的 GlobalUnitStatus 槽位血量
  bool tabStaticConnected;    // Tab 面板使用的 RobotStaticStatus 连接态

  RobotData()
      : robotId(0), type(RobotType::INFANTRY_3), team(TeamColor::RED),
        status(RobotStatus::OFFLINE), name(), level(1), currentHP(0),
        maxHP(600), currentRoundMaxHP(0), experience(0), maxExperience(1000), heroRigMode(0),
        infantry17mmMode(0), shooterPerformanceSelection(0),
        chassisPerformanceSelection(0), launchingFrequency(0),
        tabShootSpeedLimit(0), firerate(0), isClientConnected(false),
        isControllerConnected(false),
        canRemoteAmmo(false), canRemoteHeal(false), buffMask(0), power(0),
        maxPower(0), bufferEnergy(0), maxBufferEnergy(60),
        currentChassisEnergy(0), maxChassisEnergy(0),
        chassisOverPowerCutSeconds(0), muzzleVelocity(0.0f),
        shootSpeedLimit(25.0f),
        speedLockState(SpeedLockState::Normal), speedLockSeconds(0),
        allowedAmmo17mm(0), allowedAmmo42mm(0), fortressBonusAmmo(0),
        isOutOfCombat(true), outOfCombatCountdown(0), yellowCardCount(0),
        chassisEnabled(false), currentHeat(0), heatLimit(0), coolingValue(0),
        isHeatOverLimit(false), q2HeatThreshold(340), shooterLocked(false),
        shooterPermanentLocked(false), heatLockSeconds(0), posX(0.0f),
        posY(0.0f), angle(0.0f), isHighLight(0), lastUpdateTime(),
        hasTabGlobalSnapshot(false), tabGlobalCurrentHP(0),
        tabStaticConnected(false) {}

    // ===== 自定义 UI 数据字段 (来自 CustomByteBlock) =====
    bool fricEnabled = false;
    bool rammerEnabled = false;
    int chassisMode = 0;
    bool spinMode = false;
    bool followMode = false;
    bool chassisProtect = false;
    bool chassisWarning = false;
    float superCapEnergyPercent = 0.0f;
    float gimbalChassisAngle = 0.0f;
    uint8_t targetDistance = 0;
    int8_t ballisticCompensation = 0;
};

/**
 * @struct BaseData
 * @brief 基地数据结构
 */
struct BaseData {
  TeamColor team;           // 所属队伍颜色
  quint16 currentHP;        // 当前血量 (0-5000)
  quint16 maxHP;            // 最大血量
  quint16 virtualShield;    // 虚拟护盾值
  bool isInvincible;        // 是否处于无敌状态
  quint8 status;            // 基地状态 (0:无敌, 1:无敌解除, 2:开甲)
  QDateTime lastUpdateTime; // 最后更新时间

  BaseData()
      : team(TeamColor::RED), currentHP(5000), maxHP(5000), virtualShield(0),
        isInvincible(true), status(0) {}
};

/**
 * @struct OutpostData
 * @brief 前哨站数据结构
 */
struct OutpostData {
  TeamColor team;           // 所属队伍颜色
  quint16 currentHP;        // 当前血量 (0-1500)
  quint16 maxHP;            // 最大血量
  bool isDestroyed;         // 是否已被摧毁
  quint8 status;            // 前哨站状态 (0-2:存活, 3:不可重建, 4:可重建, 5:重建中)
  quint8 rebuildCount;      // 剩余重建次数
  quint8 maxRebuildCount;   // 最大重建次数
  QDateTime lastUpdateTime; // 最后更新时间

  OutpostData()
      : team(TeamColor::RED), currentHP(1500), maxHP(1500), isDestroyed(false),
        status(0), rebuildCount(2), maxRebuildCount(2) {
  }
};

struct OutpostHealthSample {
  qint64 timestampMs = 0;
  quint16 hp = 0;
};

struct BaseHealthSample {
  qint64 timestampMs = 0;
  quint16 hp = 0;
};

/**
 * @struct GameState
 * @brief 比赛状态数据结构
 */
struct GameState {
  quint8 gameType;          // 比赛类型 (1=机甲大师赛)
  GameStage gameProgress;   // 当前比赛阶段
  quint16 gameTime;         // 比赛剩余时间 (秒)
  quint16 stageElapsed;     // 当前阶段已过时间 (秒)
  quint16 redScore;         // 红方比分
  quint16 blueScore;        // 蓝方比分
  quint8 currentRound;      // 当前回合数
  quint8 lastRound;        // 上局回合数（暂无为0）
  bool gameStarted;         // 比赛是否已开始
  bool gameEnded;           // 比赛是否已结束
  bool isPaused;            // 比赛是否处于技术暂停
  quint64 syncTimeStamp;    // 同步时间戳 (用于比赛时间校准)
  QString currentEvent;     // 当前事件描述文本
  QDateTime lastUpdateTime; // 最后更新时间

  GameState()
      : gameType(1), gameProgress(GameStage::NOT_STARTED), gameTime(420),
      stageElapsed(0),
      redScore(0), blueScore(0), currentRound(1), lastRound(0),gameStarted(false),
      gameEnded(false), isPaused(false), syncTimeStamp(0),
      currentEvent(""), lastUpdateTime(QDateTime::currentDateTime()) {}
};

/**
 * @struct BuffPointData
 * @brief 增益点数据结构
 */
struct BuffPointData {
  quint8 pointId;            // 增益点ID
  TeamColor controllingTeam; // 控制该增益点的队伍
  quint16 energyValue;       // 当前能量值
  quint16 maxEnergyValue;    // 最大能量值
  quint16 cooldownTime;      // 冷却剩余时间 (秒)
  bool isActivated;          // 是否已激活
  bool isAvailable;          // 是否可用
  QDateTime lastUpdateTime;  // 最后更新时间

  BuffPointData()
      : pointId(0), controllingTeam(TeamColor::RED), energyValue(0),
        maxEnergyValue(1000), cooldownTime(0), isActivated(false),
        isAvailable(true) {}
};

/**
 * @struct DamageRecord
 * @brief 伤害记录结构
 */
struct DamageRecord {
  quint8 attackerId;   // 攻击者机器人ID
  quint8 victimId;     // 受击者机器人ID
  quint16 damage;      // 伤害值
  quint8 armorId;      // 受击装甲板ID (0-4)
  quint8 hurtType;     // 伤害类型 (弹丸/碰撞/超热量等)
  QDateTime timestamp; // 伤害发生时间戳

  DamageRecord()
      : attackerId(0), victimId(0), damage(0), armorId(0), hurtType(0) {}
};

/**
 * @struct TeamDamageStats
 * @brief 队伍伤害统计
 */
struct TeamDamageStats {
  quint32 totalDamageDealt;    // 队伍总输出伤害
  quint32 totalDamageReceived; // 队伍总承受伤害
  quint16 killCount;           // 击杀数量
  quint16 deathCount;          // 死亡数量

  TeamDamageStats()
      : totalDamageDealt(0), totalDamageReceived(0), killCount(0),
        deathCount(0) {}
};

/**
 * @struct KillRecord
 * @brief 击杀记录结构体
 */
struct KillRecord {
    quint8 killerId;    // 击杀者ID (0表示未知/环境伤害)
    quint8 victimId;    // 被击杀者ID
    int killStreak;     // 连杀数
    bool isFirstBlood;  // 是否一血
    QDateTime timestamp;// 发生时间

    KillRecord() : killerId(0), victimId(0), killStreak(0), isFirstBlood(false), timestamp(QDateTime::currentDateTime()) {}
    KillRecord(quint8 kId, quint8 vId, int streak, bool fb)
        : killerId(kId), victimId(vId), killStreak(streak), isFirstBlood(fb), timestamp(QDateTime::currentDateTime()) {}
};

/**
 * @struct RuneData
 * @brief 能量机关状态数据结构
 */
struct RuneData {
    int status;          // 1:未激活, 2:正在激活, 3:已激活
    int activatedArms;   // 已激活灯臂数 (0-5)
    int type;            // 0:小符, 1:大符 (需结合 event_id 或其他字段判断)
    float averageRings;  // 平均环数（V1.3.0 起为浮点）
    bool isActivatable;
    bool wasActivatable;
  QList<float> groupAverageRings; // 每组平均环数，按协议或 event.param 填充
  QList<int> groupDeltas;        // 每组增量/差值，按协议或 event.param 填充
    int activationStartRemainingTime; // 激活开始时的比赛剩余时间 (秒)

    RuneData()
        : status(1), activatedArms(0), type(0),  averageRings(0.0f),
          isActivatable(false), wasActivatable(false),
          activationStartRemainingTime(-1) {}
};

struct TechCoreMotionStateData {
    quint32 maximumDifficultyLevel = 0;
    quint32 basicState = 0;
    quint32 putinState = 0;
    quint32 moveState = 0;
    quint32 rotateState = 0;
    quint32 enemyCoreStatus = 0;
    quint32 remainTimeAll = 0;
    quint32 remainTimeStep = 0;
};

struct TeamLogisticsStatusData {
    quint32 remainingEconomy = 0;
    quint64 totalEconomyObtained = 0;
    quint32 techLevel = 0;
    quint32 encryptionLevel = 0;
};

/**
 * @struct SentryPathData
 * @brief 哨兵路径规划数据结构（归一化到地图宽高）
 */
struct SentryPathData {
    float startX;               // 哨兵起始X坐标 (0.0 - 1.0, 左下角为原点)
    float startY;               // 哨兵起始Y坐标 (0.0 - 1.0, 左下角为原点)
    QList<float> pathDeltasX;   // 路径X增量列表（相对地图宽度归一化）
    QList<float> pathDeltasY;   // 路径Y增量列表（相对地图高度归一化）

    SentryPathData() : startX(0.0f), startY(0.0f) {}
};

/**
 * @class GameData
 * @brief 比赛数据管理类
 * @details 核心数据类，维护所有比赛实体的状态，并提供信号槽机制以驱动UI更新。
 */
class GameData : public QObject {
  Q_OBJECT

/**
 * @brief QML属性定义
 * @details 通过 Q_PROPERTY 将 C++ 方法暴露为 QML 属性
*/

// === QML 属性绑定 ===

// === qml面板数据连接 ===
Q_PROPERTY(int redScore READ redScore NOTIFY gameStateUpdated)
Q_PROPERTY(int blueScore READ blueScore NOTIFY gameStateUpdated)
Q_PROPERTY(int currentRound READ currentRound NOTIFY gameStateUpdated)
Q_PROPERTY(int remainingTime READ getGameTime NOTIFY gameTimeUpdated)
Q_PROPERTY(QString gamePhase READ gamePhaseString NOTIFY gameStateUpdated)
  Q_PROPERTY(bool is_paused READ is_paused NOTIFY gameStateUpdated)

Q_PROPERTY(int redEconomy READ redEconomy NOTIFY economyUpdated)
Q_PROPERTY(int blueEconomy READ blueEconomy NOTIFY economyUpdated)
Q_PROPERTY(int currentTeamEconomy READ currentTeamEconomy NOTIFY currentTeamEconomyChanged)

Q_PROPERTY(int redBaseHealth READ redBaseHealth NOTIFY baseHealthUpdatedProxy)
Q_PROPERTY(int redBaseMaxHealth READ redBaseMaxHealth CONSTANT)
Q_PROPERTY(int blueBaseHealth READ blueBaseHealth NOTIFY baseHealthUpdatedProxy)
Q_PROPERTY(int blueBaseMaxHealth READ blueBaseMaxHealth CONSTANT)
Q_PROPERTY(bool blueBaseInvincible READ blueBaseInvincible NOTIFY baseHealthUpdatedProxy)
Q_PROPERTY(bool redBaseInvincible READ redBaseInvincible NOTIFY baseHealthUpdatedProxy)
Q_PROPERTY(int redDefenseBonus READ redDefenseBonus NOTIFY dataChanged)
Q_PROPERTY(int blueDefenseBonus READ blueDefenseBonus NOTIFY dataChanged)

// === minimap面板数据绑定 ===
Q_PROPERTY(bool isRedRuneActive READ isRedRuneActive NOTIFY redRuneStatusUpdated)
Q_PROPERTY(bool isBlueRuneActive READ isBlueRuneActive NOTIFY blueRuneStatusUpdated)
Q_PROPERTY(int runeStatus READ runeStatusValue NOTIFY runeStatusChanged)
Q_PROPERTY(int runeType READ runeTypeValue NOTIFY runeStatusChanged)
Q_PROPERTY(bool runeActivatable READ runeActivatable NOTIFY runeStatusChanged)
Q_PROPERTY(int runeActivatedArms READ activatedRuneArms NOTIFY runeStatusChanged)
Q_PROPERTY(float runeAverageRings READ runeAverageRings NOTIFY runeStatusChanged)
Q_PROPERTY(int runeActivationStartRemainingTime READ runeActivationStartRemainingTime NOTIFY runeStatusChanged)
Q_PROPERTY(QVariantList runeGroupAverageRings READ runeGroupAverageRings NOTIFY runeStatusChanged)
Q_PROPERTY(QVariantList runeGroupDeltas READ runeGroupDeltas NOTIFY runeStatusChanged)
Q_PROPERTY(int currentRobotId READ currentRobotId NOTIFY myRobotUpdated)
Q_PROPERTY(bool redOutpostDead READ redOutpostDead NOTIFY outpostDestroyed)
Q_PROPERTY(bool blueOutpostDead READ blueOutpostDead NOTIFY outpostDestroyed)
Q_PROPERTY(float sentryStartX READ sentryStartX NOTIFY sentryPathUpdated)
Q_PROPERTY(float sentryStartY READ sentryStartY NOTIFY sentryPathUpdated)
Q_PROPERTY(QVariantList sentryPathDeltasX READ sentryPathDeltasX NOTIFY sentryPathUpdated)
Q_PROPERTY(QVariantList sentryPathDeltasY READ sentryPathDeltasY NOTIFY sentryPathUpdated)
Q_PROPERTY(QVariantList miniMapMarkers READ getMiniMapMarkers NOTIFY robotPositionUpdated)

Q_PROPERTY(QVariantMap myRobot READ getMyRobot NOTIFY myRobotUpdated)

  // Hero 视频帧（由 VideoReceiver 提供的 QImage）
  Q_PROPERTY(QImage heroFrame READ heroFrame NOTIFY heroFrameUpdated)
  Q_PROPERTY(bool hasHeroFrame READ hasHeroFrame NOTIFY heroFrameUpdated)
  Q_PROPERTY(quint64 heroFrameRevision READ heroFrameRevision NOTIFY heroFrameUpdated)
  Q_PROPERTY(QString heroFrameSource READ heroFrameSource NOTIFY heroFrameUpdated)
  Q_PROPERTY(QString heroFrameDataUrl READ heroFrameDataUrl NOTIFY heroFrameUpdated)

Q_PROPERTY(QString redTeamName READ redTeamName NOTIFY myRobotUpdated)
Q_PROPERTY(int redOutpostHealth READ redOutpostHealth NOTIFY
               outpostHealthUpdatedProxy)
Q_PROPERTY(int redOutpostMaxHealth READ redOutpostMaxHealth CONSTANT)
Q_PROPERTY(int blueOutpostHealth READ blueOutpostHealth NOTIFY
               outpostHealthUpdatedProxy)
Q_PROPERTY(int blueOutpostMaxHealth READ blueOutpostMaxHealth CONSTANT)
Q_PROPERTY(bool redOutpostDestroyed READ redOutpostDestroyed NOTIFY
               outpostHealthUpdatedProxy)
Q_PROPERTY(bool blueOutpostDestroyed READ blueOutpostDestroyed NOTIFY
               outpostHealthUpdatedProxy)
Q_PROPERTY(int redOutpostRebuildCount READ redOutpostRebuildCount NOTIFY
               outpostRebuildCountProxy)
Q_PROPERTY(int blueOutpostRebuildCount READ blueOutpostRebuildCount NOTIFY
               outpostRebuildCountProxy)
Q_PROPERTY(int redOutpostMaxRebuildCount READ redOutpostMaxRebuildCount NOTIFY
               outpostRebuildCountProxy)
Q_PROPERTY(int blueOutpostMaxRebuildCount READ blueOutpostMaxRebuildCount NOTIFY
               outpostRebuildCountProxy)
Q_PROPERTY(QString blueTeamName READ blueTeamName NOTIFY myRobotUpdated)

// === leftbotton 面板 机器人数据列表 ===
Q_PROPERTY(int currentHealth READ currentHealth NOTIFY myRobotUpdated)
Q_PROPERTY(int maxHealth READ maxHealth NOTIFY myRobotUpdated)
Q_PROPERTY(int chassisEnergy READ chassisEnergy NOTIFY myRobotUpdated)
Q_PROPERTY(int maxChassisEnergy READ maxChassisEnergy NOTIFY myRobotUpdated)
Q_PROPERTY(int bufferEnergy READ bufferEnergyValue NOTIFY myRobotUpdated)
Q_PROPERTY(int maxBufferEnergy READ maxBufferEnergy NOTIFY myRobotUpdated)
Q_PROPERTY(int robotId READ robotIdValue NOTIFY myRobotUpdated)
Q_PROPERTY(int robotLevel READ robotLevel NOTIFY myRobotUpdated)
Q_PROPERTY(int yellowCardCount READ yellowCardCount NOTIFY myRobotUpdated)        //重置
Q_PROPERTY(QString outOfCombatStatus READ outOfCombatStatus NOTIFY myRobotUpdated)  //数据绑定
Q_PROPERTY(QString chassisStatus READ chassisStatus NOTIFY myRobotUpdated)          //数据绑定
Q_PROPERTY(QVariantList moduleStates READ moduleStates NOTIFY moduleListChanged)
Q_PROPERTY(bool isOutOfCombat READ isOutOfCombat NOTIFY myRobotUpdated)
Q_PROPERTY(bool canRemoteHeal READ canRemoteHeal NOTIFY myRobotUpdated)         //可远程兑换血量
Q_PROPERTY(bool canRemoteAmmo READ canRemoteAmmo NOTIFY myRobotUpdated)        //可远程兑换弹丸
Q_PROPERTY(bool canRemoteExchange READ canRemoteExchange NOTIFY myRobotUpdated)   //可进行远程兑换
Q_PROPERTY(int currentExp READ currentExp NOTIFY myRobotUpdated)
Q_PROPERTY(int maxExp READ maxExp NOTIFY myRobotUpdated)
//增益（统一走打包结构）
Q_PROPERTY(QVariantMap buffTimedData READ buffTimedData NOTIFY myRobotUpdated)
// 能量机关状态（RuneStatusSync 打包给 QML）
Q_PROPERTY(QVariantMap runeStatusData READ runeStatusData NOTIFY runeStatusDataChanged)
// 飞镖命中消息打包给 qml
Q_PROPERTY(QVariantMap dartMessageData READ dartMessageData
               NOTIFY dartMessageDataChanged)


// === tab面板 当前机器人属性 (我的机器人面板使用) ===
Q_PROPERTY(QVariantList redTeamData READ getRedRobotsDataQml NOTIFY dataChanged)
Q_PROPERTY(QVariantList blueTeamData READ getBlueRobotsDataQml NOTIFY dataChanged)
Q_PROPERTY(int redDartHits READ redDartHits NOTIFY dataChanged)
Q_PROPERTY(int blueDartHits READ blueDartHits NOTIFY dataChanged)
Q_PROPERTY(int dartTotal READ dartTotal CONSTANT) // 固定常量
Q_PROPERTY(int redTotalDamage READ redTotalDamage NOTIFY injuryStatsUpdated)
Q_PROPERTY(int blueTotalDamage READ blueTotalDamage NOTIFY injuryStatsUpdated)
Q_PROPERTY(int redRobotTotalHP READ redRobotTotalHP NOTIFY dataChanged)
Q_PROPERTY(int blueRobotTotalHP READ blueRobotTotalHP NOTIFY dataChanged)

// === ~ 面板伤害统计  ===
Q_PROPERTY(
    QVariantList moduleData READ getModuleData NOTIFY moduleListChanged)

Q_PROPERTY(int damage17mm READ damage17mm NOTIFY injuryStatsUpdated)
Q_PROPERTY(int damage17mmPercent READ damage17mmPercent NOTIFY injuryStatsUpdated)
Q_PROPERTY(int damage42mm READ damage42mm NOTIFY injuryStatsUpdated)
Q_PROPERTY(int damage42mmPercent READ damage42mmPercent NOTIFY injuryStatsUpdated)
Q_PROPERTY(int damageCollision READ damageCollision NOTIFY injuryStatsUpdated)
Q_PROPERTY(int damageCollisionPercent READ damageCollisionPercent NOTIFY injuryStatsUpdated)
Q_PROPERTY(int damageOffline READ damageOffline NOTIFY injuryStatsUpdated)
Q_PROPERTY(int damageOfflinePercent READ damageOfflinePercent NOTIFY injuryStatsUpdated)
Q_PROPERTY(int damageWarning READ damageWarning NOTIFY injuryStatsUpdated)
Q_PROPERTY(int damageWarningPercent READ damageWarningPercent NOTIFY injuryStatsUpdated)

// === 消息系统 ===
Q_PROPERTY(QVariantList systemMessages READ getSystemMessages NOTIFY
                systemMessagesChanged)
Q_PROPERTY(QVariantList robotMessages READ getRobotMessages NOTIFY
                robotMessagesChanged)
  Q_PROPERTY(bool kickedAll READ kickedAll NOTIFY kickAllStateChanged)

Q_PROPERTY(QVariantList activePopups READ activePopups NOTIFY activePopupsChanged)
Q_PROPERTY(QVariantMap robotRespawnStatus READ robotRespawnStatus NOTIFY robotRespawnStatusUpdated)

  // === 自定义 UI 数据 (通过 CustomByteBlock 接收) ===
  Q_PROPERTY(QVariantMap myRobotCustomData READ getMyRobotCustomData NOTIFY robotCustomDataUpdated)
  Q_PROPERTY(bool customUIEnabled READ customUIEnabled WRITE setCustomUIEnabled NOTIFY customUIEnabledChanged)

  // === 兑换系统 ===
  Q_PROPERTY(int ammo17mmExchangeCount READ ammo17mmExchangeCount NOTIFY exchangeCountsUpdated)
  Q_PROPERTY(int ammo42mmExchangeCount READ ammo42mmExchangeCount NOTIFY exchangeCountsUpdated)
  Q_PROPERTY(int engineerPer10sReward READ engineerPer10sReward NOTIFY engineerRewardUpdated)
  Q_PROPERTY(int deployModeStatus READ deployModeStatus NOTIFY deployModeStatusChanged)
  Q_PROPERTY(int airSupportStatus READ airSupportStatus NOTIFY airSupportStatusUpdated)
  Q_PROPERTY(int airSupportLeftTime READ airSupportLeftTime NOTIFY airSupportStatusUpdated)
  Q_PROPERTY(int airSupportCostCoins READ airSupportCostCoins NOTIFY airSupportStatusUpdated)
  Q_PROPERTY(int airSupportIsBeingTargeted READ airSupportIsBeingTargeted NOTIFY airSupportStatusUpdated)
  Q_PROPERTY(int airSupportShooterStatus READ airSupportShooterStatus NOTIFY airSupportStatusUpdated)
  Q_PROPERTY(int siloTargetId READ siloTargetId NOTIFY siloStatusChanged)
  Q_PROPERTY(int siloGateState READ siloGateState NOTIFY siloStatusChanged)
  Q_PROPERTY(bool siloOnline READ siloOnline NOTIFY siloStatusChanged)


  Q_PROPERTY(uint maximumDifficultyLevel READ maximumDifficultyLevel NOTIFY techCoreMotionStateSyncUpdated)
  Q_PROPERTY(uint techCoreBasicState READ techCoreBasicState NOTIFY techCoreMotionStateSyncUpdated)
  Q_PROPERTY(uint techCorePutinState READ techCorePutinState NOTIFY techCoreMotionStateSyncUpdated)
  Q_PROPERTY(uint techCoreMoveState READ techCoreMoveState NOTIFY techCoreMotionStateSyncUpdated)
  Q_PROPERTY(uint techCoreRotateState READ techCoreRotateState NOTIFY techCoreMotionStateSyncUpdated)
  Q_PROPERTY(uint enemyStatus READ enemyStatus NOTIFY techCoreMotionStateSyncUpdated)
  Q_PROPERTY(uint remainTimeAll READ remainTimeAll NOTIFY techCoreMotionStateSyncUpdated)
  Q_PROPERTY(uint remainTimeStep READ remainTimeStep NOTIFY techCoreMotionStateSyncUpdated)


public:
  explicit GameData(QObject *parent = nullptr);
  ~GameData();

  // 机器人数据访问器
  const QList<RobotData> &getAllRobots() const { return m_robots; }
  const QList<RobotData> getRobotsByTeam(TeamColor team) const;
  /**
   * @brief 根据机器人ID获取其机器人类型
   * @param robotId 机器人ID (1-7, 101-107)
   * @return RobotType 对应的机器人类型
   */
  static RobotType getRobotTypeById(quint8 robotId);
  const RobotData *getRobotById(quint8 robotId) const;
  const RobotData *getCurrentRobot() const;
  void setCurrentRobotId(quint8 robotId) {
    if (m_currentRobotId == robotId)
      return;
    const bool teamChanged = (m_currentRobotId >= 100) != (robotId >= 100);
    m_currentRobotId = robotId;
    emit myRobotUpdated();
    if (teamChanged) {
      emit currentTeamEconomyChanged();
      emit runeStatusDataChanged();
    }
  }
  quint8 getCurrentRobotId() const { return m_currentRobotId; }

  // ===== 自定义 UI 数据访问器 =====
  Q_INVOKABLE QVariantMap getMyRobotCustomData() const;
  RobotData& getRobotDataRef(quint8 robotId);

  // 基地数据访问器
  const BaseData &getRedBase() const { return m_redBase; }
  const BaseData &getBlueBase() const { return m_blueBase; }
  const BaseData &getBaseByTeam(TeamColor team) const;

  // 前哨站数据访问器
  const OutpostData &getRedOutpost() const { return m_redOutpostData; }
  const OutpostData &getBlueOutpost() const { return m_blueOutpostData; }
  const OutpostData &getOutpostByTeam(TeamColor team) const;

  // QML 属性访问器
  int redScore() const { return m_gameState.redScore; }
  int blueScore() const { return m_gameState.blueScore; }
  int redRoundScoreDelta() const;
  int blueRoundScoreDelta() const;
  int currentRound() const { return m_gameState.currentRound; }
  QString gamePhaseString() const;

  int redEconomy() const { return m_redEconomy; }
  int blueEconomy() const { return m_blueEconomy; }
  quint32 redRemainingEconomy() const {
    return m_redLogisticsStatus.remainingEconomy;
  }
  quint32 blueRemainingEconomy() const {
    return m_blueLogisticsStatus.remainingEconomy;
  }
  quint64 redTotalEconomyObtained() const {
    return m_redLogisticsStatus.totalEconomyObtained;
  }
  quint64 blueTotalEconomyObtained() const {
    return m_blueLogisticsStatus.totalEconomyObtained;
  }
  quint32 redTechLevel() const { return m_redLogisticsStatus.techLevel; }
  quint32 blueTechLevel() const { return m_blueLogisticsStatus.techLevel; }
  quint32 redEncryptionLevel() const {
    return m_redLogisticsStatus.encryptionLevel;
  }
  quint32 blueEncryptionLevel() const {
    return m_blueLogisticsStatus.encryptionLevel;
  }
  int allyFortressOccupationSec() const { return m_allyFortressOccupationSec; }
  int enemyFortressOccupationSec() const { return m_enemyFortressOccupationSec; }
  int currentTeamEconomy() const {
    return m_currentRobotId >= 100 ? m_blueEconomy : m_redEconomy;
  }

  int redBaseHealth() const { return m_redBase.currentHP; }
  int redBaseMaxHealth() const { return m_redBase.maxHP; }
  int blueBaseHealth() const { return m_blueBase.currentHP; }
  int blueBaseMaxHealth() const { return m_blueBase.maxHP; }

  bool redBaseInvincible() const { return m_redBase.isInvincible; }
  bool blueBaseInvincible() const { return m_blueBase.isInvincible; }
  int redBaseStatus() const { return m_redBase.status; }
  int blueBaseStatus() const { return m_blueBase.status; }

  int redOutpostHealth() const { return m_redOutpostData.currentHP; }
  int redOutpostMaxHealth() const { return m_redOutpostData.maxHP; }
  int blueOutpostHealth() const { return m_blueOutpostData.currentHP; }
  int blueOutpostMaxHealth() const { return m_blueOutpostData.maxHP; }
  int enemyOutpostHealth() const {
    const RobotData *robot = getCurrentRobot();
    const bool myIsRed =
        robot ? (robot->team == TeamColor::RED) : (m_currentRobotId < 100);
    return myIsRed ? m_blueOutpostData.currentHP : m_redOutpostData.currentHP;
  }
  bool customUIEnabled() const { return m_customUIEnabled; }
  void setCustomUIEnabled(bool v);

  bool redOutpostDestroyed() const { return m_redOutpostData.isDestroyed; }
  bool blueOutpostDestroyed() const { return m_blueOutpostData.isDestroyed; }
  int redOutpostRebuildCount() const { return m_redOutpostData.rebuildCount; }
  int blueOutpostRebuildCount() const { return m_blueOutpostData.rebuildCount; }
  int redOutpostMaxRebuildCount() const { return m_redOutpostData.maxRebuildCount; }
  int blueOutpostMaxRebuildCount() const { return m_blueOutpostData.maxRebuildCount; }

  static QString enemyTeamName() {
    const QString envName = qEnvironmentVariable("RM_ENEMY_TEAM_NAME").trimmed();
    return envName.isEmpty() ? QStringLiteral("上海交通大学 交龙") : envName;
  }

  QString redTeamName() const {
    return m_currentRobotId < 100 ? QStringLiteral("复旦大学 星云EGA")
                                  : enemyTeamName();
  }
  QString blueTeamName() const {
    return m_currentRobotId < 100 ? enemyTeamName()
                                  : QStringLiteral("复旦大学 星云EGA");
  }

  // 当前机器人属性访问器 (我的机器人面板使用)
  int currentHealth() const {
    const RobotData *robot = getCurrentRobot();
    return robot ? robot->currentHP : 0;
  }
  int maxHealth() const {
    const RobotData *robot = getCurrentRobot();
    return robot ? robot->maxHP : 600;
  }
  int chassisEnergy() const {
    const RobotData *robot = getCurrentRobot();
    return robot ? robot->currentChassisEnergy : 0;
  }
  int maxChassisEnergy() const {
    const RobotData *robot = getCurrentRobot();
    return robot ? robot->maxChassisEnergy : 0;
  }
  int bufferEnergyValue() const {
    const RobotData *robot = getCurrentRobot();
    return robot ? robot->bufferEnergy : 0;
  }
  int maxBufferEnergy() const {
    const RobotData *robot = getCurrentRobot();
    return robot ? robot->maxBufferEnergy : 0;
  }
  int robotIdValue() const {
    const RobotData *robot = getCurrentRobot();
    return robot ? robot->robotId : 1;
  }
  int robotLevel() const {
    const RobotData *robot = getCurrentRobot();
    return robot ? robot->level : 1;
  }
  int yellowCardCount() const {
    const RobotData *robot = getCurrentRobot();
    return robot ? robot->yellowCardCount : 0;
  }
  QString outOfCombatStatus()const;
  QString chassisStatus() const;
  bool isOutOfCombat() const {
    const RobotData *robot = getCurrentRobot();
    return robot ? robot->isOutOfCombat : true;
  }
  bool canRemoteHeal() const {
    const RobotData *robot = getCurrentRobot();
    return robot ? robot->canRemoteHeal : false;
  }
  bool canRemoteAmmo() const {
    const RobotData *robot = getCurrentRobot();
    return robot ? robot->canRemoteAmmo : false;
  }
  bool canRemoteExchange() const {
    return canRemoteHeal() || canRemoteAmmo();
  }
  //增益
  QVariantMap buffTimedData() const;

  QVariantMap runeStatusData() const;
  QVariantMap dartMessageData() const;

  int currentExp() const {
    const RobotData *robot = getCurrentRobot();
    return robot ? robot->experience : 0;
  }
  int maxExp() const {
    const RobotData *robot = getCurrentRobot();
    return robot ? robot->maxExperience : 1000;
  }

  // 兑换计数访问器
  int ammo17mmExchangeCount() const { return m_ammo17mmExchangeCount; }
  int ammo42mmExchangeCount() const { return m_ammo42mmExchangeCount; }
  int engineerPer10sReward() const { return m_engineerPer10sReward; }
  int deployModeStatus() const { return m_deployModeStatus; }
  int airSupportStatus() const { return m_airSupportStatus; }
  int airSupportLeftTime() const { return m_airSupportLeftTime; }
  int airSupportCostCoins() const { return m_airSupportCostCoins; }
  int airSupportIsBeingTargeted() const {
    return m_airSupportIsBeingTargeted;
  }
  int airSupportShooterStatus() const { return m_airSupportShooterStatus; }
  int siloTargetId() const { return m_siloTargetId; }
  int siloGateState() const { return m_siloGateState; }

  uint maximumDifficultyLevel() const {
    return m_techCoreMotionState.maximumDifficultyLevel;
  }
  bool siloOnline() const;
  void setEngineerPer10sReward(int value) {
    if (m_engineerPer10sReward == value)
      return;
    m_engineerPer10sReward = value;
    emit engineerRewardUpdated();
  }
  uint techCoreBasicState() const { return m_techCoreMotionState.basicState; }
  uint techCorePutinState() const { return m_techCoreMotionState.putinState; }
  uint techCoreMoveState() const { return m_techCoreMotionState.moveState; }
  uint techCoreRotateState() const { return m_techCoreMotionState.rotateState; }
  uint enemyStatus() const {
    return m_techCoreMotionState.enemyCoreStatus;
  }
  uint remainTimeAll() const {
    return m_techCoreMotionState.remainTimeAll;
  }
  uint remainTimeStep() const {
    return m_techCoreMotionState.remainTimeStep;
  }
  uint techCoreMaximumDifficultyLevel() const {
    return m_techCoreMotionState.maximumDifficultyLevel;
  }


  // Q_INVOKABLE 方法
  Q_INVOKABLE void incrementAmmo17mmExchangeCount();
  Q_INVOKABLE void incrementAmmo42mmExchangeCount();
  Q_INVOKABLE QVariantMap getRobotInfo(int robotId) const;
  Q_INVOKABLE bool isRobotConnected(int robotId) const; // 基于 RobotStaticStatus.connection_state
  Q_INVOKABLE bool isAerialConnected() const; // 我方空中机器人在线检测（查当前队伍的 AERIAL 类型机器人）
  Q_INVOKABLE bool isSiloConnected() const; // 基于 RobotStaticStatus / RobotModuleStatus 判断飞镖模块在线

  // 比赛状态访问器
  const GameState &getGameState() const { return m_gameState; }
  GameStage getCurrentStage() const { return m_gameState.gameProgress; }
  quint16 getGameTime() const { return m_gameState.gameTime; }
  bool isGameActive() const {
    return m_gameState.gameStarted && !m_gameState.gameEnded;
  }

  bool is_paused() const { return m_gameState.isPaused; }
  bool isRedRuneActive() const { return m_isRedRuneActive; }
  bool isBlueRuneActive() const { return m_isBlueRuneActive; }
  int runeStatusValue() const { return m_runeData.status; }
  int runeTypeValue() const { return m_runeData.type; }
  bool runeActivatable() const { return m_runeData.isActivatable; }

  int activatedRuneArms() const;
  float runeAverageRings() const { return m_runeData.averageRings; }
  int runeActivationStartRemainingTime() const { return m_runeData.activationStartRemainingTime; }
  QVariantList runeGroupAverageRings() const;
  QVariantList runeGroupDeltas() const;
  int currentRobotId() const { return m_currentRobotId; }
  bool redOutpostDead() const { return m_redOutpostData.isDestroyed; }
  bool blueOutpostDead() const { return m_blueOutpostData.isDestroyed; }
  //哨兵规划路径访问器
  float sentryStartX() const { return m_allySentryPath.startX; }
  float sentryStartY() const { return m_allySentryPath.startY; }
  QVariantList sentryPathDeltasX() const {
      QVariantList list;
      for(float v : m_allySentryPath.pathDeltasX) list << v;
      return list;
  }
  QVariantList sentryPathDeltasY() const {
      QVariantList list;
      for(float v : m_allySentryPath.pathDeltasY) list << v;
      return list;
  }


  // 增益点数据访问器
  const QList<BuffPointData> &getBuffPoints() const { return m_buffPoints; }
  const BuffPointData *getBuffPointById(quint8 pointId) const;

  // 哨兵状态访问器
  const SentryStatusData &getRedSentry() const { return m_redSentry; }
  const SentryStatusData &getBlueSentry() const { return m_blueSentry; }
  const SentryStatusData &getSentryByTeam(TeamColor team) const;

  // 模块状态访问器
  const QList<ModuleStatusData> &getModuleStatus() const {
    return m_moduleStatus;
  }
  const ModuleStatusData *getModuleByName(const QString &moduleName) const;


  // --- 小地图数据访问器 ---
  QVariantList getMiniMapMarkers() const;

  // --- tab面板 机器人数据列表访问器 ---
  QVariantList getRedRobotsDataQml() const;   // 红方机器人列表
  QVariantList getBlueRobotsDataQml() const;  // 蓝方机器人列表（协议更新驱动）

  int redTotalDamage() const;
  int blueTotalDamage() const;
  int redRobotTotalHP() const {return m_redRobotTotalHP;}
  int blueRobotTotalHP() const {return m_blueRobotTotalHP;}
  int redDartHits() const;
  int blueDartHits() const;
  int dartTotal() const;

  // --- ~ 面板伤害统计访问器 ---
  Q_INVOKABLE QVariantList getModuleData() const;
  QVariantList moduleStates() const;
  int damage17mm() const;
  int damage42mm() const;
  int damageCollision() const;
  int damageOffline() const;
  int damageWarning() const;

  int damage17mmPercent() const;
  int damage42mmPercent() const;
  int damageCollisionPercent() const;
  int damageOfflinePercent() const;
  int damageWarningPercent() const;

  // === 消息系统方法 ===
  QVariantList getSystemMessages() const;
  QVariantList getRobotMessages() const;
  Q_INVOKABLE void addSystemMessage(const QString &message);
  Q_INVOKABLE void addSystemMessage(const QString &message,
                                    const QString &color);
  Q_INVOKABLE void addRobotMessage(const QString &message);

  // 伤害统计访问器
  const QList<DamageRecord> &getDamageHistory() const {
    return m_damageHistory;
  }
  const TeamDamageStats &getRedTeamStats() const { return m_redTeamStats; }
  const TeamDamageStats &getBlueTeamStats() const { return m_blueTeamStats; }
  const TeamDamageStats &getTeamStats(TeamColor team) const;

  // 判定胜者：比较战斗阶段起点与结算阶段当前比分的增量（0=平局,1=红方,2=蓝方）
  quint8 determineWinner() const;

  bool kickedAll() const { return m_kickedAll; }
  void setKickedAll(bool value);

  // 数据更新方法

  /**
   * @brief 更新机器人数据/伤害数据
   * @param data Protobuf格式的机器人状态消息
   */
  void updateRobotData(const robomaster::RobotStatus &data);
  void updateRobotInjury(const robomaster::RobotInjuryStat &data);
  void updateRobotModuleStatus(const robomaster::RobotModuleStatus &data); // 新增：模块在线状态更新

  void updateOutpostHealth(TeamColor team, quint16 currentHP);
  void updateGlobalUnitStatus(const robomaster::GlobalUnitStatus &data);
  void updateGlobalUnitStatusInternal(const robomaster::GlobalUnitStatusInternal &data);

  //更新机器人Static和Dynamic状态
  void updateRobotStaticStatus(const robomaster::RobotStaticStatus &data);
  void updateRobotDynamicStatus(const robomaster::RobotDynamicStatus &data);

  //更新机器人位置朝向
  void updateRobotPosition(const robomaster::RobotPosition &data);
  void updateRadarInfo(const robomaster::RadarInfoToClient &data);

  /**
   * @brief 更新比赛状态
   * @param data Protobuf格式的比赛信息消息
   */
   //全局通知
  void eventInfo(const robomaster::Event &data);
  void updateGameState(const robomaster::GameInfo &data);
  // MQTT 状态更新接口
  void updateGameStatus(const robomaster::GameStatus &data);
  void updateGlobalLogisticsStatus(const robomaster::GlobalLogisticsStatus &data);
  void updateGlobalSpecialMechanism(
      const robomaster::GlobalSpecialMechanism &data);

  void updateBuff(const robomaster::Buff &data);
  void updateRuneStatusSync(const robomaster::RuneStatusSync &data);
  void updateAirSupportStatusSync(const robomaster::AirSupportStatusSync &data);
  void updateTechCoreMotionStateSync(const robomaster::TechCoreMotionStateSync &data);
  void updateDeployModeStatusSync(const robomaster::DeployModeStatusSync &data);
  void updateGroundRobotPosition(const robomaster::GroundRobotPosition &data);
  void updateEnemyPositions(const robomaster::MapRobotData &data);
  // 0x020D 雷达标记/能量机关数据更新
  void updateRadarMarkData(const robomaster::RadarMarkData &data);
  void updateSentryStatus(const SentryStatusData &data);
  void recordDamageEvent(const DamageEventData &data);
  void updateShootData(const ShootData &data);
  void updateEconomyData(const EconomyData &data);
  void updateVideoControl(const robomaster::VideoControl &data);

  // 统一判罚入口
  void updateRefereeWarning(const robomaster::RefereeWarningData &data);

  // S1 协议状态更新
  void updateGameStateFromS1(int stage, int remainingTime, int redScore, int blueScore, int round, bool isPaused);
  void updatePenalty(const robomaster::PenaltyInfo &data);

  //哨兵路径规划更新
  void updateSentryPath(const robomaster::RobotPathPlanInfo &data);
  void updateSiloStatusFromSync(int targetId, int open);

  // 弹窗桥接：将外部意图传递给弹窗状态机
  QVariantList activePopups() const; // QML 可读属性，返回当前激活弹窗列表

  // 统计方法
  void recordKill(quint8 killerId, quint8 victimId);
  void updateDamageStats();
  QMap<int, quint32> getDamageByRobot() const;
  void resetDamageStats();
  void resetInjuryStatsOnRespawn(); // 机器人复活后重置 ~ 面板伤害统计与比例

  // 手动设置方法 (用于模拟器)
  void setRedEconomy(int value) {
    m_redEconomy = value;
    emit economyUpdated();
    emit currentTeamEconomyChanged();
  }
  void setBlueEconomy(int value) {
    m_blueEconomy = value;
    emit economyUpdated();
    emit currentTeamEconomyChanged();
  }

  void setTeamLevelCap(TeamColor team, int cap);
  int redLevelCap() const { return m_redLevelCap; }
  int blueLevelCap() const { return m_blueLevelCap; }

  void setTeamDefenseBonus(TeamColor team, int percent);
  int redDefenseBonus() const { return m_redDefenseBonusPercent; }
  int blueDefenseBonus() const { return m_blueDefenseBonusPercent; }

  // 模拟器更新接口
  Q_INVOKABLE void updateRobotHealth(quint8 id, quint16 hp);
  Q_INVOKABLE void updateRobotHeat(quint8 id, quint16 heat);
  Q_INVOKABLE void updateRobotPower(quint8 id, quint16 power);
  Q_INVOKABLE void updateRobotLevel(quint8 id, quint16 level); // 添加等级更新
  Q_INVOKABLE void updateRobotBuffer(quint8 id, quint16 buffer);
  Q_INVOKABLE void toggleCurrentRobotOutOfCombat();
  Q_INVOKABLE void setFortressBonusAmmo(quint8 id, quint16 ammo); // 模拟设置堡垒弹药
  // 仅堡垒增益点，无控制区/额外弹药
  Q_INVOKABLE void setAllowedAmmo17mm(quint8 id, quint16 ammo); // 模拟设置17mm允许发弹量
  Q_INVOKABLE void setAllowedAmmo42mm(quint8 id, quint16 ammo); // 模拟设置42mm允许发弹量
  // 模拟设置超速限制 (仅用于 Ctrl+Shift+L 调试/测试，非业务逻辑)
  Q_INVOKABLE void setSpeedOverLimit(quint8 id, bool overLimit, quint16 lockSeconds);
  Q_INVOKABLE void setRobotType(quint8 id, int type); // 模拟设置机器人类型
  Q_INVOKABLE void setRobotHeat(quint8 id, quint16 heat); // 设置机器人热量
  /**
   * @brief 处理来自网络或 MQTT 的 RobotRespawnStatus 数据
   * @details 将 QVariantMap 转换为内部状态并发出 `robotRespawnStatusUpdated` 信号，供 QML 使用。
   * @param status 包含字段：is_pending_respawn, total_respawn_progress, current_respawn_progress,
   *               can_free_respawn, gold_cost_for_respawn, can_pay_for_respawn
   */
  Q_INVOKABLE void processRobotRespawnStatusMap(const QVariantMap &status);

  // 比赛更新方法
  Q_INVOKABLE void update();            // 添加update方法
  QString getFormattedGameTime() const; // 添加getFormattedGameTime方法
  QString getFormattedStageElapsedTime() const; // 添加 getFormattedStageElapsedTime 方法

  // --- Silo (飞镖) 操作: QML 可调用的后端接口 ---
  Q_INVOKABLE int selectNextSiloTarget();    // 返回新的 targetId（1-5）
  Q_INVOKABLE bool requestSiloOpen();        // 请求开启闸门（确认后会发出命令）
  Q_INVOKABLE bool requestSiloFire();        // 请求发射飞镖（发射确认）

  // 数据验证
  bool validateRobotId(quint8 robotId) const;
  bool validateTeamColor(TeamColor team) const;

public slots:
  /**
   * @brief 协议数据处理槽函数
   * @details 接收并分发来自 NetworkManager 的 Protobuf 消息。
   * @param message 接收到的 RoboMasterMessage 对象
   */
  void processProtocolData(const robomaster::RoboMasterMessage &message);

  // 比赛时间更新
  void updateGameTime();

  // 射速限制检查 (由判罚触发)
  void checkSpeedLimit(RobotData *robot, bool isPenaltyTriggered = false);

  // 处理比赛结果（来自协议或其它来源）
  void processGameResult(quint8 winner);

  // --- QML 数据访问 ---
  Q_INVOKABLE QVariantMap getMyRobot() const;
  Q_INVOKABLE int getMyRobotId() const;
  QVariantMap getLastRespawnStatus() const { return m_lastRespawnStatus; }
  QVariantMap robotRespawnStatus() const;
  void setMyRobotId(int id) { setCurrentRobotId(static_cast<quint8>(id)); } // 兼容旧调用

  // 模拟器测试接口：注入飞镖命中事件（team: 1=红方 2=蓝方, targetId: 1-5）
  Q_INVOKABLE void simulateDartHit(int team, int targetId);
  // 模拟器测试接口：使比赛时间前进 N 秒（基于当前 stage_countdown_sec 递减）
  Q_INVOKABLE void simulateGameTimeElapse(int seconds);

  // 接收来自 VideoReceiver 的视频帧（QImage），并缓存最近一帧用于 QML 展示
  public slots:
  void onVideoFrameReceived(const QImage &frame);
  void clearHeroFrame();

  // Hero 视频帧访问器（QML 绑定）
  QImage heroFrame() const;
  bool hasHeroFrame() const;
  quint64 heroFrameRevision() const;
  QString heroFrameSource() const;
  QString heroFrameDataUrl() const;

signals:
  // 兑换更新信号
  void exchangeCountsUpdated();

  // 数据更新信号
  void robotDataUpdated(quint8 robotId);
  void myRobotUpdated(); // 新增信号：主视角机器人数据更新
  void baseHealthUpdated(TeamColor team);
  void baseAttacked(TeamColor team);
  void redBaseAttackedChanged();
  void blueBaseAttackedChanged(); // 基地遭到攻击信号 (由事件触发)
  void outpostHealthUpdated(TeamColor team);
  void outpostStatusChanged(bool isRed, int status);
  void outpostRebuildCountChanged();
  void customUIEnabledChanged();
  void gameStateUpdated();
  void redRuneStatusUpdated();
  void blueRuneStatusUpdated();
  void buffPointUpdated();
  void sentryStatusUpdated(TeamColor team);
  void moduleStatusUpdated(const QString &moduleName);
  void damageEventOccurred(quint8 attackerId, quint8 victimId, quint16 damage);
  void killEventOccurred(const KillRecord &record);
  void gameTimeUpdated(quint16 remainingTime);
  void gameStageChanged(GameStage newStage);
  void videoSourceChanged(const QString &url, bool isPlaying);
  void robotPositionUpdated(quint8 robotId, float x, float y, float angle,
                            quint8 isHighLight);
  /**
   * @brief 基地状态变化信号
   * @param isRed 是否为红方基地
   * @param status 新的基地状态 (0:无敌, 1:无敌解除, 2:开甲)
   */
  void baseStatusChanged(bool isRed, int status);

  /**
   * @brief 前哨站被摧毁信号
   * @param isRed 是否为红方前哨站
   */
  void outpostDestroyed(bool isRed);

  /**
   * @brief 基地被摧毁信号
   * @param isRed 是否为红方基地
   */
  void baseDestroyed(bool isRed);

  // 新：当 hero 帧更新时发出（QML 可绑定 `heroFrame` 属性）
  void heroFrameUpdated();

  /**
   * @brief 收到系统消息信号
   */
  void systemMessageReceived(const QString &message);
  void battleMessageReceived(const QString &content, float duration, uint32_t colorHex);
  void refereeWarningUpdated(const robomaster::RefereeWarningData &data); // 新增：结构化判罚信号

  // 机器人复活状态更新（QVariantMap，字段与 QML updateFromStatus 对应）
  void robotRespawnStatusUpdated(const QVariantMap &status);

  // 弹窗 payload 增量更新（转发自 PopupStateMachine）
  void popupPayloadUpdated(const QString &type, const QVariantMap &payload);

  // 通用数据变化信号
  void dataChanged();
  void moduleListChanged();
  void systemMessagesChanged();
  void robotMessagesChanged();

  // ===== 自定义 UI 数据更新信号 =====
  void robotCustomDataUpdated(quint8 robotId);

  // ~ 面板伤害统计刷新信号
  void injuryStatsUpdated();

  // 音效播放信号
  void playAlertSound();
  void playVictorySound();
  void playDefeatSound();
  void playmin7bgmyuyin();

  // 能量机关激活提示信号
  void runeStatusChanged(const RuneData &data);
  void runeStatusDataChanged();

  // 能量机关信号
  void runeActivable(int runeType);
  void runeActived(int runeType,int runeStatus);
  void runeVoicePromptRequested(int runeType, int remainingChances);
  void dartMessageDataChanged();
  void dartMessageTriggered();

  // 空中支援信号
  void airSupportStarted(bool isRedTeam);
  void airSupportTargetingStateChanged(bool targeted);
  // event=8 参数透传，供 UI 侧映射不同提示音
  void airSupportCountered(const QString &soundSelector);

  // 官方事件弹窗信号
  void officialEventPopupRequested(int eventId, const QString &message);

  // 飞镖闸门信号
  void dartGateOpened(bool isRedTeam, bool isEnemyTeam);

  // 基地受攻击信号
  void baseUnderAttackEvent(bool isRedTeam);

  // 敌方前哨站停止信号
  void enemyOutpostStoppedEvent(bool isRedTeam);

  // Event 13：对方基地护甲展开信号
  void enemyBaseShieldOpenedEvent(bool isRedTeam);

  // 己方前哨站 5 秒窗口血量下降达到阈值时产生候选提示
  void allyOutpostHealthDropAlertTriggered();

  // 己方基地 1 秒窗口血量下降超过阈值时触发
  void allyBaseHealthDropAlertTriggered();

  // 比赛进行时，己方基地状态从非 2 进入 2（护甲展开）时触发
  void allyBaseArmorOpenedTriggered();

  // 己方堡垒被对方占领计时到 1s 时触发一次提示
  void allyFortressOccupationAlertTriggered();

  // 飞镖指令请求信号
  void siloCommandRequested(int targetId, bool open, bool launchConfirm);

  // 比赛结果信号：winner 取值 0=Draw,1=Red,2=Blue
  void gameResultReceived(quint8 winner);

  // 踢出全员事件
  void kickAllStateChanged(bool kicked);

  //哨兵规划路径变化信号
  void sentryPathUpdated();

  void economyUpdated();
  void currentTeamEconomyChanged();
  void engineerRewardUpdated();
  void deployModeStatusChanged();
  void airSupportStatusUpdated();
  void siloStatusChanged();
  void techCoreMotionStateSyncUpdated();
  void baseHealthUpdatedProxy(); // 用于 QML 的无参数信号
  void activePopupsChanged();    // 激活弹窗列表变化
  void outpostHealthUpdatedProxy();
  void outpostRebuildCountProxy();
private:
  // 内部数据成员
  mutable QMutex m_mutex;
  QList<RobotData> m_robots;

  // 私有辅助更新方法
  // 信号节流：dataChanged 最大发射频率控制在 20Hz（每 50ms）
  void emitDataChanged();
  void emitRuneVoicePromptIfDue();
  //封装了 HP/Level 等全量状态更新，以及 击杀、复活、伤害检测 逻辑
  void updateStandardStatus(RobotData *robot, const robomaster::RobotStatus &data);
  //封装了热量超限计算逻辑
  void checkHeatLimit(RobotData *robot, bool isPenaltyConfirmed = false);
  BaseData m_redBase;
  BaseData m_blueBase;
  bool m_redBaseStatusObserved = false;
  bool m_blueBaseStatusObserved = false;

  OutpostData m_redOutpostData;
  OutpostData m_blueOutpostData;
  GameState m_gameState;
  bool m_hasBattleScoreBaseline = false;
  quint8 m_battleScoreBaselineRound = 0;
  quint16 m_battleScoreBaselineRed = 0;
  quint16 m_battleScoreBaselineBlue = 0;
  QList<BuffPointData> m_buffPoints;
  SentryStatusData m_redSentry;
  SentryStatusData m_blueSentry;
  SentryPathData m_allySentryPath;      //己方哨兵路径规划
  QList<ModuleStatusData> m_moduleStatus;//用于顺序存储~按键所需的模块
  QVariantMap m_robotModuleStatusMap; // 核心模块状态映射 (字段名 -> 0/1)（统一管理）
  QMap<QString, bool> m_lastModuleOnlineStates; // 模块上次在线态，用于离线消息去重

  // 伤害统计数据
  QList<DamageRecord> m_damageHistory;
  TeamDamageStats m_redTeamStats;
  TeamDamageStats m_blueTeamStats;

  // --- ~ 面板伤害统计缓存（来自 RobotInjuryStat） ---
  quint32 m_injuryTotal = 0;          // 总伤害
  quint32 m_injury17mm = 0;           // 17mm 弹丸伤害
  quint32 m_injury42mm = 0;           // 42mm 弹丸伤害
  quint32 m_injuryCollision = 0;      // 撞击伤害
  quint32 m_injuryOffline = 0;        // 模块离线（module_offline + wifi_offline）
  quint32 m_injuryWarning = 0;        // 警告（penalty + server_kill + dart_splash）

  // --- 飞镖命中计数 ---
  int m_redDartHits = 0;              // 红方飞镖命中次数（累计）
  int m_blueDartHits = 0;             // 蓝方飞镖命中次数（累计）
  // 击杀统计
  int m_totalKills = 0; // 本局总击杀数
  QMap<quint8, int> m_killStreaks;      // 机器人ID -> 当前连杀数
  QMap<quint8, QDateTime> m_lastKillTimes; // 机器人ID -> 上次击杀时间
  QList<KillRecord> m_killRecords;      // 击杀历史记录

  // --- 兑换数据 ---
  int m_ammo17mmExchangeCount = 0;
  int m_ammo42mmExchangeCount = 0;

  // 经济数据
  int m_redEconomy = 0;
  int m_blueEconomy = 0;
  TeamLogisticsStatusData m_redLogisticsStatus;
  TeamLogisticsStatusData m_blueLogisticsStatus;
  int m_allyFortressOccupationSec = 0;//己方堡垒被对方占领倒计时
  int m_enemyFortressOccupationSec = 0;//敌方堡垒被己方占领倒计时
  int m_engineerPer10sReward = 0;
  int m_redLevelCap = 5;
  int m_blueLevelCap = 5;
  int m_redDefenseBonusPercent = 0;
  int m_blueDefenseBonusPercent = 0;

  // 总机器人血量
  int m_redRobotTotalHP = 0;
  int m_blueRobotTotalHP = 0;

  // 消息列表数据
  QVariantList m_systemMessages;
  QStringList m_robotMessages;
  bool m_customUIEnabled = true;

  // 信号节流：避免高频 dataChanged 阻塞 UI 线程
  qint64 m_lastDataChangedMs = 0;
  static constexpr qint64 kDataChangedMinIntervalMs = 50;
  QVector<quint8> m_pendingRobotUpdates;
  static constexpr int MAX_MESSAGES = 50; // 最大消息数量

  // 踢出全员标记
  bool m_kickedAll = false;
  bool m_isRedRuneActive = false;  // 红方能量机关激活状态
  bool m_isBlueRuneActive = false; // 蓝方能量机关激活状态

  // 0x0101 场地事件数据缓存
  quint32 m_eventData = 0;
  RuneData m_runeData;
  RM::RuneVoicePromptTracker m_runeVoicePromptTracker;
  int m_lastRuneStatusSyncStatus = 0;
  QVariantMap m_dartMessageData;
  TechCoreMotionStateData m_techCoreMotionState;
  int m_deployModeStatus = 0;
  int m_airSupportStatus = 0;
  int m_airSupportLeftTime = 0;
  int m_airSupportCostCoins = 0;
  int m_airSupportIsBeingTargeted = 0;
  int m_airSupportShooterStatus = 0;
  int m_lastPeriodicRewardNotifyTime = -1;
  int m_lastPeriodicAirSupportNotifyTime = -1;
  int m_siloTargetId = 1;
  int m_siloGateState = 0;
  qint64 m_lastSiloUpdateMs = 0; // 最近一次收到 DartSelectTargetStatusSync 的时间戳（ms）
  bool m_hasDartRobotConnectionState = false; // 是否收到过飞镖 RobotStaticStatus 连接态
  bool m_dartRobotOnline = false; // 飞镖 RobotStaticStatus 推导在线态
  QDateTime m_lastLocalHeatSet;

  // 当前选中机器人和定时器
  quint8 m_currentRobotId;
  QTimer *m_gameTimer;
  QTimer *m_uiRefreshTimer; // 用于每秒刷新脱战倒计时等状态

  // 金币立即复活后的底盘功率上限加成状态（持续4秒）
  QDateTime m_paidRespawnPowerBoostEndTime;
  bool m_lastPendingRespawn = false;
  uint32_t m_lastRespawnProgress = 0;
  uint32_t m_lastRespawnTotal = 0;

  PopupStateMachine *m_popupStateMachine = nullptr; // 弹窗状态机实例（桥接）
  QVariantMap m_lastRespawnStatus; // 缓存最近一次收到的复活状态（用于驱动 RobotRespawn 显示/隐藏）
  // Hero 视频帧缓存（由 VideoReceiver 提供）
  QImage m_heroFrame;
  quint64 m_heroFrameRevision = 0;
  QString m_heroFrameDataUrl;
  // 是否使用外部钟作为比赛时间源。默认 true（优先使用来自网络的 gameTime）
  bool m_useExternalGameClock = true;
  QList<OutpostHealthSample> m_redOutpostHealthSamples;
  QList<OutpostHealthSample> m_blueOutpostHealthSamples;
  // 仅记录实际发出的有效提示；被 15 秒间隔丢弃的候选不更新此时间戳。
  qint64 m_allyOutpostLastAcceptedAlertMs = 0;
  QList<BaseHealthSample> m_redBaseHealthSamples;
  QList<BaseHealthSample> m_blueBaseHealthSamples;
  qint64 m_allyBaseLastDropAlertMs = 0;
  qint64 m_lastBaseUnderAttackEventMs = 0;

  // 内部方法
  void initializeRobots();
  void initializeBases();
  void initializeGameState();
  void initializeBuffPoints();
  void initializeSentryStatus();
  void initializeModuleStatus();

public:
  void updateBaseHP(TeamColor team, quint16 hp);
  void updateBaseStatus(TeamColor team, quint8 status);
  void updateOutpostHP(TeamColor team, quint16 hp);
  void updateOutpostStatus(TeamColor team, quint8 status);

private:
  // 查找方法
  RobotData *findRobotById(quint8 robotId);
  BuffPointData *findBuffPointById(quint8 pointId);
  ModuleStatusData *findModuleByName(const QString &moduleName);
  QVariantMap normalizeRobotRespawnStatus(const QVariantMap &status) const; // 归一化复活状态增量字段
  void applyOfficialGameStateSnapshot(GameStage stage, quint16 remainingTime,
                                      quint16 redScore, quint16 blueScore,
                                      quint8 currentRound, bool isPaused);
  void updateBattleScoreBaseline(GameStage previousStage, GameStage nextStage,
                                 quint16 redScore, quint16 blueScore,
                                 quint8 currentRound);
  int roundScoreDelta(bool redSide) const;
  void notifyPeriodicRewards(GameStage previousStage, int previousGameTime);//空中支援时间/金币定时增加
  void trackAllyBaseHealthDropAlert(TeamColor team, quint16 hp, qint64 nowMs);
  void trackAllyOutpostHealthDropAlert(TeamColor team, quint16 hp, qint64 nowMs);
  int currentRuneType() const;
  void setDartMessageData(bool isRedTeam, int targetId, int teamDartHitCount);
  void notifyRuneStatusChanged();
  void updateGameStage();
  void validateAndUpdateData();

private slots:
  void onUpdateTimer();
  void onPopupStateMachineUpdated(); // 弹窗状态机更新回调槽
  void onPopupPayloadFromStateMachine(const QString &type, const QVariantMap &payload);
};

#endif // GAMEDATA_H
