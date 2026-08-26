// SPDX-License-Identifier: MIT
/**
 * @file Protocol.h
 * @brief 通信协议处理类
 * @details 定义了通信协议的数据结构、枚举以及序列化/反序列化方法。
 * @author Clear
 * @date 2025-11-29
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "robomaster.pb.h"
#include <QByteArray>
#include <QObject>

namespace robomaster {
class RoboMasterMessage;
}

/**
 * @enum PacketType
 * @brief 数据包类型枚举
 * @details 定义了所有支持的协议包类型，用于区分不同的业务数据。
 */
enum class PacketType : quint8 {
  HEARTBEAT = 0x01,      // 心跳包
  ROBOT_STATUS = 0x02,   // 机器人状态
  GAME_STATUS = 0x03,    // 比赛状态
  BASE_HEALTH = 0x04,    // 基地血量
  ROBOT_HEALTH = 0x05,   // 机器人血量
  ROBOT_POSITION = 0x06, // 机器人位置
  GAME_EVENT = 0x07,     // 比赛事件
  ROBOT_COMMAND = 0x08,  // 机器人指令
  ENERGY_STATUS = 0x09,  // 能量机关状态
  SENTRY_STATUS = 0x0A,  // 哨兵状态
  MODULE_STATUS = 0x0B,  // 模块状态
  DAMAGE_EVENT = 0x0C,   // 伤害事件
  SHOOT_DATA = 0x0D,     // 射击数据
  BUFF_STATUS = 0x0E,    // Buff状态
  ECONOMY_DATA = 0x0F,    // 经济数据
  REFEREE_WARNING = 0x10 // 裁判警告
};

/**
 * @enum RobotType
 * @brief 机器人类型枚举
 */
enum class RobotType : quint8 {
  HERO = 1,       // 英雄
  ENGINEER = 2,   // 工程
  INFANTRY_3 = 3, // 步兵3号
  INFANTRY_4 = 4, // 步兵4号
  INFANTRY_5 = 5, // 步兵5号
  AERIAL = 6,     // 空中机器人
  SENTRY = 7,     // 哨兵
  DART = 8,       // 飞镖
  RADAR = 9       // 雷达站
};

/**
 * @enum TeamColor
 * @brief 队伍颜色枚举
 */
enum class TeamColor : quint8 { RED = 0, BLUE = 1 };

#pragma pack(push, 1)

/**
 * @struct FrameHeader
 * @brief 串口协议帧头（保留用于兼容）
 * @details 用于串口通信，包含 CRC8 校验
 */
struct FrameHeader {
  quint8 sof;         // 帧起始字节（0xA5）
  quint16 dataLength; // 数据段长度
  quint8 seq;         // 包序号
  quint8 crc8;        // CRC8 校验值
  quint16 cmdId;      // 命令码
};

/**
 * @struct SimpleFrameHeader
 * @brief 自定义客户端协议帧头（简化版）
 * @details 用于 UDP 网络通信，无 CRC 校验
 *          帧格式: CmdID(2B) + Length(2B) + Data(NB)
 */
struct SimpleFrameHeader {
  quint16 cmdId;      // 命令码
  quint16 dataLength; // 数据域长度
};

/**
 * @enum CmdID
 * @brief 命令ID枚举
 */
enum class CmdID : quint16 {
  VIDEO_DATA = 0x0009,        // 视频数据
  GAME_STATUS = 0x0001,       // 比赛状态 (11字节, 1Hz)
  GAME_RESULT = 0x0002,       // 比赛结果 (1字节)
  GAME_ROBOT_HP = 0x0003,     // 机器人血量 (16字节, 3Hz)
  EVENT_DATA = 0x0101,        // 场地事件数据 (4字节)
  PROJECTILE_SUPPLY = 0x0102, // 补给站动作 (3字节)
  REFEREE_WARNING = 0x0104,   // 裁判警告数据 (3字节)
  DART_STATUS = 0x0105,       // 飞镖发射状态 (3字节)
  ROBOT_STATUS = 0x0201,      // 机器人性能体系 (13字节, 10Hz)
  POWER_HEAT = 0x0202,        // 底盘缓冲能量和热量 (14字节, 10Hz)
  ROBOT_POS = 0x0203,         // 机器人位置 (12字节)
  BUFF_STATUS = 0x0204,       // 机器人增益 (8字节)
  AERIAL_ENERGY = 0x0205,     // 空中机器人能量状态
  ROBOT_HURT = 0x0206,        // 伤害状态 (1字节)
  SHOOT_DATA = 0x0207,        // 实时射击数据 (7字节)
  BULLET_REMAINING = 0x0208,  // 允许发弹量 (8字节)
  GROUND_ROBOT_POSITION = 0x020B, // 己方地面机器人位置 (40字节)
  RFID_STATUS = 0x0209,       // RFID状态 (5字节)
  DART_CLIENT_CMD = 0x020A,   // 飞镖客户端指令

  RADAR_MARK_DATA = 0x020D,   // 雷达标记数据/哨兵与能量机关信息 (6字节)
  MAP_INTERACTION = 0x0301,   // 小地图交互数据
  MAP_ROBOT_DATA = 0x0305,    // 选手端接收机器人数据 (24字节)
  ROBOT_CONTROL = 0x0401      // 机器人控制指令
};

// --- 官方协议数据结构 (严格按照 RoboMaster 2026 通信协议 V1.0.0) ---

/**
 * @struct game_status_t
 * @brief 0x0001 比赛状态数据 (官方表1-5)
 * @details 数据长度: 11字节, 频率: 1Hz
 */
struct game_status_t {
  quint8 game_type : 4;      // 比赛类型
  quint8 game_progress : 4;  // 比赛阶段
  quint16 stage_remain_time; // 剩余时间 (秒)
  quint64 SyncTimeStamp;     // UNIX时间戳
};

/**
 * @struct game_result_t
 * @brief 0x0002 比赛结果数据 (官方表1-6)
 * @details 数据长度: 1字节
 */
struct game_result_t {
  quint8 winner; // 0=平局, 1=红方胜利, 2=蓝方胜利
};

/**
 * @struct game_robot_HP_t
 * @brief 0x0003 机器人血量数据 (官方表1-7)
 * @details 数据长度: 16字节, 频率: 3Hz, 只发送己方血量
 */
struct game_robot_HP_t {
  quint16 ally_1_robot_HP; // 己方1号英雄
  quint16 ally_2_robot_HP; // 己方2号工程
  quint16 ally_3_robot_HP; // 己方3号步兵
  quint16 ally_4_robot_HP; // 己方4号步兵
  quint16 reserved;        // 保留位
  quint16 ally_7_robot_HP; // 己方7号哨兵
  quint16 ally_outpost_HP; // 己方前哨站
  quint16 ally_base_HP;    // 己方基地
};

/**
 * @struct event_data_t
 * @brief 0x0101 场地事件数据 (官方表1-8)
 * @details 数据长度: 4字节
 */
struct event_data_t {
  quint32 event_data;
};

/**
 * @struct referee_warning_t
 * @brief 0x0104 裁判警告数据 (官方表1-9)
 * @details 数据长度: 3字节
 */
struct referee_warning_t {
  quint8 level;
  quint8 offending_robot_id;
  quint8 count;
};

/**
 * @struct robot_status_t
 * @brief 0x0201 机器人性能体系数据 (官方表1-11)
 * @details 数据长度: 13字节, 频率: 10Hz
 */
struct robot_status_t {
  quint8 robot_id;
  quint8 robot_level;
  quint16 current_HP;
  quint16 maximum_HP;
  quint16 shooter_barrel_cooling_value;
  quint16 shooter_barrel_heat_limit;
  quint16 chassis_power_limit;
  quint8 power_management_gimbal_output : 1;
  quint8 power_management_chassis_output : 1;
  quint8 power_management_shooter_output : 1;
  quint8 reserved : 5;
};


/**
 * @struct hurt_data_t
 * @brief 0x0206 伤害状态数据 (官方表1-13)
 * @details 数据长度: 1字节
 */
struct hurt_data_t {
    quint8 armor_id : 4;  // 当hurt_type=0时，表示装甲板ID(0-4)
    quint8 hurt_type : 4; // 0:装甲板, 1:模块掉线, 2:超射速, 3:超热量, 4:超功率, 5:撞击
};

/**
 * @struct power_heat_data_t
 * @brief 0x0202 底盘缓冲能量和热量 (官方表1-12)
 * @details 数据长度: 14字节, 频率: 10Hz
 */
struct power_heat_data_t {
  quint16 reserved1;
  quint16 reserved2;
  float reserved3;
  quint16 buffer_energy;
  quint16 shooter_17mm_1_barrel_heat;
  quint16 shooter_42mm_barrel_heat;
};

/**
 * @struct robot_pos_t
 * @brief 0x0203 机器人位置数据 (官方表1-13)
 * @details 数据长度: 12字节, 频率: 1Hz
 */
struct robot_pos_t {
  float x;             // 本机器人位置x坐标 (单位: m)
  float y;             // 本机器人位置y坐标 (单位: m)
  float angle;         // 本机器人测速模块的朝向 (单位: 度)
};

/**
 * @struct buff_t
 * @brief 0x0204 增益数据 (官方表1-14)
 * @details 数据长度: 8字节
 */
struct buff_t {
  quint8 recovery_buff;
  quint16 cooling_buff;
  quint8 defence_buff;
  quint8 vulnerability_buff;
  quint16 attack_buff;
  quint8 remaining_energy;
};


/**
 * @struct radar_mark_data_t
 * @brief 0x020D 雷达标记/哨兵与能量机关信息 (官方表1-22)
 * @details 数据长度: 6字节
 */
struct radar_mark_data_t {
  quint8 mark_hero_progress;
  quint8 mark_engineer_progress;
  quint8 mark_standard_3_progress;
  quint8 mark_standard_4_progress;
  quint16 info; // 第 12—13 位为哨兵姿态，第 14 位表示能量机关能否激活。
};

/**
 * @struct shoot_data_t
 * @brief 0x0207 实时射击数据 (官方表1-16)
 * @details 数据长度: 7字节
 */
struct shoot_data_t {
  quint8 bullet_type;
  quint8 shooter_number;
  quint8 launching_frequency;
  float initial_speed;
};

/**
 * @struct projectile_allowance_t
 * @brief 0x0208 允许发弹量 (官方表1-17)
 * @details 数据长度: 8字节, 频率: 10Hz
 */
struct projectile_allowance_t {
  quint16 projectile_allowance_17mm; // 17mm弹丸允许发弹量 (发)
  quint16 projectile_allowance_42mm; // 42mm弹丸允许发弹量 (发)
  quint16 remaining_gold_coin;       // 剩余金币数量 (个)
  quint16 projectile_allowance_fortress; // 堡垒提供的储备17mm允许发弹量
};

/**
 * @struct rfid_status_t
 * @brief 0x0209 RFID状态 (官方表1-18)
 * @details 数据长度: 5字节
 */
struct rfid_status_t {
  quint32 rfid_status;
  quint8 rfid_status_2;
};

/**
 * @struct ground_robot_position_t
 * @brief 0x020B 己方地面机器人位置 (官方表1-20)
 * @details 数据长度: 40字节
 */
struct ground_robot_position_t {
  float hero_x;
  float hero_y;
  float engineer_x;
  float engineer_y;
  float standard_3_x;
  float standard_3_y;
  float standard_4_x;
  float standard_4_y;
  float reserved1;
  float reserved2;
};

/**
 * @struct map_robot_data_t
 * @brief 0x0305 选手端接收机器人数据 (官方表1-37)
 * @details 数据长度: 24字节
 */
struct map_robot_data_t {
  quint16 hero_position_x;
  quint16 hero_position_y;
  quint16 engineer_position_x;
  quint16 engineer_position_y;
  quint16 infantry_3_position_x;
  quint16 infantry_3_position_y;
  quint16 infantry_4_position_x;
  quint16 infantry_4_position_y;
  quint16 infantry_5_position_x;
  quint16 infantry_5_position_y;
  quint16 sentry_position_x;
  quint16 sentry_position_y;
};

// 遗留结构体：仅用于内部逻辑兼容，网络传输已迁移至 Protobuf。

/**
 * @struct RobotStatusData
 * @brief 机器人状态数据结构
 */
struct RobotStatusData {
  quint8 robotId;                   // 机器人ID
  quint8 level;                     // 机器人等级
  quint16 currentHP;                // 当前血量
  quint16 maxHP;                    // 最大血量
  quint16 experience;               // 经验值
  quint16 maxExperience;            // 最大经验值
  quint16 power;                    // 当前功率
  quint16 maxPower;                 // 最大功率
  quint16 currentHeat;              // 当前热量
  quint16 heatLimit;                // 热量上限
  quint16 bulletCount;              // 弹药数量
  quint16 bulletLimit;              // 弹药上限
  quint8 status;                    // 机器人状态
  quint8 isClientConnected : 1;     // 客户端连接状态
  quint8 isControllerConnected : 1; // 控制器连接状态
  quint8 reserved : 6;              // 保留位
};

/**
 * @struct GameStatusData
 * @brief 比赛状态数据结构
 */
struct GameStatusData {
  quint8 gameType : 4;     // 比赛类型
  quint8 gameProgress : 4; // 比赛阶段
  quint16 gameTime;        // 比赛剩余时间
  quint16 redScore;        // 红方得分
  quint16 blueScore;       // 蓝方得分
  quint8 currentRound;     // 当前回合
  quint8 gameStarted : 1;  // 比赛是否开始
  quint8 gameEnded : 1;    // 比赛是否结束
  quint8 reserved : 6;     // 保留位
  quint64 syncTimeStamp;   // 同步时间戳
};

/**
 * @struct BaseHealthData
 * @brief 基地血量数据结构
 */
struct BaseHealthData {
  quint8 team;             // 队伍颜色
  quint16 currentHP;       // 当前血量
  quint16 maxHP;           // 最大血量
  quint16 virtualShield;   // 虚拟护盾
  quint8 isInvincible : 1; // 是否无敌
  quint8 status:2;
  quint8 reserved : 5;     // 保留位
};

/**
 * @struct GameEventData
 * @brief 比赛事件数据结构
 */
struct GameEventData {
  quint32 eventType;         // 事件类型
  quint8 eventData;          // 事件数据
  char eventDescription[64]; // 事件描述
};

/**
 * @struct EnergyStatusData
 * @brief 能量机关状态数据结构
 */
struct EnergyStatusData {
  quint16 energyValue;    // 能量值
  quint16 maxEnergyValue; // 最大能量值
  quint16 cooldownTime;   // 冷却时间
  quint8 isActivated : 1; // 是否激活
  quint8 isAvailable : 1; // 是否可用
  quint8 reserved : 6;    // 保留位
};

/**
 * @struct SentryStatusData
 * @brief 哨兵状态数据结构
 */
struct SentryStatusData {
  quint8 team;             // 队伍颜色
  quint16 currentHP;       // 当前血量
  quint16 maxHP;           // 最大血量
  quint8 isAlive : 1;      // 是否存活
  quint8 isInvincible : 1; // 是否无敌
  quint8 reserved : 6;     // 保留位
};

/**
 * @struct ModuleStatusData
 * @brief 模块状态数据结构
 */
struct ModuleStatusData {
  char moduleName[32]; // 模块名称
  quint8 status;       // 模块状态 (0:正常, 1:离线, 2:故障)
  quint8 temperature;  // 温度
  quint8 voltage;      // 电压
  quint8 isOnline : 1; // 是否在线
  quint8 isNormal : 1; // 是否正常
  quint8 reserved : 6; // 保留位
};

/**
 * @struct DamageEventData
 * @brief 伤害事件数据结构
 */
struct DamageEventData {
  quint8 attackerId;   // 攻击者ID
  quint8 victimId;     // 受害者ID
  quint16 damage;      // 伤害值
  quint8 armorId : 4;  // 装甲板ID
  quint8 hurtType : 4; // 伤害类型
};

/**
 * @struct ShootData
 * @brief 射击数据结构
 */
struct ShootData {
  quint8 robotId;      // 机器人ID
  quint8 bulletType;   // 子弹类型
  quint8 shooterId;    // 发射机构ID
  quint8 bulletFreq;   // 子弹射频
  float bulletSpeed;   // 子弹射速
  quint16 currentHeat; // 当前热量
  quint16 heatLimit;   // 热量上限
  quint16 bulletCount; // 弹药数量
  quint16 bulletLimit; // 弹药上限
};

/**
 * @struct EconomyData
 * @brief 经济数据结构
 */
struct EconomyData {
  quint8 robotId;     // 机器人ID
  quint16 goldCoin;   // 金币数量
  quint16 silverCoin; // 银币数量
  quint16 experience; // 经验值
};

#pragma pack(pop)

/**
 * @class Protocol
 * @brief 协议处理类
 * @details 提供基于 Protobuf 的消息解析和序列化功能。
 */
class Protocol : public QObject {
  Q_OBJECT

public:
  /**
   * @brief 解析数据包
   * @details 将原始 UDP 数据报解析为 RoboMasterMessage 对象。
   *
   * @param packet 原始数据包
   * @param message 解析后的消息对象引用
   * @return bool 解析是否成功
   */
  static bool parsePacket(const QByteArray &packet,
                          robomaster::RoboMasterMessage &message);

  /**
   * @brief 序列化数据包
   * @details 将 RoboMasterMessage 对象序列化为 UDP 数据报。
   *
   * @param message 待序列化的消息对象
   * @return QByteArray 序列化后的数据
   */
  static QByteArray
  serializePacket(const robomaster::RoboMasterMessage &message);

  /**
   * @brief 反序列化辅助函数
   * @details 用于处理遗留的二进制结构体数据。
   *
   * @tparam T 目标结构体类型
   * @param data 原始数据
   * @param result 结果引用
   * @return bool 反序列化是否成功
   */
  template <typename T>
  static bool deserialize(const QByteArray &data, T &result) {
    if (data.size() >= static_cast<int>(sizeof(T))) {
      memcpy(&result, data.constData(), sizeof(T));
      return true;
    }
    return false;
  }
};

#endif // PROTOCOL_H
