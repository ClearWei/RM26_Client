#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: MIT
"""
UDP数据发送器

@file udp_sender.py
@brief 将比赛状态数据通过UDP广播给客户端
@author Fudan EGA Team
@date 2025-12-07
@copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).

功能说明:
    - 定时发送比赛状态、机器人血量等数据
    - 构建符合官方协议的数据帧
    - 支持自定义目标IP和端口
"""

import socket
import time
import threading
import sys
import os
import struct

from . import protocol
from . import crc

class UDPSender:
    def __init__(self, state_manager, host="127.0.0.1", port=3333, current_robot_id=1):
        self.state_manager = state_manager
        self.host = host
        self.port = port
        self.current_robot_id = int(current_robot_id or 1)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.running = False
        self.seq = 0

    def start(self):
        self.running = True
        threading.Thread(target=self._send_loop, daemon=True).start()

    def stop(self):
        self.running = False

    def _send_loop(self):
        while self.running:
            try:
                state = self.state_manager.get_state()
                self._send_game_status(
                    state["gameStatus"],
                    state.get("globalLogisticsStatus", state["gameStatus"]),
                )
                self._send_global_unit_status(state["globalUnitStatus"])  # 基地/前哨站血量
                self._send_robot_status(state["globalUnitStatus"])
                self._send_power_heat(state["globalUnitStatus"])
                self._send_game_result_events()  # 0x0002
                self._send_warning_events()      # 0x0104
                self._send_event_data_events()   # 0x0101
                # 位置数据由 MQTT（RadarInfoToClient）即时发布，UDP 不再重复发送。
            except Exception as e:
                print(f"UDP Send Error: {e}")

            time.sleep(0.1) # 10Hz

    def _send_packet(self, cmd_id, data):
        """
        构造并发送简化协议帧 (自定义客户端协议)

        帧结构: CmdID(2B) + Length(2B) + Data(NB)
        无 CRC 校验
        """
        data_len = len(data)

        # 1. 构造简化帧头
        header = protocol.pack_simple_header(cmd_id, data_len)

        # 2. 组合帧头 + 数据
        frame = header + bytes(data)

        # 3. 发送
        self.sock.sendto(frame, (self.host, self.port))
        self.seq = (self.seq + 1) % 256

    def _send_game_status(self, status, logistics_status):
        # 0x0001：比赛阶段、时间、比分、局数、启停状态、时间戳和双方经济。

        game_type = 1 # RMUC
        game_progress = status["current_stage"]
        # C++ bitfield 布局: gameType 在低4位, gameProgress 在高4位
        # 所以 type_progress = gameType | (gameProgress << 4)
        type_progress = game_type | (game_progress << 4)

        is_started = 1 if status["current_stage"] > 0 else 0
        is_ended = 0
        is_paused = 1 if status.get("is_paused", False) else 0
        started_ended = (is_started) | (is_ended << 1) | (is_paused << 2)

        data = struct.pack(protocol.GAME_STATUS_FMT,
                           type_progress,
                           status["stage_countdown_sec"],
                           status["red_score"],
                           status["blue_score"],
                           status.get("current_round", 1),
                           started_ended,
                           int(time.time() * 1000),
                           logistics_status.get("red_economy", 550),
                           logistics_status.get("blue_economy", 550))

        self._send_packet(protocol.CmdID.GAME_STATUS, data)

    def _send_global_unit_status(self, global_status):
        """
        发送 0x0003 GAME_ROBOT_HP 协议
        UDP fallback 下按当前客户端视角发送“己方血量快照”。
        """
        robot_hp = global_status["robot_health"]
        is_blue_perspective = self.current_robot_id >= 100
        if is_blue_perspective:
            ally_robot_hp = list(robot_hp[4:8])
            outpost_hp = int(global_status.get("blue_outpost_health", global_status.get("outpost_health", 0)))
            base_hp = int(global_status.get("blue_base_health", global_status.get("base_health", 0)))
        else:
            ally_robot_hp = list(robot_hp[:4])
            outpost_hp = int(global_status.get("red_outpost_health", global_status.get("outpost_health", 0)))
            base_hp = int(global_status.get("red_base_health", global_status.get("base_health", 0)))

        while len(ally_robot_hp) < 4:
            ally_robot_hp.append(0)

        data = struct.pack(protocol.GAME_ROBOT_HP_FMT,
                           ally_robot_hp[0],  # 1号 / 101号
                           ally_robot_hp[1],  # 3号 / 103号
                           ally_robot_hp[2],  # 6号 / 106号
                           0,                 # 保留位
                           0,                 # 保留位
                           ally_robot_hp[3],  # 7号 / 107号
                           outpost_hp,                               # 前哨站
                           base_hp)                                  # 基地

        self._send_packet(protocol.CmdID.GAME_ROBOT_HP, data)

    def _send_robot_status(self, global_status):
        robot_ids = list(getattr(self.state_manager, "robot_ids", [1, 3, 6, 7, 101, 103, 106, 107]))
        robot_health = list(global_status.get("robot_health", []))
        robot_max_hp = list(global_status.get("robot_max_hp", []))
        robot_level = list(global_status.get("robot_level", []))
        robot_power = list(global_status.get("robot_power", []))

        for i, robot_id in enumerate(robot_ids):
            level = int(robot_level[i]) if i < len(robot_level) else 1
            cur_hp = int(robot_health[i]) if i < len(robot_health) else 0
            max_hp = int(robot_max_hp[i]) if i < len(robot_max_hp) else 600
            cooling_value = 80
            heat_limit = 240
            power_limit = max(120, int(robot_power[i]) if i < len(robot_power) else 0)
            outputs = 0x07  # gimbal/chassis/shooter 全开

            data = struct.pack(
                protocol.ROBOT_STATUS_FMT,
                int(robot_id),
                level,
                cur_hp,
                max_hp,
                cooling_value,
                heat_limit,
                power_limit,
                outputs,
            )

            self._send_packet(protocol.CmdID.ROBOT_STATUS, data)

    def _send_power_heat(self, global_status):
        robot_ids = list(getattr(self.state_manager, "robot_ids", [1, 3, 6, 7, 101, 103, 106, 107]))
        try:
            robot_index = robot_ids.index(int(self.current_robot_id))
        except ValueError:
            robot_index = 0

        robot_heat = list(global_status.get("robot_heat", []))
        robot_power = list(global_status.get("robot_power", []))

        current_power = int(robot_power[robot_index]) if robot_index < len(robot_power) else 0
        current_heat = int(robot_heat[robot_index]) if robot_index < len(robot_heat) else 0
        is_hero = (int(robot_ids[robot_index]) % 100) == 1 if robot_ids else True

        data = struct.pack(
            protocol.POWER_HEAT_FMT,
            current_power,
            0,
            0.0,
            current_power,
            0 if is_hero else current_heat,
            current_heat if is_hero else 0,
        )

        self._send_packet(protocol.CmdID.POWER_HEAT, data)

    def _send_event_data_events(self):
        # 0x0101 场地事件
        events = self.state_manager.get_events()
        for evt in events:
            # <IB64s
            desc = str(evt.get("param", "")).encode('utf-8')
            data = struct.pack(protocol.EVENT_DATA_FMT,
                               evt["id"],
                               0, # eventData
                               desc)
            self._send_packet(protocol.CmdID.EVENT_DATA, data)

    def _send_warning_events(self):
        # 0x0104 判罚警告
        warnings = self.state_manager.get_warning_events()
        for evt in warnings:
            self._send_referee_warning(
                evt.get("level", 2),
                evt.get("offending_robot_id", 0),
                evt.get("count", 0)
            )

    def _send_game_result_events(self):
        # 0x0002 比赛结果
        results = self.state_manager.get_game_result_events()
        for evt in results:
            self._send_game_result(evt.get("winner", 0))

    def _send_positions(self):
        # 0x0203 机器人位置：x、y、朝向和机器人编号。
        positions = self.state_manager.get_positions()
        robot_ids = list(getattr(self.state_manager, "robot_ids", [1, 3, 6, 7, 101, 103, 106, 107]))
        for i, pos in enumerate(positions):
            robot_id = robot_ids[i] if i < len(robot_ids) else 1

            data = struct.pack(protocol.ROBOT_POS_FMT,
                               pos["x"],
                               pos["y"],
                               pos["angle"],
                               robot_id) # robot_id
            self._send_packet(protocol.CmdID.ROBOT_POS, data)

    def _compute_winner(self, state):
        gs = state.get("gameStatus", {})
        red = int(gs.get("red_score", 0))
        blue = int(gs.get("blue_score", 0))
        if red > blue:
            return 1
        if blue > red:
            return 2
        return 0

    def _send_game_result(self, winner):
        # 0x0002 比赛结果
        payload = struct.pack(protocol.GAME_RESULT_FMT, int(winner) & 0xFF)
        header = bytearray(protocol.pack_header(len(payload), self.seq, protocol.CmdID.GAME_RESULT))
        header[4] = crc.get_crc8_check_sum(header, 4, 0xFF)
        frame = header + payload
        crc.append_crc16_check_sum(frame)
        self.sock.sendto(frame, (self.host, self.port))
        print(f"[UDPSender] 0x0002 sent winner={int(winner) & 0xFF} seq={self.seq} to {self.host}:{self.port}")
        self.seq = (self.seq + 1) % 256

    def _send_referee_warning(self, level, offending_robot_id, count):
        # 0x0104 判罚警告：等级、违规机器人编号和累计次数均为 uint8。
        try:
            level_u8 = int(level) & 0xFF
        except Exception:
            level_u8 = 2
        try:
            offending_u8 = int(offending_robot_id) & 0xFF
        except Exception:
            offending_u8 = 0
        try:
            count_u8 = int(count) & 0xFF
        except Exception:
            count_u8 = 0

        payload = struct.pack(protocol.REFEREE_WARNING_FMT, level_u8, offending_u8, count_u8)
        header = bytearray(protocol.pack_header(len(payload), self.seq, protocol.CmdID.REFEREE_WARNING))
        header[4] = crc.get_crc8_check_sum(header, 4, 0xFF)
        frame = header + payload
        crc.append_crc16_check_sum(frame)
        self.sock.sendto(frame, (self.host, self.port))
        self.seq = (self.seq + 1) % 256
