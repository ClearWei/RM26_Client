// SPDX-License-Identifier: MIT
/**
 * @file MqttManager.cpp
 * @brief MQTT 客户端管理器实现
 * @details 封装 Eclipse Paho MQTT C 客户端，实现与裁判系统服务器的 MQTT 通信。
 * @author Clear
 * @date 2026-02-08
 * @copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#include "MqttManager.h"
#include "MqttLogger.h"
#include <QCryptographicHash>
#include <QDebug>
#include <QDateTime>
#include <QMetaObject>
#include <QTcpSocket>
#include <QThreadPool>
#include <QUrl>

namespace {

bool verboseVideoLogEnabled() {
  static const bool enabled = qEnvironmentVariableIsSet("RM_VERBOSE_VIDEO_LOG");
  return enabled;
}

bool isHighRateVideoMessage(const QString &topic, const QByteArray &payload) {
  return topic == QLatin1String("CustomByteBlock") && payload.size() > 100;
}

} // namespace

// ============================================================================
// 构造函数 / 析构函数
// ============================================================================

MqttManager::MqttManager(QObject *parent) : QObject(parent) {
  m_client = nullptr;
  // 初始化重连定时器
  m_reconnectTimer = new QTimer(this);
  m_reconnectTimer->setInterval(m_reconnectInterval);
  connect(m_reconnectTimer, &QTimer::timeout, this, &MqttManager::tryReconnect);
}

MqttManager::~MqttManager() { disconnect(); }

// ============================================================================
// 连接管理
// ============================================================================

void MqttManager::connectToBrokerAsync(const QString &serverUri,
                                       const QString &clientId) {
#ifdef RM_HAS_MQTT
  if (m_connecting) {
    qWarning() << "MqttManager: connectToBroker already in progress, skipping";
    emit connectCompleted(false, QStringLiteral("Connection already in progress"));
    return;
  }
  m_connecting = true;
  emit connectingStarted();

  m_serverUri = serverUri;
  m_clientId = clientId;

  // 创建 MQTT 客户端（指定 MQTT 3.1.1 版本）
  MQTTClient_createOptions opts = MQTTClient_createOptions_initializer;
  opts.MQTTVersion = MQTTVERSION_3_1_1;
  int rc = MQTTClient_createWithOptions(&m_client, m_serverUri.toUtf8().constData(),
                                        m_clientId.toUtf8().constData(),
                                        MQTTCLIENT_PERSISTENCE_NONE, nullptr, &opts);
  if (rc != MQTTCLIENT_SUCCESS) {
    qWarning() << "MqttManager: Failed to create MQTT client, rc=" << rc;
    emit errorOccurred(QString("Failed to create MQTT client: %1").arg(rc));
    m_connecting = false;
    emit connectCompleted(false, QString("Failed to create MQTT client: %1").arg(rc));
    return;
  }

  // 设置回调函数
  rc =
      MQTTClient_setCallbacks(m_client, this, connectionLostCallback,
                              messageArrivedCallback, deliveryCompleteCallback);
  if (rc != MQTTCLIENT_SUCCESS) {
    qWarning() << "MqttManager: Failed to set callbacks, rc=" << rc;
    emit errorOccurred(QString("Failed to set callbacks: %1").arg(rc));
    m_connecting = false;
    emit connectCompleted(false, QString("Failed to set callbacks: %1").arg(rc));
    return;
  }

  // 配置连接选项
  MQTTClient_connectOptions connOpts = MQTTClient_connectOptions_initializer;
  connOpts.keepAliveInterval = 20; // 心跳间隔 20秒
  connOpts.cleansession = 1;       // 每次连接清除会话
  connOpts.connectTimeout = 2;     // 连接超时 2秒，避免 UI 在切换机器人时长时间卡顿
  connOpts.MQTTVersion = MQTTVERSION_3_1_1;  // 使用 MQTT 3.1.1（大多数 broker 要求）

  // 连接到 Broker
  qDebug() << "MqttManager: Connecting to" << m_serverUri;
  qInfo() << "MqttManager: Using clientId=" << m_clientId;

  // 预检：尝试解析 URI 并测试 TCP 连通性
  QUrl url(m_serverUri);
  if (!url.isValid()) {
    qWarning() << "MqttManager: [DIAGNOSIS] Invalid URI format:" << m_serverUri;
  } else {
    qDebug() << "MqttManager: Parsed host=" << url.host() << "port=" << url.port();

    // 切换机器人时会频繁切换 clientId，这里避免做 5s 的同步 TCP 预检，
    // 否则 UI 线程会明显卡顿；实际连通性由 MQTTClient_connect 返回值兜底。
    qDebug() << "MqttManager: Skip blocking TCP preflight check";
  }

  // 真正异步执行 MQTTClient_connect：后台线程执行 TCP 连接，
  // 主线程通过 onConnectCompleted 接收结果，完全无阻塞。
  QThreadPool::globalInstance()->start([this, connOpts]() mutable {
    int result = MQTTClient_connect(m_client, &connOpts);
    QMetaObject::invokeMethod(this, [this, result]() {
      onConnectCompleted(result);
    }, Qt::QueuedConnection);
  });
#else
  Q_UNUSED(serverUri);
  Q_UNUSED(clientId);
  qWarning() << "MqttManager: MQTT disabled at compile time";
  emit connectCompleted(false, QStringLiteral("MQTT disabled at compile time"));
#endif
}

void MqttManager::disconnect() {
  m_reconnectTimer->stop();

#ifdef RM_HAS_MQTT
  if (m_client && m_connected) {
    // 缩短断开等待，降低切换机器人时 UI 卡顿感
    MQTTClient_disconnect(m_client, 100);
    m_connected = false;
    emit connectionStateChanged(false);
  }

  if (m_client) {
    MQTTClient_destroy(&m_client);
    m_client = nullptr;
  }
#endif

  qDebug() << "MqttManager: Disconnected";
}

bool MqttManager::isConnected() const {
#ifdef RM_HAS_MQTT
  return m_connected && m_client && MQTTClient_isConnected(m_client);
#else
  return false;
#endif
}

// ============================================================================
// 订阅 / 发布
// ============================================================================

bool MqttManager::subscribe(const QString &topic, int qos) {
  if (!isConnected()) {
    qWarning() << "MqttManager: Cannot subscribe, not connected";
    return false;
  }

#ifdef RM_HAS_MQTT
  int rc = MQTTClient_subscribe(m_client, topic.toUtf8().constData(), qos);
  if (rc != MQTTCLIENT_SUCCESS) {
    qWarning() << "MqttManager: Failed to subscribe to" << topic
               << ", rc=" << rc;
    emit errorOccurred(
        QString("Failed to subscribe to %1: %2").arg(topic).arg(rc));
    return false;
  }

  qDebug() << "MqttManager: Subscribed to" << topic;
  return true;
#else
  Q_UNUSED(topic);
  Q_UNUSED(qos);
  return false;
#endif
}

bool MqttManager::publish(const QString &topic, const QByteArray &payload,
                          int qos, int waitForCompletionMs) {
  const qint64 publishStartMs = QDateTime::currentMSecsSinceEpoch();
  if (!isConnected()) {
    qWarning() << "MqttManager: Cannot publish, not connected";
    const qint64 publishDoneMs = QDateTime::currentMSecsSinceEpoch();
    qInfo().noquote()
        << QStringLiteral(
               "RM26_COMMAND_TIMING direction=tx topic=%1 start_ms=%2 "
               "queued_ms=%3 done_ms=%4 publish_call_ms=%5 wait_ms=%6 "
               "qos=%7 bytes=%8 ok=0 error=not_connected")
               .arg(topic)
               .arg(publishStartMs)
               .arg(-1)
               .arg(publishDoneMs)
               .arg(-1)
               .arg(-1)
               .arg(qos)
               .arg(payload.size());
    return false;
  }

#ifdef RM_HAS_MQTT
  MQTTClient_message pubmsg = MQTTClient_message_initializer;
  pubmsg.payload = (void *)payload.constData();
  pubmsg.payloadlen = payload.size();
  pubmsg.qos = qos;
  pubmsg.retained = 0;

  MQTTClient_deliveryToken token;
  int rc = MQTTClient_publishMessage(m_client, topic.toUtf8().constData(),
                                     &pubmsg, &token);
  const qint64 publishQueuedMs = QDateTime::currentMSecsSinceEpoch();
  if (rc != MQTTCLIENT_SUCCESS) {
    qWarning() << "MqttManager: Failed to publish to" << topic << ", rc=" << rc;
    const qint64 publishDoneMs = QDateTime::currentMSecsSinceEpoch();
    qInfo().noquote()
        << QStringLiteral(
               "RM26_COMMAND_TIMING direction=tx topic=%1 start_ms=%2 "
               "queued_ms=%3 done_ms=%4 publish_call_ms=%5 wait_ms=%6 "
               "qos=%7 bytes=%8 ok=0 rc=%9")
               .arg(topic)
               .arg(publishStartMs)
               .arg(publishQueuedMs)
               .arg(publishDoneMs)
               .arg(publishQueuedMs - publishStartMs)
               .arg(-1)
               .arg(qos)
               .arg(payload.size())
               .arg(rc);
    return false;
  }

  qint64 waitMs = 0;
  if (waitForCompletionMs > 0) {
    const qint64 waitStartMs = QDateTime::currentMSecsSinceEpoch();
    rc = MQTTClient_waitForCompletion(m_client, token,
                                      static_cast<unsigned long>(waitForCompletionMs));
    waitMs = QDateTime::currentMSecsSinceEpoch() - waitStartMs;
    if (rc != MQTTCLIENT_SUCCESS) {
      qWarning() << "MqttManager: Message delivery timeout for" << topic;
      const qint64 publishDoneMs = QDateTime::currentMSecsSinceEpoch();
      qInfo().noquote()
          << QStringLiteral(
                 "RM26_COMMAND_TIMING direction=tx topic=%1 start_ms=%2 "
                 "queued_ms=%3 done_ms=%4 publish_call_ms=%5 wait_ms=%6 "
                 "qos=%7 bytes=%8 ok=0 rc=%9")
                 .arg(topic)
                 .arg(publishStartMs)
                 .arg(publishQueuedMs)
                 .arg(publishDoneMs)
                 .arg(publishQueuedMs - publishStartMs)
                 .arg(waitMs)
                 .arg(qos)
                 .arg(payload.size())
                 .arg(rc);
      return false;
    }
  }

  const qint64 publishDoneMs = QDateTime::currentMSecsSinceEpoch();
  qInfo().noquote()
      << QStringLiteral(
             "RM26_COMMAND_TIMING direction=tx topic=%1 start_ms=%2 "
             "queued_ms=%3 done_ms=%4 publish_call_ms=%5 wait_ms=%6 "
             "qos=%7 bytes=%8 ok=1 rc=0")
             .arg(topic)
             .arg(publishStartMs)
             .arg(publishQueuedMs)
             .arg(publishDoneMs)
             .arg(publishQueuedMs - publishStartMs)
             .arg(waitMs)
             .arg(qos)
             .arg(payload.size());

  MqttLogger::instance()->logTx(topic, payload);
  return true;
#else
  Q_UNUSED(topic);
  Q_UNUSED(payload);
  Q_UNUSED(qos);
  Q_UNUSED(waitForCompletionMs);
  return false;
#endif
}

// ============================================================================
// 回调函数
// ============================================================================

#ifdef RM_HAS_MQTT

int MqttManager::messageArrivedCallback(void *context, char *topicName,
                                        int topicLen,
                                        MQTTClient_message *message) {
  Q_UNUSED(topicLen);
  MqttManager *self = static_cast<MqttManager *>(context);
  if (!self) {
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
  }

  QString topic = QString::fromUtf8(topicName);
  QByteArray payload((const char *)message->payload, message->payloadlen);
  const qint64 receivedMs = QDateTime::currentMSecsSinceEpoch();
  const bool detailedTrace =
      !isHighRateVideoMessage(topic, payload) || verboseVideoLogEnabled();
  bool sampledVideoObservation = false;
  if (!detailedTrace) {
    qint64 lastObservedMs =
        self->m_lastVideoObservationMs.load(std::memory_order_relaxed);
    if (receivedMs - lastObservedMs >= 1000 &&
        self->m_lastVideoObservationMs.compare_exchange_strong(
            lastObservedMs, receivedMs, std::memory_order_relaxed)) {
      sampledVideoObservation = true;
    }
  }
  const bool observeMessage = detailedTrace || sampledVideoObservation;
  QString payloadSha1;
  if (observeMessage) {
    payloadSha1 = QString::fromLatin1(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha1).toHex());
    if (detailedTrace) {
      qDebug() << "MqttManager: Message received on topic:" << topic
               << ", size:" << payload.size();
    }
    qInfo().noquote()
        << QStringLiteral(
               "RM26_MQTT_RX_TIMING direction=rx topic=%1 received_ms=%2 "
               "bytes=%3 sha1=%4")
               .arg(topic)
               .arg(receivedMs)
               .arg(payload.size())
               .arg(payloadSha1);
  }

  // 记录完整的 MQTT 消息到日志文件（排除视频帧）
  MqttLogger::instance()->logRx(topic, payload);

  QMetaObject::invokeMethod(
      self,
      [self, topic, payload, payloadSha1, receivedMs, observeMessage]() {
        if (observeMessage) {
          emit self->messageObserved(topic, payload.size(), payloadSha1,
                                     receivedMs);
        }
        emit self->messageReceived(topic, payload);
      },
      Qt::QueuedConnection);

  MQTTClient_freeMessage(&message);
  MQTTClient_free(topicName);
  return 1;
}

void MqttManager::connectionLostCallback(void *context, char *cause) {
  MqttManager *self = static_cast<MqttManager *>(context);
  if (!self)
    return;

  QString reason = cause ? QString::fromUtf8(cause) : "Unknown";
  QMetaObject::invokeMethod(
      self,
      [self, reason]() {
        self->m_connected = false;
        qWarning() << "MqttManager: Connection lost, reason:" << reason;

        emit self->connectionStateChanged(false);
        emit self->errorOccurred(QString("Connection lost: %1").arg(reason));

        // 通过 QObject 所在线程启动重连定时器，避免 Paho 回调线程直接碰 Qt timer
        self->m_reconnectTimer->start();
      },
      Qt::QueuedConnection);
}

void MqttManager::deliveryCompleteCallback(void *context,
                                           MQTTClient_deliveryToken token) {
  Q_UNUSED(context);
  Q_UNUSED(token);
  // 消息发送完成，可用于日志或调试
}

// ============================================================================
// 异步连接完成处理（在主线程执行）
// ============================================================================

void MqttManager::onConnectCompleted(int rc) {
  m_connecting = false;

  if (rc != MQTTCLIENT_SUCCESS) {
    qWarning() << "MqttManager: Failed to connect, rc=" << rc;
    QString errorDetail =
        QStringLiteral("MQTT connect failed (rc=%1, clientId=%2, broker=%3)")
            .arg(rc)
            .arg(m_clientId, m_serverUri);
    bool shouldRetry = true;

    // CONNACK 错误码诊断 (Paho MQTT C 返回码 1-5 表示 broker 拒绝)
    if (rc == 1) {
      qWarning() << "MqttManager: [DIAGNOSIS] CONNACK(1): Bad protocol version";
      errorDetail = QStringLiteral("MQTT CONNACK(1): bad protocol version");
      shouldRetry = false;
    } else if (rc == 2) {
      qWarning() << "MqttManager: [DIAGNOSIS] CONNACK(2): Identifier rejected - clientId '"
                 << m_clientId << "' was rejected by broker";
      qWarning() << "MqttManager: [SUGGESTION] Override with RM_CLIENT_ROBOT_ID=<new_id>"
                 << "(PowerShell: $env:RM_CLIENT_ROBOT_ID=<new_id>)";
      qWarning() << "MqttManager: [SUGGESTION] If the selected robot ID is correct,"
                 << "check the engine custom-client registration/IP whitelist and"
                 << "verify RM_MQTT_BROKER/RM_MQTT_PORT point to the active engine broker";
      errorDetail =
          QStringLiteral("MQTT CONNACK(2): identifier rejected for clientId=%1 "
                         "(broker=%2). Check network.client_robot_id/RM_CLIENT_ROBOT_ID; "
                         "if the ID is correct, check engine custom-client registration, "
                         "client IP whitelist, or active broker address.")
              .arg(m_clientId, m_serverUri);
      shouldRetry = false;
    } else if (rc == 3) {
      qWarning() << "MqttManager: [DIAGNOSIS] CONNACK(3): Server unavailable";
      errorDetail = QStringLiteral("MQTT CONNACK(3): server unavailable");
    } else if (rc == 4) {
      qWarning() << "MqttManager: [DIAGNOSIS] CONNACK(4): Bad username or password";
      errorDetail = QStringLiteral("MQTT CONNACK(4): bad username or password");
      shouldRetry = false;
    } else if (rc == 5) {
      qWarning() << "MqttManager: [DIAGNOSIS] CONNACK(5): Not authorized";
      errorDetail = QStringLiteral("MQTT CONNACK(5): not authorized");
      shouldRetry = false;
    } else if (rc == 133) {
      qWarning() << "MqttManager: [DIAGNOSIS] CONNACK(133): Client identifier not valid - clientId '"
                 << m_clientId << "' was rejected by broker";
      qWarning() << "MqttManager: [SUGGESTION] Use the robot ID accepted by the current field broker"
                 << "(override with RM_CLIENT_ROBOT_ID=<id>)";
      errorDetail =
          QStringLiteral("MQTT CONNACK(133): clientId=%1 not valid for broker=%2. "
                         "Check network.client_robot_id or RM_CLIENT_ROBOT_ID.")
              .arg(m_clientId, m_serverUri);
      shouldRetry = false;
    } else if (rc == 134) {
      qWarning() << "MqttManager: [DIAGNOSIS] CONNACK(134): Bad username or password";
      errorDetail = QStringLiteral("MQTT CONNACK(134): bad username or password");
      shouldRetry = false;
    } else if (rc == 135) {
      qWarning() << "MqttManager: [DIAGNOSIS] CONNACK(135): Not authorized";
      errorDetail = QStringLiteral("MQTT CONNACK(135): not authorized");
      shouldRetry = false;
    } else if (rc == 136) {
      qWarning() << "MqttManager: [DIAGNOSIS] CONNACK(136): Server unavailable";
      errorDetail = QStringLiteral("MQTT CONNACK(136): server unavailable");
    } else if (rc == -1) {
      qWarning() << "MqttManager: [DIAGNOSIS] Network/transport error (code: -1)";
      qWarning() << "MqttManager: [SUGGESTION] This usually means:";
      qWarning() << "  - TCP connection refused (broker not running)";
      qWarning() << "  - Network unreachable";
      qWarning() << "  - SSL/TLS handshake failure (if using ssl://)";
      errorDetail = QStringLiteral("MQTT transport error (rc=-1): connection refused/unreachable");
    } else if (rc < 0) {
      qWarning() << "MqttManager: [DIAGNOSIS] Network/transport error (code:" << rc << ")";
      errorDetail = QStringLiteral("MQTT transport error (rc=%1)").arg(rc);
    }

    // 清理失败的客户端句柄，避免下次重连时创建新句柄导致旧句柄泄漏
    if (m_client) {
      MQTTClient_destroy(&m_client);
      m_client = nullptr;
    }

    if (shouldRetry && m_retryCount < 3) {
      m_retryCount++;
      m_reconnectTimer->start();
    } else if (m_retryCount >= 3) {
      qWarning() << "MqttManager: Max retry attempts reached, giving up";
      m_retryCount = 0;
    }

    emit errorOccurred(errorDetail);
    emit connectCompleted(false, errorDetail);
    return;
  }

  // 连接成功
  m_connected = true;
  m_retryCount = 0;
  m_reconnectTimer->stop();
  qDebug() << "MqttManager: Connected successfully";
  emit connectionStateChanged(true);
  emit connectCompleted(true, QString());
}

// ============================================================================
// 重连逻辑
// ============================================================================

void MqttManager::tryReconnect() {
  if (isConnected()) {
    m_reconnectTimer->stop();
    return;
  }

  if (m_connecting) {
    qDebug() << "MqttManager: Reconnect skipped, already connecting";
    return;
  }

  qDebug() << "MqttManager: Attempting to reconnect...";

#ifdef RM_HAS_MQTT
  // 销毁旧客户端
  if (m_client) {
    MQTTClient_destroy(&m_client);
    m_client = nullptr;
  }
#endif

  // 重新连接
  connectToBrokerAsync(m_serverUri, m_clientId);
}

#endif // RM_HAS_MQTT
