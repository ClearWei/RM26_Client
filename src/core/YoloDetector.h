// SPDX-License-Identifier: MIT
/**
 * @file YoloDetector.h
 * @brief YOLO 目标检测器封装
 * @details 使用 ONNX Runtime 进行 YOLO 模型推理，检测图像中的 RoboMaster
 * 机器人。 支持 YOLOv8 模型格式，提供高效的实时检测能力。
 * @author Clear
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#ifndef YOLODETECTOR_H
#define YOLODETECTOR_H

#include <QImage>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QRectF>
#include <QString>

#include <memory>
#include <vector>

// 前向声明 ONNX Runtime 类型
namespace Ort {
class Session;
class Env;
class SessionOptions;
class MemoryInfo;
} // namespace Ort

namespace RM {

/**
 * @struct DetectionResult
 * @brief 检测结果结构
 * @details 存储单个目标的检测结果，包括类别、置信度和边界框。
 */
struct DetectionResult {
  int classId;        // 类别ID (0-7: 对应8种机器人)
  float confidence;   // 置信度 (0.0-1.0)
  QRectF boundingBox; // 边界框 (归一化坐标 0.0-1.0)
  int robotId;        // 推断的机器人ID (1-7红方, 101-107蓝方)
  QString className;  // 类别名称 (如 "red_infantry")

  DetectionResult() : classId(-1), confidence(0.0f), robotId(0) {}
};

/**
 * @class YoloDetector
 * @brief YOLO 目标检测器
 * @details 封装 ONNX Runtime 推理过程，提供简洁的检测接口。
 *          线程安全，可在独立线程中运行。
 */
class YoloDetector : public QObject {
  Q_OBJECT

public:
  explicit YoloDetector(QObject *parent = nullptr);
  ~YoloDetector();

  /**
   * @brief 加载 ONNX 模型
   * @param modelPath 模型文件路径
   * @return 是否加载成功
   */
  bool loadModel(const QString &modelPath);

  /**
   * @brief 检测图像中的目标
   * @param frame 输入图像 (任意尺寸，内部会自动缩放)
   * @return 检测结果列表
   */
  QList<DetectionResult> detect(const QImage &frame);

  /**
   * @brief 设置置信度阈值
   * @param threshold 阈值 (0.0-1.0)
   */
  void setConfidenceThreshold(float threshold);

  /**
   * @brief 设置 NMS 阈值
   * @param threshold IoU 阈值 (0.0-1.0)
   */
  void setNMSThreshold(float threshold);

  /**
   * @brief 检查模型是否已加载
   * @return 是否已加载
   */
  bool isModelLoaded() const { return m_modelLoaded; }

  /**
   * @brief 获取模型输入尺寸
   * @return 输入尺寸 (宽高相同)
   */
  int getInputSize() const { return m_inputSize; }

  /**
   * @brief 获取上次推理耗时
   * @return 耗时 (毫秒)
   */
  float getLastInferenceTime() const { return m_lastInferenceTime; }

signals:
  /**
   * @brief 检测完成信号
   * @param results 检测结果列表
   */
  void detectionCompleted(const QList<DetectionResult> &results);

  /**
   * @brief 错误信号
   * @param message 错误消息
   */
  void errorOccurred(const QString &message);

private:
  // 预处理图像
  std::vector<float> preprocess(const QImage &frame);

  // 后处理输出
  QList<DetectionResult> postprocess(const std::vector<float> &output,
                                     int originalWidth, int originalHeight);

  // NMS 非极大值抑制
  void applyNMS(QList<DetectionResult> &detections);

  // 根据类别ID推断机器人ID
  int inferRobotId(int classId) const;

  // 获取类别名称
  QString getClassName(int classId) const;

private:
  // --- ONNX Runtime 组件 ---
  std::unique_ptr<Ort::Env> m_env;
  std::unique_ptr<Ort::Session> m_session;
  std::unique_ptr<Ort::SessionOptions> m_sessionOptions;

  // --- 模型配置 ---
  bool m_modelLoaded = false;              // 模型是否已加载
  int m_inputSize = 640;                   // 输入尺寸 (假设正方形)
  int m_numClasses = 8;                    // 类别数量
  std::vector<int64_t> m_inputShape;       // 输入形状
  std::vector<int64_t> m_outputShape;      // 输出形状
  std::vector<const char *> m_inputNames;  // 输入节点名称
  std::vector<const char *> m_outputNames; // 输出节点名称

  // --- 检测参数 ---
  float m_confidenceThreshold = 0.5f; // 置信度阈值
  float m_nmsThreshold = 0.4f;        // NMS IoU 阈值
  float m_lastInferenceTime = 0.0f;   // 上次推理耗时 (毫秒)

  // --- 类别名称 ---
  QStringList m_classNames = {"red_hero",      "red_engineer", "red_infantry",
                              "red_sentry",    "blue_hero",    "blue_engineer",
                              "blue_infantry", "blue_sentry"};

  // --- 线程安全 ---
  mutable QMutex m_mutex;
};

} // namespace RM

#endif // YOLODETECTOR_H
