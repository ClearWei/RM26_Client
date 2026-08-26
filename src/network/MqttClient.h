// SPDX-License-Identifier: MIT
#ifndef RM_MQTTCLIENT_H
#define RM_MQTTCLIENT_H

/**
 * @file MqttClient.h
 * @brief MQTT 客户端封装（可选启用）
 * @author
 * @date 2026-02-04
 * @copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 *
 * 设计说明：
 * - 当构建系统找到 Qt6::Mqtt 时，启用完整 MQTT 功能，用于从 Broker 订阅 Protobuf 二进制消息；
 * - 未找到 Qt6::Mqtt 时，类仍可编译，但为功能空实现，便于主工程不做复杂条件分支；
 * - 连接参数等从 ConfigManager 外部化读取，遵循“配置外部化（必须）”规范。
 */

// ============================================================================
// Qt 头文件
// ============================================================================
#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <QPair>

// ============================================================================
// 项目头文件
// ============================================================================
#include "../core/GameData.h"             // 游戏数据中心，用于分发解析后的消息
#include "../config/ConfigManager.h"      // 配置管理器，读取 MQTT broker/port
#include "robomaster.pb.h"  // Protobuf 生成的消息类

// ============================================================================
// Qt MQTT（可选）
// ============================================================================
#ifdef RM_HAS_QT_MQTT
#include <QMqttClient>
#include <QMqttTopicFilter>
#include <QMqttSubscription>
#endif

namespace RM {

/**
 * @class MqttClient
 * @brief MQTT 客户端，负责连接、订阅、解析消息并分发到 GameData
 */
class MqttClient : public QObject {
  Q_OBJECT

public:
  /**
   * @brief 构造函数
   * @param gameData 游戏数据中心指针（用于分发解析后的消息）
   * @param parent   Qt 父对象
   */
  explicit MqttClient(GameData *gameData, QObject *parent = nullptr);

  /**
   * @brief 启动 MQTT 服务
   * @details 读取配置中的 broker/port，建立连接并订阅默认主题
   */
  void start();

  /**
   * @brief 设置 MQTT clientId
   * @details 若未设置则回退到默认 PID 方案。
   */
  void setClientId(const QString &clientId);

  /**
   * @brief 停止 MQTT 服务
   * @details 断开连接并清理资源
   */
  void stop();

signals:
  /**
   * @brief 连接成功信号
   */
  void connected();

  /**
   * @brief 连接断开信号
   */
  void disconnected();

  /**
   * @brief 收到消息信号（用于调试统计）
   * @param topic 主题名
   * @param bytes 负载字节数
   */
  void messageReceived(const QString &topic, int bytes);

private:
  // --- 基础指针 ---
  GameData *m_gameData = nullptr;         // 游戏数据中心（不拥有所有权）

#ifdef RM_HAS_QT_MQTT
  // --- MQTT 组件 ---
  QMqttClient *m_client = nullptr;        // MQTT 客户端对象
#endif

  QString m_clientId;                     // 自定义 clientId（为空时使用默认值）

  // --- 批量消息处理（避免 UI 线程被高频 MQTT 阻塞） ---
  QTimer *m_processTimer = nullptr;
  QVector<QPair<QString, QByteArray>> m_pendingMessages;

  // --- 订阅主题配置 ---
  QStringList m_defaultTopics{            // 默认订阅主题（可根据服务端约定调整）
      "GameStatus",
      "GlobalUnitStatus",
      "GlobalLogisticsStatus",
      "GlobalSpecialMechanism",
      "Event",
      "RobotInjuryStat",
      "RobotRespawnStatus",
      "RobotStaticStatus",
      "RobotDynamicStatus",
      "RobotModuleStatus",
      "RobotPosition",
      "Buff",
      "RobotPathPlanInfo",
      "RadarInfoToClient",
      "CustomByteBlock",
      "TechCoreMotionStateSync",
      "RobotPerformanceSelectionSync",
      "DeployModeStatusSync",
      "RuneStatusSync",
      "SentryStatusSync",
      "DartSelectTargetStatusSync",
      "SentryCtrlResult",
      "AirSupportStatusSync",
      "PenaltyInfo"
  };

#ifdef RM_HAS_QT_MQTT
private slots:
  /**
   * @brief MQTT 连接成功回调
   */
  void onMqttConnected();

  /**
   * @brief MQTT 断开连接回调
   */
  void onMqttDisconnected();

  /**
   * @brief MQTT 收到消息回调（Qt 信号）
   * @param payload 二进制消息负载
   * @param topic   主题名
   */
  void onMessageReceived(const QByteArray &payload, const QMqttTopicName &topic);

  /**
   * @brief 批量处理待处理消息（定时器触发，避免 UI 线程阻塞）
   */
  void processPendingMessages();
#endif

private:
  /**
   * @brief 订阅默认主题列表
   */
  void subscribeDefaultTopics();

  /**
   * @brief 处理二进制负载（Protobuf 反序列化并分发）
   * @param topic   主题（用于调试或按需路由）
   * @param payload 原始二进制数据
   */
  void handlePayload(const QString &topic, const QByteArray &payload);
};

} // namespace RM

#endif // RM_MQTTCLIENT_H
