#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RoboMaster YOLO 模型训练脚本

使用 Ultralytics YOLOv8 训练 RoboMaster 机器人检测模型。
支持从预训练模型微调，适合 RoboMaster 比赛场景。

作者: Clear
日期: 2026-01-09
"""

import os
import sys
import argparse
from pathlib import Path
from datetime import datetime

try:
    from ultralytics import YOLO
except ImportError:
    print("请先安装 ultralytics: pip install ultralytics")
    sys.exit(1)


# 默认训练配置
DEFAULT_CONFIG = {
    "model": "yolov8n.pt",       # 基础模型 (nano, 速度快，适合实时检测)
    "epochs": 100,                # 训练轮数
    "imgsz": 640,                 # 输入图像尺寸
    "batch": 16,                  # 批次大小
    "device": "mps",              # macOS 使用 Metal Performance Shaders
    "patience": 20,               # 早停耐心值
    "save": True,                 # 保存检查点
    "project": "runs/robomaster", # 项目目录
    "name": "train",              # 实验名称
    "exist_ok": True,             # 覆盖已有实验
    "pretrained": True,           # 使用预训练权重
    "optimizer": "AdamW",         # 优化器
    "lr0": 0.01,                  # 初始学习率
    "lrf": 0.01,                  # 最终学习率 (相对于 lr0)
    "momentum": 0.937,            # SGD 动量
    "weight_decay": 0.0005,       # 权重衰减
    "warmup_epochs": 3.0,         # 预热轮数
    "warmup_momentum": 0.8,       # 预热动量
    "box": 7.5,                   # 边界框损失权重
    "cls": 0.5,                   # 分类损失权重
    "dfl": 1.5,                   # 分布焦点损失权重
    "hsv_h": 0.015,               # HSV 色调增强
    "hsv_s": 0.7,                 # HSV 饱和度增强
    "hsv_v": 0.4,                 # HSV 亮度增强
    "degrees": 0.0,               # 旋转增强角度
    "translate": 0.1,             # 平移增强比例
    "scale": 0.5,                 # 缩放增强比例
    "shear": 0.0,                 # 剪切增强角度
    "perspective": 0.0,           # 透视增强
    "flipud": 0.0,                # 上下翻转概率
    "fliplr": 0.5,                # 左右翻转概率
    "mosaic": 1.0,                # Mosaic 增强概率
    "mixup": 0.0,                 # MixUp 增强概率
    "copy_paste": 0.0,            # Copy-Paste 增强概率
}


def train_model(data_yaml: str, config: dict = None):
    """
    训练 YOLO 模型

    Args:
        data_yaml: 数据集配置文件路径
        config: 训练配置字典
    """
    if config is None:
        config = DEFAULT_CONFIG.copy()

    # 检查数据集配置文件
    if not os.path.exists(data_yaml):
        print(f"数据集配置文件不存在: {data_yaml}")
        print("请先运行 download_dataset.py 下载或创建数据集")
        return None

    print("=" * 60)
    print("RoboMaster YOLO 模型训练")
    print("=" * 60)
    print(f"数据集配置: {data_yaml}")
    print(f"基础模型: {config['model']}")
    print(f"训练轮数: {config['epochs']}")
    print(f"图像尺寸: {config['imgsz']}")
    print(f"设备: {config['device']}")
    print("=" * 60)

    # 加载预训练模型
    model = YOLO(config["model"])

    # 开始训练
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    config["name"] = f"robomaster_{timestamp}"

    results = model.train(
        data=data_yaml,
        epochs=config["epochs"],
        imgsz=config["imgsz"],
        batch=config["batch"],
        device=config["device"],
        patience=config["patience"],
        save=config["save"],
        project=config["project"],
        name=config["name"],
        exist_ok=config["exist_ok"],
        pretrained=config["pretrained"],
        optimizer=config["optimizer"],
        lr0=config["lr0"],
        lrf=config["lrf"],
        momentum=config["momentum"],
        weight_decay=config["weight_decay"],
        warmup_epochs=config["warmup_epochs"],
        warmup_momentum=config["warmup_momentum"],
        box=config["box"],
        cls=config["cls"],
        dfl=config["dfl"],
        hsv_h=config["hsv_h"],
        hsv_s=config["hsv_s"],
        hsv_v=config["hsv_v"],
        degrees=config["degrees"],
        translate=config["translate"],
        scale=config["scale"],
        shear=config["shear"],
        perspective=config["perspective"],
        flipud=config["flipud"],
        fliplr=config["fliplr"],
        mosaic=config["mosaic"],
        mixup=config["mixup"],
        copy_paste=config["copy_paste"],
    )

    print("\n训练完成!")
    print(f"最佳模型保存在: {config['project']}/{config['name']}/weights/best.pt")

    return model, results


def validate_model(model_path: str, data_yaml: str):
    """
    验证训练好的模型

    Args:
        model_path: 模型文件路径
        data_yaml: 数据集配置文件路径
    """
    print(f"验证模型: {model_path}")
    model = YOLO(model_path)
    results = model.val(data=data_yaml)

    print("\n验证结果:")
    print(f"  mAP50: {results.box.map50:.4f}")
    print(f"  mAP50-95: {results.box.map:.4f}")

    return results


def main():
    parser = argparse.ArgumentParser(description="RoboMaster YOLO 模型训练器")
    parser.add_argument("--data", type=str, default="./datasets/robomaster/data.yaml",
                        help="数据集配置文件路径")
    parser.add_argument("--model", type=str, default="yolov8n.pt",
                        help="基础模型 (yolov8n/s/m/l/x.pt)")
    parser.add_argument("--epochs", type=int, default=100,
                        help="训练轮数")
    parser.add_argument("--batch", type=int, default=16,
                        help="批次大小")
    parser.add_argument("--imgsz", type=int, default=640,
                        help="输入图像尺寸")
    parser.add_argument("--device", type=str, default="mps",
                        help="训练设备 (cpu/cuda/mps)")
    parser.add_argument("--validate", type=str,
                        help="验证已有模型 (提供模型路径)")
    parser.add_argument("--resume", type=str,
                        help="从检查点恢复训练")

    args = parser.parse_args()

    # 验证模式
    if args.validate:
        validate_model(args.validate, args.data)
        return

    # 恢复训练
    if args.resume:
        print(f"从检查点恢复训练: {args.resume}")
        model = YOLO(args.resume)
        model.train(resume=True)
        return

    # 设置配置
    config = DEFAULT_CONFIG.copy()
    config["model"] = args.model
    config["epochs"] = args.epochs
    config["batch"] = args.batch
    config["imgsz"] = args.imgsz
    config["device"] = args.device

    # 开始训练
    train_model(args.data, config)


if __name__ == "__main__":
    main()
