#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
旧版模拟器入口兼容层。

当前真正维护的模拟器运行入口位于 `sim/server/main.py`。
保留这个文件的目的，是为了兼容旧的
`python sim/server.py ...` 启动方式，
并把它转发到现在的 FastAPI + Socket.IO 模拟器主链路。
"""

from pathlib import Path
import runpy
import sys


def main():
    sim_root = Path(__file__).resolve().parent
    # 确保 sim 目录在导入路径里，便于后续加载 server/main.py
    if str(sim_root) not in sys.path:
        sys.path.insert(0, str(sim_root))
    # 直接以 __main__ 方式执行当前正式入口
    runpy.run_path(str(sim_root / "server" / "main.py"), run_name="__main__")


if __name__ == "__main__":
    main()
