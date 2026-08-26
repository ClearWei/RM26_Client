#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: MIT
"""
MQTT数据发布器

@file mqtt_publisher.py
@brief 将比赛状态数据通过MQTT发布给客户端
@author Fudan EGA Team
@date 2026-02-08
@copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).

功能说明:
    - 使用官方协议的 MQTT + Protobuf 格式
    - 定时发送比赛状态、机器人数据等
    - Topic 名称按官方规范：GameStatus, GlobalUnitStatus, RobotDynamicStatus 等

依赖:
    pip install paho-mqtt protobuf
"""

import json
import struct
import threading
import time
from contextlib import contextmanager
from typing import TYPE_CHECKING, Optional

from google.protobuf.message import Message

# MQTT 客户端
try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("请安装 paho-mqtt: pip install paho-mqtt")
    mqtt = None

# 为静态类型检查导入（仅在类型检查时导入，避免运行时依赖问题）
if TYPE_CHECKING:
    # 为类型检查导入 Client 类型别名，避免在类型注解中使用模块变量
    from paho.mqtt.client import Client as MQTTClient  # type: ignore

# Protobuf 消息
try:
    import os
    import sys

    # 添加模拟器目录到 Python 路径
    sim_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if sim_dir not in sys.path:
        sys.path.insert(0, sim_dir)
    import robomaster_pb2 as pb
except ImportError as e:
    print(f"无法导入 Protobuf 消息: {e}")
    pb = None


class MQTTPublisher:
    """
    MQTT 数据发布器

    按照官方《RoboMaster 2026 通信协议 V2.0.0》的自定义客户端协议规范，
    通过 MQTT 发布比赛数据到客户端。

    主要 Topic:
        - GameStatus: 比赛全局状态 (5Hz)
        - GlobalUnitStatus: 全局单位状态 (1Hz)
        - RobotDynamicStatus: 当前机器人实时数据 (10Hz)
        - Event: 事件通知
    """

    PERIODIC_LOG_TOPICS = {
        "GameStatus",
        "GlobalUnitStatus",
        "GlobalLogisticsStatus",
        "GlobalSpecialMechanism",
        "RobotStaticStatus",
        "RobotDynamicStatus",
        "RobotModuleStatus",
        "RobotInjuryStat",
        "RobotPosition",
        "Buff",
        "RobotPerformanceSelectionSync",
        "RuneStatusSync",
        "SentryStatusSync",
        "DartSelectTargetStatusSync",
        "AirSupportStatusSync",
        "TechCoreMotionStateSync",
        "DeployModeStatusSync",
        "RobotRespawnStatus",
        "RadarInfoToClient",
    }
    PENALTY_EFFECT_DEFAULT_SEC = 10  # PenaltyInfo 默认处罚时长 10 秒
    PENALTY_COUNT_DEFAULT = 1  # PenaltyInfo 默认处罚次数 1 次

    def __init__(
        self,
        state_manager,
        broker_host="127.0.0.1",
        broker_port=1883,
        client_id="rm_simulator",
        current_robot_id=1,
    ):
        """
        初始化 MQTT 发布器

        @param state_manager: 状态管理器，提供比赛数据
        @param broker_host: MQTT Broker 地址
        @param broker_port: MQTT Broker 端口
        @param client_id: 客户端 ID
        """
        self.state_manager = state_manager
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.client_id = client_id

        # 使用在 TYPE_CHECKING 中声明的类型名（字符串前向引用）
        self.client: Optional["MQTTClient"] = None
        self.connected = False
        self.running = False
        # 当前机器人 ID 决定下行视角与“我的机器人”数据来源
        self.current_robot_id: Optional[int] = int(current_robot_id or 1)
        self._last_respawn_target_robot_id: Optional[int] = None
        self._last_respawn_snapshot = None
        self._publish_log_context = threading.local()

        self._init_client()

    def _init_client(self):
        """初始化 MQTT 客户端"""
        if mqtt is None:
            print("MQTTPublisher: paho-mqtt 未安装，MQTT 功能禁用")
            return

        self.client = mqtt.Client(client_id=self.client_id)
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message

    def _on_connect(self, client, userdata, flags, rc):
        """连接回调"""
        if rc == 0:
            self.connected = True
            print(
                "MQTTPublisher: 已连接到 "
                f"{self.broker_host}:{self.broker_port}, current_robot_id={self.current_robot_id}"
            )
            # 订阅官方 V2.0.0 自定义客户端上行 topic
            for topic in (
                "KeyboardMouseControl",
                "CustomControl",
                "MapClickInfoNotify",
                "AssemblyCommand",
                "RobotPerformanceSelectionCommand",
                "CommonCommand",
                "HeroDeployModeEventCommand",
                "RuneActivateCommand",
                "DartCommand",
                "SentryCtrlCommand",
                "AirSupportCommand",
            ):
                client.subscribe(topic)
        else:
            print(f"MQTTPublisher: 连接失败, rc={rc}")

    def _on_disconnect(self, client, userdata, rc):
        """断开连接回调"""
        self.connected = False
        print(f"MQTTPublisher: 连接断开, rc={rc}")

    def _on_message(self, client, userdata, msg):
        """消息接收回调 - 处理客户端发来的控制指令"""
        print(f"MQTTPublisher: 收到消息 [{msg.topic}]: {len(msg.payload)} bytes")
        topic = msg.topic
        payload = msg.payload

        if topic not in {
            "KeyboardMouseControl",
            "CustomControl",
            "MapClickInfoNotify",
            "AssemblyCommand",
            "RobotPerformanceSelectionCommand",
            "CommonCommand",
            "HeroDeployModeEventCommand",
            "RuneActivateCommand",
            "DartCommand",
            "SentryCtrlCommand",
            "AirSupportCommand",
        }:
            return

        cmd_type = None
        param = None
        robot_index = None

        if pb is None or not isinstance(payload, (bytes, bytearray)):
            return

        if topic == "KeyboardMouseControl":
            try:
                control = pb.KeyboardMouseControl()
                control.ParseFromString(payload)
                print(
                    "MQTTPublisher: KeyboardMouseControl "
                    f"mouse=({control.mouse_x},{control.mouse_y},{control.mouse_z}) "
                    f"buttons=({control.left_button_down},{control.right_button_down},{control.mid_button_down}) "
                    f"keyboard=0x{control.keyboard_value:04x}"
                )
            except Exception as e:
                print(f"MQTTPublisher: KeyboardMouseControl parse error - {e}")
            return

        if topic == "CustomControl":
            try:
                custom = pb.CustomControl()
                custom.ParseFromString(payload)
                print(
                    f"MQTTPublisher: CustomControl {len(custom.data)} bytes"
                )
            except Exception as e:
                print(f"MQTTPublisher: CustomControl parse error - {e}")
            return

        if topic == "MapClickInfoNotify":
            try:
                map_click = pb.MapClickInfoNotify()
                map_click.ParseFromString(payload)
                print(
                    "MQTTPublisher: MapClickInfoNotify "
                    f"mode={map_click.mode} type={map_click.type} enemy_id={map_click.enemy_id} "
                    f"map=({map_click.map_x:.2f},{map_click.map_y:.2f})"
                )
            except Exception as e:
                print(f"MQTTPublisher: MapClickInfoNotify parse error - {e}")
            return

        if topic == "AssemblyCommand":
            try:
                command = pb.AssemblyCommand()
                command.ParseFromString(payload)
                print(
                    "MQTTPublisher: AssemblyCommand "
                    f"operation={command.operation} difficulty={command.difficulty}"
                )
            except Exception as e:
                print(f"MQTTPublisher: AssemblyCommand parse error - {e}")
            return

        if topic == "RobotPerformanceSelectionCommand":
            try:
                command = pb.RobotPerformanceSelectionCommand()
                command.ParseFromString(payload)
                if hasattr(self.state_manager, "set_robot_performance_selection_sync"):
                    self.state_manager.set_robot_performance_selection_sync(
                        shooter=command.shooter,
                        chassis=command.chassis,
                        sentry_control=command.sentry_control,
                    )
                self.publish_robot_performance_selection_sync(
                    {
                        "shooter": int(command.shooter),
                        "chassis": int(command.chassis),
                        "sentry_control": int(command.sentry_control),
                    }
                )
                print(
                    "MQTTPublisher: RobotPerformanceSelectionCommand "
                    f"shooter={command.shooter} chassis={command.chassis} "
                    f"sentry_control={command.sentry_control}"
                )
            except Exception as e:
                print(
                    "MQTTPublisher: RobotPerformanceSelectionCommand parse error - "
                    f"{e}"
                )
            return

        if topic == "CommonCommand":
            try:
                command = pb.CommonCommand()
                command.ParseFromString(payload)
                cmd_type = int(command.cmd_type)
                param = int(command.param)
            except Exception as e:
                print(f"MQTTPublisher: CommonCommand parse error - {e}")
                return

        if topic == "HeroDeployModeEventCommand":
            try:
                command = pb.HeroDeployModeEventCommand()
                command.ParseFromString(payload)
                status = 1 if int(command.mode) == 1 else 0
                team_name = self._current_team_name()
                print(
                    "MQTTPublisher: HeroDeployModeEventCommand "
                    f"mode={command.mode} -> status={status}"
                )
                if hasattr(self.state_manager, "set_deploy_mode_status"):
                    self.state_manager.set_deploy_mode_status(status, team_name)
                if hasattr(self.state_manager, "log_deploy_mode_publish"):
                    self.state_manager.log_deploy_mode_publish(
                        status, source="MQTT", team=team_name
                    )
                self.publish_deploy_mode_status_sync(status, team=team_name)
            except Exception as e:
                print(f"MQTTPublisher: HeroDeployModeEventCommand parse error - {e}")
            return

        if topic == "RuneActivateCommand":
            try:
                command = pb.RuneActivateCommand()
                command.ParseFromString(payload)
                team_name = self._current_team_name()
                if hasattr(self.state_manager, "activate_rune"):
                    self.state_manager.activate_rune(command.activate, team_name)
                self.publish_rune_status_sync(team=team_name)
                print(f"MQTTPublisher: RuneActivateCommand activate={command.activate}")
            except Exception as e:
                print(f"MQTTPublisher: RuneActivateCommand parse error - {e}")
            return

        if topic == "DartCommand":
            try:
                command = pb.DartCommand()
                command.ParseFromString(payload)
                team_name = self._current_team_name()
                if hasattr(self.state_manager, "handle_dart_command"):
                    self.state_manager.handle_dart_command(
                        command.target_id,
                        command.open,
                        command.launch_confirm,
                        team_name,
                    )
                dart_status = {}
                if hasattr(self.state_manager, "get_dart_status"):
                    dart_status = self.state_manager.get_dart_status(team_name)
                self.publish_dart_select_target_status(
                    dart_status.get("target_id", command.target_id),
                    dart_status.get("open", 1 if command.open else 0),
                    team=team_name,
                )
                print(
                    "MQTTPublisher: DartCommand "
                    f"target_id={command.target_id} open={command.open} "
                    f"launch_confirm={command.launch_confirm}"
                )
            except Exception as e:
                print(f"MQTTPublisher: DartCommand parse error - {e}")
            return

        if topic == "SentryCtrlCommand":
            try:
                command = pb.SentryCtrlCommand()
                command.ParseFromString(payload)
                effect = self.state_manager.apply_sentry_command(command.command_id)
                result_code = int(effect.get("result_code", 1))
                self.state_manager.push_sentry_ctrl_result(
                    command_id=command.command_id,
                    result_code=result_code,
                )
                self.publish_sentry_ctrl_result(
                    command_id=command.command_id,
                    result_code=result_code,
                )
                self.publish_sentry_status_sync(
                    self.state_manager.get_sentry_status_sync()
                )
                command_name = {
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
                }.get(int(command.command_id), "unknown")
                print(
                    "MQTTPublisher: SentryCtrlCommand "
                    f"command_id={command.command_id} name={command_name}"
                )
            except Exception as e:
                print(f"MQTTPublisher: SentryCtrlCommand parse error - {e}")
            return

        if topic == "AirSupportCommand":
            try:
                command = pb.AirSupportCommand()
                command.ParseFromString(payload)
                if hasattr(self.state_manager, "handle_air_support_command"):
                    self.state_manager.handle_air_support_command(
                        self._current_team_name(), command.command_id
                    )
                self._publish_air_support_status_sync(self.state_manager.get_state())
                command_name = {
                    0: "cancel",
                    1: "free",
                    2: "paid",
                }.get(int(command.command_id), "unknown")
                print(
                    "MQTTPublisher: AirSupportCommand "
                    f"command_id={command.command_id} name={command_name}"
                )
            except Exception as e:
                print(f"MQTTPublisher: AirSupportCommand parse error - {e}")
            return

        # 2) 解析 param -> robot_index
        if param is not None:
            try:
                idx = int(param)
            except Exception:
                idx = None

            robot_ids = []
            try:
                robot_ids = getattr(
                    self.state_manager, "robot_ids", None
                ) or self.state_manager.get_state().get("robotIds", [])
            except Exception:
                robot_ids = []

            if idx is not None and 0 <= idx < len(robot_ids):
                robot_index = idx
            else:
                # 尝试以 param 作为 robot_id 查找索引
                try:
                    if robot_ids:
                        # 容错匹配（字符串/数字）
                        sval = str(param)
                        for i, rid in enumerate(robot_ids):
                            if str(rid) == sval:
                                robot_index = i
                                break
                except Exception:
                    robot_index = None

        # 3) 处理命令类型
        try:
            if int(cmd_type) == 3:
                # 确认复活
                if robot_index is None:
                    print(
                        "MQTTPublisher: CommonCommand cmd_type=3 received but robot_index unresolved"
                    )
                else:
                    state = self.state_manager.get_state()
                    respawn_info = state.get("respawnInfo", {})
                    robot_ids = state.get("robotIds", [])
                    robot_id = None
                    if 0 <= robot_index < len(robot_ids):
                        robot_id = robot_ids[robot_index]

                    info = (
                        respawn_info.get(robot_id, {}) if robot_id is not None else None
                    )
                    if info and info.get("is_pending"):
                        current = int(info.get("current_progress", 0))
                        total = int(info.get("total_progress", 0))
                        # 满条时允许执行免费复活，恢复 10% HP
                        if total > 0 and current >= total:
                            try:
                                if hasattr(
                                    self.state_manager, "global_unit_status"
                                ) and isinstance(
                                    self.state_manager.global_unit_status, dict
                                ):
                                    hp_list = self.state_manager.global_unit_status.get(
                                        "robot_health", []
                                    )
                                    if 0 <= robot_index < len(hp_list):
                                        max_hp = getattr(
                                            self.state_manager,
                                            "_default_robot_max_hp",
                                            1000,
                                        )
                                        hp_list[robot_index] = int(max_hp * 0.10)
                                        self.state_manager.global_unit_status[
                                            "robot_health"
                                        ] = hp_list
                            except Exception as e:
                                print(
                                    f"MQTTPublisher: error setting robot HP on confirm respawn - {e}"
                                )

                            try:
                                if hasattr(
                                    self.state_manager, "respawn_info"
                                ) and isinstance(self.state_manager.respawn_info, dict):
                                    rinfo = self.state_manager.respawn_info.get(
                                        robot_id, {}
                                    )
                                    rinfo["is_dead"] = False
                                    rinfo["is_pending"] = False
                                    rinfo["current_progress"] = total
                                    rinfo["can_free"] = False
                                    rinfo["notified_complete"] = False
                                    self.state_manager.respawn_info[robot_id] = rinfo
                                elif hasattr(self.state_manager, "set_respawn_info"):
                                    self.state_manager.set_respawn_info(
                                        robot_id,
                                        {
                                            "is_dead": False,
                                            "is_pending": False,
                                            "current_progress": total,
                                            "can_free": False,
                                            "notified_complete": False,
                                        },
                                    )
                            except Exception:
                                pass

                            ev = {
                                "type": "robot_respawn_status",
                                "robot_id": robot_id,
                                "robot_index": robot_index,
                                "is_pending": False,
                                "is_dead": False,
                                "current_progress": total,
                                "total_progress": total,
                                "timestamp": int(time.time() * 1000),
                            }
                            try:
                                if hasattr(
                                    self.state_manager, "event_queue"
                                ) and isinstance(self.state_manager.event_queue, list):
                                    self.state_manager.event_queue.append(ev)
                                elif hasattr(self.state_manager, "push_event"):
                                    self.state_manager.push_event(ev)
                                elif hasattr(self.state_manager, "enqueue_event"):
                                    self.state_manager.enqueue_event(ev)
                                else:
                                    try:
                                        getattr(
                                            self.state_manager,
                                            "get_events",
                                            lambda: None,
                                        )()
                                    except Exception:
                                        pass
                            except Exception:
                                pass

                            print(
                                f"MQTTPublisher: Confirmed respawn (free) for robot_index={robot_index} robot_id={robot_id}"
                            )

            elif int(cmd_type) == 4:
                # 兑换立即复活
                if robot_index is None:
                    print(
                        "MQTTPublisher: CommonCommand cmd_type=4 received but robot_index unresolved"
                    )
                else:
                    try:
                        res = False
                        if hasattr(self.state_manager, "exchange_immediate_respawn"):
                            res = bool(
                                self.state_manager.exchange_immediate_respawn(
                                    robot_index
                                )
                            )
                        print(
                            f"MQTTPublisher: exchange_immediate_respawn(robot_index={robot_index}) -> {res}"
                        )
                    except Exception as e:
                        print(f"MQTTPublisher: exchange_immediate_respawn error - {e}")

        except Exception as e:
            print(
                f"MQTTPublisher: Error handling CommonCommand cmd_type={cmd_type} param={param} - {e}"
            )

    def connect(self) -> bool:
        """连接到 MQTT Broker"""
        if self.client is None:
            return False

        try:
            self.client.connect(self.broker_host, self.broker_port, keepalive=60)
            self.client.loop_start()
            return True
        except Exception as e:
            print(f"MQTTPublisher: 连接失败 - {e}")
            return False

    def disconnect(self):
        """断开连接"""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()

    def start(self):
        """启动发布循环"""
        if not self.connect():
            print("MQTTPublisher: 无法启动，连接失败")
            return False

        self.running = True
        # 启动多个发送线程，不同频率
        threading.Thread(target=self._publish_loop_5hz, daemon=True).start()
        threading.Thread(target=self._publish_loop_1hz, daemon=True).start()
        print(
            "MQTTPublisher: 发布循环已启动 "
            f"(current_robot_id={self.current_robot_id})"
        )
        return True

    def stop(self):
        """停止发布"""
        self.running = False
        self.disconnect()
        print("MQTTPublisher: 已停止")

    # =========================================================================
    # 发布循环
    # =========================================================================

    def _publish_loop_5hz(self):
        """5Hz 发布循环 - GameStatus"""
        while self.running:
            try:
                if self.connected:
                    state = self.state_manager.get_state()
                    with self._periodic_publish_log_scope():
                        self._publish_game_status(state)
            except Exception as e:
                print(f"MQTTPublisher: 5Hz 发布错误 - {e}")
            time.sleep(0.2)

    def _publish_loop_1hz(self):
        """1Hz 发布循环 - 低频全局状态"""
        while self.running:
            try:
                if self.connected:
                    state = self.state_manager.get_state()
                    with self._periodic_publish_log_scope():
                        self._publish_robot_static_status(state)
                        self._publish_global_unit_status(state)
                        self._publish_global_logistics_status(
                            state.get("globalLogisticsStatus", state["gameStatus"])
                        )
                        self._publish_global_special_mechanism(state)
                        self.publish_rune_status_sync(
                            state, team=self._current_team_name()
                        )
                        buff_status = state.get("lastBuffStatus", {})
                        if int(buff_status.get("buff_left_time", 0)) > 0:
                            self.publish_buff(
                                buff_status.get("robot_id", 0),
                                buff_status.get("buff_type", 0),
                                buff_status.get("buff_level", 1),
                                buff_status.get("buff_max_time", 0),
                                buff_status.get("buff_left_time", 0),
                            )
                        self._publish_air_support_status_sync(
                            state, team=self._current_team_name()
                        )

                    try:
                        self._publish_current_robot_respawn_status(state)
                    except Exception as e:
                        print(f"MQTTPublisher: respawn publish loop error - {e}")

                    try:
                        self._publish_pending_events()
                    except Exception as e:
                        print(f"MQTTPublisher: pending event publish loop error - {e}")
            except Exception as e:
                print(f"MQTTPublisher: 1Hz 发布错误 - {e}")
            time.sleep(1.0)

    def _publish_loop_10hz(self):
        """10Hz 发布循环 - 机器人状态已改为触发式发送"""
        while self.running:
            time.sleep(0.1)

    # =========================================================================
    # 消息发布方法
    # =========================================================================

    def _publish(self, topic: str, payload: bytes, qos: int = 1):
        """发布消息到指定 Topic"""
        should_log = self._should_log_publish(topic)
        # 高频周期 topic（尤其是 10 Hz 的 RadarInfoToClient）关闭日志后，
        # 连 Protobuf 转 JSON 的格式化开销也应跳过。
        payload_log = (
            self._format_publish_payload_for_log(topic, payload)
            if should_log
            else ""
        )
        if self.client and self.connected:
            self.client.publish(topic, payload, qos=qos)
            if should_log:
                print(f"MQTTPublisher: 发布 [{topic}] qos={qos} payload={payload_log}")
        else:
            if should_log:
                print(
                    f"MQTTPublisher: drop {topic}, MQTT not connected, "
                    f"payload={payload_log}"
                )

    # 过滤周期新信息
    @contextmanager
    def _periodic_publish_log_scope(self):
        previous = bool(getattr(self._publish_log_context, "periodic", False))
        self._publish_log_context.periodic = True
        try:
            yield
        finally:
            self._publish_log_context.periodic = previous

    def _should_log_publish(self, topic: str) -> bool:
        is_periodic_scope = bool(
            getattr(self._publish_log_context, "periodic", False)
        )
        if is_periodic_scope and topic in self.PERIODIC_LOG_TOPICS:
            return False
        return True

    @staticmethod
    def _normalize_team_name(team) -> str:
        normalized = str(team or "red").strip().lower()
        return "blue" if normalized == "blue" else "red"

    # 发送mqtt消息日志 用于测试
    @staticmethod
    def _truncate_log_text(text: str, limit: int = 600) -> str:
        if len(text) <= limit:
            return text
        return text[: limit - 3] + "..."

    @classmethod
    def _message_field_to_log_value(cls, value):
        if isinstance(value, Message):
            return cls._message_to_log_dict(value)
        if isinstance(value, (bytes, bytearray)):
            raw = bytes(value)
            hex_value = raw.hex(" ")
            if len(raw) > 32:
                return f"{hex_value[:95]}... ({len(raw)} bytes)"
            return hex_value
        if isinstance(value, list):
            return [cls._message_field_to_log_value(item) for item in value]
        return value

    @classmethod
    def _message_to_log_dict(cls, message: Message) -> dict:
        log_dict = {}
        for field in message.DESCRIPTOR.fields:
            value = getattr(message, field.name)
            is_repeated = getattr(field, "label", None) == getattr(field, "LABEL_REPEATED", None)
            if not is_repeated:
                is_repeated = getattr(field, "is_repeated", False)

            if is_repeated:
                log_dict[field.name] = [
                    cls._message_field_to_log_value(item) for item in value
                ]
            else:
                log_dict[field.name] = cls._message_field_to_log_value(value)
        return log_dict

    @classmethod
    def _format_publish_payload_for_log(cls, topic: str, payload: bytes) -> str:
        if not isinstance(payload, (bytes, bytearray)):
            return cls._truncate_log_text(repr(payload))

        if pb is None:
            return f"<{len(payload)} bytes>"

        message_cls = getattr(pb, topic, None)
        if message_cls is None:
            return f"<{len(payload)} bytes>"

        try:
            message = message_cls()
            message.ParseFromString(bytes(payload))
            payload_dict = cls._message_to_log_dict(message)
            return cls._truncate_log_text(
                json.dumps(payload_dict, ensure_ascii=False, separators=(",", ":"))
            )
        except Exception as exc:
            return f"<{len(payload)} bytes, decode_error={exc}>"

    def _publish_game_status(self, state: dict):
        """
        发布比赛状态

        Topic: GameStatus
        """
        if pb is None:
            return

        # 周期发布传入完整状态，确保阶段和结算结果来自同一个快照。保留
        # 直接传 gameStatus 的兼容路径，避免已有调试脚本失效。
        if isinstance(state.get("gameStatus"), dict):
            status = state["gameStatus"]
            result = state.get("lastGameResult", {})
        else:
            status = state
            result = getattr(self.state_manager, "last_game_result", {})

        msg = pb.GameStatus()
        msg.current_round = status.get("current_round", 1)
        msg.total_rounds = status.get("total_rounds", 2)
        msg.red_score = status.get("red_score", 0)
        msg.blue_score = status.get("blue_score", 0)
        msg.current_stage = status.get("current_stage", 0)
        msg.stage_countdown_sec = status.get("stage_countdown_sec", 0)
        msg.stage_elapsed_sec = status.get("stage_elapsed_sec", 0)
        msg.is_paused = bool(status.get("is_paused", False))

        # 非结算阶段不能根据实时比分猜测胜负；255 表示尚未结算。
        if int(status.get("current_stage", 0)) != 5:
            msg.game_result = 255
            msg.end_reason = 255
        else:
            winner = int(result.get("winner", 0))
            msg.game_result = winner if winner in (0, 1, 2) else 0
            end_reason = int(result.get("end_reason", 1))
            msg.end_reason = max(0, min(255, end_reason))

        self._publish("GameStatus", msg.SerializeToString())

    def _publish_global_unit_status(self, state: dict):
        """
        发布全局单位状态（MQTT）。

        Topic: GlobalUnitStatus
        """
        if pb is None:
            return

        status = state.get("globalUnitStatus", {})
        red_outpost_health = int(
            status.get("red_outpost_health", status.get("outpost_health", 1500))
        )
        red_base_health = int(
            status.get("red_base_health", status.get("base_health", 5000))
        )
        blue_outpost_health = int(
            status.get("blue_outpost_health", status.get("outpost_health", 1500))
        )
        blue_base_health = int(
            status.get("blue_base_health", status.get("base_health", 5000))
        )
        # 直接使用设置的outpost_status，而不是根据血量计算
        red_outpost_status = int(
            status.get("red_outpost_status",
                       3 if status.get("red_outpost_destroyed", red_outpost_health <= 0) else 1)
        )
        blue_outpost_status = int(
            status.get("blue_outpost_status",
                       3 if status.get("blue_outpost_destroyed", blue_outpost_health <= 0) else 1)
        )
        red_base_status = int(status.get("red_base_status", status.get("base_status", 0)))
        blue_base_status = int(
            status.get("blue_base_status", status.get("base_status", 0))
        )
        red_base_shield = int(
            status.get("red_base_shield", status.get("base_shield", 0))
        )
        blue_base_shield = int(
            status.get("blue_base_shield", status.get("base_shield", 0))
        )
        total_damage_red = int(status.get("total_damage_red", 0))
        total_damage_blue = int(status.get("total_damage_blue", 0))

        is_blue_perspective = int(self.current_robot_id or 1) >= 100
        # 己方和敌方各补齐到 5 台机器人，使客户端采用中型阵容（5+5），
        # 避免把紧凑阵容（3+3）误判为仅己方的扩展阵容。
        # 红方编号为 {1,2,3,4,7}，蓝方编号为 {101,102,103,104,107}。
        if is_blue_perspective:
            ally_base_health = blue_base_health
            ally_base_status = blue_base_status
            ally_base_shield = blue_base_shield
            ally_outpost_health = blue_outpost_health
            ally_outpost_status = blue_outpost_status
            enemy_base_health = red_base_health
            enemy_base_status = red_base_status
            enemy_base_shield = red_base_shield
            enemy_outpost_health = red_outpost_health
            enemy_outpost_status = red_outpost_status
            ally_robot_health, ally_robot_bullets = (
                self._build_protocol_global_unit_robot_values(state, "blue")
            )
            enemy_robot_health, _ = self._build_protocol_global_unit_robot_values(
                state, "red"
            )
            total_damage_ally = total_damage_blue
            total_damage_enemy = total_damage_red
        else:
            ally_base_health = red_base_health
            ally_base_status = red_base_status
            ally_base_shield = red_base_shield
            ally_outpost_health = red_outpost_health
            ally_outpost_status = red_outpost_status
            enemy_base_health = blue_base_health
            enemy_base_status = blue_base_status
            enemy_base_shield = blue_base_shield
            enemy_outpost_health = blue_outpost_health
            enemy_outpost_status = blue_outpost_status
            ally_robot_health, ally_robot_bullets = (
                self._build_protocol_global_unit_robot_values(state, "red")
            )
            enemy_robot_health, _ = self._build_protocol_global_unit_robot_values(
                state, "blue"
            )
            total_damage_ally = total_damage_red
            total_damage_enemy = total_damage_blue

        msg = pb.GlobalUnitStatus()
        msg.base_health = ally_base_health
        msg.base_status = ally_base_status
        msg.base_shield = ally_base_shield
        msg.outpost_health = ally_outpost_health
        msg.outpost_status = ally_outpost_status
        msg.enemy_base_health = enemy_base_health
        msg.enemy_base_status = enemy_base_status
        msg.enemy_base_shield = enemy_base_shield
        msg.enemy_outpost_health = enemy_outpost_health
        msg.enemy_outpost_status = enemy_outpost_status

        for hp in ally_robot_health:
            msg.robot_health.append(int(hp))

        for hp in enemy_robot_health:
            msg.robot_health.append(int(hp))

        for bullets in ally_robot_bullets:
            msg.robot_bullets.append(int(bullets))

        msg.total_damage_ally = total_damage_ally
        msg.total_damage_enemy = total_damage_enemy

        self._publish("GlobalUnitStatus", msg.SerializeToString())

    def _publish_global_logistics_status(self, logistics_status):
        if pb is None:
            return

        msg = pb.GlobalLogisticsStatus()
        team_prefix = "blue" if (self.current_robot_id or 1) >= 100 else "red"
        msg.remaining_economy = int(logistics_status.get(f"{team_prefix}_economy", 0))
        msg.total_economy_obtained = int(
            logistics_status.get(
                f"{team_prefix}_total_economy_obtained",
                msg.remaining_economy,
            )
        )
        msg.tech_level = int(logistics_status.get(f"{team_prefix}_tech_level", 1))
        msg.encryption_level = int(
            logistics_status.get(f"{team_prefix}_encryption_level", 1)
        )
        self._publish("GlobalLogisticsStatus", msg.SerializeToString())

    def _publish_global_special_mechanism(self, state: dict):
        if pb is None:
            return

        mechanism = state.get("globalSpecialMechanism", {})
        msg = pb.GlobalSpecialMechanism()
        for mechanism_id in mechanism.get("mechanism_id", []):
            msg.mechanism_id.append(max(0, int(mechanism_id)))
        for mechanism_time_sec in mechanism.get("mechanism_time_sec", []):
            remaining = int(mechanism_time_sec or 0)
            # 直接发布剩余时间，不做 20 - remaining 的反转映射
            msg.mechanism_time_sec.append(max(0, remaining))
        self._publish("GlobalSpecialMechanism", msg.SerializeToString())

    def _publish_robot_injury_stat(self, state: dict):
        if pb is None:
            return

        robot_id = self.current_robot_id or 1
        stats = (
            state.get("robotInjuryStats", {}).get(robot_id)
            or state.get("robotInjuryStats", {}).get(str(robot_id))
            or {}
        )
        msg = pb.RobotInjuryStat()
        msg.total_damage = max(0, int(stats.get("total_damage", 0)))
        msg.collision_damage = max(0, int(stats.get("collision_damage", 0)))
        msg.small_projectile_damage = max(
            0, int(stats.get("small_projectile_damage", 0))
        )
        msg.large_projectile_damage = max(
            0, int(stats.get("large_projectile_damage", 0))
        )
        msg.dart_splash_damage = max(0, int(stats.get("dart_splash_damage", 0)))
        msg.module_offline_damage = max(
            0, int(stats.get("module_offline_damage", 0))
        )
        msg.offline_damage = max(0, int(stats.get("offline_damage", 0)))
        msg.penalty_damage = max(0, int(stats.get("penalty_damage", 0)))
        msg.server_kill_damage = max(0, int(stats.get("server_kill_damage", 0)))
        msg.killer_id = max(0, int(stats.get("killer_id", 0)))
        self._publish("RobotInjuryStat", msg.SerializeToString())

    def _publish_robot_position(self, state: dict):
        if pb is None:
            return

        robot_ids = list(state.get("robotIds", []))
        positions = list(state.get("positions", []))
        if not robot_ids or not positions:
            return

        robot_index = self._resolve_current_robot_index(state)
        if not (0 <= robot_index < len(positions)):
            return

        pose = positions[robot_index]
        msg = pb.RobotPosition()
        msg.x = float(pose.get("x", 0.0))
        msg.y = float(pose.get("y", 0.0))
        msg.z = 0.0
        msg.yaw = float(pose.get("angle", 0.0))
        msg.robot_id = int(robot_ids[robot_index])
        self._publish("RobotPosition", msg.SerializeToString())

    def _publish_robot_path_plan_info(self, state: dict):
        if pb is None:
            return

        info = state.get("robotPathPlanInfo", {})
        sender_id = int(info.get("sender_id", 7))
        if sender_id <= 0:
            sender_id = 7

        msg = pb.RobotPathPlanInfo()
        msg.intention = max(1, min(3, int(info.get("intention", 1))))
        msg.start_pos_x = max(0, int(info.get("start_pos_x", 0)))
        msg.start_pos_y = max(0, int(info.get("start_pos_y", 0)))
        for value in info.get("offset_x", []):
            msg.offset_x.append(int(value))
        for value in info.get("offset_y", []):
            msg.offset_y.append(int(value))
        msg.sender_id = sender_id
        self._publish("RobotPathPlanInfo", msg.SerializeToString())

    def _resolve_current_robot_index(self, state: dict) -> int:
        robot_ids = list(state.get("robotIds", []))
        if not robot_ids:
            return 0

        target_robot_id = self.current_robot_id or robot_ids[0]
        try:
            return robot_ids.index(target_robot_id)
        except ValueError:
            self.current_robot_id = robot_ids[0]
            return 0

    def _radar_robot_order(self):
        red_order = [1, 2, 3, 4, 6, 7]
        blue_order = [101, 102, 103, 104, 106, 107]
        return red_order + blue_order if (self.current_robot_id or 1) >= 100 else blue_order + red_order

    def _current_team_name(self) -> str:
        return "blue" if int(self.current_robot_id or 1) >= 100 else "red"

    def _effective_current_robot_id(self) -> int:
        try:
            state_robot_id = int(
                getattr(self.state_manager, "current_robot_id", self.current_robot_id) or self.current_robot_id or 1
            )
        except Exception:
            state_robot_id = int(self.current_robot_id or 1)
        self.current_robot_id = state_robot_id
        return state_robot_id

    @staticmethod
    def _respawn_publish_snapshot(info: dict):
        return (
            bool(info.get("is_pending", False)),
            bool(info.get("is_dead", False)),
            int(info.get("total_progress", 0) or 0),
            int(info.get("current_progress", 0) or 0),
            bool(info.get("can_free", False)),
            int(info.get("gold_cost", 0) or 0),
            bool(info.get("can_pay", False)),
        )

    def _publish_current_robot_respawn_status(self, state: dict) -> bool:
        target_robot_id = self._effective_current_robot_id()
        if target_robot_id != self._last_respawn_target_robot_id:
            self._last_respawn_target_robot_id = target_robot_id
            self._last_respawn_snapshot = None

        respawn_info = state.get("respawnInfo", {})
        info = respawn_info.get(target_robot_id)
        if info is None:
            info = respawn_info.get(str(target_robot_id))
        if not isinstance(info, dict):
            self._last_respawn_snapshot = None
            return False

        total = int(info.get("total_progress", 0) or 0)
        current = int(info.get("current_progress", 0) or 0)
        should_publish = bool(
            info.get("is_pending")
            or info.get("is_dead")
            or (total > 0 and current >= total)
        )
        if not should_publish:
            self._last_respawn_snapshot = None
            return False

        snapshot = self._respawn_publish_snapshot(info)
        if snapshot == self._last_respawn_snapshot:
            return False

        robot_index = None
        for idx, robot_id in enumerate(state.get("robotIds", [])):
            if str(robot_id) == str(target_robot_id):
                robot_index = idx
                break

        print(
            "MQTTPublisher: respawn publish check "
            f"target_robot_id={target_robot_id} idx={robot_index} info={info}"
        )
        self._publish_robot_respawn_status(target_robot_id, robot_index, info)
        self._last_respawn_snapshot = snapshot
        return True

    @staticmethod
    def _global_unit_protocol_robot_ids(team_name: str):
        if str(team_name).strip().lower() == "blue":
            return [101, 102, 103, 104, 106, 107]
        return [1, 2, 3, 4, 6, 7]

    @staticmethod
    def _global_unit_protocol_robot_aliases(protocol_robot_id: int):
        return []

    def _resolve_protocol_robot_value_maps(self, state: dict):
        global_status = state.get("globalUnitStatus", {})
        robot_ids = list(
            state.get("robotIds", getattr(self.state_manager, "robot_ids", []))
        )
        robot_health = list(global_status.get("robot_health", []))
        robot_bullets = list(global_status.get("robot_bullets", []))

        default_hp = int(
            getattr(self.state_manager, "_default_robot_max_hp", 600) or 600
        )
        health_by_robot_id = {}
        bullets_by_robot_id = {}

        for index, robot_id in enumerate(robot_ids):
            try:
                rid = int(robot_id)
            except Exception:
                continue
            health_by_robot_id[rid] = (
                int(robot_health[index]) if index < len(robot_health) else default_hp
            )
            bullets_by_robot_id[rid] = (
                int(robot_bullets[index]) if index < len(robot_bullets) else 0
            )

        return health_by_robot_id, bullets_by_robot_id, default_hp

    def _build_protocol_global_unit_robot_values(self, state: dict, team_name: str):
        (
            health_by_robot_id,
            bullets_by_robot_id,
            default_hp,
        ) = self._resolve_protocol_robot_value_maps(state)

        health_values = []
        bullet_values = []
        for protocol_robot_id in self._global_unit_protocol_robot_ids(team_name):
            source_robot_id = protocol_robot_id
            if source_robot_id not in health_by_robot_id:
                for alias_robot_id in self._global_unit_protocol_robot_aliases(
                    protocol_robot_id
                ):
                    if alias_robot_id in health_by_robot_id:
                        source_robot_id = alias_robot_id
                        break

            health_values.append(
                int(health_by_robot_id.get(source_robot_id, default_hp))
            )
            bullet_values.append(int(bullets_by_robot_id.get(source_robot_id, 0)))

        return health_values, bullet_values

    def _publish_robot_static_status(self, state: dict):
        if pb is None:
            return
        static_status = dict(state.get("robotStaticStatus", {}))

        msg = pb.RobotStaticStatus()
        msg.connection_state = max(0, int(static_status.get("connection_state", 1)))
        msg.field_state = max(0, int(static_status.get("field_state", 0)))
        msg.alive_state = max(0, int(static_status.get("alive_state", 1)))
        msg.robot_id = max(1, int(static_status.get("robot_id", self.current_robot_id or 1)))
        msg.robot_type = max(0, int(static_status.get("robot_type", msg.robot_id % 100)))
        msg.performance_system_shooter = max(
            0, int(static_status.get("performance_system_shooter", 1))
        )
        msg.performance_system_chassis = max(
            0, int(static_status.get("performance_system_chassis", 1))
        )
        msg.level = max(0, int(static_status.get("level", 1)))
        msg.max_health = max(0, int(static_status.get("max_health", 600)))
        msg.max_heat = max(0, int(static_status.get("max_heat", 240)))
        msg.heat_cooldown_rate = max(
            0.0, float(static_status.get("heat_cooldown_rate", 20.0))
        )
        msg.max_power = max(0, int(static_status.get("max_power", 120)))
        msg.max_buffer_energy = max(
            0, int(static_status.get("max_buffer_energy", 60))
        )
        msg.max_chassis_energy = max(
            0, int(static_status.get("max_chassis_energy", 120))
        )
        self._publish("RobotStaticStatus", msg.SerializeToString())
        publish_source = (
            "periodic-1hz"
            if bool(getattr(self._publish_log_context, "periodic", False))
            else "event"
        )
        print(
            "MQTTPublisher: RobotStaticStatus sent "
            f"source={publish_source} robot_id={msg.robot_id} "
            f"robot_type={msg.robot_type} level={msg.level} "
            f"perf_shooter={msg.performance_system_shooter} "
            f"perf_chassis={msg.performance_system_chassis} "
            f"max_health={msg.max_health} max_heat={msg.max_heat}"
        )

    def publish_robot_static_status(self, state: Optional[dict] = None):
        source = state or self.state_manager.get_state()
        return self._publish_robot_static_status(source)

    def _publish_robot_dynamic_status(self, state: dict):
        if pb is None:
            return
        dynamic_status = dict(state.get("robotDynamicStatus", {}))

        msg = pb.RobotDynamicStatus()
        msg.current_health = max(0, int(dynamic_status.get("current_health", 0)))
        msg.current_heat = max(0.0, float(dynamic_status.get("current_heat", 0.0)))
        msg.last_projectile_fire_rate = max(
            0.0, float(dynamic_status.get("last_projectile_fire_rate", 0.0))
        )
        msg.current_chassis_energy = max(
            0, int(dynamic_status.get("current_chassis_energy", 0))
        )
        msg.current_buffer_energy = max(
            0, int(dynamic_status.get("current_buffer_energy", 0))
        )
        msg.current_experience = max(
            0, int(dynamic_status.get("current_experience", 0))
        )
        msg.experience_for_upgrade = max(
            0, int(dynamic_status.get("experience_for_upgrade", 1000))
        )
        msg.total_projectiles_fired = max(
            0, int(dynamic_status.get("total_projectiles_fired", 0))
        )
        msg.remaining_ammo = max(0, int(dynamic_status.get("remaining_ammo", 0)))
        msg.is_out_of_combat = bool(dynamic_status.get("is_out_of_combat", False))
        msg.out_of_combat_countdown = max(
            0, int(dynamic_status.get("out_of_combat_countdown", 0))
        )
        msg.can_remote_heal = bool(dynamic_status.get("can_remote_heal", True))
        msg.can_remote_ammo = bool(dynamic_status.get("can_remote_ammo", True))
        self._publish("RobotDynamicStatus", msg.SerializeToString())

    def publish_robot_dynamic_status(self, state: Optional[dict] = None):
        source = state or self.state_manager.get_state()
        return self._publish_robot_dynamic_status(source)

    def _publish_robot_module_status(self, state: Optional[dict] = None):
        if pb is None:
            return
        module_status = dict((state or {}).get("robotModuleStatus", {}))

        msg = pb.RobotModuleStatus()
        msg.power_manager = max(0, int(module_status.get("power_manager", 1)))
        msg.rfid = max(0, int(module_status.get("rfid", 1)))
        msg.light_strip = max(0, int(module_status.get("light_strip", 1)))
        msg.small_shooter = max(0, int(module_status.get("small_shooter", 1)))
        msg.big_shooter = max(0, int(module_status.get("big_shooter", 1)))
        msg.uwb = max(0, int(module_status.get("uwb", 1)))
        msg.armor = max(0, int(module_status.get("armor", 1)))
        msg.video_transmission = max(
            0, int(module_status.get("video_transmission", 1))
        )
        msg.capacitor = max(0, int(module_status.get("capacitor", 1)))
        msg.main_controller = max(0, int(module_status.get("main_controller", 1)))
        msg.laser_detection_module = max(
            0, int(module_status.get("laser_detection_module", 1))
        )
        self._publish("RobotModuleStatus", msg.SerializeToString())

    def publish_robot_module_status(self, state: Optional[dict] = None):
        source = state or self.state_manager.get_state()
        return self._publish_robot_module_status(source)

    def publish_event(self, event_id: int, param: str = ""):
        """
        发布事件通知

        Topic: Event
        """
        if pb is None:
            return

        msg = pb.Event()
        msg.event_id = event_id
        msg.param = param

        self._publish("Event", msg.SerializeToString())
        print(f"MQTTPublisher: Event event_id={event_id} param={param}")

    def publish_buff(
        self,
        robot_id: int,
        buff_type: int,
        buff_level: int = 1,
        buff_max_time: int = 0,
        buff_left_time: int = 0,
    ):
        """
        发布 Buff 效果信息

        Topic: Buff
        """
        if pb is None:
            return False

        try:
            msg = pb.Buff()
            msg.robot_id = max(0, int(robot_id))
            msg.buff_type = max(0, int(buff_type))
            msg.buff_level = int(buff_level)
            msg.buff_max_time = max(0, int(buff_max_time))
            msg.buff_left_time = max(0, int(buff_left_time))

            self._publish("Buff", msg.SerializeToString())
            return True
        except Exception as e:
            print(f"MQTTPublisher: publish Buff failed - {e}")
            return False

    def publish_custom_byte_block(self, data: bytes) -> bool:
        """
        发布工业相机 H.264 视频分片

        Topic: CustomByteBlock
        客户端 NetworkManager 解析 CustomByteBlock.data 后转发给 VideoReceiver::feedH264Frame。
        分片格式: [H264 字节] + [末尾 1 字节 packetId]，qos=0 模拟图传不可靠传输、避免过期分片重传。
        """
        if pb is None:
            return False

        try:
            msg = pb.CustomByteBlock()
            msg.data = data
            self._publish("CustomByteBlock", msg.SerializeToString(), qos=0)
            return True
        except Exception as e:
            print(f"MQTTPublisher: publish CustomByteBlock failed - {e}")
            return False

    def publish_tech_core_motion_state_sync(self, state: dict):
        """
        发布 TechCoreMotionStateSync

        Topic: TechCoreMotionStateSync
        """
        if pb is None:
            return False

        try:
            msg = pb.TechCoreMotionStateSync()
            msg.maximum_difficulty_level = int(
                state.get("maximum_difficulty_level", 0)
            )
            msg.basic_state = int(state.get("basic_state", state.get("status", 0)))
            msg.putin_state = int(state.get("putin_state", 0))
            msg.move_state = int(state.get("move_state", 0))
            msg.rotate_state = int(state.get("rotate_state", 0))
            msg.enemy_core_status = int(state.get("enemy_core_status", 0))
            msg.remain_time_all = int(state.get("remain_time_all", 0))
            msg.remain_time_step = int(state.get("remain_time_step", 0))

            self._publish("TechCoreMotionStateSync", msg.SerializeToString())
            return True
        except Exception as e:
            print(
                f"MQTTPublisher: publish TechCoreMotionStateSync failed - {e}"
            )
            return False

    def publish_deploy_mode_status_sync(
        self,
        status: Optional[int] = None,
        team: Optional[str] = None,
        state: Optional[dict] = None,
    ):
        """
        发布 DeployModeStatusSync

        Topic: DeployModeStatusSync
        """
        if pb is None:
            return False

        try:
            if status is None:
                source = state or self.state_manager.get_state()
                team_name = self._normalize_team_name(team or self._current_team_name())
                status = (
                    source.get("deployModeStatusByTeam", {}).get(team_name)
                    if isinstance(source.get("deployModeStatusByTeam", {}), dict)
                    else source.get("deployModeStatus", 0)
                )
            msg = pb.DeployModeStatusSync()
            msg.status = 1 if int(status) == 1 else 0

            self._publish("DeployModeStatusSync", msg.SerializeToString())
            return True
        except Exception as e:
            print(f"MQTTPublisher: publish DeployModeStatusSync failed - {e}")
            return False

    def publish_robot_performance_selection_sync(self, state: dict):
        if pb is None:
            return False

        try:
            msg = pb.RobotPerformanceSelectionSync()
            msg.shooter = max(0, int(state.get("shooter", 1)))
            msg.chassis = max(0, int(state.get("chassis", 1)))
            msg.sentry_control = max(0, int(state.get("sentry_control", 0)))
            self._publish("RobotPerformanceSelectionSync", msg.SerializeToString())
            return True
        except Exception as e:
            print(
                "MQTTPublisher: publish RobotPerformanceSelectionSync failed - "
                f"{e}"
            )
            return False

    def publish_rune_status_sync(
        self, state: Optional[dict] = None, team: Optional[str] = None
    ):
        if pb is None:
            return False

        try:
            source = state or self.state_manager.get_state()
            team_name = self._normalize_team_name(
                team or self._current_team_name()
            )
            rune = (
                source.get("refereeInfoByTeam", {}).get(team_name)
                or source.get("refereeInfo", {})
            )
            msg = pb.RuneStatusSync()
            msg.rune_status = max(0, int(rune.get("rune_status", 1)))
            msg.activated_arms = max(0, int(rune.get("activated_arms", 0)))
            msg.average_rings = float(rune.get("average_rings", 0.0))
            self._publish("RuneStatusSync", msg.SerializeToString())
            publish_source = (
                "periodic-1hz"
                if bool(getattr(self._publish_log_context, "periodic", False))
                else "event"
            )
            print(
                "MQTTPublisher: RuneStatusSync sent "
                f"source={publish_source} team={team_name} "
                f"rune_status={msg.rune_status} "
                f"activated_arms={msg.activated_arms} "
                f"average_rings={msg.average_rings:.1f}"
            )
            return True
        except Exception as e:
            print(f"MQTTPublisher: publish RuneStatusSync failed - {e}")
            return False

    @staticmethod
    def _normalize_rune_type_value(rune_type) -> int:
        try:
            value = int(rune_type)
        except Exception:
            text = str(rune_type or "").strip().lower()
            return 2 if text in {"large", "big", "2"} else 1
        return 2 if value == 2 else 1

    def publish_rune_metrics_event(self, activated_arms: int, average_rings: float):
        try:
            param = f"{int(activated_arms)},{float(average_rings):.1f}"
        except Exception:
            param = f"{int(activated_arms or 0)},0.0"
        self.publish_event(3, param)

    def publish_rune_activated_event(self, rune_type=None):
        try:
            normalized = self._normalize_rune_type_value(rune_type)
        except Exception:
            normalized = 0
        self.publish_event(4, str(normalized))

    def publish_sentry_status_sync(self, state: dict):
        if pb is None:
            return False

        try:
            msg = pb.SentryStatusSync()
            msg.posture_id = max(0, int(state.get("posture_id", 0)))
            msg.is_weakened = bool(state.get("is_weakened", False))
            msg.is_powered = bool(state.get("is_powered", False))
            self._publish("SentryStatusSync", msg.SerializeToString())
            return True
        except Exception as e:
            print(f"MQTTPublisher: publish SentryStatusSync failed - {e}")
            return False

    def publish_dart_select_target_status(
        self,
        target_id: Optional[int] = None,
        open_state: Optional[int] = None,
        team: Optional[str] = None,
        state: Optional[dict] = None,
    ):
        if pb is None:
            return False

        try:
            if target_id is None or open_state is None:
                source = state or self.state_manager.get_state()
                team_name = self._normalize_team_name(
                    team or self._current_team_name()
                )
                dart_status = (
                    source.get("dartStatusByTeam", {}).get(team_name)
                    or source.get("dartStatus", {})
                )
                target_id = dart_status.get("target_id", 2)
                open_state = dart_status.get("open", 0)
            msg = pb.DartSelectTargetStatusSync()
            msg.target_id = max(0, int(target_id))
            msg.open = max(0, int(open_state))
            self._publish("DartSelectTargetStatusSync", msg.SerializeToString())
            return True
        except Exception as e:
            print(
                "MQTTPublisher: publish DartSelectTargetStatusSync failed - "
                f"{e}"
            )
            return False

    def publish_sentry_ctrl_result(self, command_id: int, result_code: int):
        if pb is None:
            return False

        try:
            msg = pb.SentryCtrlResult()
            msg.command_id = max(0, int(command_id))
            msg.result_code = max(0, int(result_code))
            self._publish("SentryCtrlResult", msg.SerializeToString())
            return True
        except Exception as e:
            print(f"MQTTPublisher: publish SentryCtrlResult failed - {e}")
            return False

    def _publish_pending_sentry_ctrl_results(self):
        if pb is None or not hasattr(self.state_manager, "get_sentry_ctrl_results"):
            return

        for result in self.state_manager.get_sentry_ctrl_results():
            self.publish_sentry_ctrl_result(
                result.get("command_id", 0),
                result.get("result_code", 0),
            )

    def _publish_air_support_status_sync(
        self, state: dict, team: Optional[str] = None
    ):
        if pb is None:
            return False

        try:
            support = state.get("airSupport", {})
            team_name = self._normalize_team_name(team or self._current_team_name())

            msg = pb.AirSupportStatusSync()
            msg.airsupport_status = max(
                0, int(support.get(f"{team_name}_status", 0))
            )
            msg.left_time = max(0, int(support.get(f"{team_name}_left_time", 0)))
            msg.cost_coins = max(0, int(support.get(f"{team_name}_cost_coins", 0)))
            msg.is_being_targeted = max(0, int(support.get(f"{team_name}_is_being_targeted", 0)))
            msg.shooter_status = max(0, int(support.get(f"{team_name}_shooter_status", 1)))
            did_publish = bool(self.client and self.connected)
            self._publish("AirSupportStatusSync", msg.SerializeToString())
            if did_publish:
                publish_source = (
                    "periodic-1hz"
                    if bool(getattr(self._publish_log_context, "periodic", False))
                    else "event"
                )
                print(
                    "MQTTPublisher: AirSupportStatusSync sent "
                    f"source={publish_source} team={team_name} "
                    f"airsupport_status={msg.airsupport_status} "
                    f"left_time={msg.left_time} "
                    f"cost_coins={msg.cost_coins} "
                    f"is_being_targeted={msg.is_being_targeted} "
                    f"shooter_status={msg.shooter_status}"
                )
            return True
        except Exception as e:
            print(f"MQTTPublisher: publish AirSupportStatusSync failed - {e}")
            return False

    def _publish_pending_events(self):
        if pb is None:
            return

        for event in self.state_manager.get_events():
            event_type = event.get("type")
            if event_type == "robot_respawn_status":
                continue

            msg = pb.Event()
            msg.event_id = int(event.get("id", 0))
            msg.param = str(event.get("param", event_type or ""))
            self._publish("Event", msg.SerializeToString())

    def publish_pending_warnings(self):
        """
        立即发布所有待处理的警告事件（PenaltyInfo）。
        触发式发送，用于裁判警告操作时立即通知客户端。
        """
        self._publish_pending_warnings()

    def _publish_pending_warnings(self):
        if pb is None:
            return

        for warning in self.state_manager.get_warning_events():
            msg = pb.PenaltyInfo()
            msg.penalty_type = int(warning.get("level", 0))
            msg.penalty_effect_sec = self.PENALTY_EFFECT_DEFAULT_SEC
            msg.total_penalty_num = max(
                self.PENALTY_COUNT_DEFAULT,
                int(warning.get("count", self.PENALTY_COUNT_DEFAULT)),
            )
            self._publish("PenaltyInfo", msg.SerializeToString())
            print(
                "MQTTPublisher: PenaltyInfo "
                f"penalty_type={msg.penalty_type} "
                f"penalty_effect_sec={msg.penalty_effect_sec} "
                f"total_penalty_num={msg.total_penalty_num}"
            )


    def _publish_robot_respawn_status(self, robot_id, robot_index, info: dict):
        """
        发布单个机器人复活状态（MQTT）。

        向"RobotRespawnStatus"话题发布，
        仅当目标 robot_id 等于当前选择的机器人时才发布。
        """
        try:
            # 没有 robot_id 就无法定向，直接跳过
            if robot_id is None:
                print(
                    "MQTTPublisher: skip RobotRespawnStatus publish because robot_id is None"
                )
                return

            # 仅向当前模拟器选中的机器人视角发布对应的复活状态。
            target_robot_id = self._effective_current_robot_id()
            if int(robot_id) != int(target_robot_id):
                return

            topic = "RobotRespawnStatus"

            if pb is None:
                print(
                    f"MQTTPublisher: Protobuf not available, cannot publish RobotRespawnStatus for robot_id={robot_id}"
                )
            else:
                msg = pb.RobotRespawnStatus()
                msg.is_pending_respawn = bool(info.get("is_pending", False))
                try:
                    msg.total_respawn_progress = int(info.get("total_progress", 0))
                except Exception:
                    msg.total_respawn_progress = 0
                try:
                    msg.current_respawn_progress = int(info.get("current_progress", 0))
                except Exception:
                    msg.current_respawn_progress = 0
                msg.can_free_respawn = bool(info.get("can_free", False))
                try:
                    msg.gold_cost_for_respawn = int(
                        info.get(
                            "gold_cost",
                            getattr(
                                self.state_manager, "_default_respawn_gold_cost", 100
                            ),
                        )
                    )
                except Exception:
                    msg.gold_cost_for_respawn = 0
                msg.can_pay_for_respawn = bool(info.get("can_pay"))

                self._publish(topic, msg.SerializeToString())
        except Exception as e:
            print(f"MQTTPublisher: 发布 RobotRespawnStatus 失败 - {e}")

    def publish_radar_info_to_client(
        self,
        robot_index,
        x,
        y,
        angle=0.0,
        is_high_light=0,
        periodic=False,
    ):
        """
        发布 V2.0.0 RadarInfoToClient 批量位置信息到 MQTT。

        periodic=True 用于赛事引擎的 10 Hz 连续帧：协议仍正常发布，但关闭逐帧
        payload 日志，避免终端 I/O 反过来拖慢模拟器。手动拖拽保持原有日志。
        """
        if pb is None:
            print("MQTTPublisher: Protobuf not available, skip RadarInfoToClient")
            return False
        if not (self.client and self.connected):
            return False

        try:
            idx = int(robot_index)
            is_high_light = int(is_high_light)
        except Exception as e:
            print(f"MQTTPublisher: invalid radar payload - {e}")
            return False

        robot_ids = list(getattr(self.state_manager, "robot_ids", []))
        positions = self.state_manager.get_positions()

        focus_robot_id = idx
        if 0 <= idx < len(robot_ids):
            focus_robot_id = int(robot_ids[idx])

        position_by_robot_id = {}
        for pos_index, robot_id in enumerate(robot_ids):
            if pos_index < len(positions):
                position_by_robot_id[int(robot_id)] = positions[pos_index]

        msg = pb.RadarInfoToClient()
        for robot_id in self._radar_robot_order():
            entry = msg.robot_info.add()
            pose = position_by_robot_id.get(int(robot_id))
            if pose is None:
                entry.target_pos_x = 0
                entry.target_pos_y = 0
                entry.is_high_light = 0
                continue

            entry.target_pos_x = max(
                0, int(round(float(pose.get("x", 0.0)) * 100.0))
            )
            entry.target_pos_y = max(
                0, int(round(float(pose.get("y", 0.0)) * 100.0))
            )
            entry.is_high_light = (
                int(is_high_light) if int(robot_id) == focus_robot_id else 0
            )

        payload = msg.SerializeToString()
        if periodic:
            with self._periodic_publish_log_scope():
                self._publish("RadarInfoToClient", payload)
        else:
            self._publish("RadarInfoToClient", payload)
            print(
                "MQTTPublisher: publish RadarInfoToClient "
                f"focus_robot_id={focus_robot_id} entries={len(msg.robot_info)}"
            )
        return True




# 测试代码
if __name__ == "__main__":
    from state_manager import StateManager

    sm = StateManager()
    publisher = MQTTPublisher(sm, broker_host="127.0.0.1", broker_port=3333)

    try:
        publisher.start()
        print("按 Ctrl+C 停止...")
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        publisher.stop()
        print("已停止")
