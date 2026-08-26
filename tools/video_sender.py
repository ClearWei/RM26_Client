#!/usr/bin/env python3
"""
video_sender.py - RoboMaster 2026 视频协议测试发送器

根据官方协议规范，将视频文件转换为 HEVC 并通过 UDP 发送。

UDP 包格式（8 字节头部）：
┌────────────┬──────────────┬──────────────┬────────────────┐
│ FrameID(2) │ SliceID(2)   │ TotalBytes(4)│ HEVC Data (N)  │
│  大端序     │  大端序       │  大端序       │  裸码流数据     │
└────────────┴──────────────┴──────────────┴────────────────┘

用法：
  python3 video_sender.py [视频文件] [目标IP] [目标端口]

示例：
  python3 video_sender.py testdemo.mov 127.0.0.1 3334
"""

import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from sim.server.hevc_annexb import HevcAnnexBFrameExtractor

# 配置常量
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 3334
MTU_SIZE = 1400  # UDP 最大传输单元（留余量避免分片）


def send_video_as_hevc_udp(video_path: str, host: str, port: int, loop: bool = True):
    """
    将视频转换为 HEVC 并通过 UDP 发送

    Args:
        video_path: 输入视频文件路径
        host: 目标 IP 地址
        port: 目标端口号（官方协议为 3334）
        loop: 是否循环播放（默认 True）
    """
    # 创建 UDP 套接字
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    frame_id = 0
    total_bytes_sent = 0
    extractor = HevcAnnexBFrameExtractor()
    process = None

    print(f"[video_sender] 开始发送到 {host}:{port}")
    print(f"[video_sender] MTU: {MTU_SIZE}")
    print(f"[video_sender] 循环模式: {'开启' if loop else '关闭'}")

    try:
        ffmpeg_cmd = [
            "ffmpeg",
            "-re",
        ]
        if loop:
            ffmpeg_cmd += ["-stream_loop", "-1"]
        ffmpeg_cmd += [
            "-i",
            video_path,
            "-c:v",
            "libx265",
            "-preset",
            "ultrafast",
            "-tune",
            "zerolatency",
            "-x265-params",
            "keyint=30:min-keyint=30:scenecut=0",
            "-bsf:v",
            "hevc_mp4toannexb",
            "-an",
            "-f",
            "hevc",
            "-",
        ]

        print(f"[video_sender] 启动 FFmpeg 转码...")
        print(f"[video_sender] 命令: {' '.join(ffmpeg_cmd)}")

        process = subprocess.Popen(
            ffmpeg_cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, bufsize=10**6
        )

        start_time = time.time()

        while True:
            chunk = process.stdout.read(4096)
            if not chunk:
                for frame_data in extractor.flush():
                    total_frame_bytes = len(frame_data)
                    num_slices = (total_frame_bytes + MTU_SIZE - 1) // MTU_SIZE

                    for slice_id in range(num_slices):
                        offset = slice_id * MTU_SIZE
                        slice_data = frame_data[offset : offset + MTU_SIZE]
                        header = struct.pack(
                            ">HHI", frame_id, slice_id, total_frame_bytes
                        )
                        packet = header + slice_data
                        sock.sendto(packet, (host, port))
                        total_bytes_sent += len(packet)

                    if frame_id % 30 == 0:
                        bitrate = (
                            total_bytes_sent
                            * 8
                            / max(time.time() - start_time, 0.001)
                            / 1024
                            / 1024
                        )
                        print(
                            f"[video_sender] Frame {frame_id}, Bitrate: {bitrate:.2f} Mbps, Slices: {num_slices}"
                        )
                    frame_id += 1
                break

            for frame_data in extractor.push(chunk):
                total_frame_bytes = len(frame_data)
                num_slices = (total_frame_bytes + MTU_SIZE - 1) // MTU_SIZE

                for slice_id in range(num_slices):
                    offset = slice_id * MTU_SIZE
                    slice_data = frame_data[offset : offset + MTU_SIZE]
                    header = struct.pack(">HHI", frame_id, slice_id, total_frame_bytes)
                    packet = header + slice_data
                    sock.sendto(packet, (host, port))
                    total_bytes_sent += len(packet)

                if frame_id % 30 == 0:
                    bitrate = (
                        total_bytes_sent
                        * 8
                        / max(time.time() - start_time, 0.001)
                        / 1024
                        / 1024
                    )
                    print(
                        f"[video_sender] Frame {frame_id}, Bitrate: {bitrate:.2f} Mbps, Slices: {num_slices}"
                    )
                frame_id += 1

        process.terminate()
        process.wait()

    except KeyboardInterrupt:
        print("\n[video_sender] 用户中断")
    finally:
        if process is not None:
            try:
                process.terminate()
                process.wait(timeout=2)
            except Exception:
                try:
                    process.kill()
                except Exception:
                    pass
        sock.close()

        print(f"\n[video_sender] 发送完成:")
        print(f"  总帧数: {frame_id}")
        print(f"  总字节: {total_bytes_sent / 1024 / 1024:.2f} MB")
        print(f"  循环模式: {'开启' if loop else '关闭'}")


def main():
    # 解析命令行参数
    if len(sys.argv) < 2:
        print("用法: python3 video_sender.py <视频文件> [目标IP] [目标端口]")
        print("示例: python3 video_sender.py testdemo.mov 127.0.0.1 3334")
        sys.exit(1)

    video_path = sys.argv[1]
    host = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_HOST
    port = int(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_PORT

    print(f"[video_sender] RoboMaster 2026 视频协议测试发送器")
    print(f"[video_sender] 视频文件: {video_path}")
    print(f"[video_sender] 目标地址: {host}:{port}")
    print()

    send_video_as_hevc_udp(video_path, host, port)


if __name__ == "__main__":
    main()
