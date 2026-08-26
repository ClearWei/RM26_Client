#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: MIT
"""
视频推流模拟器 (HEVC版)

@file video_streamer.py
@brief 模拟图传视频推流，将本地视频通过UDP发送给客户端
@author Fudan EGA Team
@date 2026-02-01
@copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).

功能说明:
    - 使用 FFmpeg 将视频转码为 HEVC 格式
    - 通过 UDP 3334 端口发送，遵循官方协议
    - 支持手动开始/停止推流 (不再自动启动)
    - 支持自定义视频源和上传视频

官方协议规范:
    UDP 端口: 3334
    编码格式: HEVC (H.265)
    包头格式 (8 字节):
        帧编号（2 字节，递增）
        分片序号（2 字节）
        当前帧总字节数（4 字节）
"""

import socket
import struct
import threading
import time
import os
import json
import subprocess
import queue
from pathlib import Path

from .hevc_annexb import HevcAnnexBFrameExtractor

class VideoStreamer:
    """
    HEVC 视频流推流器

    使用 FFmpeg 将视频转码为 HEVC 格式，通过 UDP 发送给客户端。
    遵循官方协议：8字节包头 + HEVC 数据
    """

    # UDP 最佳负载大小 (MTU 1500 - IP头20 - UDP头8 - 协议头8)
    MAX_PAYLOAD_SIZE = 1464

    # 包头格式: 帧编号(H 2B), 分片序号(H 2B), 帧总字节数(I 4B)
    # 使用大端序 (!)
    HEADER_FORMAT = "!HHI"
    HEADER_SIZE = 8  # 2 + 2 + 4 = 8 字节

    def __init__(self, target_ip="127.0.0.1", port=3334, video_dir=None):
        """
        初始化推流器
        :param target_ip: 目标 IP 地址 (客户端)
        :param port: 目标端口 (官方协议: 3334)
        :param video_dir: 视频文件目录
        """
        self.target_ip = target_ip
        self.port = port
        self.script_dir = Path(__file__).resolve().parent
        self.project_root = self.script_dir.parent.parent
        self.video_dir = self._resolve_path(video_dir or "sim/server/video")

        # 加载配置（统一使用项目根 config.json）
        self.config_path = self.project_root / "config.json"
        self.video_resource_path = self._resolve_path("resources/videos")
        self._load_config()

        # 状态控制
        self.current_video = ""  # 当前视频源 (空表示未设置)
        self.is_streaming = False  # 是否正在推流
        self.running = False  # 后台线程运行标志
        self.paused = False  # 暂停状态

        # FFmpeg 进程
        self.ffmpeg_process = None

        # UDP 套接字
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 1024 * 1024)

        # 帧计数器
        self.frame_id = 0
        self.frame_extractor = HevcAnnexBFrameExtractor()

        # 日志回调
        self.log_callback = None

    def _load_config(self):
        """加载配置文件"""
        try:
            if self.config_path.exists():
                with open(self.config_path, 'r', encoding='utf-8') as f:
                    config = json.load(f)
                    if "video_resource_path" in config:
                        self.video_resource_path = self._resolve_path(config["video_resource_path"])
                        print(f"[VideoStreamer] 已加载视频资源路径: {self.video_resource_path}")
        except Exception as e:
            print(f"[VideoStreamer] 加载配置文件失败: {e}")

    def _resolve_path(self, input_path, base_dir=None):
        """将输入路径标准化为可跨平台运行的绝对路径。"""
        if not input_path:
            return ""

        path = Path(str(input_path)).expanduser()
        if path.is_absolute():
            return str(path)

        candidates = []
        if base_dir:
            candidates.append(Path(base_dir) / path)
        candidates.append(self.project_root / path)
        candidates.append(self.script_dir / path)

        for candidate in candidates:
            if candidate.exists():
                return str(candidate.resolve())

        # 文件不存在时，优先返回项目根目录下的目标路径，避免依赖当前工作目录
        return str((self.project_root / path).resolve())

    def _log(self, message):
        """输出日志"""
        print(f"[VideoStreamer] {message}")
        if self.log_callback:
            self.log_callback("VideoStreamer", message, "video")

    def start(self):
        """
        启动后台线程 (但不开始推流)
        推流需要调用 start_streaming()
        """
        if self.running:
            return
        self.running = True
        threading.Thread(target=self._stream_loop, daemon=True).start()
        self._log("后台线程已启动 (等待开始推流指令)")

    def stop(self):
        """停止推流并关闭资源"""
        self.stop_streaming()
        self.running = False
        if self.sock:
            self.sock.close()
        self._log("推流器已完全停止")

    def start_streaming(self):
        """开始推流 (由 Web 界面手动触发)"""
        if not self.current_video:
            self._log("错误: 未设置视频源")
            return False

        if not os.path.exists(self.current_video):
            self._log(f"错误: 视频文件不存在: {self.current_video}")
            return False

        self.is_streaming = True
        self.paused = False
        self._log(f"开始推流: {self.current_video}")
        return True

    def stop_streaming(self):
        """停止推流"""
        self.is_streaming = False
        self._stop_ffmpeg()
        self._log("推流已停止")

    def pause(self):
        """暂停推流"""
        self.paused = True
        self._log("推流已暂停")

    def resume(self):
        """恢复推流"""
        self.paused = False
        self._log("推流已恢复")

    def set_video_file(self, filename):
        """
        设置视频源
        :param filename: 视频文件名或完整路径
        :return: 成功返回 True
        """
        # 尝试多个路径
        input_name = (filename or "").strip()
        paths_to_check = [
            self._resolve_path(input_name, base_dir=self.video_resource_path),
            self._resolve_path(input_name, base_dir=self.video_dir),
            self._resolve_path(input_name)  # 直接作为完整路径
        ]

        for path in paths_to_check:
            if os.path.exists(path):
                self.current_video = path
                self._log(f"视频源已设置: {path}")
                return True

        self._log(f"未找到视频文件: {filename}")
        return False

    def get_available_videos(self):
        """获取可用视频列表"""
        videos = []
        dirs = {
            self._resolve_path("resources/videos"),
            self.video_dir,
            self.video_resource_path
        }
        for d in dirs:
            if d and os.path.isdir(d):
                for f in os.listdir(d):
                    if f.endswith(('.mp4', '.avi', '.mov', '.mkv', '.hevc', '.h265')):
                        videos.append(f)
        return list(set(videos))

    def get_status(self):
        """获取当前状态"""
        return {
            "is_streaming": self.is_streaming,
            "paused": self.paused,
            "current_video": os.path.basename(self.current_video) if self.current_video else "",
            "frame_id": self.frame_id
        }

    def _start_ffmpeg(self):
        """启动 FFmpeg 进程进行 HEVC 转码"""
        if self.ffmpeg_process:
            self._stop_ffmpeg()

        # FFmpeg 命令: 读取视频 -> HEVC 编码 -> 输出原始 HEVC 流到 stdout
        # 增加 Annex B 格式支持 (vbsf hevc_mp4toannexb) 确保码流包含必要头信息
        cmd = [
            "ffmpeg",
            "-re",                       # 按原始帧率读取
            "-stream_loop", "-1",        # 循环播放
            "-i", self.current_video,
            "-c:v", "libx265",           # HEVC 编码
            "-preset", "ultrafast",      # 最快编码速度
            "-tune", "zerolatency",      # 零延迟
            "-x265-params", "keyint=15:min-keyint=15:scenecut=0", # 强制固定 I 帧间隔 (0.5s @ 30fps)
            "-bsf:v", "hevc_mp4toannexb", # 强制转换为 Annex B 格式 (包含 VPS/SPS/PPS)
            "-an",                       # 禁用音频
            "-f", "hevc",                # 输出格式为原始 HEVC
            "-"                          # 输出到 stdout
        ]

        try:
            self.ffmpeg_process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                bufsize=10**6
            )
            self.frame_extractor.reset()
            self._log("FFmpeg HEVC 编码器已启动")
            return True
        except FileNotFoundError:
            self._log("错误: FFmpeg 未安装或不在 PATH 中")
            return False
        except Exception as e:
            self._log(f"启动 FFmpeg 失败: {e}")
            return False

    def _stop_ffmpeg(self):
        """停止 FFmpeg 进程"""
        if self.ffmpeg_process:
            try:
                self.ffmpeg_process.terminate()
                self.ffmpeg_process.wait(timeout=2)
            except:
                self.ffmpeg_process.kill()
            self.ffmpeg_process = None
            self._log("FFmpeg 进程已停止")

    def _stream_loop(self):
        """推流主循环"""
        while self.running:
            # 等待开始推流
            if not self.is_streaming:
                time.sleep(0.1)
                continue

            # 暂停状态
            if self.paused:
                time.sleep(0.1)
                continue

            # 启动 FFmpeg (如果尚未启动)
            if not self.ffmpeg_process:
                if not self._start_ffmpeg():
                    self.is_streaming = False
                    continue

            try:
                # 读取连续 HEVC Annex-B 码流，并提取访问单元后发送
                data = self.ffmpeg_process.stdout.read(4096)

                if not data:
                    for frame in self.frame_extractor.flush():
                        self._send_frame(frame)
                    self._log("FFmpeg 输出结束，重启...")
                    self._stop_ffmpeg()
                    continue

                for frame in self.frame_extractor.push(data):
                    self._send_frame(frame)

            except Exception as e:
                self._log(f"推流错误: {e}")
                self._stop_ffmpeg()
                time.sleep(1)

    def _send_frame(self, data):
        """
        发送一帧数据 (按官方协议分片)

        协议头格式 (8字节):
            帧编号（2 字节，递增）
            分片序号（2 字节）
            当前帧总字节数（4 字节）
        """
        data_len = len(data)
        max_chunk_data = self.MAX_PAYLOAD_SIZE - self.HEADER_SIZE
        total_chunks = (data_len + max_chunk_data - 1) // max_chunk_data

        for chunk_id in range(total_chunks):
            start = chunk_id * max_chunk_data
            end = min(start + max_chunk_data, data_len)
            chunk_data = data[start:end]

            # 构建官方协议头 (8字节)
            header = struct.pack(
                self.HEADER_FORMAT,
                self.frame_id % 65536,  # 帧编号 (2B)
                chunk_id,               # 分片序号 (2B)
                data_len                # 帧总字节数 (4B)
            )

            packet = header + chunk_data

            try:
                self.sock.sendto(packet, (self.target_ip, self.port))
            except Exception as e:
                self._log(f"UDP 发送错误: {e}")
                break

        if self.frame_id % 60 == 0:
            self._log(f"已发送帧 {self.frame_id}, 大小 {data_len}B, 分片 {total_chunks}")

        self.frame_id = (self.frame_id + 1) % 65536
