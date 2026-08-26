#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: MIT
"""
工业相机推流模拟器 (H.264 over MQTT 版)

@file industrial_camera_streamer.py
@brief 模拟英雄机器人工业相机，将本地视频转码为 H.264 Annex-B 分片，
        通过 MQTT CustomByteBlock topic 发送给客户端的 HeroVideoWidget。
@author Fudan EGA Team
@date 2026-07-07
@copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).

功能说明:
    - 使用 FFmpeg 将视频转码为 H.264 Annex-B 格式
    - 切成 ~300B 分片，末尾追加 1 字节 packetId (0..255 回绕)
    - 封装成 CustomByteBlock protobuf，发到 MQTT topic "CustomByteBlock"
    - 支持手动开始/停止推流 (与图传控制独立)
    - 复用 resources/videos 目录的视频文件

客户端协议 (与 NetworkManager/VideoReceiver::feedH264Frame 对齐):
    每个 CustomByteBlock.data = [H264 分片字节] + [1 字节 packetId]
    - 末字节 packetId 用于丢包检测 (feedH264Frame: data.back())
    - 前面所有字节为 H264 Annex-B 码流片段 (任意大小均可，H264Decoder 内部累积重组)
    - 无需 303B wrapper (客户端仅在 data.size()==303 且带 0x0a 0xac 0x02 前缀时才剥头，
      其它大小原样处理)
    - H264 码流须含 SPS/PPS/IDR (h264_mp4toannexb 保证)，解码器 reset 后等到 IDR 才出图
"""

import threading
import time
import os
import json
import subprocess
from pathlib import Path


class IndustrialCameraStreamer:
    """
    H.264 over MQTT 工业相机推流器

    使用 FFmpeg 将视频转码为 H.264 Annex-B，切成固定大小分片，
    每片追加 packetId 字节后封装成 CustomByteBlock 发到 MQTT。
    镜像 VideoStreamer 的状态机/线程/FFmpeg 结构，但走 MQTT 而非 UDP。
    """

    # 每个分片的 H264 字节数 (不含末尾 packetId 字节)。
    # 工业相机链路走 CustomByteBlock topic，但业务语义上仍保持“视频上传 -> 协议分片发送”。
    # 默认按协议习惯使用 300B 分片；如需临时调试可通过构造参数覆盖。
    FRAGMENT_SIZE = 300

    # H.264 编码器候选 (按优先级)。客户端 H264Decoder 严格要求真 IDR (NAL type 5)，
    # 某些 libx264 构建版本（如部分 Homebrew 版本）默认启用 open-gop，只产生非 IDR 的 I 帧，
    # 因此把 h264_videotoolbox (macOS) 也列入候选，启动时探测选第一个能产 IDR 的。
    # 每项格式：(编码器名称, 额外 FFmpeg 参数)
    ENCODER_CANDIDATES = [
        ("libx264", [
            "-preset", "ultrafast", "-tune", "zerolatency",
            "-g", "15",
            "-x264-params", "keyint=15:min-keyint=15:scenecut=0:open-gop=0:bframes=0",
        ]),
        ("h264_videotoolbox", [  # macOS 硬件编码，产标准 IDR
            "-g", "15", "-b:v", "500k",
            "-bsf:v", "h264_mp4toannexb",  # videotoolbox 输出 AVCC，需转 Annex-B
        ]),
    ]

    def __init__(self, mqtt_publisher, video_dir=None, fragment_size=None,
                 log_callback=None):
        """
        :param mqtt_publisher: MQTTPublisher 实例 (提供 publish_custom_byte_block)
        :param video_dir: 视频文件目录 (默认 sim/server/video)
        :param fragment_size: 分片大小 (默认 FRAGMENT_SIZE)
        :param log_callback: 可选日志回调 (source, message, category)
        """
        self.mqtt_publisher = mqtt_publisher
        self.fragment_size = self._normalize_fragment_size(fragment_size)

        self.script_dir = Path(__file__).resolve().parent
        self.project_root = self.script_dir.parent.parent
        self.video_dir = self._resolve_path(video_dir or "sim/server/video")

        # 加载配置（统一使用项目根 config.json）
        self.config_path = self.project_root / "config.json"
        self.video_resource_path = self._resolve_path("resources/videos")
        self._load_config()

        # 状态控制 (与 VideoStreamer 保持一致)
        self.current_video = ""      # 当前视频源 (空表示未设置)
        self.is_streaming = False    # 是否正在推流
        self.running = False         # 后台线程运行标志
        self.paused = False          # 暂停状态

        # FFmpeg 进程
        self.ffmpeg_process = None

        # packetId: 0..255 回绕递增 (对应 VideoReceiver 的 m_lastH264PacketId)
        self.packet_id = 0

        # 选定的编码器 (首次推流时探测确定)
        self._chosen_encoder = None  # (encoder_name, extra_args)

        # 日志回调
        self.log_callback = log_callback

    def _load_config(self):
        """加载配置文件中的 video_resource_path"""
        try:
            if self.config_path.exists():
                with open(self.config_path, 'r', encoding='utf-8') as f:
                    config = json.load(f)
                    if "video_resource_path" in config:
                        self.video_resource_path = self._resolve_path(config["video_resource_path"])
        except Exception as e:
            print(f"[IndustrialCameraStreamer] 加载配置文件失败: {e}")

    def _resolve_path(self, input_path, base_dir=None):
        """将输入路径标准化为可跨平台运行的绝对路径 (与 VideoStreamer 一致)。"""
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

        return str((self.project_root / path).resolve())

    def _log(self, message):
        """输出日志"""
        print(f"[IndustrialCameraStreamer] {message}")
        if self.log_callback:
            self.log_callback("IndustrialCameraStreamer", message, "video")

    @classmethod
    def _normalize_fragment_size(cls, fragment_size):
        """规范化分片大小，避免非法值破坏 CustomByteBlock 分片。"""
        try:
            value = int(fragment_size) if fragment_size is not None else cls.FRAGMENT_SIZE
        except (TypeError, ValueError):
            value = cls.FRAGMENT_SIZE
        return max(1, value)

    # ------------------------------------------------------------------ #
    # 生命周期 / 状态机 (镜像 VideoStreamer)
    # ------------------------------------------------------------------ #

    def start(self):
        """启动后台线程 (但不开始推流)。推流需调用 start_streaming()。"""
        if self.running:
            return
        self.running = True
        threading.Thread(target=self._stream_loop, daemon=True).start()
        self._log("后台线程已启动 (等待开始推流指令)")

    def stop(self):
        """停止推流并关闭资源"""
        self.stop_streaming()
        self.running = False
        self._log("推流器已完全停止")

    def start_streaming(self):
        """开始推流 (由 Web 界面手动触发)"""
        if not self.current_video:
            self._log("错误: 未设置视频源")
            return False

        if not os.path.exists(self.current_video):
            self._log(f"错误: 视频文件不存在: {self.current_video}")
            return False

        if not self.mqtt_publisher:
            self._log("错误: MQTT publisher 不可用，无法推流")
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
        """获取可用视频列表 (与 VideoStreamer 扫描相同目录)"""
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
        """获取当前状态。frame_id 用 packet_id (0..255) 供 UI 显示递增计数。"""
        return {
            "is_streaming": self.is_streaming,
            "paused": self.paused,
            "current_video": os.path.basename(self.current_video) if self.current_video else "",
            "frame_id": self.packet_id
        }

    # ------------------------------------------------------------------ #
    # FFmpeg
    # ------------------------------------------------------------------ #

    def _has_idr(self, data: bytes) -> bool:
        """检查 Annex-B H.264 数据中是否含 IDR (NAL type 5)"""
        # 4B 或 3B 起始码后的首字节低 5 位等于 5 时为 IDR
        idx = 0
        while True:
            j4 = data.find(b"\x00\x00\x00\x01", idx)
            j3 = data.find(b"\x00\x00\x01", idx)
            candidates = [x for x in (j4, j3) if x >= 0]
            if not candidates:
                return False
            j = min(candidates)
            sc_len = 4 if j == j4 else 3
            if j + sc_len < len(data) and (data[j + sc_len] & 0x1F) == 5:
                return True
            idx = j + sc_len

    def _choose_encoder(self):
        """探测编码器候选，选定第一个能产出真 IDR (NAL type 5) 的。

        用实际视频文件探测 (非合成源)，因为某些 libx264 build 对真实视频
        不产 IDR、对合成源却产 IDR，必须按真实输入判断。
        """
        for encoder, extra_args in self.ENCODER_CANDIDATES:
            cmd = [
                "ffmpeg",
                "-i", self.current_video,
                "-t", "1",  # 仅编码 1 秒用于探测 (不用 -re，尽快完成)
                "-c:v", encoder,
            ] + extra_args + ["-an", "-f", "h264", "-"]
            try:
                proc = subprocess.run(cmd, stdout=subprocess.PIPE,
                                      stderr=subprocess.DEVNULL, timeout=8)
            except (FileNotFoundError, subprocess.TimeoutExpired):
                continue
            if proc.returncode == 0 and self._has_idr(proc.stdout):
                self._chosen_encoder = (encoder, extra_args)
                self._log(f"选定编码器: {encoder} (探测到 IDR)")
                return
            else:
                self._log(f"编码器 {encoder} 不可用或未产出 IDR，跳过")
        # 兜底：用第一个候选 (即使没 IDR，至少能跑)
        self._chosen_encoder = self.ENCODER_CANDIDATES[0]
        self._log(f"警告: 未找到能产 IDR 的编码器，兜底用 {self._chosen_encoder[0]}")

    def _start_ffmpeg(self):
        """启动 FFmpeg 进程进行 H.264 Annex-B 转码"""
        if self.ffmpeg_process:
            self._stop_ffmpeg()

        if self._chosen_encoder is None:
            self._choose_encoder()

        encoder, extra_args = self._chosen_encoder
        # 小分辨率/低码率，控制 MQTT 分片频率 (~200 片/秒)。
        # -g 15 强制 0.5s 一个 IDR，便于客户端 reset 后快速恢复。
        cmd = [
            "ffmpeg",
            "-re",                       # 按原始帧率读取
            "-stream_loop", "-1",        # 循环播放
            "-i", self.current_video,
            "-vf", "scale=480:-2",       # 降分辨率，降低码率与分片量
            "-r", "30",
            "-b:v", "500k",
            "-c:v", encoder,
        ] + extra_args + [
            "-an",                       # 禁用音频
            "-f", "h264",                # 输出原始 H.264 流
            "-"                          # 输出到 stdout
        ]

        try:
            self.ffmpeg_process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                bufsize=10**6
            )
            self._log(f"FFmpeg H.264 编码器已启动 ({encoder})")
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
            except Exception:
                self.ffmpeg_process.kill()
            self.ffmpeg_process = None
            self._log("FFmpeg 进程已停止")

    # ------------------------------------------------------------------ #
    # 推流主循环
    # ------------------------------------------------------------------ #

    def _stream_loop(self):
        """推流主循环：读 ffmpeg stdout，切成协议分片，追加 packetId 发 MQTT"""
        buffer = bytearray()
        fragments_sent = 0  # 本次推流已发分片数 (用于周期日志)

        while self.running:
            # 等待开始推流
            if not self.is_streaming:
                time.sleep(0.1)
                buffer.clear()
                fragments_sent = 0
                continue

            # 暂停状态 (不停 ffmpeg，与 VideoStreamer 行为一致)
            if self.paused:
                time.sleep(0.1)
                continue

            # 启动 FFmpeg (如果尚未启动)
            if not self.ffmpeg_process:
                if not self._start_ffmpeg():
                    self.is_streaming = False
                    continue
                buffer.clear()
                fragments_sent = 0

            try:
                data = self.ffmpeg_process.stdout.read(4096)

                if not data:
                    self._log("FFmpeg 输出结束，重启...")
                    # 发送缓存中的剩余数据
                    if buffer:
                        self._send_fragment(bytes(buffer))
                        fragments_sent += 1
                        buffer.clear()
                    self._stop_ffmpeg()
                    continue

                buffer.extend(data)
                fragments_sent += self._drain_ready_fragments(buffer)
                if fragments_sent > 0 and fragments_sent % 200 == 0:
                    self._log(
                        f"已发送分片 {fragments_sent}, packetId={self.packet_id}, "
                        f"fragment_size={self.fragment_size}"
                    )

            except Exception as e:
                self._log(f"推流错误: {e}")
                self._stop_ffmpeg()
                time.sleep(1)

    def _drain_ready_fragments(self, buffer: bytearray) -> int:
        """
        按当前 fragment_size 从缓存中切出可发送分片。

        返回值为本轮实际发送的分片数，便于状态统计与单元测试。
        """
        sent = 0
        while len(buffer) >= self.fragment_size:
            chunk = bytes(buffer[:self.fragment_size])
            del buffer[:self.fragment_size]
            self._send_fragment(chunk)
            sent += 1
        return sent

    def _build_custom_byte_payload(self, h264_chunk: bytes) -> bytes:
        """
        将 H.264 裸片段反向封装成客户端解码链期望的 CustomByteBlock.data。

        格式:
            [H.264 Annex-B 分片字节] + [1 字节 packetId]
        """
        return h264_chunk + bytes([self.packet_id])

    def _send_fragment(self, h264_chunk: bytes):
        """
        发送一个分片: [h264_chunk] + [packetId 字节]

        客户端 feedH264Frame 取 data.back() 当 packetId，data.left(size-1) 交 H264Decoder。
        """
        data = self._build_custom_byte_payload(h264_chunk)
        if self.mqtt_publisher:
            self.mqtt_publisher.publish_custom_byte_block(data)
        self.packet_id = (self.packet_id + 1) % 256
