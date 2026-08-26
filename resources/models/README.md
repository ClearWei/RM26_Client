# 可选 YOLO 模型训练指南

本项目不随源码提供检测模型。AR 叠加默认关闭，`ar_overlay.model_path` 默认为空；启用前需要自行
训练或取得有权使用的 ONNX 模型，并在本地配置中填写路径。

## 模型文件

- 推荐本地文件名：`robomaster_yolo.onnx`；模型文件默认不进入 Git。

## 获取模型

### 方法一：使用已获授权的数据集训练

使用 `tools/yolo_training/` 目录中的脚本进行训练：

```bash
cd tools/yolo_training
pip3 install -r requirements.txt
python3 download_dataset.py  # 下载 RoboMaster 数据集
python3 train.py             # 开始训练
python3 export_onnx.py       # 导出 ONNX 模型
```

### 方法二：评估 Roboflow 数据集

1. 打开 [Roboflow Universe 的 RoboMaster 数据集搜索页](https://universe.roboflow.com/search?q=robomaster)；
2. 核对具体数据集的作者、许可证、人物和比赛画面使用范围；
3. 仅在许可允许当前用途时下载 YOLO 格式数据；
4. 使用 `train.py` 训练并保存数据来源记录。

### 方法三：评估公开代码仓库

- [robomaster-2022-cv/detection](https://github.com/robomaster-2022-cv/detection)
- [YOLO-of-RoboMaster-Keypoints-Detection-2023](https://github.com/zRzRzRzRzRzRzR/YOLO-of-RoboMaster-Keypoints-Detection-2023)

仓库公开不等于其中的数据集、权重和比赛画面都允许再次分发。使用前分别核对代码许可证、数据许可
和模型权重条款，不能只引用仓库链接作为授权证据。

## 类别定义

当前类别表包含 8 类机器人：

| 类别 ID | 类别名称 | 说明 |
|---------|----------|------|
| 0 | red_hero | 红方英雄 |
| 1 | red_engineer | 红方工程 |
| 2 | red_infantry | 红方步兵 |
| 3 | red_sentry | 红方哨兵 |
| 4 | blue_hero | 蓝方英雄 |
| 5 | blue_engineer | 蓝方工程 |
| 6 | blue_infantry | 蓝方步兵 |
| 7 | blue_sentry | 蓝方哨兵 |

## 模型要求

- 输入尺寸: 640x640
- 格式: ONNX
- 输出: 边界框 + 类别置信度

## 公开发行要求

模型进入源码或安装包前，需要在素材台账中记录训练数据、训练者、基础权重、导出工具、许可证和
模型摘要，并重新执行公开发布预检。只在本机使用的模型路径不要提交到仓库。
