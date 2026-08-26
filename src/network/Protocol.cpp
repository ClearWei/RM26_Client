// SPDX-License-Identifier: MIT
/**
 * @file Protocol.cpp
 * @brief 通信协议处理实现
 * @details 本文件实现了 Protocol 类的静态方法，包括：
 *          - parsePacket: 解析服务器下发的二进制协议帧
 *          - serializePacket: 将 Protobuf 消息序列化为二进制协议帧
 *
 *          协议帧结构（符合 RoboMaster 裁判系统规范）：
 *          ┌───────┬────────┬─────┬───────┬────────┬──────────┬────────┐
 *          │ SOF   │ Length │ Seq │ CRC8  │ CmdID  │   Data   │ CRC16  │
 *          │ (1B)  │  (2B)  │(1B) │ (1B)  │  (2B)  │   (NB)   │  (2B)  │
 *          └───────┴────────┴─────┴───────┴────────┴──────────┴────────┘
 *          └──────── FrameHeader (7B) ────────┘
 *
 *          帧解析流程：
 *          1. 长度校验 → 2. SOF校验 → 3. CRC8校验 → 4. 解析Header
 *          → 5. 总长度校验 → 6. CRC16校验 → 7. 根据CmdID分发处理
 *
 * @author Clear
 * @date 2025-11-29
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

// --- 头文件包含 ---
#include "Protocol.h"
#include "CRC.h"           // CRC8/CRC16 校验函数
#include "robomaster.pb.h" // Protobuf 生成的消息类
#include <QDebug>          // Qt 调试输出
#include <QString>
#include <QDateTime>       // 修复 C2027 错误

// 兼容 MSVC 与 GCC/Clang 的紧凑结构体声明
#if defined(_MSC_VER)
#define PACKED_STRUCT(name) __pragma(pack(push, 1)) struct name
#define PACKED_STRUCT_END __pragma(pack(pop))
#else
#define PACKED_STRUCT(name) struct __attribute__((__packed__)) name
#define PACKED_STRUCT_END
#endif

// 本地紧凑结构体声明（用于二进制反序列化）
PACKED_STRUCT(RobotPosRaw) {
  float x, y, angle;
  quint8 id;
};
PACKED_STRUCT_END;

PACKED_STRUCT(RobotCmdRaw) {
  quint8 type;
  quint8 id;
};
PACKED_STRUCT_END;

PACKED_STRUCT(MapMarkRaw) {
  float x, y;
  quint8 type;
};
PACKED_STRUCT_END;

// --- 协议解析函数 ---

/**
 * @brief 解析二进制协议数据包
 * @details 将收到的原始二进制数据解析为 Protobuf 消息对象。
 *          该函数作为协议适配层，将官方二进制协议转换为应用内部使用的 Protobuf
 * 格式。
 *
 *          支持的命令 ID：
 *          - GAME_STATUS (0x0001): 比赛状态
 *          - ROBOT_STATUS (0x0201): 机器人状态
 *          - EVENT_DATA (0x0101): 场地事件
 *          - ROBOT_POS (0x0203): 机器人位置
 *          - GAME_ROBOT_HP (0x0003): 机器人血量
 *          - POWER_HEAT (0x0202): 功率热量
 *
 * @param packet  原始二进制数据包
 * @param message 输出的 Protobuf 消息对象
 * @return true 解析成功，false 解析失败（校验失败或不支持的 CmdID）
 */
bool Protocol::parsePacket(const QByteArray &packet,
                           robomaster::RoboMasterMessage &message) {
  bool hasPayload = false;
  quint16 cmdId = 0;
  quint16 dataLen = 0;
  QByteArray payload;

  if (!packet.isEmpty() && static_cast<quint8>(packet.at(0)) == 0xA5) {
    // -----------------------------------------------------------------------
    // 官方协议帧：SOF(0xA5) + Header + Payload + CRC16
    // -----------------------------------------------------------------------

    // 步骤 1: 最小长度校验（FrameHeader + CRC16）
    // 最小长度校验 FrameHeader(7) + CRC16(2)
    if (packet.size() < static_cast<int>(sizeof(FrameHeader) + 2)) {
      return false;
    }

    // 步骤 2: CRC8 校验（前5字节）
    // CRC8 校验（前5字节）
    if (!Verify_CRC8_Check_Sum(reinterpret_cast<unsigned char *>(const_cast<char *>(packet.constData())), 5)) {
      qDebug() << "Protocol: Official frame CRC8 failed";
      return false;
    }

    // 步骤 3: 解析 FrameHeader
    const FrameHeader *header =
        reinterpret_cast<const FrameHeader *>(packet.constData());
    cmdId = header->cmdId;
    dataLen = header->dataLength;

    // 步骤 4: 总长度校验
    // 总长度校验
    const int expectedLen = static_cast<int>(sizeof(FrameHeader) + dataLen + 2);
    if (packet.size() < expectedLen) {
      qDebug() << "Protocol: Official frame too short, expected" << expectedLen
               << "got" << packet.size();
      return false;
    }

    // 步骤 5: CRC16 校验（整帧）
    // CRC16 校验（整帧）
    if (!Verify_CRC16_Check_Sum(
            reinterpret_cast<unsigned char *>(const_cast<char *>(packet.constData())),
            expectedLen)) {
      qDebug() << "Protocol: Official frame CRC16 failed";
      return false;
    }

    // 步骤 6: 提取数据域
    payload = QByteArray(packet.constData() + sizeof(FrameHeader), dataLen);
  } else {
    // -------------------------------------------------------------------------
    // 自定义客户端协议：简化帧格式 (无 CRC 校验)
    // 帧结构: CmdID(2B) + Length(2B) + Data(NB)
    // -------------------------------------------------------------------------

    // -------------------------------------------------------------------------
    // 步骤 1: 最小长度校验
    // -------------------------------------------------------------------------
    // 最小包长度 = SimpleFrameHeader (4字节)
    if (packet.size() < (int)sizeof(SimpleFrameHeader)) {
      return false;
    }

    // -------------------------------------------------------------------------
    // 步骤 2: 解析 SimpleFrameHeader
    // -------------------------------------------------------------------------
    const SimpleFrameHeader *header =
        reinterpret_cast<const SimpleFrameHeader *>(packet.constData());
    cmdId = header->cmdId;
    dataLen = header->dataLength;

    // -------------------------------------------------------------------------
    // 步骤 3: 总长度校验
    // -------------------------------------------------------------------------
    // 总长度 = SimpleFrameHeader + DataLength
    if (packet.size() < (int)(sizeof(SimpleFrameHeader) + dataLen)) {
      qDebug() << "Packet too short: expected"
               << (sizeof(SimpleFrameHeader) + dataLen) << "got" << packet.size();
      return false;
    }

    // -------------------------------------------------------------------------
    // 步骤 4: 提取数据域并根据 CmdID 分发处理
    // -------------------------------------------------------------------------
    // 数据域起始位置 = SimpleFrameHeader 之后
    const char *dataPtr = packet.constData() + sizeof(SimpleFrameHeader);
    payload = QByteArray(dataPtr, dataLen);
  }
  // 根据命令 ID 进行分发处理
  switch ((CmdID)cmdId) {

  // ---------------------------------------------------------------------------
  // 0x0001: 比赛状态数据
  // ---------------------------------------------------------------------------
  case CmdID::GAME_STATUS: {
#pragma pack(push, 1)
    struct GameStatusRawV1 {
      quint8 typeProgress; // 低 4 位为类型，高 4 位为阶段。
      quint16 gameTime;
      quint16 redScore;
      quint16 blueScore;
      quint8 round;
      quint8 startedEnded; // bit0 表示开始，bit1 表示结束。
      quint64 syncTimestamp;
    };
    struct GameStatusRawV2 {
      quint8 typeProgress;
      quint16 gameTime;
      quint16 redScore;
      quint16 blueScore;
      quint8 round;
      quint8 startedEnded;
      quint64 syncTimestamp;
      quint32 redEconomy;
      quint32 blueEconomy;
    };
#pragma pack(pop)

    auto *info = message.mutable_game_info();
    if (payload.size() >= static_cast<int>(sizeof(GameStatusRawV2))) {
      GameStatusRawV2 data{};
      memcpy(&data, payload.constData(), sizeof(GameStatusRawV2));
      const quint8 gameProgress = (data.typeProgress >> 4) & 0x0F;
      info->set_stage(static_cast<robomaster::GameStage>(gameProgress));
      info->set_time_remaining(data.gameTime);
      info->set_red_score(data.redScore);
      info->set_blue_score(data.blueScore);
      info->set_current_round(data.round);
      info->set_is_paused((data.startedEnded & 0x04) != 0);
      info->set_red_economy(data.redEconomy);
      info->set_blue_economy(data.blueEconomy);
      hasPayload = true;
    } else if (payload.size() >= static_cast<int>(sizeof(GameStatusRawV1))) {
      GameStatusRawV1 data{};
      memcpy(&data, payload.constData(), sizeof(GameStatusRawV1));
      const quint8 gameProgress = (data.typeProgress >> 4) & 0x0F;
      info->set_stage(static_cast<robomaster::GameStage>(gameProgress));
      info->set_time_remaining(data.gameTime);
      info->set_red_score(data.redScore);
      info->set_blue_score(data.blueScore);
      info->set_current_round(data.round);
      info->set_is_paused((data.startedEnded & 0x04) != 0);
      hasPayload = true;
    } else {
      return false;
    }
    break;
  }

  // ---------------------------------------------------------------------------
  // 0x0201: 机器人性能体系数据
  // ---------------------------------------------------------------------------
  case CmdID::ROBOT_STATUS: {
    robot_status_t data;
    if (deserialize(payload, data)) {
      // 转换为 Protobuf RobotStatus 消息
      auto *robot = message.mutable_robot_status();
      robot->set_id(data.robot_id);
      robot->set_level(data.robot_level);
      robot->set_hp(data.current_HP);
      robot->set_max_hp(data.maximum_HP);
      robot->set_cooling_value(data.shooter_barrel_cooling_value);
      robot->set_heat_limit(data.shooter_barrel_heat_limit);
      robot->set_power_limit(data.chassis_power_limit);
      robot->set_chassis_enabled(data.power_management_chassis_output);
      robot->set_shooter_enabled(data.power_management_shooter_output);
      hasPayload = true;
    }
    break;
  }

  // ---------------------------------------------------------------------------
  // 0x0101: 场地事件数据
  // ---------------------------------------------------------------------------
  case CmdID::EVENT_DATA: {
    if (payload.size() == 4) {
        event_data_t data;
        if (deserialize(payload, data)) {
            auto *evt = message.mutable_event();
            evt->set_event_id(0x0101); // 使用 0x0101 作为事件ID
            evt->set_param(std::to_string(data.event_data)); // 将32位数据转为字符串传递
        hasPayload = true;
        }
    } else {
        GameEventData data;
        if (deserialize(payload, data)) {
            // 转换为 Protobuf Event 消息
            auto *evt = message.mutable_event();
            evt->set_event_id(data.eventType);
        hasPayload = true;
        }
    }
    break;
  }

  // ---------------------------------------------------------------------------
  // 0x020D: 雷达标记/哨兵与能量机关信息
  // ---------------------------------------------------------------------------
  case CmdID::RADAR_MARK_DATA: {
    radar_mark_data_t data;
    if (deserialize(payload, data)) {
      // 转换为 Protobuf RadarMarkData 消息
      auto *radar = message.mutable_game_info()->mutable_radar_mark_data();
      radar->set_mark_hero_progress(data.mark_hero_progress);
      radar->set_mark_engineer_progress(data.mark_engineer_progress);
      radar->set_mark_standard_3_progress(data.mark_standard_3_progress);
      radar->set_mark_standard_4_progress(data.mark_standard_4_progress);
      radar->set_info(data.info);

      // 解析 info 中的能量机关可激活状态 (bit 14)
      // 官方协议: bit 14 为 energy activatable
      bool isActivatable = (data.info & (1 << 14)) != 0;
      radar->set_energy_activatable(isActivatable);

      hasPayload = true;
    }
    break;
  }

  // ---------------------------------------------------------------------------
  // 0x0104: 裁判警告数据
  // ---------------------------------------------------------------------------
  case CmdID::REFEREE_WARNING: {
    referee_warning_t data;
    if (deserialize(payload, data)) {
      auto *warning = message.mutable_referee_warning();
      warning->set_level(data.level);
      warning->set_offending_robot_id(data.offending_robot_id);
      warning->set_count(data.count);
      warning->set_source("SERIAL");
      warning->set_timestamp(QDateTime::currentMSecsSinceEpoch());

      hasPayload = true;
    }
    break;
  }

  // ---------------------------------------------------------------------------
  // 0x0203: 机器人位置数据
  // ---------------------------------------------------------------------------
  case CmdID::ROBOT_POS: {
    robot_pos_t data;
    if (deserialize(payload, data)) {
      //RobotStatus 统一管理
      auto *status = message.mutable_robot_position();
      status->set_x(data.x);
      status->set_y(data.y);
      status->set_angle(data.angle);
      hasPayload = true;
    }
    break;
  }

  // ---------------------------------------------------------------------------
  // 0x0003: 机器人血量数据（官方协议）
  // ---------------------------------------------------------------------------
  case CmdID::GAME_ROBOT_HP: {
    game_robot_HP_t data;
    if (deserialize(payload, data)) {
      // UDP 兜底链路将 0x0003 的“己方视角”血量转为 GlobalUnitStatus。
      // 客户端再根据 currentRobotId 解释为红/蓝方己方。
      auto *status = message.mutable_global_unit_status();
      status->set_base_health(data.ally_base_HP);
      status->set_base_status(data.ally_base_HP == 0 ? 3 : 1);
      status->set_outpost_health(data.ally_outpost_HP);
      status->set_outpost_status(data.ally_outpost_HP == 0 ? 3 : 1);
      status->add_robot_health(data.ally_1_robot_HP);
      status->add_robot_health(data.ally_2_robot_HP);
      status->add_robot_health(data.ally_3_robot_HP);
      if (data.ally_4_robot_HP > 0 || data.ally_7_robot_HP > 0) {
        status->add_robot_health(data.ally_4_robot_HP);
        if (data.ally_7_robot_HP > 0) {
          status->add_robot_health(data.ally_7_robot_HP);
        }
      }
      hasPayload = true;
    }
    break;
  }

  // ---------------------------------------------------------------------------
  // 0x0202: 底盘缓冲能量和热量数据（官方协议）
  // ---------------------------------------------------------------------------
  case CmdID::POWER_HEAT: {
    power_heat_data_t data;
    if (deserialize(payload, data)) {
      // 更新机器人热量缓冲能量信息
      auto *robot = message.mutable_robot_status();
      robot->set_id(0);
      robot->set_heat(data.shooter_17mm_1_barrel_heat);
      if (data.shooter_42mm_barrel_heat > 0) {
        robot->set_heat(data.shooter_42mm_barrel_heat);
      }
      // UDP 兜底链路复用 reserved1 传递当前底盘功率，避免左下角面板长期为 0。
      robot->set_chassis_power(static_cast<float>(data.reserved1));
      robot->set_buffer_energy(data.buffer_energy);
      hasPayload = true;
    }
    break;
  }

  // ---------------------------------------------------------------------------
  // 0x0204: 机器人增益数据
  // ---------------------------------------------------------------------------
  case CmdID::BUFF_STATUS: {
    buff_t data;
    if (deserialize(payload, data)) {
      auto *evt = message.mutable_event();
      evt->set_event_id(0x0204);
      // 将多个字节编码为 CSV 格式字符串: recovery,cooling,defence,vulnerability,attack,energy
      QString params = QString("%1,%2,%3,%4,%5,%6")
                           .arg(data.recovery_buff)
                           .arg(data.cooling_buff)
                           .arg(data.defence_buff)
                           .arg(data.vulnerability_buff)
                           .arg(data.attack_buff)
                           .arg(data.remaining_energy);
      evt->set_param(params.toStdString());
    }
    break;
  }

  // ---------------------------------------------------------------------------
  // 0x0207: 实时射击数据
  // ---------------------------------------------------------------------------
  case CmdID::SHOOT_DATA: {
    shoot_data_t data;
    if (deserialize(payload, data)) {
      auto *robot = message.mutable_robot_status();
      // 使用特殊 ID 201 标记这是射击数据更新包 (区分于 ID 0 的热量包)
      robot->set_id(201);
      robot->set_muzzle_velocity(data.initial_speed);
      hasPayload = true;
    }
    break;
  }

  // ---------------------------------------------------------------------------
  // 0x0208: 允许发弹量
  // ---------------------------------------------------------------------------
  case CmdID::BULLET_REMAINING: {
    projectile_allowance_t data;
    if (deserialize(payload, data)) {
      auto *robot = message.mutable_robot_status();
      // 使用特殊 ID 200 标记这是弹量更新包 (区分于 ID 0 的热量包)
      robot->set_id(200);
      // 拆分 17mm / 42mm 允许发弹量，并传递堡垒储备量
      robot->set_allowed_ammo_17mm(data.projectile_allowance_17mm);
      robot->set_allowed_ammo_42mm(data.projectile_allowance_42mm);
      robot->set_fortress_ammo(data.projectile_allowance_fortress);
      hasPayload = true;
    }
    break;
  }

  // ---------------------------------------------------------------------------
  // 0x020B: 己方地面机器人位置
  // ---------------------------------------------------------------------------
  case CmdID::GROUND_ROBOT_POSITION: {
    ground_robot_position_t data;
    if (deserialize(payload, data)) {
      auto *groundPos = message.mutable_ground_robot_position();
      groundPos->set_hero_x(data.hero_x);
      groundPos->set_hero_y(data.hero_y);
      groundPos->set_engineer_x(data.engineer_x);
      groundPos->set_engineer_y(data.engineer_y);
      groundPos->set_infantry_3_x(data.standard_3_x);
      groundPos->set_infantry_3_y(data.standard_3_y);
      groundPos->set_infantry_4_x(data.standard_4_x);
      groundPos->set_infantry_4_y(data.standard_4_y);
      hasPayload = true;
    }
    break;
  }

  // ---------------------------------------------------------------------------
  // 0x0305: 敌方机器人位置 (选手端接收)
  // ---------------------------------------------------------------------------
  case CmdID::MAP_ROBOT_DATA: {
    map_robot_data_t data;
    if (deserialize(payload, data)) {
      auto *enemyPos = message.mutable_enemy_positions();
      enemyPos->set_hero_x(data.hero_position_x);
      enemyPos->set_hero_y(data.hero_position_y);
      enemyPos->set_engineer_x(data.engineer_position_x);
      enemyPos->set_engineer_y(data.engineer_position_y);
      enemyPos->set_infantry_3_x(data.infantry_3_position_x);
      enemyPos->set_infantry_3_y(data.infantry_3_position_y);
      enemyPos->set_infantry_4_x(data.infantry_4_position_x);
      enemyPos->set_infantry_4_y(data.infantry_4_position_y);
      enemyPos->set_infantry_5_x(data.infantry_5_position_x);
      enemyPos->set_infantry_5_y(data.infantry_5_position_y);
      enemyPos->set_sentry_x(data.sentry_position_x);
      enemyPos->set_sentry_y(data.sentry_position_y);
      hasPayload = true;
    }
    break;
  }

  // ---------------------------------------------------------------------------
  // 0x0002: 比赛结果数据
  // ---------------------------------------------------------------------------
  case CmdID::GAME_RESULT: {
    game_result_t data;
    if (deserialize(payload, data)) {
      auto *evt = message.mutable_event();
      evt->set_event_id(2000); // 自定义事件 ID: 2000 -> GAME_RESULT
      evt->set_param(
          QString::number(static_cast<int>(data.winner)).toStdString());
      qDebug() << "Protocol: GAME_RESULT parsed, winner=" << static_cast<int>(data.winner);
      hasPayload = true;
      break; // 解析成功，退出 switch
    }
    break;
  }

  // ---------------------------------------------------------------------------
  // 未知命令 ID
  // ---------------------------------------------------------------------------
  default:
    // 不支持的命令 ID，静默返回失败
    return false;
  }

    return hasPayload;
  }

  // --- 协议序列化函数 ---
  /**
   * @brief 序列化 Protobuf 消息为二进制协议帧
   * @details 将应用内部的 Protobuf 消息转换为可通过 UDP 发送的二进制协议帧。
   *
   *          当前支持序列化的消息类型：
   *          - robot_status: 机器人状态
   *          - robot_cmd:    机器人控制指令
   *          - map_marking:  小地图标记
   *
   *          帧构造流程：
   *          1. 根据消息类型确定 CmdID 和 Payload
   *          2. 分配帧缓冲区
   *          3. 填充 FrameHeader
   *          4. 计算并填充 CRC8（帧头）
   *          5. 拷贝 Payload
   *          6. 计算并填充 CRC16（整帧）
   *
   * @param message Protobuf 消息对象
   * @return QByteArray 序列化后的二进制帧，如果消息类型不支持则返回空数组
   */
  QByteArray Protocol::serializePacket(
      const robomaster::RoboMasterMessage &message) {
    QByteArray payload; // 数据域内容
    quint16 cmdId = 0;  // 命令 ID

    // -------------------------------------------------------------------------
    // 根据消息类型确定 CmdID 和 Payload
    // -------------------------------------------------------------------------

    // ---------------------------------------------------------------------------
    // robot_status: 机器人状态（用于单元测试和模拟）
    // ---------------------------------------------------------------------------
    if (message.has_robot_status()) {
      cmdId = (quint16)CmdID::ROBOT_STATUS;

      // 构造原始二进制结构
      RobotStatusData raw;
      memset(&raw, 0, sizeof(raw));
      raw.robotId = (quint8)message.robot_status().id();
      raw.level = (quint8)message.robot_status().level();
      raw.currentHP = (quint16)message.robot_status().hp();
      raw.maxHP = (quint16)message.robot_status().max_hp();
      raw.currentHeat = (quint16)message.robot_status().heat();
      raw.heatLimit = (quint16)message.robot_status().heat_limit();
      payload.resize(sizeof(RobotStatusData));
      memcpy(payload.data(), &raw, sizeof(RobotStatusData));
    }
    // ---------------------------------------------------------------------------
    // robot_cmd: 机器人控制指令
    // ---------------------------------------------------------------------------
    else if (message.has_robot_cmd()) {
      cmdId = (quint16)CmdID::ROBOT_CONTROL;

// 简化结构：type(1B) + id(1B)
#pragma pack(push, 1)
      struct RobotCmdRaw {
        quint8 type;
        quint8 id;
      };
#pragma pack(pop)

      RobotCmdRaw raw;
      raw.type = (quint8)message.robot_cmd().cmd_type();
      raw.id = (quint8)message.robot_cmd().target_id();

      payload.resize(sizeof(RobotCmdRaw));
      memcpy(payload.data(), &raw, sizeof(RobotCmdRaw));
    }
    // ---------------------------------------------------------------------------
    // map_marking: 小地图标记
    // ---------------------------------------------------------------------------
    else if (message.has_map_marking()) {
      cmdId = (quint16)CmdID::MAP_INTERACTION;

// 结构：x(float) + y(float) + type(1B)
#pragma pack(push, 1)
      struct MapMarkRaw {
        float x, y;
        quint8 type;
      };
#pragma pack(pop)

      MapMarkRaw raw;
      raw.x = message.map_marking().x();
      raw.y = message.map_marking().y();
      raw.type = (quint8)message.map_marking().mark_type();

      payload.resize(sizeof(MapMarkRaw));
      memcpy(payload.data(), &raw, sizeof(MapMarkRaw));
    }
    // ---------------------------------------------------------------------------
    // client_status: 客户端状态（暂不支持二进制序列化）
    // ---------------------------------------------------------------------------
    else if (message.has_client_status()) {
      return QByteArray(); // 返回空，表示不支持
    }
    // ---------------------------------------------------------------------------
    // 其他不支持的消息类型
    // ---------------------------------------------------------------------------
    else {
      return QByteArray();
    }

    // -------------------------------------------------------------------------
    // 构造完整协议帧 (自定义客户端协议：简化格式)
    // -------------------------------------------------------------------------
    // 帧结构：CmdID(2B) + Length(2B) + Data(NB)
    QByteArray frame;
    frame.resize(sizeof(SimpleFrameHeader) + payload.size());

    // 填充 SimpleFrameHeader
    SimpleFrameHeader *header =
        reinterpret_cast<SimpleFrameHeader *>(frame.data());
    header->cmdId = cmdId;               // 命令 ID
    header->dataLength = payload.size(); // 数据域长度

    // 拷贝 Payload 到数据域
    memcpy(frame.data() + sizeof(SimpleFrameHeader), payload.data(),
           payload.size());

    return frame;
  }
