#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
YOLO 模型导出为 ONNX 格式

将训练好的 YOLOv8 模型导出为 ONNX 格式，供 C++ ONNX Runtime 推理使用。

作者: Clear
日期: 2026-01-09
"""

import os
import sys
import argparse
import shutil
from pathlib import Path

try:
    from ultralytics import YOLO
except ImportError:
    print("请先安装 ultralytics: pip install ultralytics")
    sys.exit(1)


def export_to_onnx(model_path: str, output_path: str = None,
                   imgsz: int = 640, simplify: bool = True,
                   opset: int = 12, dynamic: bool = False):
    """
    将 YOLO 模型导出为 ONNX 格式

    Args:
        model_path: 训练好的模型路径 (.pt)
        output_path: 输出 ONNX 文件路径
        imgsz: 输入图像尺寸
        simplify: 是否简化 ONNX 模型
        opset: ONNX opset 版本
        dynamic: 是否使用动态形状
    """
    print("=" * 60)
    print("YOLO 模型导出为 ONNX")
    print("=" * 60)
    print(f"输入模型: {model_path}")
    print(f"图像尺寸: {imgsz}x{imgsz}")
    print(f"简化模型: {simplify}")
    print(f"Opset 版本: {opset}")
    print(f"动态形状: {dynamic}")
    print("=" * 60)

    # 加载模型
    model = YOLO(model_path)

    # 导出为 ONNX
    success = model.export(
        format="onnx",
        imgsz=imgsz,
        simplify=simplify,
        opset=opset,
        dynamic=dynamic,
        half=False,  # 不使用 FP16 以确保兼容性
    )

    if success:
        # 获取导出的 ONNX 文件路径
        onnx_path = Path(model_path).with_suffix(".onnx")

        # 如果指定了输出路径，复制文件
        if output_path:
            output_dir = os.path.dirname(output_path)
            if output_dir and not os.path.exists(output_dir):
                os.makedirs(output_dir)
            shutil.copy(str(onnx_path), output_path)
            print(f"\nONNX 模型已导出到: {output_path}")
        else:
            print(f"\nONNX 模型已导出到: {onnx_path}")

        return True
    else:
        print("导出失败!")
        return False


def verify_onnx_model(onnx_path: str):
    """
    验证 ONNX 模型

    Args:
        onnx_path: ONNX 模型文件路径
    """
    try:
        import onnx
        import onnxruntime as ort
    except ImportError:
        print("请安装 onnx 和 onnxruntime: pip install onnx onnxruntime")
        return False

    print(f"\n验证 ONNX 模型: {onnx_path}")

    # 检查模型结构
    model = onnx.load(onnx_path)
    onnx.checker.check_model(model)
    print("✓ ONNX 模型结构验证通过")

    # 获取模型信息
    print("\n模型输入:")
    for input in model.graph.input:
        name = input.name
        shape = [dim.dim_value for dim in input.type.tensor_type.shape.dim]
        print(f"  {name}: {shape}")

    print("\n模型输出:")
    for output in model.graph.output:
        name = output.name
        shape = [dim.dim_value for dim in output.type.tensor_type.shape.dim]
        print(f"  {name}: {shape}")

    # 使用 ONNX Runtime 加载测试
    print("\n测试 ONNX Runtime 加载...")
    session = ort.InferenceSession(onnx_path)
    print("✓ ONNX Runtime 加载成功")

    # 打印推理提供者
    providers = session.get_providers()
    print(f"可用推理提供者: {providers}")

    return True


def main():
    parser = argparse.ArgumentParser(description="YOLO 模型导出为 ONNX")
    parser.add_argument("--model", type=str, required=True,
                        help="训练好的模型路径 (.pt)")
    parser.add_argument("--output", type=str,
                        default="../../resources/models/robomaster_yolo.onnx",
                        help="输出 ONNX 文件路径")
    parser.add_argument("--imgsz", type=int, default=640,
                        help="输入图像尺寸")
    parser.add_argument("--simplify", action="store_true", default=True,
                        help="简化 ONNX 模型")
    parser.add_argument("--no-simplify", action="store_false", dest="simplify",
                        help="不简化 ONNX 模型")
    parser.add_argument("--opset", type=int, default=12,
                        help="ONNX opset 版本")
    parser.add_argument("--dynamic", action="store_true",
                        help="使用动态形状 (用于可变输入尺寸)")
    parser.add_argument("--verify", action="store_true",
                        help="导出后验证模型")

    args = parser.parse_args()

    # 导出模型
    success = export_to_onnx(
        model_path=args.model,
        output_path=args.output,
        imgsz=args.imgsz,
        simplify=args.simplify,
        opset=args.opset,
        dynamic=args.dynamic
    )

    # 验证模型
    if success and args.verify:
        verify_onnx_model(args.output if args.output else
                          str(Path(args.model).with_suffix(".onnx")))


if __name__ == "__main__":
    main()
