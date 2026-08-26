#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: MIT
"""
RoboMaster 裁判系统通信协议定义

@file protocol.py
@brief 定义协议CmdID、帧结构和CRC校验
@author Fudan EGA Team
@date 2025-12-07
@copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).

协议参考: RoboMaster 2026 机甲大师高校系列赛通信协议 V1.2.0
"""

import struct
from enum import IntEnum

# 帧起始字节
SOF = 0xA5

class CmdID(IntEnum):
    VIDEO_DATA = 0x0009
    GAME_STATUS = 0x0001
    GAME_RESULT = 0x0002
    GAME_ROBOT_HP = 0x0003
    EVENT_DATA = 0x0101
    PROJECTILE_SUPPLY = 0x0102
    REFEREE_WARNING = 0x0104
    DART_STATUS = 0x0105
    ROBOT_STATUS = 0x0201
    POWER_HEAT = 0x0202
    ROBOT_POS = 0x0203
    BUFF_STATUS = 0x0204
    AERIAL_ENERGY = 0x0205
    ROBOT_HURT = 0x0206
    SHOOT_DATA = 0x0207
    BULLET_REMAINING = 0x0208
    RFID_STATUS = 0x0209
    DART_CLIENT_CMD = 0x020A

    MAP_INTERACTION = 0x0301
    ROBOT_CONTROL = 0x0401

# 结构体格式（小端序 <）
# B、H、I、f 分别表示 uint8、uint16、uint32 和 float。
# 位域由打包逻辑手动处理，必要时简化为字节

# 帧头：SOF(1) + DataLen(2) + Seq(1) + CRC8(1) + CmdID(2)
HEADER_FMT = "<BHBBH"

# 0x0401 机器人控制：控制类型、目标编号
ROBOT_CONTROL_FMT = "<BB"

# 0x0301 地图交互：x、y、交互类型
MAP_INTERACTION_FMT = "<ffB"

# 0x0001 比赛状态（模拟器扩展）
# 依次编码比赛阶段、剩余时间、红蓝比分、局数、启停状态、时间戳和双方经济。
# 扩展字段用于在 Web 端联动“比分/经济”控制，不影响前向兼容解析。
GAME_STATUS_FMT = "<BHHHBBQII"

# 0x0002 比赛结果：winner 为 0 表示平局、1 表示红方、2 表示蓝方。
GAME_RESULT_FMT = "<B"

# 0x0201 机器人状态：编号、等级、当前/最大血量、冷却值、热量上限、功率上限和输出状态。
ROBOT_STATUS_FMT = "<BBHHHHHB"

# 0x0202 功率热量：当前功率（兼容扩展）、保留字段、缓冲能量和两种枪口热量。
POWER_HEAT_FMT = "<HHfHHH"

# 0x0203 机器人位置：x、y、朝向和目标机器人编号。
ROBOT_POS_FMT = "<fffB"

# 0x0101 场地事件：事件类型、数值和 64 字节说明。
EVENT_DATA_FMT = "<IB64s"

# 0x0104 判罚警告：等级、违规机器人编号和累计次数。
REFEREE_WARNING_FMT = "<BBB"

# 0x0003 全场单位血量（GlobalUnitStatus）
# 官方协议：己方1号英雄、工程、3号步兵、4号步兵、保留、7号哨兵、前哨站、基地 (各2字节)
# 简化：16字节 = 8 个 uint16
GAME_ROBOT_HP_FMT = "<HHHHHHHH"

def pack_header(data_len, seq, cmd_id):
    # CRC8 在数据装配完成后计算
    return struct.pack(HEADER_FMT, SOF, data_len, seq, 0, cmd_id)

# =============================================================================
# 自定义客户端协议：简化帧格式 (无 CRC 校验)
# =============================================================================
# 帧结构: CmdID(2B) + Length(2B) + Data(NB)
SIMPLE_HEADER_FMT = "<HH"  # CmdID(uint16) + Length(uint16)
SIMPLE_HEADER_SIZE = struct.calcsize(SIMPLE_HEADER_FMT)  # 4 字节

def pack_simple_header(cmd_id, data_len):
    """
    构造简化帧头

    参数:
        cmd_id: 命令 ID
        data_len: 数据域长度

    返回:
        bytes: 简化帧头 (4 字节)
    """
    return struct.pack(SIMPLE_HEADER_FMT, cmd_id, data_len)
