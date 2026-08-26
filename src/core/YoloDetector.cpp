// SPDX-License-Identifier: MIT
/**
 * @file YoloDetector.cpp
 * @brief YOLO 目标检测器实现
 * @details 使用 ONNX Runtime 进行 YOLO 模型推理。
 * @author Clear
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#include "YoloDetector.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>

#include <algorithm>
#include <cmath>
#include <numeric>

// ONNX Runtime 头文件
#include <onnxruntime/onnxruntime_cxx_api.h>

namespace RM {

// ============================================================================
// 构造函数和析构函数
// ============================================================================

YoloDetector::YoloDetector(QObject *parent) : QObject(parent) {
  // 创建 ONNX Runtime 环境
  try {
    m_env =
        std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "YoloDetector");
    m_sessionOptions = std::make_unique<Ort::SessionOptions>();

    // 设置推理优化
    m_sessionOptions->SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_ALL);
    m_sessionOptions->SetIntraOpNumThreads(4); // 使用 4 个线程

    qDebug() << "YoloDetector: ONNX Runtime 环境初始化成功";
  } catch (const Ort::Exception &e) {
    qWarning() << "YoloDetector: ONNX Runtime 初始化失败:" << e.what();
  }
}

YoloDetector::~YoloDetector() {
  QMutexLocker locker(&m_mutex);
  m_session.reset();
  m_sessionOptions.reset();
  m_env.reset();
}

// ============================================================================
// 公共方法
// ============================================================================

bool YoloDetector::loadModel(const QString &modelPath) {
  QMutexLocker locker(&m_mutex);

  // 检查文件是否存在
  if (!QFile::exists(modelPath)) {
    qDebug() << "YoloDetector: 模型文件未部署，跳过加载:" << modelPath;
    return false;
  }

  try {
    // 创建推理会话
    std::string path = modelPath.toStdString();

#ifdef _WIN32
    std::wstring wpath(path.begin(), path.end());
    m_session = std::make_unique<Ort::Session>(*m_env, wpath.c_str(),
                                               *m_sessionOptions);
#else
    m_session =
        std::make_unique<Ort::Session>(*m_env, path.c_str(), *m_sessionOptions);
#endif

    // 获取输入信息
    Ort::AllocatorWithDefaultOptions allocator;

    size_t numInputNodes = m_session->GetInputCount();
    if (numInputNodes == 0) {
      emit errorOccurred("模型没有输入节点");
      return false;
    }

    // 获取输入名称和形状
    auto inputName = m_session->GetInputNameAllocated(0, allocator);
    m_inputNames.clear();
    m_inputNames.push_back(inputName.get());

    auto inputInfo = m_session->GetInputTypeInfo(0);
    auto tensorInfo = inputInfo.GetTensorTypeAndShapeInfo();
    m_inputShape = tensorInfo.GetShape();

    // 假设输入形状为 [batch, channels, height, width]
    if (m_inputShape.size() >= 4) {
      m_inputSize = static_cast<int>(m_inputShape[2]); // 输入张量高度
    }

    // 获取输出信息
    size_t numOutputNodes = m_session->GetOutputCount();
    if (numOutputNodes == 0) {
      emit errorOccurred("模型没有输出节点");
      return false;
    }

    auto outputName = m_session->GetOutputNameAllocated(0, allocator);
    m_outputNames.clear();
    m_outputNames.push_back(outputName.get());

    auto outputInfo = m_session->GetOutputTypeInfo(0);
    auto outputTensorInfo = outputInfo.GetTensorTypeAndShapeInfo();
    m_outputShape = outputTensorInfo.GetShape();

    m_modelLoaded = true;

    qDebug() << "YoloDetector: 模型加载成功";
    qDebug() << "  输入形状:" << m_inputShape[0] << "x" << m_inputShape[1]
             << "x" << m_inputShape[2] << "x" << m_inputShape[3];
    qDebug() << "  输入尺寸:" << m_inputSize;

    return true;

  } catch (const Ort::Exception &e) {
    QString error = QString("加载模型失败: %1").arg(e.what());
    qWarning() << "YoloDetector:" << error;
    emit errorOccurred(error);
    m_modelLoaded = false;
    return false;
  }
}

QList<DetectionResult> YoloDetector::detect(const QImage &frame) {
  QList<DetectionResult> results;

  if (!m_modelLoaded || frame.isNull()) {
    return results;
  }

  QMutexLocker locker(&m_mutex);
  QElapsedTimer timer;
  timer.start();

  try {
    // 预处理
    std::vector<float> inputTensor = preprocess(frame);

    // 创建输入张量
    Ort::MemoryInfo memoryInfo =
        Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    std::vector<int64_t> inputDims = {1, 3, m_inputSize, m_inputSize};
    Ort::Value inputOrtTensor = Ort::Value::CreateTensor<float>(
        memoryInfo, inputTensor.data(), inputTensor.size(), inputDims.data(),
        inputDims.size());

    // 执行推理
    std::vector<const char *> inputNames = {m_inputNames[0]};
    std::vector<const char *> outputNames = {m_outputNames[0]};

    auto outputTensors =
        m_session->Run(Ort::RunOptions{nullptr}, inputNames.data(),
                       &inputOrtTensor, 1, outputNames.data(), 1);

    // 获取输出数据
    float *outputData = outputTensors[0].GetTensorMutableData<float>();
    auto outputShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();

    size_t outputSize = 1;
    for (auto dim : outputShape) {
      outputSize *= dim;
    }

    std::vector<float> output(outputData, outputData + outputSize);

    // 后处理
    results = postprocess(output, frame.width(), frame.height());

    // 应用 NMS
    applyNMS(results);

    m_lastInferenceTime = timer.elapsed();

  } catch (const Ort::Exception &e) {
    qWarning() << "YoloDetector: 推理失败:" << e.what();
  }

  return results;
}

void YoloDetector::setConfidenceThreshold(float threshold) {
  QMutexLocker locker(&m_mutex);
  m_confidenceThreshold = std::clamp(threshold, 0.0f, 1.0f);
}

void YoloDetector::setNMSThreshold(float threshold) {
  QMutexLocker locker(&m_mutex);
  m_nmsThreshold = std::clamp(threshold, 0.0f, 1.0f);
}

// ============================================================================
// 私有方法
// ============================================================================

std::vector<float> YoloDetector::preprocess(const QImage &frame) {
  // 缩放图像到模型输入尺寸
  QImage resized = frame.scaled(m_inputSize, m_inputSize, Qt::IgnoreAspectRatio,
                                Qt::SmoothTransformation);

  // 转换为 RGB 格式
  QImage rgb = resized.convertToFormat(QImage::Format_RGB888);

  // 创建输入张量 (CHW 格式, 归一化到 0-1)
  std::vector<float> inputTensor(3 * m_inputSize * m_inputSize);

  const uchar *bits = rgb.constBits();
  int stride = rgb.bytesPerLine();

  for (int y = 0; y < m_inputSize; ++y) {
    for (int x = 0; x < m_inputSize; ++x) {
      const uchar *pixel = bits + y * stride + x * 3;

      // RGB -> CHW, 归一化
      int idx = y * m_inputSize + x;
      inputTensor[0 * m_inputSize * m_inputSize + idx] = pixel[0] / 255.0f; // R
      inputTensor[1 * m_inputSize * m_inputSize + idx] = pixel[1] / 255.0f; // G
      inputTensor[2 * m_inputSize * m_inputSize + idx] = pixel[2] / 255.0f; // B
    }
  }

  return inputTensor;
}

QList<DetectionResult>
YoloDetector::postprocess(const std::vector<float> &output, int originalWidth,
                          int originalHeight) {
  QList<DetectionResult> results;

  // YOLOv8 输出格式: [batch, num_classes + 4, num_boxes]
  // 其中 4 是边界框坐标 (cx, cy, w, h)
  // num_classes 是类别数量

  // 假设输出形状为 [1, 84, 8400] (YOLOv8 典型输出)
  // 84 = 4 (bbox) + 80 (COCO classes), 但我们只有 8 个类别
  // 根据实际模型调整

  int numBoxes = 8400;                // 默认 YOLOv8 检测框数量
  int numFeatures = m_numClasses + 4; // 4 个坐标参数加类别数

  if (output.size() != static_cast<size_t>(numBoxes * numFeatures)) {
    // 尝试推断正确的维度
    if (output.size() % numFeatures == 0) {
      numBoxes = output.size() / numFeatures;
    } else {
      qWarning() << "YoloDetector: 输出尺寸不匹配";
      return results;
    }
  }

  // 解析每个检测框
  for (int i = 0; i < numBoxes; ++i) {
    // 获取边界框坐标 (归一化)
    float cx = output[0 * numBoxes + i];
    float cy = output[1 * numBoxes + i];
    float w = output[2 * numBoxes + i];
    float h = output[3 * numBoxes + i];

    // 获取类别置信度
    float maxConf = 0.0f;
    int maxClassId = -1;

    for (int c = 0; c < m_numClasses; ++c) {
      float conf = output[(4 + c) * numBoxes + i];
      if (conf > maxConf) {
        maxConf = conf;
        maxClassId = c;
      }
    }

    // 过滤低置信度检测
    if (maxConf < m_confidenceThreshold) {
      continue;
    }

    // 创建检测结果
    DetectionResult result;
    result.classId = maxClassId;
    result.confidence = maxConf;
    result.className = getClassName(maxClassId);
    result.robotId = inferRobotId(maxClassId);

    // 转换边界框坐标 (中心点 + 宽高 -> 归一化矩形)
    float x1 = (cx - w / 2.0f) / m_inputSize;
    float y1 = (cy - h / 2.0f) / m_inputSize;
    float bw = w / m_inputSize;
    float bh = h / m_inputSize;

    result.boundingBox = QRectF(x1, y1, bw, bh);

    results.append(result);
  }

  return results;
}

void YoloDetector::applyNMS(QList<DetectionResult> &detections) {
  if (detections.size() <= 1) {
    return;
  }

  // 按置信度排序
  std::sort(detections.begin(), detections.end(),
            [](const DetectionResult &a, const DetectionResult &b) {
              return a.confidence > b.confidence;
            });

  // 非极大值抑制
  QList<DetectionResult> kept;

  while (!detections.isEmpty()) {
    DetectionResult best = detections.takeFirst();
    kept.append(best);

    // 移除与最佳框重叠过大的检测
    detections.erase(
        std::remove_if(
            detections.begin(), detections.end(),
            [&](const DetectionResult &det) {
              // 计算 IoU
              QRectF intersection =
                  best.boundingBox.intersected(det.boundingBox);
              if (intersection.isEmpty()) {
                return false;
              }

              float intersectionArea =
                  intersection.width() * intersection.height();
              float unionArea =
                  best.boundingBox.width() * best.boundingBox.height() +
                  det.boundingBox.width() * det.boundingBox.height() -
                  intersectionArea;

              float iou = intersectionArea / unionArea;
              return iou > m_nmsThreshold && det.classId == best.classId;
            }),
        detections.end());
  }

  detections = kept;
}

int YoloDetector::inferRobotId(int classId) const {
  // 根据类别推断机器人 ID
  // 类别 0-3: 红方 (hero=1, engineer=2, infantry=3-5, sentry=7)
  // 类别 4-7: 蓝方 (hero=101, engineer=102, infantry=103-105, sentry=107)

  switch (classId) {
  case 0:
    return 1; // 红方英雄
  case 1:
    return 2; // 红方工程
  case 2:
    return 3; // red_infantry (默认3号)
  case 3:
    return 7; // 红方哨兵
  case 4:
    return 101; // 蓝方英雄
  case 5:
    return 102; // 蓝方工程
  case 6:
    return 103; // blue_infantry (默认103号)
  case 7:
    return 107; // 蓝方哨兵
  default:
    return 0;
  }
}

QString YoloDetector::getClassName(int classId) const {
  if (classId >= 0 && classId < m_classNames.size()) {
    return m_classNames[classId];
  }
  return "unknown";
}

} // namespace RM
