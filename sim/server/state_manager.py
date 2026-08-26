#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: MIT
"""
比赛状态管理器

@file state_manager.py
@brief 管理比赛全局状态，包括阶段、比分、血量等
@author Fudan EGA Team
@date 2025-12-07
@copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).

功能说明:
    - 管理比赛阶段（准备/比赛/结算）
    - 管理红蓝方比分和机器人血量
    - 提供状态读写的线程安全访问
"""

import time
import threading
from datetime import datetime

class StateManager:
    BASE_HP_MAX = 5000
    BASE_HP_STEP = 50
    SCORE_MIN = 0
    SCORE_MAX = 999
    ECONOMY_MIN = 0
    ECONOMY_MAX = 100000
    TEAM_NAMES = ("red", "blue")
    FORTRESS_CAPTURE_DURATION_SEC = 20

    def __init__(self):
        self.lock = threading.Lock()
        self.running = False

        # 按协议消息分组维护后端状态；红蓝方数据仍保留在服务端，
        # 发送 MQTT/UDP 时再根据客户端视角转换成己方/敌方字段。
        self.game_status = self._create_game_status_state()
        self.global_logistics_status = self._create_global_logistics_status_state()
        self.global_unit_status = self._create_global_unit_status_state()

        self.referee_info_by_team = {
            "red": self._create_rune_state(),
            "blue": self._create_rune_state(),
        }
        self.active_rune_team = "red"
        self.referee_info = dict(self.referee_info_by_team[self.active_rune_team])

        self.air_support = {
            "red_status": 0,
            "blue_status": 0,
            "red_left_time": 30,
            "blue_left_time": 30,
            "red_default_time": 30,
            "blue_default_time": 30,
            "red_cost_coins": 0,
            "blue_cost_coins": 0,
            "red_mode": "free",
            "blue_mode": "free",
            "red_is_being_targeted": 0,
            "blue_is_being_targeted": 0,
            "red_shooter_status": 1,
            "blue_shooter_status": 1,
            "active_team": "",
            "last_command_team": "",
        }

        self.tech_core_motion_state = {
            "maximum_difficulty_level": 1,
            "basic_state": 1,
            "status": 1,  # 兼容旧版模拟器界面和状态结构
            "putin_state": 0,
            "move_state": 0,
            "rotate_state": 0,
            "enemy_core_status": 0,
            "remain_time_all": 0,
            "remain_time_step": 0,
        }

        self.deploy_mode_status_by_team = self._create_deploy_mode_status_by_team()
        self.deploy_mode_status = 0
        self.current_robot_id = 1
        self.global_special_mechanism = {
            "mechanism_id": [1, 2],
            "mechanism_time_sec": [0, 0],
        }
        self.robot_performance_selection_sync = {
            "shooter": 1,
            "chassis": 1,
            "sentry_control": 0,
        }
        self.robot_static_status = self._create_robot_static_status_state()
        self.robot_dynamic_status = self._create_robot_dynamic_status_state()
        self.robot_module_status = self._create_robot_module_status_state()
        self.sentry_status_sync = {
            "posture_id": 0,
            "is_weakened": False,
            "is_powered": False,
        }
        self.robot_path_plan_info = {
            "intention": 1,
            "start_pos_x": 0,
            "start_pos_y": 0,
            "offset_x": [],
            "offset_y": [],
            "sender_id": 7,
        }
        self.last_sentry_ctrl_result = {
            "command_id": 0,
            "result_code": 0,
        }
        self.sentry_ctrl_result_queue = []
        self.last_game_result = {
            "winner": 0,
            "winner_label": "未结算",
            "reason": "",
            "end_reason": 255,
            "red_score": 0,
            "blue_score": 0,
        }
        self.pending_game_result_reason = ""
        self.pending_game_result_end_reason = 255

        self.event_queue = []        # 0x0101 EVENT_DATA 专用队列
        self.warning_queue = []      # 0x0104 REFEREE_WARNING 专用队列
        self.game_result_queue = []  # 0x0002 GAME_RESULT 专用队列
        self._game_result_enqueued = False
        self.kicked_all = False # 踢出全员标记（用于裁判系统“踢出所有”）
        self.penalty_counts = {} # 判罚计数字典: key=(level, offending_robot_id), value=次数

        # 机器人位置 (x, y, 角度) - 归一化 0.0 到 1.0 还是米?
        # 协议使用米。地图通常为 28m x 15m。
        self.positions = [
            {"x": 2.0, "y": 2.0, "angle": 0.0},   # 红方 1
            {"x": 5.0, "y": 2.0, "angle": 0.0},   # 红方 3
            {"x": 8.0, "y": 2.0, "angle": 0.0},   # 红方 6
            {"x": 11.0, "y": 2.0, "angle": 0.0},  # 红方 7
            {"x": 17.0, "y": 10.0, "angle": 180.0}, # 蓝方 101
            {"x": 20.0, "y": 10.0, "angle": 180.0}, # 蓝方 103
            {"x": 23.0, "y": 10.0, "angle": 180.0}, # 蓝方 106
            {"x": 26.0, "y": 10.0, "angle": 180.0}, # 蓝方 107
        ]
        self._initial_positions = [dict(position) for position in self.positions]
        self._match_simulation_clock_active = False
        self.match_simulation_status = {
            "state": "idle",
            "status": "idle",
            "mode": "quick",
            "speed": 1.0,
            "features": {"positions": True, "hp": True, "events": True},
            "elapsed": 0.0,
            "elapsed_sec": 0.0,
            "remaining": 90.0,
            "remaining_sec": 90.0,
            "duration": 90.0,
            "duration_sec": 90.0,
            "current_stage": 0,
            "phase": "idle",
            "pause_reason": "",
            "recent_events": [],
        }

        self.logs = []

        # --- 复活相关 ---
        # 机器人 ID 顺序与 robot_health 列表索引保持一致
        default_robot_count = len(self.global_unit_status.get("robot_health", []))
        # 机器人 ID 列表：默认使用当前模拟器重点联调的 8 台机器人
        # 红方: 1/3/6/7，蓝方: 101/103/106/107
        if default_robot_count == 8:
            self.robot_ids = [1, 3, 6, 7, 101, 103, 106, 107]
        else:
            # 兜底使用从 1 开始的连续 ID
            self.robot_ids = list(range(1, default_robot_count + 1))

        self.robot_injury_stats = {
            rid: self._empty_robot_injury_stat() for rid in self.robot_ids
        }

        # 黄牌统计状态
        self.last_yellow_ts = {rid: 0.0 for rid in self.robot_ids}
        self.yellow_streak = {rid: 0 for rid in self.robot_ids}
        self.yellow_count = {rid: 0 for rid in self.robot_ids}
        self.double_yellow_count = 0

        # 复活信息以 robot_id 为键保存
        self.respawn_info = {}
        for rid in self.robot_ids:
            self.respawn_info[rid] = {
                "is_pending": False,              # 是否在复活队列中
                "start_ts": 0.0,                  # 触发时间戳
                "total_progress": 0,              # 需要的总进度
                "current_progress": 0,            # 当前进度
                "can_free": False,                # 是否允许免费复活
                "can_pay": False,                # 是否允许金币买活
                "gold_cost": 100,                 # 默认金币消耗
                "immediate_exchange_count": 0,    # 累计立即复活次数
                "is_dead": False,                 # 是否处于死亡状态
                "last_immediate_ts": 0.0,         # 最近一次立即复活时间戳
                "notified_complete": False        # 读条完成是否已通知（防止重复入队）
            }

        # 进度推进时间基准
        self._last_respawn_tick = time.time()
        # 默认最大 HP（与初始化 robot_health 保持一致）
        self._default_robot_max_hp = 600
        self._default_respawn_gold_cost = 100

        # --- 飞镖相关 ---
        self.dart_status_by_team = {
            "red": self._create_dart_status(),
            "blue": self._create_dart_status(),
        }
        self.active_dart_team = "red"
        self.dart_status = dict(self.dart_status_by_team[self.active_dart_team])
        self.last_buff_status = self._create_buff_status_state()
        self.last_event_command = self._create_event_command_state()
        self.custom_byte_block_form = self._create_custom_byte_block_form_state()
        self.custom_byte_block_hex = ""

        self.log("System", "Server Initialized")

    @staticmethod
    def _create_game_status_state():
        return {
            "current_round": 1,
            "total_rounds": 3,
            "red_score": 0,
            "blue_score": 0,
            "current_stage": 0, # 0: 未开始
            "stage_countdown_sec": 300,
            "stage_elapsed_sec": 0,
            "is_paused": False,
        }

    @staticmethod
    def _create_rune_state():
        return {
            "rune_status": 1,
            "activated_arms": 0,
            "average_rings": 0,
            "last_rune_activation": "",
        }

    @staticmethod
    def _create_deploy_mode_status_by_team():
        return {
            "red": 0,
            "blue": 0,
        }

    @staticmethod
    def _normalize_rune_type(rune_type):
        try:
            value = int(rune_type)
        except Exception:
            text = str(rune_type or "").strip().lower()
            return 2 if text in {"large", "big", "2"} else 1
        return 2 if value == 2 else 1

    @classmethod
    def _rune_type_label(cls, rune_type):
        return "大能量机关" if cls._normalize_rune_type(rune_type) == 2 else "小能量机关"

    @staticmethod
    def _create_dart_status():
        return {
            "target_id": 2,
            "open": 0,
            "opening_started_ts": 0.0,
            "opening_duration_sec": 2.0,
        }

    @staticmethod
    def _create_event_command_state():
        return {
            "event_id": 14,
            "param": "",
        }

    @staticmethod
    def _create_custom_byte_block_form_state():
        return {
            "encoding": "hex",
            "hex_data": "01 02 03 04",
            "text_data": "SIM",
        }

    @staticmethod
    def _create_buff_status_state():
        return {
            "robot_id": 0,
            "buff_type": 1,
            "buff_level": 1,
            "buff_max_time": 30,
            "buff_left_time": 30,
        }

    def _create_robot_static_status_state(self):
        return {
            "connection_state": 1,
            "field_state": 0,
            "alive_state": 1,
            "robot_id": int(self.current_robot_id or 1),
            "robot_type": int(self.current_robot_id or 1) % 100,
            "performance_system_shooter": 1,
            "performance_system_chassis": 1,
            "level": 1,
            "max_health": 600,
            "max_heat": 240,
            "heat_cooldown_rate": 20.0,
            "max_power": 120,
            "max_buffer_energy": 60,
            "max_chassis_energy": 120,
        }

    @staticmethod
    def _create_robot_dynamic_status_state():
        return {
            "current_health": 600,
            "current_heat": 0.0,
            "last_projectile_fire_rate": 0.0,
            "current_chassis_energy": 0,
            "current_buffer_energy": 0,
            "current_experience": 100,
            "experience_for_upgrade": 1000,
            "total_projectiles_fired": 0,
            "remaining_ammo": 0,
            "is_out_of_combat": False,
            "out_of_combat_countdown": 0,
            "can_remote_heal": True,
            "can_remote_ammo": True,
        }

    @staticmethod
    def _create_robot_module_status_state():
        return {
            "power_manager": 1,
            "rfid": 1,
            "light_strip": 1,
            "small_shooter": 1,
            "big_shooter": 1,
            "uwb": 1,
            "armor": 1,
            "video_transmission": 1,
            "capacitor": 1,
            "main_controller": 1,
            "laser_detection_module": 1,
        }

    @staticmethod
    def _normalize_sentry_posture_id(posture_id):
        try:
            value = int(posture_id)
        except Exception:
            value = 1
        return value if value in (1, 2, 3) else 1

    @classmethod
    def _resolve_sentry_command_effect(cls, command_id):
        try:
            value = int(command_id or 0)
        except Exception:
            value = 0

        command_name_map = {
            1: "supply_hp_ammo",
            3: "remote_ammo",
            4: "remote_heal",
            5: "confirm_respawn",
            6: "pay_respawn",
            7: "attack_posture",
            8: "defense_posture",
            9: "move_posture",
            10: "enhanced_attack_posture",
            11: "enhanced_defense_posture",
            12: "enhanced_move_posture",
        }
        posture_map = {
            7: 1,
            8: 2,
            9: 3,
            10: 1,
            11: 2,
            12: 3,
        }
        return {
            "command_id": value,
            "name": command_name_map.get(value, "unknown"),
            "is_valid": value in command_name_map,
            "posture_id": posture_map.get(value),
        }

    @staticmethod
    def _create_global_logistics_status_state():
        return {
            "red_economy": 550,
            "blue_economy": 550,
            "red_total_economy_obtained": 550,
            "blue_total_economy_obtained": 550,
            "red_total_damage": 0,
            "blue_total_damage": 0,
            "red_tech_level": 1,
            "blue_tech_level": 1,
            "red_encryption_level": 1,
            "blue_encryption_level": 1,
        }

    @staticmethod
    def _create_global_unit_status_state():
        return {
            "base_health": 5000,
            "base_status": 0,
            "base_shield": 1000,
            "outpost_health": 1500,
            "outpost_status": 1,
            # --- 分队伍数据 ---
            "red_base_health": 5000,
            "blue_base_health": 5000,
            "red_base_status": 0,
            "blue_base_status": 0,
            "red_base_shield": 1000,
            "blue_base_shield": 1000,
            "red_outpost_health": 1500,
            "blue_outpost_health": 1500,
            "red_outpost_status": 1,
            "blue_outpost_status": 1,
            "red_outpost_destroyed": False,
            "blue_outpost_destroyed": False,
            "robot_health": [600] * 8, # 8 个机器人
            "robot_max_hp": [600] * 8, # 8 个机器人最大血量
            "robot_bullets": [0] * 8,
            "robot_heat": [0] * 8,
            "robot_heat_manual_hold": [False] * 8,
            "robot_fire_rate": [0] * 8,
            "robot_speed_over_limit": [False] * 8,
            "robot_power_over_limit": [False] * 8,
            "robot_level": [1] * 8,
            "robot_power": [0] * 8,
            "total_damage_red": 0,
            "total_damage_blue": 0
        }

    def _get_robot_fire_rate_limit(self, robot_id):
        """根据机器人类型返回射速上限（m/s）。"""
        try:
            normalized_id = int(robot_id) % 100
        except Exception:
            normalized_id = 0

        if normalized_id == 1:
            return 12.0
        if normalized_id == 2:
            return 30.0
        return 25.0

    def _get_robot_power_limit(self, robot_id):
        """返回机器人功率超限阈值（W）。"""
        return 120.0

    @staticmethod
    def _stage_default_countdown(stage):
        defaults = {
            0: 300,
            1: 180,
            2: 15,
            3: 5,
            4: 420,
            5: 0,
        }
        try:
            return int(defaults.get(int(stage), 0))
        except Exception:
            return 0

    @staticmethod
    def _winner_label(winner):
        labels = {
            0: "平局",
            1: "红方",
            2: "蓝方",
        }
        try:
            return labels.get(int(winner), "未知")
        except Exception:
            return "未知"

    @staticmethod
    def _empty_robot_injury_stat():
        return {
            "total_damage": 0,
            "collision_damage": 0,
            "small_projectile_damage": 0,
            "large_projectile_damage": 0,
            "dart_splash_damage": 0,
            "module_offline_damage": 0,
            "offline_damage": 0,
            "penalty_damage": 0,
            "server_kill_damage": 0,
            "killer_id": 0,
        }

    @staticmethod
    def _normalize_robot_performance_value(value, maximum, default=0):
        try:
            normalized = int(value)
        except Exception:
            normalized = int(default)
        return max(0, min(int(maximum), normalized))

    @staticmethod
    def _normalize_robot_status_uint(value, default=0, minimum=0, maximum=4294967295):
        try:
            normalized = int(value)
        except Exception:
            normalized = int(default)
        return max(int(minimum), min(int(maximum), normalized))

    @staticmethod
    def _normalize_robot_status_float(value, default=0.0, minimum=None, maximum=None):
        try:
            normalized = float(value)
        except Exception:
            normalized = float(default)
        if minimum is not None:
            normalized = max(float(minimum), normalized)
        if maximum is not None:
            normalized = min(float(maximum), normalized)
        return normalized

    @staticmethod
    def _normalize_robot_status_bool(value, default=False):
        if isinstance(value, str):
            normalized = value.strip().lower()
            if normalized in {"1", "true", "yes", "on"}:
                return True
            if normalized in {"0", "false", "no", "off"}:
                return False
        if value is None:
            return bool(default)
        return bool(value)

    @classmethod
    def _normalize_robot_injury_stat_payload(cls, payload):
        normalized = cls._empty_robot_injury_stat()
        for key in normalized.keys():
            try:
                normalized[key] = max(0, int((payload or {}).get(key, 0) or 0))
            except Exception:
                normalized[key] = 0

        non_small_keys = (
            "collision_damage",
            "large_projectile_damage",
            "dart_splash_damage",
            "module_offline_damage",
            "offline_damage",
            "penalty_damage",
            "server_kill_damage",
        )
        non_small_sum = sum(int(normalized.get(key, 0)) for key in non_small_keys)
        total_damage = int(normalized.get("total_damage", 0))
        small_projectile_damage = total_damage - non_small_sum
        if small_projectile_damage < 0:
            total_damage = non_small_sum
            small_projectile_damage = 0

        normalized["total_damage"] = total_damage
        normalized["small_projectile_damage"] = small_projectile_damage
        return normalized

    def _resolve_robot_id(self, robot_ref):
        try:
            rid = int(robot_ref)
        except Exception:
            return None

        if rid in self.robot_ids:
            return rid

        if 0 <= rid < len(self.robot_ids):
            return self.robot_ids[rid]

        return None

    def _record_robot_damage_locked(
        self,
        robot_index,
        damage,
        *,
        damage_key="penalty_damage",
        killer_id=0,
    ):
        try:
            damage_value = max(0, int(damage))
        except Exception:
            return

        if damage_value <= 0 or not (0 <= robot_index < len(self.robot_ids)):
            return

        rid = self.robot_ids[robot_index]
        stats = self.robot_injury_stats.setdefault(
            rid, self._empty_robot_injury_stat()
        )
        stats["total_damage"] = int(stats.get("total_damage", 0)) + damage_value
        if damage_key in stats:
            stats[damage_key] = int(stats.get(damage_key, 0)) + damage_value
        stats["killer_id"] = max(0, int(killer_id or 0))

    def can_pay_for_respawn(self, robot_id, cost):
        """
        检查指定机器人是否有足够的金币支付复活费用。
        参数 robot_id: 机器人 id（与 self.robot_ids 中的 id 对应）
        参数 cost: 需要支付的金币数（int）
        返回: bool - 是否可以支付
        """
        try:
            if cost is None:
                return False
            cost = int(cost)
        except Exception:
            return False

        # 根据机器人 id 判断队伍（小于 100 视为红，>=100 视为蓝）
        try:
            team = "blue" if int(robot_id) >= 100 else "red"
        except Exception:
            team = "red"

        if team == "red":
            econ = self.global_logistics_status.get("red_economy", 0)
        else:
            econ = self.global_logistics_status.get("blue_economy", 0)

        try:
            econ = int(econ)
        except Exception:
            econ = 0

        return econ >= cost

    @staticmethod
    def _clamp(value, minimum, maximum):
        return max(minimum, min(maximum, int(value)))

    @classmethod
    def _clamp_base_hp(cls, hp):
        hp = cls._clamp(hp, 0, cls.BASE_HP_MAX)
        return int(round(hp / cls.BASE_HP_STEP) * cls.BASE_HP_STEP)

    @staticmethod
    def _normalize_team(team):
        if isinstance(team, str):
            value = team.strip().lower()
            if value in ("blue", "b", "2", "enemy"):
                return "blue"
        if team == 2:
            return "blue"
        return "red"

    def _current_team_name(self):
        try:
            return "blue" if int(self.current_robot_id or 1) >= 100 else "red"
        except Exception:
            return "red"

    def start(self):
        self.running = True
        self.log("System", "Simulation Started")
        threading.Thread(target=self._update_loop, daemon=True).start()

    def stop(self):
        self.running = False
        self.log("System", "Simulation Stopped")

    def _update_loop(self):
        while self.running:
            time.sleep(1)
            with self.lock:
                now = time.time()
                if self.game_status["is_paused"]:
                    self._last_respawn_tick = now  # 暂停时不推进复活进度，重置基准时间
                    continue

                stage = self.game_status["current_stage"]

                # 一键赛事演示由单调时钟以 10 Hz 原子写入阶段，避免与这里的 1 Hz
                # 传统倒计时重复推进；其余热量、复活等后台状态仍沿用本循环。
                if (
                    not self._match_simulation_clock_active
                    and stage in [1, 2, 3, 4]
                ):  # 有倒计时的阶段
                    self.game_status["stage_countdown_sec"] -= 1
                    self.game_status["stage_elapsed_sec"] += 1

                    # 阶段自动转换
                    if self.game_status["stage_countdown_sec"] <= 0:
                        if stage == 1:  # 准备阶段结束 -> 自检阶段
                            self.game_status["current_stage"] = 2
                            self.game_status["stage_countdown_sec"] = 15
                            self.game_status["stage_elapsed_sec"] = 0
                            self.log("Referee", "自动进入自检阶段 (Stage 2)")
                        elif stage == 2:  # 自检阶段结束 -> 五秒倒计时
                            self.game_status["current_stage"] = 3
                            self.game_status["stage_countdown_sec"] = 5
                            self.game_status["stage_elapsed_sec"] = 0
                            self.log("Referee", "自动进入五秒倒计时 (Stage 3)")
                        elif stage == 3:  # 五秒倒计时结束 -> 比赛开始
                            self.game_status["current_stage"] = 4
                            self.game_status["stage_countdown_sec"] = 420
                            self.game_status["stage_elapsed_sec"] = 0
                            self.log("Referee", "比赛开始! (Stage 4)")
                        elif stage == 4:  # 比赛时间结束 -> 结算
                            self.game_status["current_stage"] = 5
                            self.game_status["stage_countdown_sec"] = 0
                            self.game_status["stage_elapsed_sec"] = 0
                            self.game_status["is_paused"] = False
                            self.log("Referee", "比赛时间结束，进入结算阶段 (Stage 5)")
                            self.pending_game_result_reason = "比赛时间结束"
                            self.pending_game_result_end_reason = 1
                            self._enqueue_game_result()

                # 热量自动冷却：统一 30/s
                try:
                    robot_heat = self.global_unit_status.get("robot_heat", [])
                    robot_fire_rate = self.global_unit_status.get("robot_fire_rate", [])
                    robot_speed_over_limit = self.global_unit_status.get("robot_speed_over_limit", [])
                    robot_power = self.global_unit_status.get("robot_power", [])
                    robot_power_over_limit = self.global_unit_status.get("robot_power_over_limit", [])
                    robot_heat_manual_hold = self.global_unit_status.get("robot_heat_manual_hold", [])
                    for i in range(len(robot_heat)):
                        h = int(robot_heat[i])
                        is_manual_hold = bool(robot_heat_manual_hold[i]) if i < len(robot_heat_manual_hold) else False
                        if h > 0 and not is_manual_hold:
                            robot_heat[i] = max(0, h - 30)

                        # 检查热量上限并发送判罚 (超限)
                        if robot_heat[i] > 240:
                            try:
                                rid = self.robot_ids[i]
                                self.warning_queue.append({
                                    "type": "referee_warning",
                                    "level": 5, # 5 对应 PenaltyInfo penalty_type=5
                                    "offending_robot_id": rid,
                                    "count": 1
                                })
                                self.log("Referee", f"Robot {rid} overheated! Heat: {robot_heat[i]} > 240. Issuing level 5 penalty.")
                            except Exception:
                                pass

                        # 检查射速上限并发送判罚（超限 -> penalty_type=6）
                        try:
                            current_fire_rate = float(robot_fire_rate[i]) if i < len(robot_fire_rate) else 0.0
                            rid = self.robot_ids[i]
                            fire_rate_limit = self._get_robot_fire_rate_limit(rid)
                            was_over_limit = bool(robot_speed_over_limit[i]) if i < len(robot_speed_over_limit) else False
                            is_over_limit = current_fire_rate > fire_rate_limit
                            if i < len(robot_speed_over_limit):
                                robot_speed_over_limit[i] = is_over_limit
                            if is_over_limit and not was_over_limit:
                                self.warning_queue.append({
                                    "type": "referee_warning",
                                    "level": 6,
                                    "offending_robot_id": rid,
                                    "count": 1
                                })
                                self.log(
                                    "Referee",
                                    f"Robot {rid} fire rate over limit! FireRate: {current_fire_rate} > {fire_rate_limit}. Issuing level 6 penalty."
                                )
                        except Exception:
                            pass

                        # 检查功率上限并发送判罚（超限 -> penalty_type=4）
                        try:
                            current_power = float(robot_power[i]) if i < len(robot_power) else 0.0
                            rid = self.robot_ids[i]
                            power_limit = self._get_robot_power_limit(rid)
                            was_power_over_limit = bool(robot_power_over_limit[i]) if i < len(robot_power_over_limit) else False
                            is_power_over_limit = current_power > power_limit
                            if i < len(robot_power_over_limit):
                                robot_power_over_limit[i] = is_power_over_limit
                            if is_power_over_limit and not was_power_over_limit:
                                self.warning_queue.append({
                                    "type": "referee_warning",
                                    "level": 4,
                                    "offending_robot_id": rid,
                                    "count": 1
                                })
                                self.log(
                                    "Referee",
                                    f"Robot {rid} power over limit! Power: {current_power} > {power_limit}. Issuing level 4 penalty."
                                )
                        except Exception:
                            pass
                except Exception as e:
                    self.log("Referee", f"Heat cooldown error: {e}")

                # --- 复活进度推进（按时间戳差值计算，每秒推进 1）---
                delta_sec = int(now - self._last_respawn_tick)
                if delta_sec > 0:
                    for rid, info in list(self.respawn_info.items()):
                        if info.get("is_pending"):
                            info["current_progress"] += delta_sec * 1
                            # 检查是否完成：不再自动复活，等待手动确认
                            if info["current_progress"] >= info.get("total_progress", 0):
                                info["current_progress"] = info.get("total_progress", info["current_progress"])
                                info["can_free"] = True  # 读条完成后允许免费复活
                                # 仅首次读条完成时入队通知，保持 is_pending=True，is_dead=True 由确认指令处理
                                if not info.get("notified_complete", False):
                                    robot_index = None
                                    try:
                                        robot_index = self.robot_ids.index(rid)
                                    except ValueError:

                                        robot_index = None
                                    self.event_queue.append({
                                        "id": 201,
                                        "robot_id": rid,
                                        "robot_index": robot_index,
                                        "type": "robot_respawn_status",
                                        "is_pending": True
                                    })
                                    info["notified_complete"] = True

                    # 更新基准时间，避免浮点累积
                    self._last_respawn_tick += delta_sec

                # --- Dart 开闸推进 ---
                for team_name, dart_status in self.dart_status_by_team.items():
                    if int(dart_status.get("open", 0)) != 1:
                        continue
                    started_ts = float(dart_status.get("opening_started_ts", 0.0))
                    opening_duration = float(dart_status.get("opening_duration_sec", 2.0))
                    if started_ts > 0.0 and (now - started_ts) >= opening_duration:
                        dart_status["open"] = 2
                        self.log("Referee", f"{team_name.upper()} dart gate state -> opened (2)")
                self.dart_status = dict(
                    self.dart_status_by_team.get(
                        self.active_dart_team, self._create_dart_status()
                    )
                )

                active_team = str(self.air_support.get("active_team", "")).strip().lower()
                if active_team in ("red", "blue"):
                    left_key = f"{active_team}_left_time"
                    status_key = f"{active_team}_status"
                    cost_key = f"{active_team}_cost_coins"
                    mode_key = f"{active_team}_mode"
                    remaining = max(0, int(self.air_support.get(left_key, 0)))
                    mode_name = str(self.air_support.get(mode_key, "free")).strip().lower()
                    if remaining > 0:
                        self.air_support[left_key] = remaining - 1
                    elif mode_name == "paid":
                        self.air_support[left_key] = 0
                        self.air_support[cost_key] = max(
                            0, int(self.air_support.get(cost_key, 0))
                        ) + 1
                    else:
                        self.air_support[left_key] = 0
                        self.air_support[status_key] = 0
                        self.air_support["active_team"] = ""
                        self.log("Referee", f"Air support finished automatically for {active_team}")

                # Buff 剩余时间倒计时
                buff_status = self.last_buff_status
                if int(buff_status.get("buff_left_time", 0)) > 0:
                    buff_status["buff_left_time"] = max(
                        0, int(buff_status.get("buff_left_time", 0)) - 1
                    )
                    if buff_status["buff_left_time"] == 0:
                        self.log(
                            "Referee",
                            f"Buff expired: robot_id={buff_status.get('robot_id', 0)} type={buff_status.get('buff_type', 0)}",
                        )

                mechanism_time_sec = list(
                    self.global_special_mechanism.get("mechanism_time_sec", [0, 0])
                )
                mechanism_finished = []
                for index, remaining in enumerate(mechanism_time_sec[:2]):
                    remaining_value = max(0, int(remaining or 0))
                    if remaining_value <= 0:
                        mechanism_time_sec[index] = 0
                        continue

                    next_value = remaining_value - 1
                    mechanism_time_sec[index] = next_value
                    if next_value == 0:
                        mechanism_finished.append("ally" if index == 0 else "enemy")

                self.global_special_mechanism["mechanism_time_sec"] = mechanism_time_sec
                for team_name in mechanism_finished:
                    self.log(
                        "Referee",
                        f"{team_name} fortress capture countdown finished",
                    )

    # --- 裁判动作 ---
    def start_match(self):
        """
        开始比赛流程
        按照官方协议阶段流转: 0(未开始) -> 1(准备阶段) -> 2(自检阶段) -> 3(五秒倒计时) -> 4(比赛中) -> 5(结算)
        """
        with self.lock:
            if self.game_status["current_stage"] == 0:
                # 从未开始 -> 准备阶段
                self.game_status["current_stage"] = 1  # 准备阶段
                self.game_status["stage_countdown_sec"] = 180  # 准备阶段 3 分钟
                self.game_status["stage_elapsed_sec"] = 0
                self.game_status["is_paused"] = False
                self.log("Referee", "进入准备阶段 (Stage 1)")
            elif self.game_status["current_stage"] == 1:
                # 准备阶段 -> 自检阶段
                self.game_status["current_stage"] = 2  # 自检阶段
                self.game_status["stage_countdown_sec"] = 15  # 15秒自检
                self.game_status["stage_elapsed_sec"] = 0
                self.log("Referee", "进入自检阶段 (Stage 2)")
            elif self.game_status["current_stage"] == 2:
                # 自检阶段 -> 五秒倒计时
                self.game_status["current_stage"] = 3  # 五秒倒计时
                self.game_status["stage_countdown_sec"] = 5
                self.game_status["stage_elapsed_sec"] = 0
                self.log("Referee", "进入五秒倒计时 (Stage 3)")
            elif self.game_status["current_stage"] == 3:
                # 五秒倒计时 -> 比赛中
                self.game_status["current_stage"] = 4  # 比赛中
                self.game_status["stage_countdown_sec"] = 420  # 7分钟比赛
                self.game_status["stage_elapsed_sec"] = 0
                self.log("Referee", "比赛开始! (Stage 4)")
            elif self.game_status["current_stage"] == 5:
                # 结算阶段：再次点击“开始比赛”进入下一轮（仅当未到最后一轮）
                current_round = int(self.game_status.get("current_round", 1))
                total_rounds = max(1, int(self.game_status.get("total_rounds", 1)))

                if current_round < total_rounds:
                    self.game_status["current_round"] = current_round + 1
                    # 进入下一轮时保留当前大比分（red_score/blue_score）
                    self._reset_game_state(reset_series_score=False)
                    self.game_status["current_stage"] = 1  # 准备阶段
                    self.game_status["stage_countdown_sec"] = 180  # 准备阶段 3 分钟
                    self.log("Referee", f"进入第 {self.game_status['current_round']} 轮准备阶段 (Stage 1)")
                else:
                    # 最后一轮结算完成后点击“开始比赛”回到未开始状态
                    self._reset_game_state()
                    self.game_status["current_stage"] = 0  # 回到未开始状态
                    self.game_status["stage_countdown_sec"] = self._stage_default_countdown(0)
                    self.game_status["stage_elapsed_sec"] = 0
                    self.game_status["is_paused"] = False
                    self.log("Referee", "已是最后一轮结算，点击开始比赛回到未开始状态")
    '''
            elif self.game_status["current_stage"] in [4, 5]:
                # 如果在比赛中或结算阶段，重置为未开始状态，然后进入准备阶段
                self._reset_game_state()
                self.game_status["current_stage"] = 1  # 准备阶段
                self.game_status["stage_countdown_sec"] = 180  # 准备阶段 3 分钟
                self.log("Referee", "重置比赛，进入准备阶段 (Stage 1)")
    '''

    def skip_to_battle(self):
        with self.lock:
            previous_stage = int(self.game_status.get("current_stage", 0))
            self.game_status["current_stage"] = 4
            self.game_status["stage_countdown_sec"] = 420
            self.game_status["stage_elapsed_sec"] = 0
            self.game_status["is_paused"] = False
            self.log("Referee", f"Skip to battle stage: {previous_stage} -> 4")

    def _reset_game_state(self, reset_series_score=True):
        """重置比赛状态到初始值"""
        if reset_series_score:
            self.game_status["red_score"] = 0
            self.game_status["blue_score"] = 0
        self.global_logistics_status = self._create_global_logistics_status_state()
        self.game_status["stage_countdown_sec"] = self._stage_default_countdown(
            self.game_status.get("current_stage", 0)
        )
        self.game_status["stage_elapsed_sec"] = 0
        self.game_status["is_paused"] = False
        self._game_result_enqueued = False
        self.penalty_counts = {}
        self.event_queue.clear()
        self.warning_queue.clear()
        self.game_result_queue.clear()
        self.last_yellow_ts = {rid: 0.0 for rid in self.robot_ids}
        self.yellow_streak = {rid: 0 for rid in self.robot_ids}
        self.yellow_count = {rid: 0 for rid in self.robot_ids}
        self.double_yellow_count = 0
        # 重置血量
        self.global_unit_status["robot_max_hp"] = [self._default_robot_max_hp] * len(self.global_unit_status["robot_health"])
        for i in range(len(self.global_unit_status["robot_health"])):
            self.global_unit_status["robot_health"][i] = int(self.global_unit_status["robot_max_hp"][i])
            self.global_unit_status["robot_bullets"][i] = 0
            self.global_unit_status["robot_heat"][i] = 0
            self.global_unit_status["robot_heat_manual_hold"][i] = False
            self.global_unit_status["robot_fire_rate"][i] = 0
            self.global_unit_status["robot_speed_over_limit"][i] = False
            self.global_unit_status["robot_power_over_limit"][i] = False
            self.global_unit_status["robot_level"][i] = 1
            self.global_unit_status["robot_power"][i] = 0
        self.global_unit_status["base_health"] = 5000
        self.global_unit_status["base_status"] = 0
        self.global_unit_status["base_shield"] = 1000
        self.global_unit_status["outpost_health"] = 1500
        self.global_unit_status["outpost_status"] = 1
        self.global_unit_status["red_base_health"] = 5000
        self.global_unit_status["blue_base_health"] = 5000
        self.global_unit_status["red_base_status"] = 0
        self.global_unit_status["blue_base_status"] = 0
        self.global_unit_status["red_base_shield"] = 1000
        self.global_unit_status["blue_base_shield"] = 1000
        self.global_unit_status["red_outpost_health"] = 1500
        self.global_unit_status["blue_outpost_health"] = 1500
        self.global_unit_status["red_outpost_status"] = 1
        self.global_unit_status["blue_outpost_status"] = 1
        self.global_unit_status["red_outpost_destroyed"] = False
        self.global_unit_status["blue_outpost_destroyed"] = False
        self.global_unit_status["total_damage_red"] = 0
        self.global_unit_status["total_damage_blue"] = 0
        self.referee_info_by_team = {
            team_name: self._create_rune_state() for team_name in self.TEAM_NAMES
        }
        self.active_rune_team = "red"
        self.referee_info = dict(self.referee_info_by_team[self.active_rune_team])
        self.air_support["red_status"] = 0
        self.air_support["blue_status"] = 0
        self.air_support["red_left_time"] = 30
        self.air_support["blue_left_time"] = 30
        self.air_support["red_default_time"] = 30
        self.air_support["blue_default_time"] = 30
        self.air_support["red_cost_coins"] = 0
        self.air_support["blue_cost_coins"] = 0
        self.air_support["red_mode"] = "free"
        self.air_support["blue_mode"] = "free"
        self.air_support["red_is_being_targeted"] = 0
        self.air_support["blue_is_being_targeted"] = 0
        self.air_support["red_shooter_status"] = 1
        self.air_support["blue_shooter_status"] = 1
        self.air_support["active_team"] = ""
        self.air_support["last_command_team"] = ""
        self.deploy_mode_status_by_team = self._create_deploy_mode_status_by_team()
        self.deploy_mode_status = 0
        self.global_special_mechanism["mechanism_id"] = [1, 2]
        self.global_special_mechanism["mechanism_time_sec"] = [0, 0]
        self.robot_performance_selection_sync["shooter"] = 1
        self.robot_performance_selection_sync["chassis"] = 1
        self.robot_performance_selection_sync["sentry_control"] = 0
        self.sentry_status_sync["posture_id"] = 0
        self.sentry_status_sync["is_weakened"] = False
        self.robot_path_plan_info["intention"] = 1
        self.robot_path_plan_info["start_pos_x"] = 0
        self.robot_path_plan_info["start_pos_y"] = 0
        self.robot_path_plan_info["offset_x"] = []
        self.robot_path_plan_info["offset_y"] = []
        self.robot_path_plan_info["sender_id"] = 7
        self.last_sentry_ctrl_result["command_id"] = 0
        self.last_sentry_ctrl_result["result_code"] = 0
        self.sentry_ctrl_result_queue.clear()
        self.robot_injury_stats = {
            rid: self._empty_robot_injury_stat() for rid in self.robot_ids
        }
        self.dart_status_by_team = {
            team_name: self._create_dart_status() for team_name in self.TEAM_NAMES
        }
        self.active_dart_team = "red"
        self.dart_status = dict(self.dart_status_by_team[self.active_dart_team])
        self.last_buff_status = self._create_buff_status_state()
        self.last_event_command = self._create_event_command_state()
        self.custom_byte_block_form = self._create_custom_byte_block_form_state()
        self.custom_byte_block_hex = ""
        self.last_game_result = {
            "winner": 0,
            "winner_label": "未结算",
            "reason": "",
            "end_reason": 255,
            "red_score": int(self.game_status.get("red_score", 0)),
            "blue_score": int(self.game_status.get("blue_score", 0)),
        }
        self.pending_game_result_reason = ""
        self.pending_game_result_end_reason = 255
        # 重置 respawn 状态
        for rid, info in self.respawn_info.items():
            info.update({
                "is_pending": False,
                "start_ts": 0.0,
                "total_progress": 0,
                "current_progress": 0,
                "can_free": False,
                "can_pay": False,
                "gold_cost": self._default_respawn_gold_cost,
                "immediate_exchange_count": 0,
                "is_dead": False,
                "last_immediate_ts": 0.0,
                "notified_complete": False
            })
        self.log("Referee", "比赛状态已重置")

    def kick_all(self):
        """踢出所有客户端：标记 kicked_all 并推送事件"""
        with self.lock:
            self.kicked_all = True
            self.log("Referee", "踢出所有客户端")
            self.event_queue.append({"id": 3001, "type": "kick_all"})

    def reset_all(self):
        """全局重置：阶段回到未开始，重置所有状态并清除踢出标记"""
        with self.lock:
            self._reset_game_state()
            self.game_status["current_stage"] = 0
            self.game_status["stage_countdown_sec"] = 300
            self.kicked_all = False
            self.event_queue.append({"id": 3002, "type": "reset_all"})
            self.log("Referee", "全局重置，阶段回到未开始")

    def handle_dart_command(self, target_id, open_flag, launch_confirm, team=None):
        """处理 DartCommand（客户端 -> 服务端）。"""
        with self.lock:
            team_name = self._normalize_team(team or self.active_dart_team)
            dart_status = self.dart_status_by_team.setdefault(
                team_name, self._create_dart_status()
            )
            try:
                normalized_target = int(target_id)
            except Exception:
                normalized_target = int(dart_status.get("target_id", 2))
            normalized_target = max(1, min(5, normalized_target))

            dart_status["target_id"] = normalized_target

            if bool(open_flag):
                # 请求开闸：进入开启中
                dart_status["open"] = 1
                dart_status["opening_started_ts"] = time.time()
                self.log(
                    "Referee",
                    f"{team_name.upper()} dart command open accepted: target_id={normalized_target}",
                )

            if bool(launch_confirm):
                # 确认发射：仅在已开启状态下允许，发射后闸门关闭
                if int(dart_status.get("open", 0)) == 2:
                    dart_status["open"] = 0
                    dart_status["opening_started_ts"] = 0.0
                    self.log(
                        "Referee",
                        f"{team_name.upper()} dart launch confirmed: target_id={normalized_target}",
                    )
                else:
                    self.log(
                        "Referee",
                        f"{team_name.upper()} dart launch ignored (gate not ready): target_id={normalized_target}",
                    )
            self.active_dart_team = team_name
            self.dart_status = dict(dart_status)

    def get_dart_status(self, team=None):
        with self.lock:
            team_name = self._normalize_team(team or self.active_dart_team)
            dart_status = self.dart_status_by_team.get(
                team_name, self._create_dart_status()
            )
            return {
                "target_id": int(dart_status.get("target_id", 2)),
                "open": int(dart_status.get("open", 0)),
            }

    def set_dart_status_sync(self, target_id, open_state, team="red"):
        with self.lock:
            team_name = self._normalize_team(team)
            dart_status = self.dart_status_by_team.setdefault(
                team_name, self._create_dart_status()
            )
            dart_status["target_id"] = max(1, min(5, int(target_id or 2)))
            dart_status["open"] = max(0, min(2, int(open_state or 0)))
            if dart_status["open"] != 1:
                dart_status["opening_started_ts"] = 0.0
            self.log(
                "Referee",
                f"Set {team_name.upper()} DartSelectTargetStatusSync: "
                f"target_id={dart_status['target_id']}, open={dart_status['open']}",
            )
            self.active_dart_team = team_name
            self.dart_status = dict(dart_status)

    def set_global_special_mechanism(
        self, ally_fortress_sec=0, enemy_fortress_sec=0
    ):
        with self.lock:
            self.global_special_mechanism["mechanism_id"] = [1, 2]
            self.global_special_mechanism["mechanism_time_sec"] = [
                max(0, int(ally_fortress_sec or 0)),
                max(0, int(enemy_fortress_sec or 0)),
            ]
            self.log(
                "Referee",
                "Set GlobalSpecialMechanism: "
                f"ally={self.global_special_mechanism['mechanism_time_sec'][0]}, "
                f"enemy={self.global_special_mechanism['mechanism_time_sec'][1]}",
            )

    def get_global_special_mechanism(self):
        with self.lock:
            return {
                "mechanism_id": list(self.global_special_mechanism["mechanism_id"]),
                "mechanism_time_sec": list(
                    self.global_special_mechanism["mechanism_time_sec"]
                ),
            }

    def set_robot_injury_stat(self, robot_ref, **updates):
        with self.lock:
            robot_id = self._resolve_robot_id(robot_ref)
            if robot_id is None:
                return False

            stats = self.robot_injury_stats.setdefault(
                robot_id, self._empty_robot_injury_stat()
            )
            for key in stats.keys():
                if key not in updates or updates[key] is None:
                    continue
                stats[key] = max(0, int(updates[key]))

            stats.update(self._normalize_robot_injury_stat_payload(stats))

            self.log("Referee", f"Set RobotInjuryStat: robot_id={robot_id}, stats={stats}")
            return True

    def get_robot_injury_stat(self, robot_ref):
        with self.lock:
            robot_id = self._resolve_robot_id(robot_ref)
            if robot_id is None:
                return self._empty_robot_injury_stat()
            return dict(
                self.robot_injury_stats.get(robot_id, self._empty_robot_injury_stat())
            )

    def set_robot_path_plan(
        self,
        intention=1,
        start_pos_x=0,
        start_pos_y=0,
        offset_x=None,
        offset_y=None,
        sender_id=7,
    ):
        with self.lock:
            next_offset_x = [int(v) for v in (offset_x or [])]
            next_offset_y = [int(v) for v in (offset_y or [])]
            self.robot_path_plan_info = {
                "intention": max(1, min(3, int(intention or 1))),
                "start_pos_x": max(0, int(start_pos_x or 0)),
                "start_pos_y": max(0, int(start_pos_y or 0)),
                "offset_x": next_offset_x,
                "offset_y": next_offset_y,
                "sender_id": max(1, int(sender_id or 7)),
            }
            self.log(
                "Referee",
                "Set RobotPathPlanInfo: "
                f"intention={self.robot_path_plan_info['intention']}, "
                f"sender_id={self.robot_path_plan_info['sender_id']}, "
                f"points={min(len(next_offset_x), len(next_offset_y))}",
            )

    def get_robot_path_plan_info(self):
        with self.lock:
            return {
                "intention": int(self.robot_path_plan_info["intention"]),
                "start_pos_x": int(self.robot_path_plan_info["start_pos_x"]),
                "start_pos_y": int(self.robot_path_plan_info["start_pos_y"]),
                "offset_x": list(self.robot_path_plan_info["offset_x"]),
                "offset_y": list(self.robot_path_plan_info["offset_y"]),
                "sender_id": int(self.robot_path_plan_info["sender_id"]),
            }

    def set_robot_performance_selection_sync(
        self, shooter=1, chassis=1, sentry_control=0
    ):
        with self.lock:
            self.robot_performance_selection_sync = {
                "shooter": self._normalize_robot_performance_value(shooter, 4),
                "chassis": self._normalize_robot_performance_value(chassis, 4),
                "sentry_control": self._normalize_robot_performance_value(
                    sentry_control, 1
                ),
            }
            self.log(
                "Referee",
                "Set RobotPerformanceSelectionSync: "
                f"shooter={self.robot_performance_selection_sync['shooter']}, "
                f"chassis={self.robot_performance_selection_sync['chassis']}, "
                f"sentry_control={self.robot_performance_selection_sync['sentry_control']}",
            )

    def get_robot_performance_selection_sync(self):
        with self.lock:
            return dict(self.robot_performance_selection_sync)

    def set_robot_static_status(self, payload=None):
        with self.lock:
            current_robot_id = int(self.current_robot_id or 1)
            normalized = self._create_robot_static_status_state()
            source = dict(payload or {})
            normalized["connection_state"] = self._normalize_robot_status_uint(
                source.get("connection_state", normalized["connection_state"]), 1
            )
            normalized["field_state"] = self._normalize_robot_status_uint(
                source.get("field_state", normalized["field_state"]), 0
            )
            normalized["alive_state"] = self._normalize_robot_status_uint(
                source.get("alive_state", normalized["alive_state"]), 1
            )
            normalized["robot_id"] = self._normalize_robot_status_uint(
                source.get("robot_id", current_robot_id), current_robot_id, 1
            )
            normalized["robot_type"] = self._normalize_robot_status_uint(
                source.get("robot_type", normalized["robot_id"] % 100),
                normalized["robot_id"] % 100,
            )
            normalized["performance_system_shooter"] = self._normalize_robot_status_uint(
                source.get("performance_system_shooter", normalized["performance_system_shooter"]),
                1,
            )
            normalized["performance_system_chassis"] = self._normalize_robot_status_uint(
                source.get("performance_system_chassis", normalized["performance_system_chassis"]),
                1,
            )
            normalized["level"] = self._normalize_robot_status_uint(
                source.get("level", normalized["level"]), 1
            )
            normalized["max_health"] = self._normalize_robot_status_uint(
                source.get("max_health", normalized["max_health"]), 600
            )
            normalized["max_heat"] = self._normalize_robot_status_uint(
                source.get("max_heat", normalized["max_heat"]), 240
            )
            normalized["heat_cooldown_rate"] = self._normalize_robot_status_float(
                source.get("heat_cooldown_rate", normalized["heat_cooldown_rate"]), 20.0, 0.0
            )
            normalized["max_power"] = self._normalize_robot_status_uint(
                source.get("max_power", normalized["max_power"]), 120
            )
            normalized["max_buffer_energy"] = self._normalize_robot_status_uint(
                source.get("max_buffer_energy", normalized["max_buffer_energy"]), 60
            )
            normalized["max_chassis_energy"] = self._normalize_robot_status_uint(
                source.get("max_chassis_energy", normalized["max_chassis_energy"]), 120
            )
            self.robot_static_status = normalized
            self.log("Referee", f"Set RobotStaticStatus: {normalized}")
            return dict(self.robot_static_status)

    def get_robot_static_status(self):
        with self.lock:
            return dict(self.robot_static_status)

    def set_robot_dynamic_status(self, payload=None):
        with self.lock:
            normalized = self._create_robot_dynamic_status_state()
            source = dict(payload or {})
            normalized["current_health"] = self._normalize_robot_status_uint(
                source.get("current_health", normalized["current_health"]), 600
            )
            normalized["current_heat"] = self._normalize_robot_status_float(
                source.get("current_heat", normalized["current_heat"]), 0.0, 0.0
            )
            normalized["last_projectile_fire_rate"] = self._normalize_robot_status_float(
                source.get("last_projectile_fire_rate", normalized["last_projectile_fire_rate"]),
                0.0,
                0.0,
            )
            normalized["current_chassis_energy"] = self._normalize_robot_status_uint(
                source.get("current_chassis_energy", normalized["current_chassis_energy"]), 0
            )
            normalized["current_buffer_energy"] = self._normalize_robot_status_uint(
                source.get("current_buffer_energy", normalized["current_buffer_energy"]), 0
            )
            normalized["current_experience"] = self._normalize_robot_status_uint(
                source.get("current_experience", normalized["current_experience"]), 100
            )
            normalized["experience_for_upgrade"] = self._normalize_robot_status_uint(
                source.get("experience_for_upgrade", normalized["experience_for_upgrade"]), 1000
            )
            normalized["total_projectiles_fired"] = self._normalize_robot_status_uint(
                source.get("total_projectiles_fired", normalized["total_projectiles_fired"]), 0
            )
            normalized["remaining_ammo"] = self._normalize_robot_status_uint(
                source.get("remaining_ammo", normalized["remaining_ammo"]), 0
            )
            normalized["is_out_of_combat"] = self._normalize_robot_status_bool(
                source.get("is_out_of_combat", normalized["is_out_of_combat"]), False
            )
            normalized["out_of_combat_countdown"] = self._normalize_robot_status_uint(
                source.get("out_of_combat_countdown", normalized["out_of_combat_countdown"]), 0
            )
            normalized["can_remote_heal"] = self._normalize_robot_status_bool(
                source.get("can_remote_heal", normalized["can_remote_heal"]), True
            )
            normalized["can_remote_ammo"] = self._normalize_robot_status_bool(
                source.get("can_remote_ammo", normalized["can_remote_ammo"]), True
            )
            self.robot_dynamic_status = normalized
            self.log("Referee", f"Set RobotDynamicStatus: {normalized}")
            return dict(self.robot_dynamic_status)

    def get_robot_dynamic_status(self):
        with self.lock:
            return dict(self.robot_dynamic_status)

    def set_robot_module_status(self, payload=None):
        with self.lock:
            normalized = self._create_robot_module_status_state()
            source = dict(payload or {})
            for key, default_value in normalized.items():
                normalized[key] = self._normalize_robot_status_uint(
                    source.get(key, default_value),
                    default_value,
                )
            self.robot_module_status = normalized
            self.log("Referee", f"Set RobotModuleStatus: {normalized}")
            return dict(self.robot_module_status)

    def get_robot_module_status(self):
        with self.lock:
            return dict(self.robot_module_status)

    def set_sentry_status_sync(self, posture_id=0, is_weakened=False, is_powered=False):
        with self.lock:
            self.sentry_status_sync = {
                "posture_id": self._normalize_sentry_posture_id(posture_id),
                "is_weakened": bool(is_weakened),
                "is_powered": bool(is_powered),
            }
            self.log(
                "Referee",
                "Set SentryStatusSync: "
                f"posture_id={self.sentry_status_sync['posture_id']}, "
                f"is_weakened={self.sentry_status_sync['is_weakened']}, "
                f"is_powered={self.sentry_status_sync['is_powered']}",
            )

    def get_sentry_status_sync(self):
        with self.lock:
            return dict(self.sentry_status_sync)

    def apply_sentry_command(self, command_id):
        with self.lock:
            effect = self._resolve_sentry_command_effect(command_id)
            result_code = 0 if effect["is_valid"] else 1
            posture_id = effect.get("posture_id")
            if posture_id is not None and result_code == 0:
                self.sentry_status_sync = {
                    "posture_id": int(posture_id),
                    # 协议同步状态仅包含 posture_id/is_weakened/is_powered，强化姿态先映射到同姿态常态。
                    "is_weakened": False,
                    "is_powered": False,
                }
                self.log(
                    "Referee",
                    "Apply SentryCtrlCommand: "
                    f"command_id={effect['command_id']} name={effect['name']} "
                    f"-> posture_id={posture_id}, is_weakened={self.sentry_status_sync['is_weakened']}, "
                    f"is_powered={self.sentry_status_sync['is_powered']}",
                )
            else:
                self.log(
                    "Referee",
                    "Apply SentryCtrlCommand: "
                    f"command_id={effect['command_id']} name={effect['name']} "
                    f"result_code={result_code}",
                )
            return {
                "command_id": int(effect["command_id"]),
                "command_name": str(effect["name"]),
                "result_code": int(result_code),
                "posture_id": posture_id,
            }

    def push_sentry_ctrl_result(self, command_id=0, result_code=0):
        with self.lock:
            payload = {
                "command_id": max(0, int(command_id or 0)),
                "result_code": max(0, int(result_code or 0)),
            }
            self.last_sentry_ctrl_result = dict(payload)
            self.sentry_ctrl_result_queue.append(dict(payload))
            self.log(
                "Referee",
                f"Queue SentryCtrlResult: command_id={payload['command_id']}, result_code={payload['result_code']}",
            )

    def get_sentry_ctrl_results(self):
        with self.lock:
            results = list(self.sentry_ctrl_result_queue)
            self.sentry_ctrl_result_queue.clear()
            return results

    def set_buff_status(
        self,
        robot_id=0,
        buff_type=1,
        buff_level=1,
        buff_max_time=0,
        buff_left_time=0,
    ):
        with self.lock:
            self.last_buff_status = {
                "robot_id": max(0, int(robot_id or 0)),
                "buff_type": max(0, int(buff_type or 0)),
                "buff_level": int(buff_level or 0),
                "buff_max_time": max(0, int(buff_max_time or 0)),
                "buff_left_time": max(0, int(buff_left_time or 0)),
            }
            self.log(
                "Referee",
                "Set Buff status: "
                f"robot_id={self.last_buff_status['robot_id']}, "
                f"buff_type={self.last_buff_status['buff_type']}, "
                f"buff_level={self.last_buff_status['buff_level']}, "
                f"buff_max_time={self.last_buff_status['buff_max_time']}, "
                f"buff_left_time={self.last_buff_status['buff_left_time']}",
            )

    def get_buff_status(self):
        with self.lock:
            return dict(self.last_buff_status)

    def set_last_event_command(self, event_id=0, param=""):
        with self.lock:
            self.last_event_command = {
                "event_id": max(0, int(event_id or 0)),
                "param": str(param or ""),
            }
            self.log(
                "Referee",
                "Set Event command draft: "
                f"event_id={self.last_event_command['event_id']}, "
                f"param={self.last_event_command['param']!r}",
            )

    def set_custom_byte_block(
        self,
        payload_bytes,
        *,
        encoding="hex",
        hex_data=None,
        text_data=None,
    ):
        normalized = bytes(payload_bytes or b"")
        with self.lock:
            self.custom_byte_block_hex = normalized.hex(" ")
            normalized_encoding = str(encoding or "hex").strip().lower()
            if normalized_encoding not in {"hex", "utf8"}:
                normalized_encoding = "hex"
            self.custom_byte_block_form = {
                "encoding": normalized_encoding,
                "hex_data": str(
                    self.custom_byte_block_hex if hex_data is None else hex_data
                ),
                "text_data": str("" if text_data is None else text_data),
            }
            self.log("Referee", f"Set CustomByteBlock: {len(normalized)} bytes")
        return normalized

    def pause_match(self):
        with self.lock:
            self.game_status["is_paused"] = True
            self.log("Referee", "比赛暂停")

    def resume_match(self):
        with self.lock:
            self.game_status["is_paused"] = False
            self.log("Referee", "比赛恢复")

    def end_match(self):
        with self.lock:
            self.game_status["current_stage"] = 5  # 结算阶段
            self.game_status["stage_countdown_sec"] = 0
            self.game_status["stage_elapsed_sec"] = 0
            self.game_status["is_paused"] = False
            self.pending_game_result_reason = "裁判手动结束比赛"
            self.pending_game_result_end_reason = 1
            self.log("Referee", "比赛结束，进入结算阶段 (Stage 5)")
            self._enqueue_game_result()

    def set_robot_hp(self, robot_index, hp):
        with self.lock:
            if 0 <= robot_index < len(self.global_unit_status["robot_health"]):
                old_hp = self.global_unit_status["robot_health"][robot_index]
                max_hp_list = self.global_unit_status.get("robot_max_hp", [])
                max_hp = (
                    int(max_hp_list[robot_index])
                    if robot_index < len(max_hp_list)
                    else self._default_robot_max_hp
                )
                next_hp = max(0, min(max_hp, int(hp)))
                self.global_unit_status["robot_health"][robot_index] = next_hp
                if old_hp > next_hp:
                    self._record_robot_damage_locked(
                        robot_index, old_hp - next_hp, damage_key="penalty_damage"
                    )
                self.log("Referee", f"Set Robot {robot_index} HP: {old_hp} -> {hp}")
                # HP 由正数变为 0：入队击杀/阵亡事件（不依赖比赛阶段），并尝试触发复活
                if old_hp > 0 and next_hp <= 0:
                    # 入队 Event(1) param=被击杀者ID,击杀者ID
                    try:
                        rid = self.robot_ids[robot_index]
                        killer_id = 0
                        victim_id = int(rid)
                        self.event_queue.append({
                            "id": 1,
                            "param": f"{victim_id},{killer_id}"
                        })
                        self.log("Referee", f"Enqueue kill/death event: victim={victim_id}, killer={killer_id}")
                    except Exception:
                        pass
                    # 触发 respawn（仅在比赛阶段推进读条）
                    try:
                        rid = self.robot_ids[robot_index]
                    except Exception:
                        rid = None
                    if rid is not None and not self.respawn_info.get(rid, {}).get("is_pending", False):
                        self._start_respawn(robot_index)

    def adjust_robot_hp(self, robot_index, delta):
        """按增减量调整机器人血量"""
        with self.lock:
            if 0 <= robot_index < len(self.global_unit_status["robot_health"]):
                old_hp = self.global_unit_status["robot_health"][robot_index]
                max_hp_list = self.global_unit_status.get("robot_max_hp", [])
                max_hp = (
                    int(max_hp_list[robot_index])
                    if robot_index < len(max_hp_list)
                    else self._default_robot_max_hp
                )
                next_hp = max(0, min(max_hp, int(old_hp) + int(delta)))
                self.global_unit_status["robot_health"][robot_index] = next_hp
                if old_hp > next_hp:
                    self._record_robot_damage_locked(
                        robot_index, old_hp - next_hp, damage_key="penalty_damage"
                    )
                self.log("Referee", f"Adjust Robot {robot_index} HP: {old_hp} -> {next_hp} (delta={delta})")
                if old_hp > 0 and next_hp <= 0:
                    try:
                        rid = self.robot_ids[robot_index]
                        self.event_queue.append({"id": 1, "param": f"0,{rid}"})
                        self.log("Referee", f"Enqueue kill/death event: victim={rid}")
                    except Exception:
                        pass
                    try:
                        rid = self.robot_ids[robot_index]
                    except Exception:
                        rid = None
                    if rid is not None and not self.respawn_info.get(rid, {}).get("is_pending", False):
                        self._start_respawn(robot_index)

    def set_base_hp(self, hp, team="red"):
        with self.lock:
            team_name = self._normalize_team(team)
            clamped_hp = self._clamp_base_hp(hp)
            if team_name == "blue":
                old_hp = self.global_unit_status.get("blue_base_health", self.BASE_HP_MAX)
                self.global_unit_status["blue_base_health"] = clamped_hp
                self.log("Referee", f"Set Blue Base HP: {old_hp} -> {clamped_hp}")
            else:
                old_hp = self.global_unit_status.get("red_base_health", self.BASE_HP_MAX)
                self.global_unit_status["red_base_health"] = clamped_hp
                # 兼容现有 0x0003 简化发送字段（沿用红方为 base_health）
                self.global_unit_status["base_health"] = clamped_hp
                self.log("Referee", f"Set Red Base HP: {old_hp} -> {clamped_hp}")

    def adjust_base_hp(self, delta, team="red"):
        with self.lock:
            team_name = self._normalize_team(team)
            if team_name == "blue":
                current_hp = self.global_unit_status.get("blue_base_health", self.BASE_HP_MAX)
                next_hp = self._clamp_base_hp(current_hp + int(delta))
                self.global_unit_status["blue_base_health"] = next_hp
                self.log("Referee", f"Adjust Blue Base HP: {current_hp} -> {next_hp} (delta={delta})")
            else:
                current_hp = self.global_unit_status.get("red_base_health", self.BASE_HP_MAX)
                next_hp = self._clamp_base_hp(current_hp + int(delta))
                self.global_unit_status["red_base_health"] = next_hp
                # 兼容现有 0x0003 简化发送字段（沿用红方为 base_health）
                self.global_unit_status["base_health"] = next_hp
                self.log("Referee", f"Adjust Red Base HP: {current_hp} -> {next_hp} (delta={delta})")

    def set_outpost_hp(self, hp, team=None):
        with self.lock:
            # 记录旧血量用于事件判断
            old_hp = None
            if team == "blue":
                old_hp = self.global_unit_status.get("blue_outpost_health", hp)
            else:
                old_hp = self.global_unit_status.get("red_outpost_health", hp)
            if team == "blue":
                self.global_unit_status["blue_outpost_health"] = hp
                self.global_unit_status["blue_outpost_destroyed"] = hp <= 0
                self.global_unit_status["blue_outpost_status"] = 3 if hp <= 0 else 1
            else:
                self.global_unit_status["red_outpost_health"] = hp
                self.global_unit_status["red_outpost_destroyed"] = hp <= 0
                self.global_unit_status["red_outpost_status"] = 3 if hp <= 0 else 1
            self.global_unit_status["outpost_health"] = self.global_unit_status.get("red_outpost_health", hp)
            self.global_unit_status["outpost_status"] = self.global_unit_status.get("red_outpost_status", 1)
            self.log("Referee", f"Set Outpost HP ({team or 'red'}): {hp}")
            try:
                if int(old_hp) > 0 and int(hp) <= 0:
                    target_id = 111 if team == "blue" else 11
                    self.event_queue.append({
                        "id": 2,
                        "param": str(target_id)
                    })
                    self.log("Referee", f"Enqueue outpost destroyed event: target_id={target_id}")
            except Exception:
                pass

    def adjust_outpost_hp(self, team, delta):
        """按增减量调整前哨站血量，用于测试骤降检测等场景"""
        with self.lock:
            team_name = self._normalize_team(team)
            if team_name == "blue":
                current_hp = self.global_unit_status.get("blue_outpost_health", 1500)
            else:
                current_hp = self.global_unit_status.get("red_outpost_health", 1500)
            next_hp = max(0, min(1500, int(current_hp) + int(delta)))
            if team_name == "blue":
                self.global_unit_status["blue_outpost_health"] = next_hp
                self.global_unit_status["blue_outpost_destroyed"] = next_hp <= 0
                self.global_unit_status["blue_outpost_status"] = 3 if next_hp <= 0 else 1
                if int(current_hp) > 0 and next_hp <= 0:
                    self.event_queue.append({"id": 2, "param": "111"})
                    self.log("Referee", "Enqueue blue outpost destroyed event")
            else:
                self.global_unit_status["red_outpost_health"] = next_hp
                self.global_unit_status["red_outpost_destroyed"] = next_hp <= 0
                self.global_unit_status["red_outpost_status"] = 3 if next_hp <= 0 else 1
                self.global_unit_status["outpost_health"] = next_hp
                self.global_unit_status["outpost_status"] = self.global_unit_status["red_outpost_status"]
                if int(current_hp) > 0 and next_hp <= 0:
                    self.event_queue.append({"id": 2, "param": "11"})
                    self.log("Referee", "Enqueue red outpost destroyed event")
            self.log("Referee", f"Adjust Outpost HP ({team_name}): {current_hp} -> {next_hp} (delta={delta})")

    def set_outpost_status(self, status, team=None):
        """设置前哨站状态码
        0: 无敌
        1: 存活，解除无敌，中部装甲旋转
        2: 存活，解除无敌，中部装甲停转
        3: 被击毁，不可重建
        4: 被击毁，可重建
        5: 被击毁，重建中
        """
        with self.lock:
            team_name = self._normalize_team(team)
            if team_name == "blue":
                self.global_unit_status["blue_outpost_status"] = status
            else:
                self.global_unit_status["red_outpost_status"] = status
                self.global_unit_status["outpost_status"] = status
            self.log("Referee", f"Set Outpost Status ({team_name}): {status}")

    def set_score(self, team, score):
        with self.lock:
            team_name = self._normalize_team(team)
            key = f"{team_name}_score"
            old_value = self.game_status.get(key, 0)
            new_value = self._clamp(score, self.SCORE_MIN, self.SCORE_MAX)
            self.game_status[key] = new_value
            self.log("Referee", f"Set {team_name.capitalize()} Score: {old_value} -> {new_value}")

    def adjust_score(self, team, delta):
        with self.lock:
            team_name = self._normalize_team(team)
            key = f"{team_name}_score"
            current = self.game_status.get(key, 0)
            next_score = self._clamp(current + int(delta), self.SCORE_MIN, self.SCORE_MAX)
            self.game_status[key] = next_score
            self.log("Referee", f"Adjust {team_name.capitalize()} Score: {current} -> {next_score} (delta={delta})")

    def set_economy(self, team, economy):
        with self.lock:
            team_name = self._normalize_team(team)
            key = f"{team_name}_economy"
            total_key = f"{team_name}_total_economy_obtained"
            old_value = self.global_logistics_status.get(key, 0)
            total_value = self.global_logistics_status.get(total_key, 0)

            # 超过累计经济时，自动限制到累计经济
            new_value = self._clamp(economy, self.ECONOMY_MIN, self.ECONOMY_MAX)
            if new_value > total_value:
                new_value = total_value
                self.log(
                    "Referee",
                    f"Economy auto-limited to total_economy_obtained ({total_value})",
                )

            self.global_logistics_status[key] = new_value
            self.log("Referee", f"Set {team_name.capitalize()} Economy: {old_value} -> {new_value}")

    def adjust_economy(self, team, delta):
        with self.lock:
            team_name = self._normalize_team(team)
            key = f"{team_name}_economy"
            total_key = f"{team_name}_total_economy_obtained"
            current = self.global_logistics_status.get(key, 0)
            total_value = self.global_logistics_status.get(total_key, 0)

            next_value = self._clamp(current + int(delta), self.ECONOMY_MIN, self.ECONOMY_MAX)

            # 超过累计经济时，自动限制到累计经济
            if next_value > total_value:
                next_value = total_value
                self.log(
                    "Referee",
                    f"Economy auto-limited to total_economy_obtained ({total_value})",
                )

            self.global_logistics_status[key] = next_value
            self.log("Referee", f"Adjust {team_name.capitalize()} Economy: {current} -> {next_value} (delta={delta})")

    def set_round_config(self, total_rounds=None, current_round=None):
        with self.lock:
            previous_round = int(self.game_status.get("current_round", 1))
            previous_total = int(self.game_status.get("total_rounds", 3))

            next_total = previous_total
            if total_rounds is not None:
                next_total = max(1, min(9, int(total_rounds)))
                self.game_status["total_rounds"] = next_total

            if current_round is None:
                next_round = max(
                    1,
                    min(int(self.game_status.get("current_round", 1)), next_total),
                )
            else:
                next_round = max(1, min(int(current_round), next_total))
            self.game_status["current_round"] = next_round

            self.log(
                "Referee",
                "Set RoundConfig: "
                f"current_round {previous_round} -> {next_round}, "
                f"total_rounds {previous_total} -> {next_total}",
            )

    def set_match_stage(self, stage, countdown_sec=None):
        with self.lock:
            previous_stage = int(self.game_status.get("current_stage", 0))
            next_stage = max(0, min(5, int(stage)))
            next_countdown = (
                self._stage_default_countdown(next_stage)
                if countdown_sec is None
                else max(0, int(countdown_sec))
            )

            self.game_status["current_stage"] = next_stage
            self.game_status["stage_countdown_sec"] = next_countdown
            self.game_status["is_paused"] = False
            if next_stage == 4:
                self.game_status["stage_elapsed_sec"] = max(0, 420 - next_countdown)
            else:
                self.game_status["stage_elapsed_sec"] = 0
                if next_stage != 5:
                    self.pending_game_result_reason = ""
                    self.pending_game_result_end_reason = 255
                    self._game_result_enqueued = False

            self.log(
                "Referee",
                f"Set MatchStage: {previous_stage} -> {next_stage}, countdown={next_countdown}",
            )

            if next_stage == 5:
                self.pending_game_result_reason = "裁判切换到结算阶段"
                self.pending_game_result_end_reason = 1
                self._enqueue_game_result()

    def set_logistics_protocol_state(
        self,
        team="red",
        total_economy_obtained=None,
        total_damage=None,
        tech_level=None,
        encryption_level=None,
    ):
        with self.lock:
            team_name = self._normalize_team(team)
            updates = []
            if total_economy_obtained is not None:
                key = f"{team_name}_total_economy_obtained"
                value = max(0, int(total_economy_obtained))
                self.global_logistics_status[key] = value
                updates.append(f"total_economy_obtained={value}")
            if total_damage is not None:
                logistics_key = f"{team_name}_total_damage"
                unit_key = f"total_damage_{team_name}"
                value = max(0, int(total_damage))
                self.global_logistics_status[logistics_key] = value
                self.global_unit_status[unit_key] = value
                updates.append(f"total_damage={value}")
            if tech_level is not None:
                key = f"{team_name}_tech_level"
                value = max(0, int(tech_level))
                self.global_logistics_status[key] = value
                updates.append(f"tech_level={value}")
            if encryption_level is not None:
                key = f"{team_name}_encryption_level"
                value = max(0, int(encryption_level))
                self.global_logistics_status[key] = value
                updates.append(f"encryption_level={value}")
            if updates:
                self.log(
                    "Referee",
                    f"Set {team_name.upper()} logistics protocol fields: {', '.join(updates)}",
                )

    def set_stage_countdown(self, seconds):
        with self.lock:
            previous = int(self.game_status.get("stage_countdown_sec", 0))
            next_value = max(0, int(seconds))
            self.game_status["stage_countdown_sec"] = next_value

            current_stage = int(self.game_status.get("current_stage", 0))
            if current_stage == 4:
                self.game_status["stage_elapsed_sec"] = max(0, 420 - next_value)

            self.log("Referee", f"Set Stage Countdown: {previous} -> {next_value}")

    def set_base_protocol_state(self, team="red", status=None, shield=None):
        with self.lock:
            team_name = self._normalize_team(team)
            updates = []

            if status is not None:
                status_value = max(0, int(status))
                self.global_unit_status[f"{team_name}_base_status"] = status_value
                if team_name == "red":
                    self.global_unit_status["base_status"] = status_value
                updates.append(f"status={status_value}")

            if shield is not None:
                shield_value = max(0, int(shield))
                self.global_unit_status[f"{team_name}_base_shield"] = shield_value
                if team_name == "red":
                    self.global_unit_status["base_shield"] = shield_value
                updates.append(f"shield={shield_value}")

            if updates:
                self.log(
                    "Referee",
                    f"Set {team_name.upper()} base protocol fields: {', '.join(updates)}",
                )

    def start_air_support(self, team, mode="free", duration_sec=None):
        with self.lock:
            team_name = self._normalize_team(team)
            other_team = "blue" if team_name == "red" else "red"

            status_key = f"{team_name}_status"
            left_key = f"{team_name}_left_time"
            default_key = f"{team_name}_default_time"
            cost_key = f"{team_name}_cost_coins"
            mode_key = f"{team_name}_mode"
            other_status_key = f"{other_team}_status"
            other_mode_key = f"{other_team}_mode"
            mode_name = str(mode or "free").strip().lower()
            if mode_name not in ("free", "paid"):
                mode_name = "free"
            if duration_sec is not None:
                self.air_support[default_key] = max(0, int(duration_sec))
            default_time = max(0, int(self.air_support.get(default_key, 30)))

            if mode_name == "free":
                self.air_support[left_key] = default_time
            else:
                if int(self.air_support.get(status_key, 0)) == 0:
                    self.air_support[left_key] = default_time
                else:
                    self.air_support[left_key] = max(
                        0, int(self.air_support.get(left_key, 0))
                    )

            self.air_support[status_key] = 1
            self.air_support[mode_key] = mode_name
            self.air_support[other_status_key] = 0
            self.air_support[other_mode_key] = "free"
            self.air_support[f"{team_name}_is_being_targeted"] = 0
            self.air_support[f"{team_name}_shooter_status"] = 1
            self.air_support["active_team"] = team_name
            self.air_support["last_command_team"] = team_name

            self.log(
                "Referee",
                f"Air support started: team={team_name}, mode={mode_name}, "
                f"default={default_time}, left={self.air_support[left_key]}, "
                f"cost={self.air_support[cost_key]}",
            )

    def _stop_air_support_locked(self, team=None):
        active_team = str(self.air_support.get("active_team", "")).strip().lower()
        team_name = self._normalize_team(team) if team is not None else active_team
        if team_name not in ("red", "blue"):
            return None

        self.air_support[f"{team_name}_status"] = 0
        self.air_support[f"{team_name}_mode"] = "free"
        if active_team == team_name:
            self.air_support["active_team"] = ""
        self.air_support["last_command_team"] = team_name
        return team_name

    def stop_air_support(self, team=None):
        with self.lock:
            team_name = self._stop_air_support_locked(team)
            if team_name is None:
                return

            self.log("Referee", f"Air support stopped: team={team_name}")

    def set_air_support_duration(self, team="red", duration_sec=30):
        with self.lock:
            team_name = self._normalize_team(team)
            duration_value = max(0, int(duration_sec))
            self.air_support[f"{team_name}_default_time"] = duration_value
            self.air_support[f"{team_name}_left_time"] = duration_value
            self.air_support["last_command_team"] = team_name
            self.log(
                "Referee",
                f"Set air support default duration: team={team_name}, duration={duration_value}",
            )

    def set_air_support_status_sync(
        self,
        team="red",
        airsupport_status=None,
        left_time=None,
        cost_coins=None,
        is_being_targeted=None,
        shooter_status=None,
    ):
        with self.lock:
            team_name = self._normalize_team(team)
            other_team = "blue" if team_name == "red" else "red"

            status_key = f"{team_name}_status"
            left_key = f"{team_name}_left_time"
            cost_key = f"{team_name}_cost_coins"
            targeted_key = f"{team_name}_is_being_targeted"
            shooter_key = f"{team_name}_shooter_status"
            other_status_key = f"{other_team}_status"

            current_status = int(self.air_support.get(status_key, 0))
            current_left_time = int(self.air_support.get(left_key, 0))
            current_cost_coins = int(self.air_support.get(cost_key, 0))
            current_targeted = int(self.air_support.get(targeted_key, 0))
            current_shooter_status = int(self.air_support.get(shooter_key, 1))

            status_value = self._normalize_robot_status_uint(
                airsupport_status,
                default=current_status,
                minimum=0,
                maximum=2,
            )
            left_time_value = self._normalize_robot_status_uint(
                left_time,
                default=current_left_time,
                minimum=0,
                maximum=999,
            )
            cost_coins_value = self._normalize_robot_status_uint(
                cost_coins,
                default=current_cost_coins,
                minimum=0,
                maximum=9999,
            )
            targeted_value = self._normalize_robot_status_uint(
                is_being_targeted,
                default=current_targeted,
                minimum=0,
                maximum=1,
            )
            shooter_status_value = self._normalize_robot_status_uint(
                shooter_status,
                default=current_shooter_status,
                minimum=0,
                maximum=2,
            )

            self.air_support[status_key] = status_value
            self.air_support[left_key] = left_time_value
            self.air_support[cost_key] = cost_coins_value
            self.air_support[targeted_key] = targeted_value
            self.air_support[shooter_key] = shooter_status_value
            self.air_support["last_command_team"] = team_name

            if status_value > 0:
                self.air_support["active_team"] = team_name
                self.air_support[other_status_key] = 0
            elif str(self.air_support.get("active_team", "")).strip().lower() == team_name:
                self.air_support["active_team"] = ""

            self.log(
                "Referee",
                "Set AirSupportStatusSync: "
                f"team={team_name}, status={status_value}, left_time={left_time_value}, "
                f"cost_coins={cost_coins_value}, is_being_targeted={targeted_value}, "
                f"shooter_status={shooter_status_value}",
            )

    def reset_air_support_cost(self, team="red"):
        with self.lock:
            team_name = self._normalize_team(team)
            self.air_support[f"{team_name}_cost_coins"] = 0
            self.air_support["last_command_team"] = team_name
            self.log("Referee", f"Reset air support cost: team={team_name}")

    def get_air_support_status(self):
        with self.lock:
            return dict(self.air_support)

    def counter_air_support(self, team):
        with self.lock:
            team_name = self._normalize_team(team)
            if team_name not in ("red", "blue"):
                return False
            current_shooter_status = int(
                self.air_support.get(f"{team_name}_shooter_status", 1)
            )
            if current_shooter_status == 2:
                self.air_support[f"{team_name}_shooter_status"] = 1
                self.air_support[f"{team_name}_is_being_targeted"] = 0
                self.air_support["last_command_team"] = team_name
                self.log(
                    "Referee",
                    f"Air support restored: team={team_name}, shooter_status=1, is_being_targeted=0",
                )
                return False
            self._stop_air_support_locked(team_name)
            self.air_support[f"{team_name}_shooter_status"] = 2
            self.air_support[f"{team_name}_is_being_targeted"] = 1
            self.air_support["last_command_team"] = team_name
            self.log(
                "Referee",
                f"Air support countered: team={team_name}, shooter_status=2, is_being_targeted=1",
            )
            return True

    def activate_rune(self, rune_type=None, team="red"):
        with self.lock:
            team_name = self._normalize_team(team)
            rune_status = self.referee_info_by_team.setdefault(
                team_name, self._create_rune_state()
            )
            normalized_rune_type = self._normalize_rune_type(rune_type)
            rune_label = self._rune_type_label(normalized_rune_type)
            previous_status = int(rune_status.get("rune_status", 1))
            rune_status["rune_status"] = 2
            if normalized_rune_type == 1:
                # 小能量机关重新激活时只清空环数和灯臂，保持激活状态不变。
                rune_status["activated_arms"] = 0
                rune_status["average_rings"] = 0
            rune_status["last_rune_activation"] = rune_label
            self.log(
                "Referee",
                f"{team_name.upper()} rune activation requested: type={rune_label}, status {previous_status} -> 2",
            )
            self.active_rune_team = team_name
            self.referee_info = dict(rune_status)

    def set_rune_metrics(self, activated_arms=0, average_rings=0, team="red"):
        with self.lock:
            team_name = self._normalize_team(team)
            rune_status = self.referee_info_by_team.setdefault(
                team_name, self._create_rune_state()
            )
            rune_status["activated_arms"] = max(0, int(activated_arms))
            rune_status["average_rings"] = max(0, int(average_rings))
            self.log(
                "Referee",
                f"{team_name.upper()} rune metrics updated: "
                f"activated_arms={rune_status['activated_arms']}, "
                f"average_rings={rune_status['average_rings']}",
            )
            self.active_rune_team = team_name
            self.referee_info = dict(rune_status)

    def finish_activate_rune(self, rune_type=None, team="red"):
        with self.lock:
            team_name = self._normalize_team(team)
            rune_status = self.referee_info_by_team.setdefault(
                team_name, self._create_rune_state()
            )
            normalized_rune_type = self._normalize_rune_type(rune_type)
            rune_label = self._rune_type_label(normalized_rune_type)
            previous_status = int(rune_status.get("rune_status", 1))
            rune_status["rune_status"] = 3
            rune_status["last_rune_activation"] = f"{rune_label}_finished"
            self.log(
                "Referee",
                f"{team_name.upper()} rune activation finished: type={rune_label}, status {previous_status} -> 3",
            )
            self.active_rune_team = team_name
            self.referee_info = dict(rune_status)

    def reset_activate_rune(self, rune_type=None, team="red"):
        with self.lock:
            team_name = self._normalize_team(team)
            rune_status = self.referee_info_by_team.setdefault(
                team_name, self._create_rune_state()
            )
            normalized_rune_type = self._normalize_rune_type(rune_type)
            rune_label = self._rune_type_label(normalized_rune_type)
            previous_status = int(rune_status.get("rune_status", 1))
            rune_status["rune_status"] = 1
            rune_status["activated_arms"] = 0
            rune_status["average_rings"] = 0
            rune_status["last_rune_activation"] = f"{rune_label}_reset"
            self.log(
                "Referee",
                f"{team_name.upper()} rune activation reset: type={rune_label}, status {previous_status} -> 1",
            )
            self.active_rune_team = team_name
            self.referee_info = dict(rune_status)

    def get_rune_status(self, team=None):
        with self.lock:
            team_name = self._normalize_team(team or self.active_rune_team)
            return dict(
                self.referee_info_by_team.get(team_name, self._create_rune_state())
            )

    def get_events(self):
        with self.lock:
            events = list(self.event_queue)
            self.event_queue.clear()
            return events

    def get_warning_events(self):
        with self.lock:
            warnings = list(self.warning_queue)
            self.warning_queue.clear()
            return warnings

    def get_game_result_events(self):
        with self.lock:
            results = list(self.game_result_queue)
            self.game_result_queue.clear()
            return results

    def set_robot_detail(self, robot_index, level, heat, power, fire_rate=0, bullets=0, hold_heat=False):
        with self.lock:
            if 0 <= robot_index < len(self.global_unit_status["robot_health"]):
                self.global_unit_status["robot_level"][robot_index] = int(level)
                self.global_unit_status["robot_heat"][robot_index] = int(heat)
                if robot_index < len(self.global_unit_status["robot_heat_manual_hold"]):
                    self.global_unit_status["robot_heat_manual_hold"][robot_index] = bool(hold_heat)
                self.global_unit_status["robot_power"][robot_index] = int(power)
                self.global_unit_status["robot_fire_rate"][robot_index] = int(fire_rate)
                self.global_unit_status["robot_bullets"][robot_index] = int(bullets)
                self.log(
                    "Referee",
                    f"Set Robot {robot_index} Detail: Lvl{level} Heat{heat} HoldHeat{bool(hold_heat)} Pwr{power} FireRate{fire_rate} Bullets{bullets}"
                )
                current_robot_id = int(self.current_robot_id or 1)
                if robot_index < len(self.robot_ids):
                    target_robot_id = int(self.robot_ids[robot_index] or 0)
                    if target_robot_id == current_robot_id:
                        self.robot_static_status["level"] = max(0, int(level))
                        self.log(
                            "Referee",
                            "Sync RobotStaticStatus level from robot detail: "
                            f"robot_id={target_robot_id}, level={self.robot_static_status['level']}"
                        )

    def set_tech_core_motion_state(
        self,
        maximum_difficulty_level=None,
        basic_state=None,
        status=None,
        putin_state=None,
        move_state=None,
        rotate_state=None,
        enemy_core_status=None,
        remain_time_all=None,
        remain_time_step=None,
    ):
        with self.lock:
            if maximum_difficulty_level is not None:
                self.tech_core_motion_state["maximum_difficulty_level"] = max(
                    0, int(maximum_difficulty_level)
                )
            next_basic_state = basic_state if basic_state is not None else status
            if next_basic_state is not None:
                value = max(0, int(next_basic_state))
                self.tech_core_motion_state["basic_state"] = value
                self.tech_core_motion_state["status"] = value
            if putin_state is not None:
                self.tech_core_motion_state["putin_state"] = max(0, int(putin_state))
            if move_state is not None:
                self.tech_core_motion_state["move_state"] = max(0, int(move_state))
            if rotate_state is not None:
                self.tech_core_motion_state["rotate_state"] = max(0, int(rotate_state))
            if enemy_core_status is not None:
                self.tech_core_motion_state["enemy_core_status"] = max(
                    0, int(enemy_core_status)
                )
            if remain_time_all is not None:
                self.tech_core_motion_state["remain_time_all"] = max(
                    0, int(remain_time_all)
                )
            if remain_time_step is not None:
                self.tech_core_motion_state["remain_time_step"] = max(
                    0, int(remain_time_step)
                )

            self.log(
                "Referee",
                "Set TechCoreMotionStateSync: "
                f"max={self.tech_core_motion_state['maximum_difficulty_level']} "
                f"basic={self.tech_core_motion_state['basic_state']} "
                f"putin={self.tech_core_motion_state['putin_state']} "
                f"move={self.tech_core_motion_state['move_state']} "
                f"rotate={self.tech_core_motion_state['rotate_state']} "
                f"enemy={self.tech_core_motion_state['enemy_core_status']} "
                f"remain_all={self.tech_core_motion_state['remain_time_all']} "
                f"remain_step={self.tech_core_motion_state['remain_time_step']}",
            )

    def log_event_publish(self, event_id, param=""):
        self.log(
            "Referee",
            f"Publish Event: event_id={int(event_id)}, param={str(param)}",
        )

    def log_buff_publish(
        self,
        robot_id,
        buff_type,
        buff_level=1,
        buff_max_time=0,
        buff_left_time=0,
    ):
        self.log(
            "Referee",
            "Publish Buff: "
            f"robot_id={int(robot_id)}, "
            f"buff_type={int(buff_type)}, "
            f"buff_level={int(buff_level)}, "
            f"buff_max_time={int(buff_max_time)}, "
            f"buff_left_time={int(buff_left_time)}",
        )

    def get_deploy_mode_status(self, team=None):
        with self.lock:
            team_name = self._normalize_team(team or self._current_team_name())
            return int(self.deploy_mode_status_by_team.get(team_name, 0))

    def set_deploy_mode_status(self, status, team=None):
        with self.lock:
            normalized_status = 1 if int(status) == 1 else 0
            team_name = self._normalize_team(team or self._current_team_name())
            self.deploy_mode_status_by_team[team_name] = normalized_status
            self.deploy_mode_status = int(
                self.deploy_mode_status_by_team.get(self._current_team_name(), 0)
            )
            self.log(
                "Referee",
                f"Set DeployModeStatusSync: team={team_name}, status={normalized_status}",
            )

    def log_deploy_mode_publish(self, status, source="Referee", team=None):
        team_name = self._normalize_team(team or self._current_team_name())
        self.log(
            source,
            f"Publish DeployModeStatusSync: team={team_name}, status={1 if int(status) == 1 else 0}",
        )

    def get_positions(self):
        with self.lock:
            return [dict(position) for position in self.positions]

    def set_match_simulation_status(self, status):
        """将赛事演示引擎状态写入常规状态快照。"""
        with self.lock:
            normalized = dict(status or {})
            normalized["features"] = dict(normalized.get("features", {}))
            normalized["recent_events"] = [
                dict(event) for event in normalized.get("recent_events", [])
            ]
            self.match_simulation_status = normalized
            engine_state = str(normalized.get("status", normalized.get("state", "idle")))
            self._match_simulation_clock_active = engine_state in ("running", "paused")
            if engine_state == "paused":
                self.game_status["is_paused"] = True
            elif engine_state in ("running", "completed", "stopped", "idle"):
                self.game_status["is_paused"] = False

    def reset_match_simulation(self):
        """开始新一轮可重复演示前重置可变比赛数据。"""
        with self.lock:
            self._reset_game_state()
            self.game_status["current_stage"] = 0
            self.game_status["stage_countdown_sec"] = self._stage_default_countdown(0)
            self.game_status["stage_elapsed_sec"] = 0
            self.game_status["is_paused"] = False
            self.positions = [dict(position) for position in self._initial_positions]
            self._match_simulation_clock_active = False

    def apply_match_simulation_frame(
        self,
        *,
        positions,
        robot_health,
        current_stage,
        stage_countdown_sec,
        stage_elapsed_sec,
        events=None,
        structure_health=None,
        simulation_status=None,
    ):
        """将一帧 10 Hz 演示数据原子写入全部共享比赛状态。"""
        with self.lock:
            if positions is not None:
                if len(positions) != len(self.positions):
                    raise ValueError("simulation position count does not match robot count")
                self.positions = [
                    {
                        "x": max(0.0, min(28.0, float(position.get("x", 0.0)))),
                        "y": max(0.0, min(15.0, float(position.get("y", 0.0)))),
                        "angle": float(position.get("angle", 0.0)) % 360.0,
                    }
                    for position in positions
                ]

            if robot_health is not None:
                current_health = self.global_unit_status.get("robot_health", [])
                if len(robot_health) != len(current_health):
                    raise ValueError("simulation HP count does not match robot count")
                maximum_health = self.global_unit_status.get("robot_max_hp", [])
                next_health = []
                for index, value in enumerate(robot_health):
                    maximum = (
                        int(maximum_health[index])
                        if index < len(maximum_health)
                        else self._default_robot_max_hp
                    )
                    hp = max(0, min(maximum, int(value)))
                    previous = int(current_health[index])
                    if previous > hp:
                        self._record_robot_damage_locked(
                            index,
                            previous - hp,
                            damage_key="projectile_damage",
                        )
                    next_health.append(hp)
                self.global_unit_status["robot_health"] = next_health

            structure = dict(structure_health or {})
            if structure:
                red_outpost = max(0, int(structure.get("red_outpost_health", 1500)))
                blue_outpost = max(0, int(structure.get("blue_outpost_health", 1500)))
                red_base = max(0, int(structure.get("red_base_health", 5000)))
                blue_base = max(0, int(structure.get("blue_base_health", 5000)))
                red_outpost_status = max(
                    0,
                    int(
                        structure.get(
                            "red_outpost_status",
                            3 if red_outpost <= 0 else 1,
                        )
                    ),
                )
                blue_outpost_status = max(
                    0,
                    int(
                        structure.get(
                            "blue_outpost_status",
                            3 if blue_outpost <= 0 else 1,
                        )
                    ),
                )
                if red_outpost <= 0:
                    red_outpost_status = 3
                if blue_outpost <= 0:
                    blue_outpost_status = 3
                self.global_unit_status.update(
                    {
                        "red_outpost_health": red_outpost,
                        "blue_outpost_health": blue_outpost,
                        "red_outpost_destroyed": red_outpost <= 0,
                        "blue_outpost_destroyed": blue_outpost <= 0,
                        "red_outpost_status": red_outpost_status,
                        "blue_outpost_status": blue_outpost_status,
                        "red_base_health": red_base,
                        "blue_base_health": blue_base,
                        # 保留旧字段别名，兼容现有红方视角的 UDP 链路。
                        "outpost_health": red_outpost,
                        "outpost_status": red_outpost_status,
                        "base_health": red_base,
                    }
                )

            previous_stage = int(self.game_status.get("current_stage", 0))
            next_stage = max(0, min(5, int(current_stage)))
            self.game_status["current_stage"] = next_stage
            self.game_status["stage_countdown_sec"] = max(0, int(stage_countdown_sec))
            self.game_status["stage_elapsed_sec"] = max(0, int(stage_elapsed_sec))

            for event in events or []:
                event_team_value = str(event.get("team", "")).strip()
                target_team_value = str(event.get("target_team", "")).strip()
                queued_event = {
                    "id": max(
                        0,
                        int(event.get("event_id", event.get("id", 0))),
                    ),
                    "param": str(event.get("param", "")),
                    "type": "match_simulation",
                    "key": str(event.get("key", "")),
                    "team": (
                        self._normalize_team(event_team_value)
                        if event_team_value
                        else ""
                    ),
                    "target_team": (
                        self._normalize_team(target_team_value)
                        if target_team_value
                        else ""
                    ),
                }
                self.event_queue.append(queued_event)
                event_key = queued_event["key"]
                event_team = queued_event["team"] or self._current_team_name()
                target_team = queued_event["target_team"] or event_team
                if event_key == "rune_metrics":
                    self.active_rune_team = event_team
                    rune_state = self.referee_info_by_team[event_team]
                    rune_state["activated_arms"] = 4
                    rune_state["average_rings"] = 7.2
                    self.referee_info = dict(rune_state)
                elif event_key == "rune_activated":
                    self.active_rune_team = event_team
                    rune_state = self.referee_info_by_team[event_team]
                    rune_state["rune_status"] = 2
                    self.referee_info = dict(rune_state)
                elif event_key == "air_support":
                    self.air_support[f"{event_team}_status"] = 1
                    self.air_support[f"{event_team}_left_time"] = 30
                    self.air_support["active_team"] = event_team
                elif event_key == "dart_hit":
                    self.active_dart_team = event_team
                    self.dart_status_by_team[event_team]["target_id"] = 1
                    self.dart_status_by_team[event_team]["open"] = 0
                    self.dart_status = dict(self.dart_status_by_team[event_team])
                elif event_key == "outpost_stopped":
                    target_health = int(
                        self.global_unit_status.get(
                            f"{target_team}_outpost_health",
                            0,
                        )
                    )
                    if target_health > 0:
                        self.global_unit_status[f"{target_team}_outpost_status"] = 2
                        if target_team == "red":
                            self.global_unit_status["outpost_status"] = 2
                elif event_key == "outpost_destroyed":
                    # 该状态转换由受血量开关控制的建筑帧负责，Event 通道不能
                    # 绕过已关闭的血量模拟开关。
                    pass

            if simulation_status is not None:
                normalized_status = dict(simulation_status)
                normalized_status["features"] = dict(
                    normalized_status.get("features", {})
                )
                normalized_status["recent_events"] = [
                    dict(event)
                    for event in normalized_status.get("recent_events", [])
                ]
                self.match_simulation_status = normalized_status
                engine_state = str(
                    normalized_status.get("status", normalized_status.get("state", "idle"))
                )
                self._match_simulation_clock_active = engine_state in (
                    "running",
                    "paused",
                )
                self.game_status["is_paused"] = engine_state == "paused"

            if next_stage == 5 and previous_stage != 5:
                self.pending_game_result_reason = "一键赛事演示完成"
                self.pending_game_result_end_reason = 1
                self._enqueue_game_result()

    def set_robot_position(self, robot_index, x, y, angle=None):
        with self.lock:
            if 0 <= robot_index < len(self.positions):
                self.positions[robot_index]["x"] = x
                self.positions[robot_index]["y"] = y
                if angle is not None:
                    self.positions[robot_index]["angle"] = float(angle)

    def apply_penalty(self, robot_index, damage):
        with self.lock:
            if 0 <= robot_index < len(self.global_unit_status["robot_health"]):
                current = self.global_unit_status["robot_health"][robot_index]
                new_hp = max(0, current - damage)
                self.global_unit_status["robot_health"][robot_index] = new_hp
                self._record_robot_damage_locked(
                    robot_index, current - new_hp, damage_key="penalty_damage"
                )
                self.log("Referee", f"Penalty Robot {robot_index}: -{damage} HP")
                # 若由正数扣到 0：入队击杀/阵亡事件，并尝试触发复活
                if current > 0 and new_hp <= 0:
                    # 入队 Event(1) param=被击杀者ID,击杀者ID
                    try:
                        rid = self.robot_ids[robot_index]
                        killer_id = 0
                        victim_id = int(rid)
                        self.event_queue.append({
                            "id": 1,
                            "param": f"{victim_id},{killer_id}"
                        })
                        self.log("Referee", f"Enqueue kill/death event via penalty: victim={victim_id}, killer={killer_id}")
                    except Exception:
                        pass
                    # 触发 respawn（仅在比赛阶段推进读条）
                    try:
                        rid = self.robot_ids[robot_index]
                    except Exception:
                        rid = None
                    if rid is not None and not self.respawn_info.get(rid, {}).get("is_pending", False):
                        self._start_respawn(robot_index)

    def _apply_yellow_penalty_locked(self, robot_id):
        """执行单方黄牌扣血。"""
        try:
            offending_robot_id = int(robot_id)
        except Exception:
            return False
        if offending_robot_id not in self.robot_ids:
            return False

        try:
            offending_index = self.robot_ids.index(offending_robot_id)
        except ValueError:
            return False

        now = time.time()
        last_ts = float(self.last_yellow_ts.get(offending_robot_id, 0.0))
        if last_ts > 0 and (now - last_ts) <= 30.0:
            offender_multiplier = 2
            self.yellow_streak[offending_robot_id] = int(self.yellow_streak.get(offending_robot_id, 0)) + 1
        else:
            offender_multiplier = 1
            self.yellow_streak[offending_robot_id] = 1
        self.last_yellow_ts[offending_robot_id] = now
        self.yellow_count[offending_robot_id] = int(self.yellow_count.get(offending_robot_id, 0)) + 1

        is_red_team = offending_robot_id < 100
        robot_health = self.global_unit_status.get("robot_health", [])
        robot_max_hp = self.global_unit_status.get("robot_max_hp", [])

        for idx, rid in enumerate(self.robot_ids):
            if idx >= len(robot_health):
                continue
            hp = int(robot_health[idx])
            if hp <= 0:
                continue

            same_team = (rid < 100) == is_red_team
            if not same_team:
                continue

            max_hp = int(robot_max_hp[idx]) if idx < len(robot_max_hp) else self._default_robot_max_hp
            if rid == offending_robot_id:
                penalty = max(1, int(max_hp * 0.15 * offender_multiplier))
            else:
                penalty = max(1, int(max_hp * 0.05))

            new_hp = max(1, hp - penalty)
            robot_health[idx] = new_hp

        self.log(
            "Referee",
            f"Yellow penalty applied: robot_id={offending_robot_id}, yellow_count={self.yellow_count.get(offending_robot_id, 0)}, streak={self.yellow_streak.get(offending_robot_id, 0)}"
        )
        return True

    def apply_yellow_penalty(self, robot_id):
        with self.lock:
            return self._apply_yellow_penalty_locked(robot_id)

    def _apply_double_yellow_penalty_locked(self):
        """执行双方黄牌扣血。"""
        robot_health = self.global_unit_status.get("robot_health", [])
        robot_max_hp = self.global_unit_status.get("robot_max_hp", [])

        for idx in range(len(robot_health)):
            hp = int(robot_health[idx])
            if hp <= 0:
                continue
            max_hp = int(robot_max_hp[idx]) if idx < len(robot_max_hp) else self._default_robot_max_hp
            penalty = max(1, int(max_hp * 0.05))
            robot_health[idx] = max(1, hp - penalty)

        self.double_yellow_count = int(self.double_yellow_count) + 1
        self.log("Referee", f"Double yellow penalty applied: count={self.double_yellow_count}")
        return True

    def apply_double_yellow_penalty(self):
        with self.lock:
            return self._apply_double_yellow_penalty_locked()

    def issue_referee_warning(self, level, robot_id=None):
        with self.lock:
            # 等级：1=双方黄牌，2=黄牌，3=红牌，4=判负
            try:
                level_value = int(level)
            except Exception:
                level_value = 2
            if level_value < 1 or level_value > 4:
                level_value = 2

            valid_robot_ids = set(self.robot_ids)
            offending_robot_id = 0

            if level_value in (2, 3):
                try:
                    rid = int(robot_id)
                except Exception:
                    rid = 0
                if rid in valid_robot_ids:
                    offending_robot_id = rid
            # 判罚（等级，机器人id）
            penalty_key = (level_value, offending_robot_id)
            # 判罚次数
            count = int(self.penalty_counts.get(penalty_key, 0)) + 1
            self.penalty_counts[penalty_key] = count

            self.warning_queue.append({
                "type": "referee_warning",
                "level": level_value,
                "offending_robot_id": offending_robot_id,
                "count": count
            })

            if level_value == 1:
                # 双方黄牌 给所有机器人扣血5%
                self._apply_double_yellow_penalty_locked()
            elif level_value == 2 and offending_robot_id != 0:
                # 黄牌惩罚
                self._apply_yellow_penalty_locked(offending_robot_id)
                yellow_total = int(self.yellow_count.get(offending_robot_id, 0))
                if yellow_total >= 3:
                    auto_level = 3
                    auto_penalty_key = (auto_level, offending_robot_id)
                    auto_count = int(self.penalty_counts.get(auto_penalty_key, 0)) + 1
                    self.penalty_counts[auto_penalty_key] = auto_count
                    self.warning_queue.append({
                        "type": "referee_warning",
                        "level": auto_level,
                        "offending_robot_id": offending_robot_id,
                        "count": auto_count
                    })
                    self.log(
                        "Referee",
                        f"Auto escalate warning: level=3, offending_robot_id={offending_robot_id}, yellow_count={yellow_total}, count={auto_count}"
                    )

            self.log(
                "Referee",
                f"Referee warning issued: level={level_value}, offending_robot_id={offending_robot_id}, count={count}"
            )

    def _start_respawn(self, robot_index):
        """启动机器人复活计时器并记录 respawn_info"""
        if not (0 <= robot_index < len(self.robot_ids)):
            return
        rid = self.robot_ids[robot_index]
        info = self.respawn_info.get(rid)
        if info is None:
            return
        now = time.time()
        # 仅在比赛阶段（current_stage == 4）触发复活，避免准备或结算阶段发送 pending 状态
        current_stage = int(self.game_status.get("current_stage", 0))
        if current_stage != 4:
            self.log("Referee", f"Ignored respawn trigger for Robot {rid} in non-battle stage {current_stage}")
            return
        remaining_sec = int(self.game_status.get("stage_countdown_sec", 0))
        imm = int(info.get("immediate_exchange_count", 0))
        # total_progress 计算公式：round(10 + (420 - remaining_sec)/10 + 20 * immediate_exchange_count)
        try:
            total = int(round(10 + (420 - remaining_sec) / 10.0 + 20 * imm))
        except Exception:
            total = 10
        info["is_pending"] = True
        info["start_ts"] = now
        info["current_progress"] = 0
        info["total_progress"] = max(1, total)
        info["gold_cost"] = info.get("gold_cost", self._default_respawn_gold_cost)
        info["is_dead"] = True
        info["can_free"] = False
        info["can_pay"] = self.can_pay_for_respawn(rid, info.get("gold_cost", self._default_respawn_gold_cost))
        info["notified_complete"] = False
        self.log("Referee", f"Robot {rid} entered respawn: total={info['total_progress']} start_ts={now}")
        # 发布 respawn 状态（pending=true），包含 robot_id 与索引
        self.event_queue.append({"id": 201, "robot_id": rid, "robot_index": robot_index, "is_pending": True, "type": "robot_respawn_status"})

    def exchange_immediate_respawn(self, robot_index):
        """外部调用：用于客户端支付立即复活（cmd_type=4）后调用。
        返回 True 表示成功，False 表示失败。
        仅当队伍金币足够且机器人处于待复活状态时生效。
        """
        with self.lock:
            if not (0 <= robot_index < len(self.robot_ids)):
                return False
            rid = self.robot_ids[robot_index]
            info = self.respawn_info.get(rid)
            if info is None:
                return False
            if not info.get("is_pending") and not info.get("is_dead"):
                return False
            gold_cost = int(info.get("gold_cost", self._default_respawn_gold_cost))
            is_blue = int(rid) >= 100
            econ_key = "blue_economy" if is_blue else "red_economy"
            current_econ = int(self.game_status.get(econ_key, 0))
            if current_econ < gold_cost:
                self.log("Referee", f"Robot {rid} coin respawn FAILED: need {gold_cost} gold, have {current_econ}")
                return False
            # 扣除金币
            self.game_status[econ_key] = current_econ - gold_cost
            self.log("Referee", f"Robot {rid} coin respawn: deducted {gold_cost} gold, remaining {self.game_status[econ_key]}")
            # 增加立即复活计数
            info["immediate_exchange_count"] = int(info.get("immediate_exchange_count", 0)) + 1
            info["last_immediate_ts"] = time.time()
            # 立即复活：恢复满血，取消 pending
            try:
                self.global_unit_status["robot_health"][robot_index] = int(self._default_robot_max_hp * 1.0)
            except Exception:
                pass
            info["is_dead"] = False
            info["is_pending"] = False
            info["current_progress"] = info.get("total_progress", 0)
            info["notified_complete"] = False
            info["can_free"] = False
            info["can_pay"] = False
            # 记录短期无敌窗口时间
            info.setdefault("invulnerable_until", time.time() + 3.0)
            # 发布一次 respawn 状态更新（is_pending=false）
            self.event_queue.append({"id": 201, "robot_id": rid, "robot_index": robot_index, "is_pending": False, "type": "robot_respawn_status", "immediate": True})
            return True

    def log(self, source, message, type="info"):
        entry = {
            "timestamp": datetime.now().isoformat(),
            "source": source,
            "message": message,
            "type": type
        }
        self.logs.append(entry)
        if len(self.logs) > 100:
            self.logs.pop(0)
        print(f"[{entry['timestamp']}] [{source}] [{type.upper()}] {message}")

    def get_state(self):
        with self.lock:
            game_status_payload = self.game_status.copy()
            # Web 管理端仍消费 gameStatus.red_economy/blue_economy，先保留兼容导出。
            game_status_payload.update(self.global_logistics_status)
            return {
                "gameStatus": game_status_payload,
                "globalLogisticsStatus": self.global_logistics_status.copy(),
                "globalUnitStatus": self.global_unit_status.copy(),
                "refereeInfo": dict(self.referee_info),
                "refereeInfoByTeam": {
                    team_name: dict(info)
                    for team_name, info in self.referee_info_by_team.items()
                },
                "selectedRuneTeam": str(self.active_rune_team),
                "airSupport": dict(self.air_support),
                "techCoreMotionState": self.tech_core_motion_state.copy(),
                "deployModeStatus": int(
                    self.deploy_mode_status_by_team.get(self._current_team_name(), 0)
                ),
                "deployModeStatusByTeam": dict(self.deploy_mode_status_by_team),
                "globalSpecialMechanism": {
                    "mechanism_id": list(self.global_special_mechanism["mechanism_id"]),
                    "mechanism_time_sec": list(
                        self.global_special_mechanism["mechanism_time_sec"]
                    ),
                },
                "robotInjuryStats": {
                    rid: dict(stats) for rid, stats in self.robot_injury_stats.items()
                },
                "robotPerformanceSelectionSync": dict(
                    self.robot_performance_selection_sync
                ),
                "robotStaticStatus": dict(self.robot_static_status),
                "robotDynamicStatus": dict(self.robot_dynamic_status),
                "robotModuleStatus": dict(self.robot_module_status),
                "sentryStatusSync": dict(self.sentry_status_sync),
                "lastSentryCtrlResult": dict(self.last_sentry_ctrl_result),
                "robotPathPlanInfo": {
                    "intention": int(self.robot_path_plan_info["intention"]),
                    "start_pos_x": int(self.robot_path_plan_info["start_pos_x"]),
                    "start_pos_y": int(self.robot_path_plan_info["start_pos_y"]),
                    "offset_x": list(self.robot_path_plan_info["offset_x"]),
                    "offset_y": list(self.robot_path_plan_info["offset_y"]),
                    "sender_id": int(self.robot_path_plan_info["sender_id"]),
                },
                "positions": [dict(position) for position in self.positions],
                "simulationStatus": {
                    **dict(self.match_simulation_status),
                    "features": dict(
                        self.match_simulation_status.get("features", {})
                    ),
                    "recent_events": [
                        dict(event)
                        for event in self.match_simulation_status.get(
                            "recent_events", []
                        )
                    ],
                },
                "logs": list(self.logs),
                "robotIds": list(self.robot_ids),
                "respawnInfo": {rid: dict(info) for rid, info in self.respawn_info.items()},
                "kicked_all": self.kicked_all,
                "lastGameResult": dict(self.last_game_result),
                "dartStatus": {
                    "target_id": int(self.dart_status.get("target_id", 2)),
                    "open": int(self.dart_status.get("open", 0)),
                },
                "dartStatusByTeam": {
                    team_name: {
                        "target_id": int(info.get("target_id", 2)),
                        "open": int(info.get("open", 0)),
                    }
                    for team_name, info in self.dart_status_by_team.items()
                },
                "selectedDartTeam": str(self.active_dart_team),
                "lastBuffStatus": dict(self.last_buff_status),
                "lastEventCommand": dict(self.last_event_command),
                "customByteBlockForm": dict(self.custom_byte_block_form),
                "customByteBlockHex": str(self.custom_byte_block_hex),
                "currentRobotId": int(self.current_robot_id),
            }

    def _determine_winner(self):
        """返回 0 平局 / 1 红方 / 2 蓝方，遵循官方判定优先级"""
        gs = self.game_status
        gu = self.global_unit_status

        # --- 基地血量优先 ---
        red_base = int(gu.get("red_base_health", gu.get("base_health", 0)))
        blue_base = int(gu.get("blue_base_health", gu.get("base_health", 0)))
        if red_base != blue_base:
            return 1 if red_base > blue_base else 2

        # --- 前哨站判定 ---
        red_outpost_hp = int(gu.get("red_outpost_health", gu.get("outpost_health", 0)))
        blue_outpost_hp = int(gu.get("blue_outpost_health", gu.get("outpost_health", 0)))
        red_outpost_dead = bool(gu.get("red_outpost_destroyed", red_outpost_hp <= 0))
        blue_outpost_dead = bool(gu.get("blue_outpost_destroyed", blue_outpost_hp <= 0))

        # 规则2：双方前哨未被击毁，比较前哨剩余血量
        if not red_outpost_dead and not blue_outpost_dead and red_outpost_hp != blue_outpost_hp:
            return 1 if red_outpost_hp > blue_outpost_hp else 2

        # 规则3：仅一方前哨被击毁
        if red_outpost_dead != blue_outpost_dead:
            return 2 if red_outpost_dead else 1

        # --- 总伤害判定 ---
        total_dmg_red = int(gu.get("total_damage_red", 0))
        total_dmg_blue = int(gu.get("total_damage_blue", 0))
        if total_dmg_red != total_dmg_blue:
            return 1 if total_dmg_red > total_dmg_blue else 2

        # --- 全队总剩余血量 ---
        robot_hp = list(gu.get("robot_health", []))
        if len(robot_hp) >= 6:
            red_hp_sum = sum(robot_hp[:3])
            blue_hp_sum = sum(robot_hp[3:6])
        else:
            half = len(robot_hp) // 2
            red_hp_sum = sum(robot_hp[:half])
            blue_hp_sum = sum(robot_hp[half:])
        if red_hp_sum != blue_hp_sum:
            return 1 if red_hp_sum > blue_hp_sum else 2

        # --- 平局 ---
        return 0

    def _enqueue_game_result(self):
        if self._game_result_enqueued:
            return
        winner = self._determine_winner()
        if winner == 1:
            self.game_status["red_score"] = self._clamp(
                self.game_status.get("red_score", 0) + 1,
                self.SCORE_MIN,
                self.SCORE_MAX,
            )
        elif winner == 2:
            self.game_status["blue_score"] = self._clamp(
                self.game_status.get("blue_score", 0) + 1,
                self.SCORE_MIN,
                self.SCORE_MAX,
            )

        self.game_result_queue.append({
            "type": "game_result",
            "winner": winner,
            "red_score": int(self.game_status.get("red_score", 0)),
            "blue_score": int(self.game_status.get("blue_score", 0)),
        })
        self.last_game_result = {
            "winner": int(winner),
            "winner_label": self._winner_label(winner),
            "reason": str(self.pending_game_result_reason or "结算完成"),
            "end_reason": int(self.pending_game_result_end_reason),
            "red_score": int(self.game_status.get("red_score", 0)),
            "blue_score": int(self.game_status.get("blue_score", 0)),
        }
        self.log("Referee", f"0x0002 enqueue game_result: winner={winner}, queue_size={len(self.game_result_queue)}")
        self._game_result_enqueued = True
        self.log("Referee", f"比赛结果入队 winner={winner}")
