#!/usr/bin/env python3
"""从 canonical schema 重新生成模拟器 Python Protobuf 代码。"""

from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path
from typing import Sequence


class GenerationError(RuntimeError):
    """表示生成环境或 protoc 执行结果不满足要求。"""


def build_command(project_root: Path, protoc: str) -> list[str]:
    proto_root = project_root / "src/network/proto"
    canonical = proto_root / "robomaster.proto"
    output_dir = project_root / "sim"
    if not canonical.is_file():
        raise GenerationError("缺少 src/network/proto/robomaster.proto")
    if not output_dir.is_dir():
        raise GenerationError("缺少 sim 目录")
    return [
        protoc,
        f"--proto_path={proto_root}",
        f"--python_out={output_dir}",
        str(canonical),
    ]


def generate(project_root: Path, *, protoc: str | None = None) -> Path:
    executable = protoc or shutil.which("protoc")
    if not executable:
        raise GenerationError("找不到 protoc，请先安装 Protobuf 编译器")

    command = build_command(project_root, executable)
    completed = subprocess.run(command, capture_output=True, text=True, check=False)
    if completed.returncode != 0:
        details = (completed.stderr or completed.stdout).strip()
        raise GenerationError(f"protoc 执行失败：{details or completed.returncode}")

    generated = project_root / "sim/robomaster_pb2.py"
    if not generated.is_file():
        raise GenerationError("protoc 未生成 sim/robomaster_pb2.py")
    return generated


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="从 src/network/proto/robomaster.proto 生成模拟器 pb2"
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="仓库根目录，默认由脚本位置推导",
    )
    parser.add_argument("--protoc", help="protoc 可执行文件，默认从 PATH 查找")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        generated = generate(args.project_root.resolve(), protoc=args.protoc)
    except GenerationError as error:
        print(f"模拟器 Protobuf 生成失败：{error}")
        return 1
    print(f"模拟器 Protobuf 已生成：{generated.relative_to(args.project_root.resolve())}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
