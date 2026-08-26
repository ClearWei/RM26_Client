// SPDX-License-Identifier: MIT
/**
 * @file MqttLogger.h
 * @brief MQTT 消息完整日志记录器
 * @details 记录所有非图传的 MQTT 消息（接收和发送）到每次运行独立的 JSONL 日志文件，
 *          用于赛后分析官方客户端的稳定性和消息交互详情。
 *          线程安全：支持从 Paho 回调线程和 Qt 主线程同时写入。
 * @author Clear
 * @date 2026-05-30
 * @copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#ifndef MQTTLOGGER_H
#define MQTTLOGGER_H

#include <QByteArray>
#include <QFile>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

/**
 * @class MqttLogger
 * @brief MQTT 消息日志记录器（单例）
 * @details 每次运行产生一个带时间戳的 JSONL 文件，记录所有非视频 MQTT 消息。
 *
 *          JSONL 格式（每行一条消息）：
 *          {"ts":"2026-05-30T20:48:04.123","dir":"rx","topic":"GameStatus","bytes":256,"hex":"0a2b08..."}
 *
 *          性能设计：
 *          - 内存缓冲写入，500ms 定时刷盘
 *          - 缓冲区超过 200 条立刻触发刷盘
 *          - 使用 QMutex 保证跨线程安全
 *
 *          消息过滤：
 *          - CustomByteBlock 且 payload > 100 字节 = 视频帧，跳过
 *          - 其余所有 MQTT 消息全部记录
 */
class MqttLogger : public QObject {
  Q_OBJECT

public:
  /**
   * @brief 获取单例实例
   */
  static MqttLogger *instance();

  /**
   * @brief 启动新日志会话
   * @details 创建带时间戳的日志文件，启动定时刷盘。
   *          如果已处于活跃状态，先停止当前会话再启动新会话。
   */
  void startSession();

  /**
   * @brief 停止当前日志会话
   * @details 停止定时器，刷盘并关闭文件。
   */
  void stopSession();

  /**
   * @brief 检查是否处于活跃记录状态
   */
  bool isActive() const;

  /**
   * @brief 记录接收到的 MQTT 消息（线程安全）
   * @param topic   主题名称
   * @param payload 消息载荷
   * @note 可从任意线程调用，内部有 mutex 保护
   */
  void logRx(const QString &topic, const QByteArray &payload);

  /**
   * @brief 记录发送的 MQTT 消息（线程安全）
   * @param topic   主题名称
   * @param payload 消息载荷
   * @note 可从任意线程调用，内部有 mutex 保护
   */
  void logTx(const QString &topic, const QByteArray &payload);

private slots:
  /**
   * @brief 定时刷盘：将缓冲区的日志行写入文件
   */
  void flushBuffer();

private:
  explicit MqttLogger(QObject *parent = nullptr);
  ~MqttLogger() override;

  // 禁止拷贝
  MqttLogger(const MqttLogger &) = delete;
  MqttLogger &operator=(const MqttLogger &) = delete;

  /**
   * @brief 判断是否应跳过该消息（视频帧过滤）
   */
  static bool shouldSkip(const QString &topic, const QByteArray &payload);

  /**
   * @brief 将一条 JSONL 行写入缓冲区（带 mutex）
   */
  void appendLine(const QString &line);

  // --- 成员变量 ---
  QFile m_file;               ///< 日志文件句柄
  QTimer m_flushTimer;        ///< 定时刷盘定时器
  QStringList m_buffer;       ///< 日志行缓冲区
  mutable QMutex m_mutex;     ///< 缓冲区访问锁
  bool m_isActive = false;    ///< 是否处于活跃记录状态
  QString m_logFilePath;      ///< 当前日志文件路径

  static constexpr int MAX_BUFFER_SIZE = 200;    ///< 缓冲区最大行数
  static constexpr int FLUSH_INTERVAL_MS = 500;  ///< 刷盘间隔（毫秒）
};

#endif // MQTTLOGGER_H
