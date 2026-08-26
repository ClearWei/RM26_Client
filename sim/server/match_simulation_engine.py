#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Web 模拟器使用的可重复一键赛事演示引擎。"""

import math
import random
import threading
import time
from collections import deque


class MatchSimulationEngine:
    """在不改变原有 2 Hz 界面状态循环的前提下推进可重复赛事时间线。"""

    TICK_HZ = 10.0
    QUICK_DURATION_SEC = 90.0
    FULL_DURATION_SEC = 420.0
    OFFICIAL_BATTLE_DURATION_SEC = 420.0
    COUNTDOWN_DURATION_SEC = 5.0
    DEFAULT_SEED = 2026
    DEFAULT_FEATURES = {
        "positions": True,
        "hp": True,
        "events": True,
    }

    # progress 表示比赛阶段内的相对进度（0.0 - 1.0），不包含开赛前短倒计时。
    # 官方协议中的 Event 5/7/11/12/13 使用己方/对方语义。
    # 元组只固定协议 ID 与阈值；队伍相关参数和文案按演示开始时的操作手视角解析。
    EVENT_TIMELINE = (
        ("rune_metrics", 0.08, 3, "4,7.2", "己方完成能量机关识别"),
        ("rune_activated", 0.12, 4, "1", "己方激活小能量机关"),
        ("air_support", 0.22, 7, "", "对方空中支援入场"),
        ("hero_snipe", 0.30, 5, "100", "己方英雄完成狙击"),
        ("own_kill", 0.34, 1, "", "己方英雄击毁对方步兵"),
        ("dart_hit", 0.48, 9, "", "己方飞镖命中对方前哨站"),
        ("enemy_kill", 0.62, 1, "", "己方空中机器人阵亡"),
        ("outpost_stopped", 0.68, 12, "", "对方前哨站停转"),
        ("outpost_destroyed", 0.76, 2, "", "对方前哨站被摧毁"),
        ("base_under_attack", 0.84, 11, "", "己方基地遭到攻击"),
        ("armor_deployed", 0.92, 13, "", "对方基地护甲展开"),
    )

    def __init__(
        self,
        state_manager,
        mqtt_publisher=None,
        clock=None,
        seed=DEFAULT_SEED,
    ):
        self.state_manager = state_manager
        self.mqtt_publisher = mqtt_publisher
        self.clock = clock or time.monotonic
        self.seed = int(seed)
        self._lock = threading.RLock()
        self._status = "idle"
        self._mode = "quick"
        self._duration_sec = self.QUICK_DURATION_SEC
        self._speed = 1.0
        self._features = dict(self.DEFAULT_FEATURES)
        self._elapsed_sec = 0.0
        self._last_clock = None
        self._pause_reason = ""
        self._fired_events = set()
        self._recent_events = deque(maxlen=8)
        self._path_profiles = self._make_path_profiles()
        self._last_stage = 0
        self._operator_team = self._current_operator_team()
        self._sync_status_to_state()

    def set_mqtt_publisher(self, publisher):
        with self._lock:
            self.mqtt_publisher = publisher

    def control(self, action, mode=None, speed=None, features=None):
        """执行幂等控制动作并返回当前公开状态。"""
        normalized_action = str(action or "").strip().lower()
        with self._lock:
            if normalized_action == "configure":
                self._accrue_elapsed_without_publishing()
                self._configure(mode=mode, speed=speed, features=features)
                self._sync_status_to_state()
                return self.get_status()

            if normalized_action == "start":
                # 重复 start 只更新配置，不重启正在运行的演示。
                if self._status in ("running", "paused"):
                    self._accrue_elapsed_without_publishing()
                    self._configure(mode=mode, speed=speed, features=features)
                    self._sync_status_to_state()
                    return self.get_status()

                self._configure(mode=mode, speed=speed, features=features)
                self._elapsed_sec = 0.0
                self._last_clock = float(self.clock())
                self._pause_reason = ""
                self._fired_events.clear()
                self._recent_events.clear()
                self._path_profiles = self._make_path_profiles()
                self._last_stage = 0
                self._operator_team = self._current_operator_team()
                self._status = "running"
                self.state_manager.reset_match_simulation()
                self._apply_current_frame(publish_radar=False)
                self._sync_status_to_state()
                return self.get_status()

            if normalized_action == "pause":
                return self.pause(reason="user")

            if normalized_action == "resume":
                if self._status == "paused":
                    self._status = "running"
                    self._last_clock = float(self.clock())
                    self._pause_reason = ""
                    self._sync_status_to_state()
                return self.get_status()

            if normalized_action == "stop":
                if self._status != "stopped":
                    self._status = "stopped"
                    self._elapsed_sec = 0.0
                    self._last_clock = None
                    self._pause_reason = ""
                    self._fired_events.clear()
                    self._recent_events.clear()
                    self._last_stage = 0
                    self._operator_team = self._current_operator_team()
                    self.state_manager.reset_match_simulation()
                    self._sync_status_to_state()
                return self.get_status()

            if normalized_action == "reset":
                self._status = "idle"
                self._elapsed_sec = 0.0
                self._last_clock = None
                self._pause_reason = ""
                self._fired_events.clear()
                self._recent_events.clear()
                self._last_stage = 0
                self._operator_team = self._current_operator_team()
                self.state_manager.reset_match_simulation()
                self._sync_status_to_state()
                return self.get_status()

            raise ValueError("unsupported simulation action: %s" % normalized_action)

    def pause(self, reason="user"):
        with self._lock:
            if self._status == "running":
                self._accrue_elapsed_without_publishing()
                if self._status != "running":
                    self._sync_status_to_state()
                    return self.get_status()
                self._status = "paused"
                self._last_clock = None
                self._pause_reason = str(reason or "user")
                self._sync_status_to_state()
            return self.get_status()

    def pause_for_manual_override(self):
        """应用手动拖拽位置前暂停正在运行的演示。"""
        with self._lock:
            was_running = self._status == "running"
            status = self.pause(reason="manual_position")
            return was_running, status

    def tick(self, now=None):
        """推进一个引擎 tick；测试可传入模拟的单调时间戳。"""
        with self._lock:
            if self._status != "running":
                return None

            now_value = float(self.clock() if now is None else now)
            if self._last_clock is None:
                self._last_clock = now_value
            delta_sec = max(0.0, now_value - self._last_clock)
            self._last_clock = now_value
            self._elapsed_sec = min(
                self._duration_sec,
                self._elapsed_sec + delta_sec * self._speed,
            )
            if self._elapsed_sec >= self._duration_sec:
                self._status = "completed"
                self._last_clock = None

            frame = self._apply_current_frame(publish_radar=True)
            self._sync_status_to_state()
            return frame

    def get_status(self):
        with self._lock:
            remaining = max(0.0, self._duration_sec - self._elapsed_sec)
            stage, countdown, stage_elapsed, phase, _ = self._timeline_state()
            return {
                "state": self._status,
                "status": self._status,
                "mode": self._mode,
                "speed": self._speed,
                "features": dict(self._features),
                "elapsed": round(self._elapsed_sec, 3),
                "elapsed_sec": round(self._elapsed_sec, 3),
                "remaining": round(remaining, 3),
                "remaining_sec": round(remaining, 3),
                "duration": round(self._duration_sec, 3),
                "duration_sec": round(self._duration_sec, 3),
                "current_stage": stage,
                "stage_countdown_sec": countdown,
                "stage_elapsed_sec": stage_elapsed,
                "phase": phase,
                "pause_reason": self._pause_reason,
                "operator_team": self._operator_team,
                "recent_events": list(self._recent_events),
            }

    def _configure(self, mode=None, speed=None, features=None):
        old_duration = self._duration_sec
        if mode is not None:
            normalized_mode = str(mode).strip().lower()
            if normalized_mode not in ("quick", "full"):
                raise ValueError("mode must be 'quick' or 'full'")
            self._mode = normalized_mode
            self._duration_sec = (
                self.QUICK_DURATION_SEC
                if normalized_mode == "quick"
                else self.FULL_DURATION_SEC
            )
            if old_duration > 0 and self._elapsed_sec > 0:
                progress = min(1.0, self._elapsed_sec / old_duration)
                self._elapsed_sec = progress * self._duration_sec

        if speed is not None:
            normalized_speed = float(speed)
            if not 0.25 <= normalized_speed <= 4.0:
                raise ValueError("speed must be between 0.25 and 4.0")
            self._speed = normalized_speed

        if features is not None:
            if not isinstance(features, dict):
                raise ValueError("features must be an object")
            for feature_name in self.DEFAULT_FEATURES:
                if feature_name in features:
                    self._features[feature_name] = bool(features[feature_name])

    def _accrue_elapsed_without_publishing(self):
        if self._status != "running" or self._last_clock is None:
            return
        now_value = float(self.clock())
        delta_sec = max(0.0, now_value - self._last_clock)
        self._last_clock = now_value
        self._elapsed_sec = min(
            self._duration_sec,
            self._elapsed_sec + delta_sec * self._speed,
        )
        if self._elapsed_sec >= self._duration_sec:
            self._status = "completed"
            self._last_clock = None
            self._apply_current_frame(publish_radar=False)

    def _timeline_state(self):
        if self._status == "idle" or (
            self._status == "stopped" and self._elapsed_sec <= 0.0
        ):
            return 0, 0, 0, "idle", 0.0

        duration = max(0.001, self._duration_sec)
        countdown_duration = min(self.COUNTDOWN_DURATION_SEC, duration)
        if self._elapsed_sec >= duration:
            return 5, 0, 0, "settlement", 1.0

        if self._elapsed_sec < countdown_duration:
            ratio = self._elapsed_sec / max(0.001, countdown_duration)
            countdown = max(1, int(math.ceil(5.0 * (1.0 - ratio))))
            return 3, countdown, 0, "countdown", 0.0

        battle_duration = max(0.001, duration - countdown_duration)
        battle_elapsed = self._elapsed_sec - countdown_duration
        battle_progress = min(1.0, max(0.0, battle_elapsed / battle_duration))
        official_elapsed = min(
            self.OFFICIAL_BATTLE_DURATION_SEC,
            battle_progress * self.OFFICIAL_BATTLE_DURATION_SEC,
        )
        countdown = max(
            0,
            int(math.ceil(self.OFFICIAL_BATTLE_DURATION_SEC - official_elapsed)),
        )
        return 4, countdown, int(official_elapsed), "battle", battle_progress

    def _apply_current_frame(self, publish_radar):
        stage, countdown, stage_elapsed, phase, battle_progress = self._timeline_state()
        positions = self._build_positions(battle_progress)
        robot_health = self._build_robot_health(battle_progress)
        events = self._collect_due_events(battle_progress, stage)
        structure_health = self._build_structure_health(battle_progress)
        status = self.get_status()

        if not self._features["positions"]:
            positions = self.state_manager.get_positions()
        if not self._features["hp"]:
            robot_health = list(
                self.state_manager.get_state()
                .get("globalUnitStatus", {})
                .get("robot_health", [600] * len(positions))
            )

        self.state_manager.apply_match_simulation_frame(
            positions=positions if self._features["positions"] else None,
            robot_health=robot_health if self._features["hp"] else None,
            current_stage=stage,
            stage_countdown_sec=countdown,
            stage_elapsed_sec=stage_elapsed,
            events=events,
            structure_health=structure_health if self._features["hp"] else None,
            simulation_status=status,
        )

        if (
            publish_radar
            and self._features["positions"]
            and self.mqtt_publisher is not None
            and positions
        ):
            # 该旧命名方法会序列化完整的 8 机器人 RadarInfoToClient 消息；
            # 每个 tick 只调用一次，避免重复发送 8 份完整帧。
            focus = positions[0]
            self.mqtt_publisher.publish_radar_info_to_client(
                robot_index=0,
                x=focus["x"],
                y=focus["y"],
                angle=focus["angle"],
                is_high_light=0,
                periodic=True,
            )

        frame = dict(status)
        frame["robots"] = self._build_robot_payload(positions, robot_health)
        frame["recent_events"] = list(self._recent_events)
        frame["sequence_time"] = round(self._elapsed_sec, 3)
        self._last_stage = stage
        return frame

    def _collect_due_events(self, battle_progress, stage):
        due_events = []
        # 循环过慢时可能直接跨入结算阶段；所有跨过的时间线事件都要发送一次，
        # 不能静默丢弃末尾事件。
        if stage not in (4, 5):
            return due_events

        for key, threshold, event_id, param, label in self.EVENT_TIMELINE:
            if key in self._fired_events or battle_progress < threshold:
                continue
            self._fired_events.add(key)
            if not self._features["events"]:
                continue
            # Event 2 会直接让客户端进入前哨站已摧毁状态。关闭血量模拟时不发送，
            # 避免事件开关绕过独立的血量功能开关。
            if key == "outpost_destroyed" and not self._features["hp"]:
                continue
            event = self._resolve_event(
                key=key,
                event_id=event_id,
                param=param,
                label=label,
            )
            event.update({
                "key": key,
                # id 供界面稳定去重，event_id 保留协议值。
                "id": key,
                "elapsed_sec": round(self._elapsed_sec, 3),
            })
            due_events.append(event)
            self._recent_events.append(dict(event))
        return due_events

    def _current_operator_team(self):
        try:
            robot_id = int(getattr(self.state_manager, "current_robot_id", 1) or 1)
        except Exception:
            robot_id = 1
        return "blue" if robot_id >= 100 else "red"

    @staticmethod
    def _team_label(team):
        return "蓝方" if team == "blue" else "红方"

    def _resolve_event(self, *, key, event_id, param, label):
        own_team = self._operator_team
        enemy_team = "red" if own_team == "blue" else "blue"
        own_ids = (
            {"hero": 101, "infantry": 103, "aerial": 106}
            if own_team == "blue"
            else {"hero": 1, "infantry": 3, "aerial": 6}
        )
        enemy_ids = (
            {"hero": 101, "infantry": 103, "aerial": 106}
            if enemy_team == "blue"
            else {"hero": 1, "infantry": 3, "aerial": 6}
        )
        resolved_param = str(param)
        resolved_label = str(label)
        associated_team = ""
        target_team = ""

        if key in ("rune_metrics", "rune_activated", "hero_snipe"):
            associated_team = own_team
            resolved_label = resolved_label.replace("己方", self._team_label(own_team))
        elif key == "air_support":
            associated_team = enemy_team
            resolved_label = resolved_label.replace("对方", self._team_label(enemy_team))
        elif key == "own_kill":
            associated_team = own_team
            target_team = enemy_team
            resolved_param = "%d,%d" % (enemy_ids["infantry"], own_ids["hero"])
            resolved_label = "%s英雄击毁%s步兵" % (
                self._team_label(own_team),
                self._team_label(enemy_team),
            )
        elif key == "dart_hit":
            associated_team = own_team
            target_team = enemy_team
            resolved_param = "%d,1" % (2 if own_team == "blue" else 1)
            resolved_label = "%s飞镖命中%s前哨站" % (
                self._team_label(own_team),
                self._team_label(enemy_team),
            )
        elif key == "enemy_kill":
            associated_team = enemy_team
            target_team = own_team
            resolved_param = "%d,%d" % (own_ids["aerial"], enemy_ids["infantry"])
            resolved_label = "%s空中机器人被%s步兵击毁" % (
                self._team_label(own_team),
                self._team_label(enemy_team),
            )
        elif key in ("outpost_stopped", "outpost_destroyed", "armor_deployed"):
            associated_team = enemy_team
            target_team = enemy_team
            resolved_label = resolved_label.replace("对方", self._team_label(enemy_team))
            if key == "outpost_destroyed":
                resolved_param = "111" if enemy_team == "blue" else "11"
        elif key == "base_under_attack":
            associated_team = own_team
            target_team = own_team
            resolved_label = resolved_label.replace("己方", self._team_label(own_team))

        if not target_team:
            target_team = associated_team

        return {
            "event_id": int(event_id),
            "param": resolved_param,
            "label": resolved_label,
            "team": associated_team,
            "target_team": target_team,
        }

    def _make_path_profiles(self):
        rng = random.Random(self.seed)
        profiles = []
        for index in range(8):
            profiles.append(
                {
                    "phase": rng.uniform(0.0, math.tau),
                    "frequency": rng.uniform(1.35, 2.2),
                    "x_amplitude": rng.uniform(4.2, 6.4),
                    "y_amplitude": rng.uniform(0.8, 1.8),
                    "lane": 2.2 + (index % 4) * 3.25,
                }
            )
        return profiles

    def _build_positions(self, battle_progress):
        official_elapsed = battle_progress * self.OFFICIAL_BATTLE_DURATION_SEC
        positions = []
        for index, profile in enumerate(self._path_profiles):
            team_index = index if index < 4 else index - 4
            team_direction = 1.0 if index < 4 else -1.0
            phase = profile["phase"]
            angular_rate = math.tau * profile["frequency"] / 90.0
            wave = math.sin(official_elapsed * angular_rate + phase)
            forward = 3.8 + team_index * 0.9 + profile["x_amplitude"] * (0.5 + 0.5 * wave)
            x = forward if index < 4 else 28.0 - forward
            y_phase = official_elapsed * angular_rate * 0.73 + phase * 0.61
            y = profile["lane"] + profile["y_amplitude"] * math.sin(y_phase)

            dx = team_direction * profile["x_amplitude"] * 0.5 * angular_rate * math.cos(
                official_elapsed * angular_rate + phase
            )
            dy = profile["y_amplitude"] * angular_rate * 0.73 * math.cos(y_phase)
            angle = math.degrees(math.atan2(dy, dx)) if abs(dx) + abs(dy) > 1e-6 else 0.0
            positions.append(
                {
                    "x": round(max(0.5, min(27.5, x)), 3),
                    "y": round(max(0.5, min(14.5, y)), 3),
                    "angle": round(angle % 360.0, 2),
                }
            )
        return positions

    def _build_robot_health(self, battle_progress):
        damage_ramp = min(1.0, max(0.0, battle_progress / 0.08))
        health = []
        for index, profile in enumerate(self._path_profiles):
            wave = 0.5 + 0.5 * math.sin(
                math.tau * (battle_progress * (2.0 + (index % 3) * 0.35))
                + profile["phase"]
            )
            hp = max(
                140,
                600
                - int((250 + 20 * (index % 4)) * wave * damage_ramp),
            )
            health.append(hp)

        own_is_blue = self._operator_team == "blue"
        enemy_infantry_index = 1 if own_is_blue else 5
        own_aerial_index = 6 if own_is_blue else 2

        # 两段可见的阵亡/复活窗口与相对击杀事件对应：先由己方英雄击毁对方步兵，
        # 再由对方步兵击毁己方空中机器人。
        if 0.34 <= battle_progress < 0.38:
            health[enemy_infantry_index] = 0
        elif 0.38 <= battle_progress < 0.43:
            health[enemy_infantry_index] = min(
                600,
                500 + int((battle_progress - 0.38) * 2000),
            )

        if 0.62 <= battle_progress < 0.66:
            health[own_aerial_index] = 0
        elif 0.66 <= battle_progress < 0.71:
            health[own_aerial_index] = min(
                600,
                500 + int((battle_progress - 0.66) * 2000),
            )
        return [max(0, min(600, int(value))) for value in health]

    def _build_structure_health(self, battle_progress):
        own_team = self._operator_team
        enemy_team = "red" if own_team == "blue" else "blue"

        outpost_health = {"red": 1500, "blue": 1500}
        outpost_status = {"red": 1, "blue": 1}
        base_health = {"red": 5000, "blue": 5000}

        if battle_progress > 0.48:
            enemy_damage = int((battle_progress - 0.48) * (1500.0 / 0.28))
            outpost_health[enemy_team] = max(0, 1500 - enemy_damage)
        if battle_progress >= 0.76:
            outpost_health[enemy_team] = 0
            outpost_status[enemy_team] = 3
        elif battle_progress >= 0.68:
            outpost_status[enemy_team] = 2

        if battle_progress > 0.70:
            outpost_health[own_team] = max(
                900,
                1500 - int((battle_progress - 0.70) * 2000),
            )
        if battle_progress > 0.82:
            base_health[own_team] = max(
                3400,
                5000 - int((battle_progress - 0.82) * 9000),
            )
        if battle_progress > 0.88:
            base_health[enemy_team] = max(
                2600,
                5000 - int((battle_progress - 0.88) * 12000),
            )
        return {
            "red_outpost_health": outpost_health["red"],
            "blue_outpost_health": outpost_health["blue"],
            "red_outpost_status": outpost_status["red"],
            "blue_outpost_status": outpost_status["blue"],
            "red_base_health": base_health["red"],
            "blue_base_health": base_health["blue"],
        }

    def _build_robot_payload(self, positions, robot_health):
        robot_ids = list(getattr(self.state_manager, "robot_ids", []))
        payload = []
        for index, pose in enumerate(positions):
            robot_id = robot_ids[index] if index < len(robot_ids) else index
            payload.append(
                {
                    "index": index,
                    "robot_id": int(robot_id),
                    "team": "red" if index < 4 else "blue",
                    "x": pose["x"],
                    "y": pose["y"],
                    "angle": pose["angle"],
                    "hp": int(robot_health[index]),
                    "max_hp": 600,
                }
            )
        return payload

    def _sync_status_to_state(self):
        self.state_manager.set_match_simulation_status(self.get_status())
