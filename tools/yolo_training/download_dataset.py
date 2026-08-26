#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RoboMaster 数据集下载脚本

从 Roboflow 和其他来源下载 RoboMaster 机器人检测数据集。
支持多个数据源，可以合并多个数据集以获得更好的训练效果。

作者: Clear
日期: 2026-01-09
"""

import os
import sys
import argparse
import shutil
from pathlib import Path

try:
    from roboflow import Roboflow
except ImportError:
    print("请先安装 roboflow: pip install roboflow")
    sys.exit(1)


# 数据集配置
DATASETS = {
    "nyu_robomasters": {
        "workspace": "nyu-robomasters",
        "project": "robomasters",
        "version": 1,
        "description": "NYU RoboMasters 数据集 - 包含多种机器人类型"
    },
    "robomaster_detection": {
        "workspace": "robomaster-oagp2",
        "project": "dataset_robomaster",
        "version": 1,
        "description": "Dataset_Robomaster - 通用机器人检测数据集"
    }
}


def download_from_roboflow(api_key: str, dataset_name: str, output_dir: str):
    """
    从 Roboflow 下载数据集

    Args:
        api_key: Roboflow API 密钥 (可在 roboflow.com 获取)
        dataset_name: 数据集名称 (见 DATASETS 字典)
        output_dir: 输出目录
    """
    if dataset_name not in DATASETS:
        print(f"未知数据集: {dataset_name}")
        print(f"可用数据集: {list(DATASETS.keys())}")
        return False

    config = DATASETS[dataset_name]
    print(f"正在下载数据集: {config['description']}")

    try:
        rf = Roboflow(api_key=api_key)
        project = rf.workspace(config["workspace"]).project(config["project"])
        dataset = project.version(config["version"]).download("yolov8", location=output_dir)
        print(f"数据集已下载到: {output_dir}")
        return True
    except Exception as e:
        print(f"下载失败: {e}")
        return False


def create_sample_dataset(output_dir: str):
    """
    创建示例数据集结构（用于测试）

    Args:
        output_dir: 输出目录
    """
    print("创建示例数据集结构...")

    # 创建目录结构
    dirs = [
        "train/images",
        "train/labels",
        "valid/images",
        "valid/labels",
        "test/images",
        "test/labels"
    ]

    for d in dirs:
        Path(os.path.join(output_dir, d)).mkdir(parents=True, exist_ok=True)

    # 创建 data.yaml 配置文件
    yaml_content = """
# RoboMaster 机器人检测数据集配置
# 类别定义遵循 RoboMaster 比赛规则

path: {output_dir}
train: train/images
val: valid/images
test: test/images

# 类别定义 (8类，红蓝双方各4种机器人)
names:
  0: red_hero
  1: red_engineer
  2: red_infantry
  3: red_sentry
  4: blue_hero
  5: blue_engineer
  6: blue_infantry
  7: blue_sentry

# 类别说明:
# - hero: 英雄机器人 (42mm弹丸，高伤害)
# - engineer: 工程机器人 (无武器，资源操作)
# - infantry: 步兵机器人 (17mm弹丸，基础兵种)
# - sentry: 哨兵机器人 (自动防守)
""".format(output_dir=output_dir)

    yaml_path = os.path.join(output_dir, "data.yaml")
    with open(yaml_path, "w", encoding="utf-8") as f:
        f.write(yaml_content)

    print(f"示例数据集结构已创建于: {output_dir}")
    print(f"配置文件: {yaml_path}")
    print("\n请将训练图片放入 train/images 和 valid/images 目录")
    print("对应的 YOLO 格式标注放入 train/labels 和 valid/labels 目录")
    print("\nYOLO 标注格式: class_id center_x center_y width height")
    print("所有值均为归一化坐标 (0-1)")

    return True


def main():
    parser = argparse.ArgumentParser(description="RoboMaster 数据集下载器")
    parser.add_argument("--api-key", type=str, help="Roboflow API 密钥")
    parser.add_argument("--dataset", type=str, choices=list(DATASETS.keys()),
                        help="要下载的数据集")
    parser.add_argument("--output", type=str, default="./datasets/robomaster",
                        help="输出目录")
    parser.add_argument("--sample", action="store_true",
                        help="仅创建示例数据集结构")
    parser.add_argument("--list", action="store_true",
                        help="列出所有可用数据集")

    args = parser.parse_args()

    # 列出可用数据集
    if args.list:
        print("可用数据集:")
        for name, config in DATASETS.items():
            print(f"  - {name}: {config['description']}")
        return

    # 创建示例数据集
    if args.sample:
        create_sample_dataset(args.output)
        return

    # 下载数据集
    if not args.api_key:
        print("请提供 Roboflow API 密钥: --api-key YOUR_API_KEY")
        print("您可以在 https://app.roboflow.com/settings/api 获取 API 密钥")
        print("\n或者使用 --sample 创建示例数据集结构，手动添加您自己的数据")
        return

    if not args.dataset:
        print("请指定要下载的数据集: --dataset DATASET_NAME")
        print("使用 --list 查看可用数据集")
        return

    download_from_roboflow(args.api_key, args.dataset, args.output)


if __name__ == "__main__":
    main()
