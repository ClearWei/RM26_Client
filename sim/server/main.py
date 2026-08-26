#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: MIT
"""
RoboMaster 2026 模拟服务器入口

@file main.py
@brief FastAPI + SocketIO 模拟服务器，用于无硬件时测试客户端
@author Fudan EGA Team
@date 2025-12-07
@copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).

功能说明:
    - StateManager: 管理比赛状态（分数、血量、阶段等）
    - UDPSender: 通过UDP广播协议数据给客户端
    - UDPReceiver: 接收客户端指令
    - VideoStreamer: 模拟视频推流

使用方法:
    python3 main.py --target-ip 127.0.0.1
    浏览器访问 http://localhost:8000 进入Web控制台
"""

import argparse
import os
import shutil
import sys
import threading

import socketio
import uvicorn
from fastapi import FastAPI, File, UploadFile
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles

from server.mqtt_publisher import MQTTPublisher
from server.match_simulation_engine import MatchSimulationEngine

from server.state_manager import StateManager
from server.udp_receiver import UDPReceiver
from server.udp_sender import UDPSender
from server.video_streamer import VideoStreamer
from server.industrial_camera_streamer import IndustrialCameraStreamer


def getenv_int(name: str, default: int) -> int:
    value = os.getenv(name, "").strip()
    if not value:
        return default
    try:
        return int(value)
    except ValueError:
        return default


# 解析命令行参数
parser = argparse.ArgumentParser(description="RoboMaster Simulator Server")
parser.add_argument(
    "--target-ip",
    type=str,
    default=os.getenv("RM_SIM_TARGET_IP", "127.0.0.1"),
    help="Target IP address for UDP packets (default: 127.0.0.1)",
)
parser.add_argument(
    "--mqtt-host",
    type=str,
    default=os.getenv("RM_SIM_MQTT_HOST", "127.0.0.1"),
    help="MQTT broker host (default: 127.0.0.1)",
)
parser.add_argument(
    "--mqtt-port",
    type=int,
    default=getenv_int("RM_SIM_MQTT_PORT", 1883),
    help="MQTT broker port (default: 1883)",
)
parser.add_argument(
    "--web-port",
    type=int,
    default=getenv_int("RM_SIM_WEB_PORT", 8000),
    help="Web console port (default: 8000)",
)
parser.add_argument(
    "--web-host",
    type=str,
    default=os.getenv("RM_SIM_WEB_HOST", "127.0.0.1"),
    help="Web console bind address (default: 127.0.0.1)",
)
parser.add_argument(
    "--enable-receiver",
    action="store_true",
    default=False,
    help="Enable UDP receiver (binds port 3333, may conflict with client)",
)
parser.add_argument(
    "--enable-udp-sender",
    action="store_true",
    default=os.getenv("RM_SIM_ENABLE_UDP", "").strip() in ("1", "true", "True"),
    help="Enable legacy UDP sender (default: disabled, MQTT-only)",
)
parser.add_argument(
    "--current-robot-id",
    type=int,
    default=getenv_int("RM_SIM_CURRENT_ROBOT_ID", getenv_int("RM_CLIENT_ROBOT_ID", 1)),
    help="Current operator robot ID used for MQTT perspective and current robot data (default: 1)",
)
args = parser.parse_args()

print(f"Target IP: {args.target_ip}")
print(f"MQTT Broker: {args.mqtt_host}:{args.mqtt_port}")
print(f"Web Port: {args.web_port}")
print(f"Current Robot ID: {args.current_robot_id}")
print(f"UDP Receiver: {'已启用' if args.enable_receiver else '已禁用 (默认)'}")
print(f"UDP Sender: {'已启用' if args.enable_udp_sender else '已禁用 (MQTT模式)'}")

# 初始化核心组件
# StateManager: 管理比赛状态（分数、血量、阶段等）
state_manager = StateManager()
state_manager.current_robot_id = args.current_robot_id
# MQTTPublisher（可选）：向客户端发布 RobotRespawnStatus 等主题
mqtt_publisher = None
mqtt_started = False
try:
    mqtt_publisher = MQTTPublisher(
        state_manager,
        broker_host=args.mqtt_host,
        broker_port=args.mqtt_port,
        current_robot_id=args.current_robot_id,
    )
    mqtt_started = mqtt_publisher.start()
    if mqtt_started:
        print("MQTTPublisher: started")
    else:
        print("MQTTPublisher: failed to start")
except Exception as e:
    print(f"MQTTPublisher: initialization failed - {e}")
# UDPSender: 负责将状态数据通过 UDP 广播给客户端
# 当 MQTT broker 不可用时，自动回退到 UDP，避免 Web 控制台只能改内存状态但客户端无任何反应。
udp_sender = None
enable_udp_sender = args.enable_udp_sender or not mqtt_started
if enable_udp_sender:
    udp_sender = UDPSender(
        state_manager, host=args.target_ip, current_robot_id=args.current_robot_id
    )
    if not mqtt_started and not args.enable_udp_sender:
        print("UDP Sender: MQTT unavailable, fallback enabled on port 3333")
# UDPReceiver: 负责接收客户端的指令 (可选，默认禁用以避免端口冲突)
udp_receiver = None
if args.enable_receiver:
    udp_receiver = UDPReceiver(state_manager)
# VideoStreamer: 负责视频流的模拟和发送
video_streamer = VideoStreamer(target_ip=args.target_ip)
# IndustrialCameraStreamer：通过 MQTT 推送工业相机 H.264 码流，供 HeroVideoWidget 使用
industrial_camera_streamer = IndustrialCameraStreamer(mqtt_publisher=mqtt_publisher)
# MatchSimulationEngine 以 10 Hz 推进可重复的一键赛事演示时间线。
# 默认保持空闲，不会影响原有手动控制。
match_simulation_engine = MatchSimulationEngine(
    state_manager=state_manager,
    mqtt_publisher=mqtt_publisher,
)

# 启动后台任务线程
state_manager.start()
if udp_sender:
    udp_sender.start()
if udp_receiver:
    udp_receiver.start()
video_streamer.start()
industrial_camera_streamer.start()


def _build_console_state():
    """构造 Web 控制台统一状态，确保视频与工业相机状态总是一起返回。"""
    state = state_manager.get_state()
    state["videos"] = video_streamer.get_available_videos()
    state["videoStatus"] = video_streamer.get_status()
    state["industrialCameraStatus"] = industrial_camera_streamer.get_status()
    return state


def _normalize_team_name(team: object) -> str:
    return "blue" if str(team).strip().lower() == "blue" else "red"


def _current_operator_team() -> str:
    current_robot_id = getattr(state_manager, "current_robot_id", args.current_robot_id)
    return "blue" if int(current_robot_id or 1) >= 100 else "red"


def _get_team_base_health(global_unit_status: dict, team: str) -> int:
    team_name = _normalize_team_name(team)
    if team_name == "blue":
        return int(
            global_unit_status.get("blue_base_health", global_unit_status.get("base_health", 0))
        )
    return int(
        global_unit_status.get("red_base_health", global_unit_status.get("base_health", 0))
    )


def _normalize_rune_type(rune_type: object) -> int:
    try:
        value = int(rune_type)
    except Exception:
        text = str(rune_type or "").strip().lower()
        return 2 if text in {"large", "big", "2"} else 1
    return 2 if value == 2 else 1

# 初始化 FastAPI 和 SocketIO
# 创建异步 SocketIO 服务器，允许跨域
sio = socketio.AsyncServer(async_mode="asgi", cors_allowed_origins="*")
# 创建 FastAPI 应用
app = FastAPI()

# 配置 CORS 中间件，允许所有来源访问
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# 将 SocketIO 应用挂载到 FastAPI
socket_app = socketio.ASGIApp(sio, app)

# 挂载静态文件目录，用于提供 Web 控制台页面
static_dir = os.path.join(os.path.dirname(__file__), "static")
project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
video_resource_dir = os.path.join(project_root, "resources", "videos")
app.mount("/", StaticFiles(directory=static_dir, html=True), name="static")


@app.post("/upload")
async def upload_video(file: UploadFile = File(...)):
    """
    处理视频文件上传接口
    将上传的视频保存到 resources/videos 目录，并通知前端更新视频列表
    """
    try:
        os.makedirs(video_resource_dir, exist_ok=True)
        safe_name = os.path.basename(file.filename)
        file_location = os.path.join(video_resource_dir, safe_name)
        with open(file_location, "wb+") as file_object:
            shutil.copyfileobj(file.file, file_object)

        # 广播完整状态，避免前端把局部 update 当成全量状态导致页面崩溃。
        await sio.emit("update", _build_console_state())

        rel_path = os.path.relpath(file_location, project_root)
        return {"info": f"文件 '{safe_name}' 已保存至 '{rel_path}'"}
    except Exception as e:
        return {"error": str(e)}


@sio.event
async def connect(sid, environ):
    """
    SocketIO 客户端连接事件
    当 Web 控制台连接时，发送当前的完整状态和视频列表
    """
    print(f"客户端已连接: {sid}")
    await sio.emit("update", _build_console_state(), to=sid)
    await sio.emit(
        "simulation_status",
        match_simulation_engine.get_status(),
        to=sid,
    )


@sio.event
async def disconnect(sid):
    """
    SocketIO 客户端断开连接事件
    """
    print(f"客户端已断开: {sid}")


@sio.event
async def simulation_control(sid, data):
    """控制可选的确定性赛事演示引擎。"""
    payload = data if isinstance(data, dict) else {}
    try:
        status = match_simulation_engine.control(
            action=payload.get("action"),
            mode=payload.get("mode"),
            speed=payload.get("speed"),
            features=payload.get("features"),
        )
    except (TypeError, ValueError) as exc:
        status = match_simulation_engine.get_status()
        status["error"] = str(exc)
        await sio.emit("simulation_status", status, to=sid)
        return status

    state_manager.log(
        "MatchSimulation",
        "Control: action=%s mode=%s speed=%s status=%s"
        % (
            payload.get("action"),
            status.get("mode"),
            status.get("speed"),
            status.get("status"),
        ),
        type="command",
    )
    await sio.emit("simulation_status", status)
    await sio.emit("update", _build_console_state())
    return status


@sio.event
async def referee_command(sid, data):
    """
    处理来自 Web 控制台的裁判指令
    根据指令类型调用相应的组件方法
    """
    cmd = data.get("command")
    print(f"收到指令: {cmd}, 数据: {data}")
    state_manager.log("WebConsole", f"Command: {cmd}", type="command")

    if cmd == "start":
        state_manager.start_match()  # 开始比赛
    elif cmd == "pause":
        state_manager.pause_match()  # 暂停比赛
    elif cmd == "resume":
        state_manager.resume_match()  # 恢复比赛
    elif cmd == "end":
        state_manager.end_match()  # 结束比赛
    elif cmd == "set_hp":
        # 设置机器人血量
        state_manager.set_robot_hp(data.get("robot_id"), data.get("hp"))
    elif cmd == "set_base_hp":
        # 设置基地血量
        team = _normalize_team_name(data.get("team", "red"))
        before_status = state_manager.get_state().get("globalUnitStatus", {})
        previous_hp = _get_team_base_health(before_status, team)
        next_hp = int(data.get("hp", previous_hp))
        state_manager.set_base_hp(next_hp, team)
        if team == _current_operator_team() and previous_hp > 0 and next_hp <= 0:
            state_manager.log(
                "Referee",
                f"Ally base destroyed: team={team.upper()}, hp={previous_hp} -> {next_hp}, event=11",
            )
            if mqtt_publisher:
                mqtt_publisher.publish_event(11, "")
    elif cmd == "adjust_base_hp":
        # 基地血量增减（推荐 50 步长）
        team = _normalize_team_name(data.get("team", "red"))
        before_status = state_manager.get_state().get("globalUnitStatus", {})
        previous_hp = _get_team_base_health(before_status, team)
        delta = int(data.get("delta", 0))
        next_hp = max(0, previous_hp + delta)
        state_manager.adjust_base_hp(delta, team)
        if team == _current_operator_team() and previous_hp > 0 and next_hp <= 0:
            state_manager.log(
                "Referee",
                f"Ally base destroyed: team={team.upper()}, hp={previous_hp} -> {next_hp}, event=11",
            )
            if mqtt_publisher:
                mqtt_publisher.publish_event(11, "")
    elif cmd == "set_outpost_hp":
        # 设置前哨站血量
        state_manager.set_outpost_hp(data.get("hp"), data.get("team", "red"))
    elif cmd == "adjust_outpost_hp":
        # 增减前哨站血量（用于测试骤降检测）
        state_manager.adjust_outpost_hp(data.get("team", "red"), data.get("delta", 0))
    elif cmd == "set_outpost_status":
        # 设置前哨战状态码
        state_manager.set_outpost_status(data.get("status", 1), data.get("team", "red"))
    elif cmd == "adjust_robot_hp":
        # 增减机器人血量
        state_manager.adjust_robot_hp(data.get("robot_id"), data.get("delta", 0))
    elif cmd == "set_score":
        # 设置比分
        state_manager.set_score(data.get("team", "red"), data.get("score", 0))
    elif cmd == "adjust_score":
        # 比分增减
        state_manager.adjust_score(data.get("team", "red"), data.get("delta", 0))
    elif cmd == "set_round_config":
        state_manager.set_round_config(
            total_rounds=data.get("total_rounds"),
            current_round=data.get("current_round"),
        )
    elif cmd == "set_match_stage":
        state_manager.set_match_stage(
            data.get("stage", 0),
            data.get("countdown_sec"),
        )
    elif cmd == "set_economy":
        # 设置经济
        state_manager.set_economy(data.get("team", "red"), data.get("economy", 0))
    elif cmd == "adjust_economy":
        # 经济增减
        state_manager.adjust_economy(data.get("team", "red"), data.get("delta", 0))
    elif cmd == "set_logistics_protocol_state":
        state_manager.set_logistics_protocol_state(
            data.get("team", "red"),
            total_economy_obtained=data.get("total_economy_obtained"),
            total_damage=data.get("total_damage"),
            tech_level=data.get("tech_level"),
            encryption_level=data.get("encryption_level"),
        )
    elif cmd == "set_position":
        # 设置机器人位置
        was_simulation_running, simulation_status = (
            match_simulation_engine.pause_for_manual_override()
        )
        robot_index = data.get("robot_id")
        x = data.get("x")
        y = data.get("y")
        angle = data.get("angle")

        # 拖拽位置先写入统一状态，再立即通过 MQTT 发布雷达位置。
        state_manager.set_robot_position(robot_index, x, y, angle)

        if mqtt_publisher:
            angle = 0.0
            # 当前模拟器统一使用左下角为原点，前后端保持同一坐标系。
            y_for_mqtt = y
            try:
                idx = int(robot_index)
                x = float(x)
                y = float(y)
                y_for_mqtt = y
                poses = state_manager.get_positions()
                if 0 <= idx < len(poses):
                    angle = float(poses[idx].get("angle", 0.0))
            except Exception:
                angle = 0.0
                y_for_mqtt = y

            ok = mqtt_publisher.publish_radar_info_to_client(
                robot_index=robot_index,
                x=x,
                y=y_for_mqtt,
                angle=angle,
                is_high_light=0,
            )

            print(
                f"set_position mqtt publish: radar_ok={ok} "
                f"pose={{'robot_index': {robot_index}, 'x': {x}, 'y_web': {y}, 'y_mqtt': {y_for_mqtt}, 'angle': {angle}}}"
            )
        if was_simulation_running:
            state_manager.log(
                "MatchSimulation",
                "Paused because a robot position was changed manually",
            )
            await sio.emit("simulation_status", simulation_status)
    elif cmd == "penalty":
        # 实施判罚
        state_manager.apply_penalty(data.get("robot_id"), data.get("damage"))
    elif cmd == "referee_warning":
        # 裁判警告（黄牌/红牌/双方黄牌）
        state_manager.issue_referee_warning(data.get("level", 2), data.get("robot_id"))
        if mqtt_publisher:
            # 触发式发送 PenaltyInfo，立即通知客户端
            mqtt_publisher.publish_pending_warnings()
    elif cmd == "set_robot_detail":
        # 设置机器人详细状态（等级、热量、功率、射速）
        state_manager.set_robot_detail(
            data.get("robot_id"),
            data.get("level"),
            data.get("heat"),
            data.get("power"),
            data.get("fire_rate", 0),
            data.get("bullets", 0),
            data.get("hold_heat", False),
        )
        if mqtt_publisher:
            mqtt_publisher.publish_robot_static_status(state_manager.get_state())
    elif cmd == "set_tech_core_motion_state":
        state_manager.set_tech_core_motion_state(
            maximum_difficulty_level=data.get("maximum_difficulty_level"),
            basic_state=data.get("basic_state"),
            status=data.get("status"),
            putin_state=data.get("putin_state"),
            move_state=data.get("move_state"),
            rotate_state=data.get("rotate_state"),
            enemy_core_status=data.get("enemy_core_status"),
            remain_time_all=data.get("remain_time_all"),
            remain_time_step=data.get("remain_time_step"),
        )
        if data.get("enemy_core_status") == 1 :
            if mqtt_publisher:
                mqtt_publisher.publish_event(14, "")

    elif cmd == "set_global_special_mechanism":
        state_manager.set_global_special_mechanism(
            data.get("ally_fortress_sec", 0),
            data.get("enemy_fortress_sec", 0),
        )

    elif cmd == "set_robot_injury_stat":
        state_manager.set_robot_injury_stat(
            data.get("robot_id", args.current_robot_id),
            total_damage=data.get("total_damage"),
            collision_damage=data.get("collision_damage"),
            small_projectile_damage=data.get("small_projectile_damage"),
            large_projectile_damage=data.get("large_projectile_damage"),
            dart_splash_damage=data.get("dart_splash_damage"),
            module_offline_damage=data.get("module_offline_damage"),
            offline_damage=data.get("offline_damage"),
            penalty_damage=data.get("penalty_damage"),
            server_kill_damage=data.get("server_kill_damage"),
            killer_id=data.get("killer_id"),
        )

    elif cmd == "set_robot_path_plan":
        state_manager.set_robot_path_plan(
            intention=data.get("intention", 0),
            start_pos_x=data.get("start_pos_x", 0),
            start_pos_y=data.get("start_pos_y", 0),
            offset_x=data.get("offset_x", []),
            offset_y=data.get("offset_y", []),
            sender_id=data.get("sender_id", 0),
        )
        if mqtt_publisher:
            mqtt_publisher._publish_robot_path_plan_info(state_manager.get_state())
    elif cmd == "send_robot_static_status":
        state_manager.set_robot_static_status(data)
        if mqtt_publisher:
            mqtt_publisher.publish_robot_static_status(state_manager.get_state())
    elif cmd == "send_robot_dynamic_status":
        state_manager.set_robot_dynamic_status(data)
        if mqtt_publisher:
            mqtt_publisher.publish_robot_dynamic_status(state_manager.get_state())
    elif cmd == "send_robot_module_status":
        state_manager.set_robot_module_status(data)
        if mqtt_publisher:
            mqtt_publisher.publish_robot_module_status(state_manager.get_state())
    elif cmd == "set_robot_performance_selection_sync":
        state_manager.set_robot_performance_selection_sync(
            shooter=data.get("shooter", 1),
            chassis=data.get("chassis", 1),
            sentry_control=data.get("sentry_control", 0),
        )

    elif cmd == "set_sentry_status_sync":
        state_manager.set_sentry_status_sync(
            posture_id=data.get("posture_id", 0),
            is_weakened=data.get("is_weakened", False),
            is_powered=data.get("is_powered", False),
        )

    elif cmd == "send_sentry_ctrl_result":
        command_id = int(data.get("command_id", 0))
        result_code = int(data.get("result_code", 0))
        if result_code == 0:
            sentry_effect = state_manager.apply_sentry_command(command_id)
            result_code = int(sentry_effect.get("result_code", 0))
        state_manager.push_sentry_ctrl_result(
            command_id=command_id,
            result_code=result_code,
        )
        if mqtt_publisher:
            mqtt_publisher.publish_sentry_ctrl_result(
                command_id=command_id,
                result_code=result_code,
            )

    elif cmd == "send_custom_byte_block":
        try:
            encoding = str(data.get("encoding", "hex")).strip().lower()
            if encoding == "utf8":
                payload_bytes = str(data.get("text_data", "")).encode("utf-8")
            else:
                hex_data = str(data.get("hex_data", "")).replace(",", " ")
                hex_data = "".join(hex_data.split())
                payload_bytes = bytes.fromhex(hex_data) if hex_data else b""
            state_manager.set_custom_byte_block(
                payload_bytes,
                encoding=encoding,
                hex_data=data.get("hex_data", ""),
                text_data=data.get("text_data", ""),
            )
            if mqtt_publisher:
                mqtt_publisher.publish_custom_byte_block(payload_bytes)
        except Exception as exc:
            state_manager.log("Referee", f"Publish CustomByteBlock failed: {exc}", "error")
    elif cmd == "send_event":
        try:
            event_id = int(data.get("event_id", 0))
            param = str(data.get("param", ""))
            state_manager.set_last_event_command(event_id, param)
            state_manager.log_event_publish(event_id, param)
            if mqtt_publisher:
                mqtt_publisher.publish_event(event_id, param)
        except Exception as exc:
            state_manager.log("Referee", f"Publish Event failed: {exc}", "error")
    elif cmd == "send_buff":
        try:
            robot_id = int(data.get("robot_id", args.current_robot_id))
            if robot_id <= 0:
                robot_id = int(getattr(state_manager, "current_robot_id", args.current_robot_id) or args.current_robot_id)
            buff_type = int(data.get("buff_type", 0))
            buff_level = int(data.get("buff_level", 1))
            buff_max_time = int(data.get("buff_max_time", 0))
            buff_left_time = int(data.get("buff_left_time", 0))

            state_manager.set_buff_status(
                robot_id,
                buff_type,
                buff_level,
                buff_max_time,
                buff_left_time,
            )
            state_manager.log_buff_publish(
                robot_id,
                buff_type,
                buff_level,
                buff_max_time,
                buff_left_time,
            )
            if mqtt_publisher:
                if buff_left_time != 0:
                    mqtt_publisher.publish_buff(
                        robot_id,
                        buff_type,
                        buff_level,
                        buff_max_time,
                        buff_left_time,
                    )
        except Exception as exc:
            state_manager.log("Referee", f"Publish Buff failed: {exc}", "error")
    elif cmd == "set_deploy_mode_status":
        try:
            status = 1 if int(data.get("status", 0)) == 1 else 0
            team = _normalize_team_name(data.get("team", _current_operator_team()))
            state_manager.set_deploy_mode_status(status, team)
            state_manager.log_deploy_mode_publish(status, team=team)
            if mqtt_publisher:
                try:
                    mqtt_publisher.publish_deploy_mode_status_sync(status, team=team)
                except Exception as exc:
                    state_manager.log("Referee", f"Publish DeployModeStatusSync via MQTT failed: {exc}", "error")
        except Exception as exc:
            state_manager.log(
                "Referee",
                f"Publish DeployModeStatusSync failed: {exc}",
                "error",
            )
    elif cmd == "set_stage_countdown":
        state_manager.set_stage_countdown(data.get("seconds", 0))
    elif cmd == "set_base_protocol_state":
        # 对方护甲展开 status为2的时候 发送event=13
        team = _normalize_team_name(data.get("team", "red"))
        if team != _current_operator_team() and data.get("status") == 2:
            if mqtt_publisher:
                mqtt_publisher.publish_event(13, "")
        state_manager.set_base_protocol_state(
            data.get("team", "red"),
            status=data.get("status"),
            shield=data.get("shield"),
        )

    elif cmd == "skip_to_battle":
        state_manager.skip_to_battle()
    elif cmd == "set_video":
        # 切换当前播放的视频文件
        video_streamer.set_video_file(data.get("filename"))
    elif cmd == "video_control":
        # 视频推流控制（开始、暂停、停止）
        action = data.get("action")
        if action == "start":
            video_streamer.start_streaming()
        elif action == "pause":
            video_streamer.pause()
        elif action == "resume":
            video_streamer.resume()
        elif action == "stop":
            video_streamer.stop_streaming()
    elif cmd == "set_custom_url":
        # 设置自定义视频源 URL/路径
        url = data.get("url", "")
        if url:
            video_streamer.set_video_file(url)
    elif cmd == "set_current_robot":
        previous_robot_id = int(
            getattr(state_manager, "current_robot_id", args.current_robot_id)
            or args.current_robot_id
        )
        try:
            robot_id = int(data.get("robot_id", args.current_robot_id))
        except Exception:
            robot_id = int(args.current_robot_id)
        if state_manager.robot_ids and robot_id not in state_manager.robot_ids:
            robot_id = state_manager.robot_ids[0]
        simulation_status = match_simulation_engine.get_status()
        crosses_team = (previous_robot_id >= 100) != (robot_id >= 100)
        if crosses_team and simulation_status.get("status") in ("running", "paused"):
            robot_id = previous_robot_id
            blocked_status = dict(simulation_status)
            blocked_status["message"] = (
                "赛事演示运行期间不能跨阵营切换当前机器人，请先停止并复位"
            )
            state_manager.log(
                "MatchSimulation",
                "Rejected cross-team operator switch while demonstration is active",
                type="error",
            )
            await sio.emit("simulation_status", blocked_status, to=sid)
        args.current_robot_id = robot_id
        state_manager.current_robot_id = robot_id
        static_status = state_manager.get_robot_static_status()
        if int(static_status.get("robot_id", previous_robot_id) or previous_robot_id) == previous_robot_id:
            static_status["robot_id"] = robot_id
            static_status["robot_type"] = int(robot_id) % 100
            state_manager.set_robot_static_status(static_status)
        if mqtt_publisher:
            mqtt_publisher.current_robot_id = robot_id
        if udp_sender:
            udp_sender.current_robot_id = robot_id
        state_manager.log(
            "WebConsole",
            "Current robot set: "
            f"robot_id={robot_id}, "
            f"state_manager={state_manager.current_robot_id}, "
            f"mqtt={getattr(mqtt_publisher, 'current_robot_id', robot_id)}, "
            f"udp={getattr(udp_sender, 'current_robot_id', robot_id) if udp_sender else 'disabled'}",
        )
    elif cmd == "set_industrial_video":
        # 切换工业相机当前播放的视频文件
        industrial_camera_streamer.set_video_file(data.get("filename"))
    elif cmd == "industrial_camera_control":
        # 工业相机推流控制（开始、暂停、恢复、停止）
        action = data.get("action")
        if action == "start":
            industrial_camera_streamer.start_streaming()
        elif action == "pause":
            industrial_camera_streamer.pause()
        elif action == "resume":
            industrial_camera_streamer.resume()
        elif action == "stop":
            industrial_camera_streamer.stop_streaming()
    elif cmd == "kick_all":
        state_manager.kick_all()
    elif cmd == "reset_all":
        state_manager.reset_all()
    #能量机关
    elif cmd == "activate_rune":
        team_name = data.get("team", "red")
        rune_type = _normalize_rune_type(data.get("rune_type", 0))
        state_manager.activate_rune(rune_type, team_name)
    elif cmd == "set_rune_metrics":
        team_name = data.get("team", "red")
        rune_type = _normalize_rune_type(data.get("rune_type", 0))
        state_manager.set_rune_metrics(
            data.get("activated_arms", 0),
            data.get("average_rings", 0),
            team_name,
        )
        if mqtt_publisher:
            # 发送event=3
            arms = data.get("activated_arms", 0)
            rings = data.get("average_rings", 0)
            # 用逗号分隔拼接成字符串
            mqtt_publisher.publish_event(3, f"{arms},{rings}")
    elif cmd == "finish_activate_rune":
        team_name = data.get("team", "red")
        rune_type = _normalize_rune_type(data.get("rune_type", 1))
        state_manager.finish_activate_rune(rune_type, team_name)
        if mqtt_publisher:
            mqtt_publisher.publish_event(4, f"{rune_type}")

    elif cmd == "reset_activate_rune":
        team_name = data.get("team", "red")
        rune_type = _normalize_rune_type(data.get("rune_type", 0))
        state_manager.reset_activate_rune(rune_type, team_name)
    elif cmd == "set_dart_target":
        target_id = int(data.get("target_id", 2))
        team_name = _normalize_team_name(data.get("team", "red"))
        current_dart_status = state_manager.get_dart_status(team_name)
        state_manager.set_dart_status_sync(
            target_id,
            int(current_dart_status.get("open", 0)),
            team_name,
        )
        if mqtt_publisher:
            hit_team = 1 if team_name == "red" else 2
            mqtt_publisher.publish_event(9, f"{hit_team},{target_id}")
    elif cmd in ("set_dart_gate_status", "set_ally_dart_gate_status"):
        target_id = int(data.get("target_id", 2))
        open_state = int(data.get("open_state", 2))
        team_name = _normalize_team_name(data.get("team", "red"))
        state_manager.set_dart_status_sync(target_id, open_state, team_name)
        state_manager.log(
            "Referee",
            f"{str(team_name).upper()} dart gate status sync: target_id={target_id}, open={open_state}",
        )
        if mqtt_publisher:
            if open_state == 2 and team_name != _current_operator_team():
                mqtt_publisher.publish_event(10, "")
    elif cmd == "trigger_base_under_attack":
        state_manager.log("Referee", "Base under attack event sent")
        if mqtt_publisher:
            mqtt_publisher.publish_event(11, "")
    elif cmd == "trigger_outpost_stopped":
        team = _normalize_team_name(data.get("team", "red"))
        if team != _current_operator_team() :
            if mqtt_publisher:
                mqtt_publisher.publish_event(12, "")
        # 更新前哨战状态为2（停转状态）
        state_manager.set_outpost_status(2, team)
    elif cmd == "trigger_air_support_countered":
        team = str(data.get("team", "red")).strip().lower()
        state_manager.log("Referee", f"{team.upper()} air support countered event sent")
        did_counter = state_manager.counter_air_support(team)
        team = _normalize_team_name(data.get("team", "red"))
        if did_counter and team != _current_operator_team():
            if mqtt_publisher:
                mqtt_publisher.publish_event(8, "1")

    elif cmd == "trigger_hero_snipe":
        robot_id = int(data.get("robot_id", 1))
        state_manager.log("Referee", f"Hero {robot_id} snipe event sent")
        if mqtt_publisher:
            # 英雄狙击事件，使用event id 5、6
            team = _normalize_team_name(data.get("team", "red"))
            if team == _current_operator_team() :
                if mqtt_publisher:
                    mqtt_publisher.publish_event(5, "100") #默认造成100狙击伤害
            else:
                if mqtt_publisher:
                    mqtt_publisher.publish_event(6, "100") #默认造成100狙击伤害
    elif cmd == "set_air_support_status_sync":
        team = str(data.get("team", "red")).strip().lower()
        state_manager.set_air_support_status_sync(
            team=team,
            airsupport_status=data.get("airsupport_status"),
            left_time=data.get("left_time"),
            cost_coins=data.get("cost_coins"),
            is_being_targeted=data.get("is_being_targeted"),
            shooter_status=data.get("shooter_status"),
        )


    # 指令执行后立即广播最新状态
    if cmd == "start_air_support":
        team = str(data.get("team", "red")).strip().lower()
        state_manager.start_air_support(
            team,
            data.get("mode", "free"),
            data.get("duration_sec"),
        )
        if mqtt_publisher:
            # 对方空中支援才发送7
            if team != _current_operator_team():
                mqtt_publisher.publish_event(7, "")
    elif cmd == "set_air_support_duration":
        team = str(data.get("team", "red")).strip().lower()
        state_manager.set_air_support_duration(team, data.get("duration_sec", 30))
    elif cmd == "reset_air_support_cost":
        team = str(data.get("team", "red")).strip().lower()
        state_manager.reset_air_support_cost(team)
    elif cmd == "stop_air_support":
        state_manager.stop_air_support(data.get("team"))


    await sio.emit("update", _build_console_state())


@sio.event
async def common_command(sid, data):
    """
    处理来自客户端的 CommonCommand（用于 QML 中的 sendCommonCommand）
    预期 data: {"cmd_type": int, "param": int}
    cmd_type 4 -> exchange_immediate_respawn
    cmd_type 3 -> confirm respawn (if completed)
    """
    try:
        cmd_type = int(data.get("cmd_type", -1))
        param = data.get("param", None)
    except Exception:
        await sio.emit(
            "common_command_response",
            {"success": False, "error": "invalid_payload"},
            to=sid,
        )
        return

    # 处理命令
    if cmd_type == 4:
        # 尝试将 param 解析为 robot_index 或 robot_id
        robot_index = None
        try:
            robot_index = int(param)
        except Exception:
            robot_index = None
        if robot_index is not None and robot_index > 50:
            try:
                robot_index = state_manager.robot_ids.index(robot_index)
            except Exception:
                robot_index = None

        success = False
        try:
            if robot_index is None:
                # 找到第一个 pending 的机器人
                for rid, info in state_manager.respawn_info.items():
                    if info.get("is_pending"):
                        robot_index = state_manager.robot_ids.index(rid)
                        break
            if robot_index is not None:
                success = state_manager.exchange_immediate_respawn(robot_index)
        except Exception as e:
            state_manager.log("SocketIO", f"common_command exchange error: {e}")

        await sio.emit("common_command_response", {"success": bool(success)}, to=sid)
        await sio.emit("update", _build_console_state())

    elif cmd_type == 3:
        # 确认复活：若完成则触发
        robot_index = None
        try:
            robot_index = int(param)
        except Exception:
            robot_index = None
        if robot_index is not None and robot_index > 50:
            try:
                robot_index = state_manager.robot_ids.index(robot_index)
            except Exception:
                robot_index = None

        applied = False
        try:
            if robot_index is not None:
                rid = state_manager.robot_ids[robot_index]
                info = state_manager.respawn_info.get(rid, {})
                if info.get("is_pending") and info.get(
                    "current_progress", 0
                ) >= info.get("total_progress", 0):
                    state_manager.global_unit_status["robot_health"][robot_index] = int(
                        state_manager._default_robot_max_hp * 0.10
                    )
                    info["is_dead"] = False
                    info["is_pending"] = False
                    info["current_progress"] = info.get("total_progress", 0)
                    state_manager.event_queue.append(
                        {
                            "id": 201,
                            "robot_id": rid,
                            "robot_index": robot_index,
                            "is_pending": False,
                            "type": "robot_respawn_status",
                        }
                    )
                    state_manager.log(
                        "SocketIO",
                        f"common_command confirm respawn applied robot_index={robot_index}",
                    )
                    applied = True
        except Exception as e:
            state_manager.log("SocketIO", f"common_command confirm error: {e}")

        await sio.emit("common_command_response", {"success": bool(applied)}, to=sid)
        await sio.emit("update", _build_console_state())

    else:
        await sio.emit(
            "common_command_response",
            {"success": False, "error": "unsupported_cmd"},
            to=sid,
        )


# 后台状态广播循环
async def broadcast_state():
    """
    定期向 Web 控制台广播当前状态 (2Hz)
    """
    while True:
        await sio.sleep(0.5)
        await sio.emit("update", _build_console_state())


async def run_match_simulation():
    """以 10 Hz 推进并广播可选的一键赛事演示。"""
    last_status = match_simulation_engine.get_status().get("status")
    while True:
        await sio.sleep(1.0 / MatchSimulationEngine.TICK_HZ)
        try:
            frame = match_simulation_engine.tick()
            if frame is not None:
                await sio.emit("simulation_frame", frame)

            status = match_simulation_engine.get_status()
            current_status = status.get("status")
            if current_status != last_status:
                await sio.emit("simulation_status", status)
            last_status = current_status
        except Exception as exc:
            state_manager.log(
                "MatchSimulation",
                f"Engine tick failed: {exc}",
                "error",
            )
            match_simulation_engine.pause(reason="engine_error")
            await sio.emit(
                "simulation_status",
                match_simulation_engine.get_status(),
            )


@app.on_event("startup")
async def startup_event():
    """
    FastAPI 启动事件
    启动后台广播任务
    """
    sio.start_background_task(broadcast_state)
    sio.start_background_task(run_match_simulation)


if __name__ == "__main__":
    # 启动 uvicorn 服务器
    uvicorn.run(socket_app, host=args.web_host, port=args.web_port)
