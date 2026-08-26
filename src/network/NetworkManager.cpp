// SPDX-License-Identifier: MIT
/**
 * @file NetworkManager.cpp
 * @brief 网络通信管理器实现文件
 * @details 本文件实现了 NetworkManager 类的所有成员函数。
 *          主要包括 UDP 套接字的初始化、数据收发、以及数据解析分发逻辑。
 *
 * @author Clear
 * @date 2025-11-29
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

// --- 头文件包含 ---
#include "NetworkManager.h"
#include "../core/CustomDataTypes.h"  // 自定义数据结构定义
#include "../core/LogFileNaming.h"
#include "../config/ConfigManager.h" // 配置管理器，获取服务器 IP 和端口
#include "MqttLogger.h"
#include "robomaster.pb.h" // Protobuf 生成的消息类
#include <QCoreApplication>
#include <QDebug>                    // Qt 调试输出
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkProxy>
#include <cstring>
#include <QDateTime>
#include <algorithm>

namespace RM {

#ifdef RM_HAS_MQTT
namespace {
bool verboseVideoLogEnabled() {
  static const bool enabled = qEnvironmentVariableIsSet("RM_VERBOSE_VIDEO_LOG");
  return enabled;
}

bool isHighRateVideoMessage(const QString &topic, const QByteArray &payload) {
  return topic == QLatin1String("CustomByteBlock") && payload.size() > 100;
}

template <typename RepeatedFieldT>
QJsonArray repeatedFieldToJsonArray(const RepeatedFieldT &field) {
  QJsonArray array;
  for (const auto value : field) {
    array.append(static_cast<int>(value));
  }
  return array;
}

template <typename MessageT>
QByteArray serializeMessage(const MessageT &message) {
  std::string buffer;
  if (!message.SerializeToString(&buffer)) {
    return {};
  }
  return QByteArray(buffer.data(), static_cast<int>(buffer.size()));
}

bool shouldThrottleCommand(qint64 &lastSentMs, qint64 minIntervalMs) {
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  if (lastSentMs > 0 && (nowMs - lastSentMs) < minIntervalMs) {
    return true;
  }
  lastSentMs = nowMs;
  return false;
}

QString mqttTopicForMessage(const robomaster::RoboMasterMessage &message) {
  switch (message.payload_case()) {
  case robomaster::RoboMasterMessage::kMapClick:
    return QStringLiteral("MapClickInfoNotify");
  case robomaster::RoboMasterMessage::kCommonCommand:
    return QStringLiteral("CommonCommand");
  default:
    return {};
  }
}

QString airSupportCommandName(int commandId) {
  switch (commandId) {
  case 0:
    return QStringLiteral("interrupt");
  case 1:
    return QStringLiteral("free");
  case 2:
    return QStringLiteral("paid");
  default:
    return QStringLiteral("unknown");
  }
}

QString assemblyOperationName(int operation) {
  switch (operation) {
  case 0:
    return QStringLiteral("start_exchange");
  case 1:
    return QStringLiteral("confirm_assembly");
  case 2:
    return QStringLiteral("cancel_assembly");
  default:
    return QStringLiteral("unknown");
  }
}

QString resolveRepoRootForMqttDump() {
  const QString overrideRoot =
      qEnvironmentVariable("RM_MQTT_DUMP_ROOT").trimmed();
  if (!overrideRoot.isEmpty()) {
    return overrideRoot;
  }

  QDir dir(QCoreApplication::applicationDirPath());
  for (int i = 0; i < 8; ++i) {
    const bool hasConfig = QFileInfo::exists(dir.filePath("config.json"));
    const bool hasSourceDir = QFileInfo(dir.filePath("src")).isDir();
    if (hasConfig && hasSourceDir) {
      return dir.absolutePath();
    }
    if (!dir.cdUp()) {
      break;
    }
  }

  return QDir::currentPath();
}

QString mqttDumpFilePath() {
  static const QString path = []() {
    QDir root(resolveRepoRootForMqttDump());
    root.mkpath(QStringLiteral("tmp/log"));
    return timestampedFilePath(
        root.filePath(QStringLiteral("tmp/log/mqtt_rx.log")));
  }();
  return path;
}

QJsonObject mqttDumpBase(const QString &stage, const QString &topic,
                         const QByteArray &payload, const GameData *gameData) {
  QJsonObject obj;
  obj.insert(QStringLiteral("ts"),
             QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
  obj.insert(QStringLiteral("stage"), stage);
  obj.insert(QStringLiteral("topic"), topic);
  obj.insert(QStringLiteral("bytes"), payload.size());
  obj.insert(QStringLiteral("prefix_hex"),
             QString::fromLatin1(payload.left(24).toHex()));
  if (gameData) {
    obj.insert(QStringLiteral("current_robot_id"), gameData->getMyRobotId());
  }
  return obj;
}

void appendMqttDump(QJsonObject obj) {
  static QMutex mutex;
  QMutexLocker locker(&mutex);

  const QString path = mqttDumpFilePath();
  QFileInfo info(path);
  QDir().mkpath(info.absolutePath());

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    qWarning() << "NetworkManager: failed to open MQTT dump file" << path;
    return;
  }

  file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
  file.write("\n");
  file.flush();
}

void dumpMqttEvent(const QString &stage, const QString &topic,
                   const QByteArray &payload, const GameData *gameData,
                   const QString &summary = QString(),
                   const QJsonObject &extra = QJsonObject()) {
  // 视频分片每秒可达数百条。默认禁止逐片 open/write/flush；需要抓取完整
  // 诊断证据时显式设置 RM_VERBOSE_VIDEO_LOG=1。
  if (isHighRateVideoMessage(topic, payload) && !verboseVideoLogEnabled()) {
    return;
  }
  QJsonObject obj = mqttDumpBase(stage, topic, payload, gameData);
  if (!summary.isEmpty()) {
    obj.insert(QStringLiteral("summary"), summary);
  }
  for (auto it = extra.begin(); it != extra.end(); ++it) {
    obj.insert(it.key(), it.value());
  }
  appendMqttDump(obj);
}
} // namespace
#endif

// --- 构造与析构 ---

/**
 * @brief 构造函数实现
 * @details 执行以下初始化步骤：
 *          1. 调用父类构造函数
 *          2. 保存 GameData 指针
 *          3. 创建 QUdpSocket 实例
 *          4. 连接 readyRead 信号到 onReadyRead 槽
 *
 * @param gameData 比赛数据中心指针
 * @param parent   Qt 父对象
 */
NetworkManager::NetworkManager(GameData *gameData, QObject *parent)
    : QObject(parent), m_gameData(gameData) {
  // 创建 UDP 套接字，设置 this 为父对象以便自动内存管理
  m_udpSocket = new QUdpSocket(this);
  // 环境里可能存在 HTTP/SOCKS 代理；UDP bind 在代理模式下会失败。
  m_udpSocket->setProxy(QNetworkProxy::NoProxy);

  // 连接信号槽：当有数据可读时调用 onReadyRead
  connect(m_udpSocket, &QUdpSocket::readyRead, this,
          &NetworkManager::onReadyRead);
}

/**
 * @brief 析构函数实现
 * @details 确保在对象销毁前调用 stopListening 关闭套接字。
 *          QUdpSocket 由于设置了父对象，会在父对象销毁时自动删除。
 */
NetworkManager::~NetworkManager() { stopListening(); }

// --- 连接控制函数 ---

/**
 * @brief 启动 UDP 监听
 * @details 尝试将 UDP 套接字绑定到指定端口。
 *
 *          绑定过程：
 *          1. 调用 QUdpSocket::bind(QHostAddress::Any, port)
 *          2. QHostAddress::Any 表示监听所有可用网络接口
 *          3. 成功则输出调试信息，失败则发送错误信号
 *
 * @param port 要监听的端口号
 * @return true 绑定成功，false 绑定失败
 */
bool NetworkManager::startListening(quint16 port) {
  if (!m_udpSocket) {
    qCritical() << "NetworkManager: UDP socket is null";
    emit errorOccurred(QStringLiteral("UDP socket is null"));
    return false;
  }

  if (m_udpSocket->state() != QAbstractSocket::UnconnectedState ||
      m_udpSocket->localPort() != 0) {
    qInfo() << "NetworkManager: closing stale UDP socket before rebinding"
            << "state=" << m_udpSocket->state()
            << "localPort=" << m_udpSocket->localPort();
    m_udpSocket->close();
  }

  // 尝试绑定端口
  if (m_udpSocket->bind(QHostAddress::Any, port,
                        QUdpSocket::ShareAddress |
                            QUdpSocket::ReuseAddressHint)) {
    qDebug() << "NetworkManager: Listening on port" << port;
    return true;
  } else {
    // 绑定失败，输出错误日志并发送信号
    qCritical() << "NetworkManager: Failed to bind port" << port
                << "error=" << m_udpSocket->errorString();
    emit errorOccurred(m_udpSocket->errorString());
    return false;
  }
}

/**
 * @brief 停止 UDP 监听
 * @details 如果套接字当前处于已连接状态，则关闭它。
 *          检查状态避免重复关闭导致的警告。
 */
void NetworkManager::stopListening() {
  // 只有在套接字未处于未连接状态时才需要关闭
  if (m_udpSocket->state() != QAbstractSocket::UnconnectedState) {
    m_udpSocket->close();
    qDebug() << "NetworkManager: Stopped listening";
  }
}

/**
bool NetworkManager::isUdpListening() const {
  return m_udpSocket && m_udpSocket->state() == QAbstractSocket::BoundState;
}

quint16 NetworkManager::listeningPort() const {
  return m_udpSocket ? m_udpSocket->localPort() : 0;
}
*/
bool NetworkManager::isUdpListening() const {
  return m_udpSocket && m_udpSocket->state() == QAbstractSocket::BoundState;
}

quint16 NetworkManager::listeningPort() const {
  return m_udpSocket ? m_udpSocket->localPort() : 0;
}

// --- 数据发送函数 ---

/**
 * @brief 发送原始字节数据
 * @details 直接将 QByteArray 数据发送到指定的目标地址和端口。
 *          该方法适用于：
 *          - 已经序列化好的数据
 *          - 自定义格式的数据
 *          - 转发从其他来源接收的数据
 *
 * @param data    待发送的数据
 * @param address 目标 IP 地址
 * @param port    目标端口号
 */
void NetworkManager::sendData(const QByteArray &data,
                              const QHostAddress &address, quint16 port) {
#ifdef RM_HAS_MQTT
  if (m_mqttManager && m_mqttManager->isConnected()) {
    robomaster::CustomControl message;
    message.set_data(data.constData(), static_cast<size_t>(data.size()));

    const QByteArray payload = serializeMessage(message);
    if (payload.isEmpty()) {
      qWarning() << "NetworkManager: Failed to serialize CustomControl payload";
      return;
    }

    m_mqttManager->publish(QStringLiteral("CustomControl"), payload, 1);
    emit dataSent(payload);
    return;
  }
  Q_UNUSED(address);
  Q_UNUSED(port);
  qWarning() << "NetworkManager: MQTT mode enabled but broker not connected, dropping raw payload";
  return;
#endif
  // 调用 QUdpSocket 发送数据报
  m_udpSocket->writeDatagram(data, address, port);

  // 发送 dataSent 信号，用于调试追踪
  emit dataSent(data);
}

/**
 * @brief 发送 Protobuf 消息
 * @details 将 RoboMasterMessage 对象序列化为字节数组后发送。
 *          目标服务器地址从 ConfigManager 单例获取。
 *
 *          发送流程：
 *          1. 调用 Protocol::serializePacket 序列化消息
 *          2. 检查序列化结果是否为空
 *          3. 从 ConfigManager 获取服务器 IP 和端口
 *          4. 发送数据包
 *
 * @param message 待发送的 Protobuf 消息
 * @param address 目标地址（实际不使用，保留用于接口一致性）
 * @param port    目标端口（实际不使用，保留用于接口一致性）
 */
void NetworkManager::sendData(const robomaster::RoboMasterMessage &message,
                              const QHostAddress &address, quint16 port) {
#ifdef RM_HAS_MQTT
  if (m_mqttManager && m_mqttManager->isConnected()) {
    const QString topic = mqttTopicForMessage(message);
    if (topic.isEmpty()) {
      qWarning() << "NetworkManager: Unsupported official MQTT payload case:"
                 << message.payload_case();
      return;
    }

    QByteArray payload;
    switch (message.payload_case()) {
    case robomaster::RoboMasterMessage::kMapClick:
      payload = serializeMessage(message.map_click());
      break;
    case robomaster::RoboMasterMessage::kCommonCommand:
      payload = serializeMessage(message.common_command());
      break;
    default:
      break;
    }

    if (payload.isEmpty()) {
      qWarning() << "NetworkManager: Failed to serialize message for MQTT topic"
                 << topic;
      return;
    }

    m_mqttManager->publish(topic, payload, 1);
    emit dataSent(payload);
    return;
  }
  Q_UNUSED(address);
  Q_UNUSED(port);
  qWarning() << "NetworkManager: MQTT mode enabled but broker not connected, dropping message payload";
  return;
#endif

  // 序列化 Protobuf 消息为字节数组
  QByteArray packet = Protocol::serializePacket(message);

  if (!packet.isEmpty()) {
    // 从配置中获取服务器地址
    QString serverIp = ConfigManager::instance().getServerIP();
    quint16 serverPort = ConfigManager::instance().getServerPort();

    // 发送到服务器
    m_udpSocket->writeDatagram(packet, QHostAddress(serverIp), serverPort);
    emit dataSent(packet);
  } else {
    // 序列化失败警告
    qWarning() << "NetworkManager: Failed to serialize message for sending";
  }
}

/**
 * @brief 发送地图标记消息
 * @details 旧版 UDP 接口。
 *          官方 V1.3.0 MQTT 模式下应改用 MapClickInfoNotify。
 *
 * @param x    标记点 X 坐标
 * @param y    标记点 Y 坐标
 * @param type 标记类型（攻击/防守/其他）
 */
void NetworkManager::sendMapMarking(float x, float y, int type) {
#ifdef RM_HAS_MQTT
  if (m_mqttManager) {
    qWarning() << "NetworkManager: MapMarking is a legacy UDP-only message and"
                  " is not part of the official MQTT custom client protocol";
    return;
  }
#endif

  // 创建消息对象
  robomaster::RoboMasterMessage msg;

  // 设置 map_marking 字段
  auto *marking = msg.mutable_map_marking();
  marking->set_x(x);
  marking->set_y(y);
  marking->set_mark_type(type);

  // 序列化并发送
  QByteArray packet = Protocol::serializePacket(msg);
  if (!packet.isEmpty()) {
    QString serverIp = ConfigManager::instance().getServerIP();
    quint16 serverPort = ConfigManager::instance().getServerPort();
    m_udpSocket->writeDatagram(packet, QHostAddress(serverIp), serverPort);
    emit dataSent(packet);
  } else {
    qWarning() << "NetworkManager: Failed to serialize map marking message";
  }
}

/**
 * @brief 发送机器人控制指令
 * @details 旧版 UDP 接口。
 *          官方 V1.3.0 MQTT 模式下不再使用 RobotCommand。
 *
 * @param cmdType  指令类型编码
 * @param targetId 目标机器人 ID
 */
void NetworkManager::sendRobotCommand(int cmdType, int targetId) {
#ifdef RM_HAS_MQTT
  if (m_mqttManager) {
    qWarning() << "NetworkManager: RobotCommand is a legacy UDP-only message and"
                  " is not part of the official MQTT custom client protocol";
    return;
  }
#endif

  // 创建消息对象
  robomaster::RoboMasterMessage msg;

  // 设置 robot_cmd 字段
  auto *cmd = msg.mutable_robot_cmd();
  cmd->set_cmd_type(cmdType);
  cmd->set_target_id(targetId);

  // 序列化并发送
  QByteArray packet = Protocol::serializePacket(msg);
  if (!packet.isEmpty()) {
    QString serverIp = ConfigManager::instance().getServerIP();
    quint16 serverPort = ConfigManager::instance().getServerPort();
    m_udpSocket->writeDatagram(packet, QHostAddress(serverIp), serverPort);
    emit dataSent(packet);
  } else {
    qWarning() << "NetworkManager: Failed to serialize robot command message";
  }
}

void NetworkManager::sendCommonCommand(int cmdType, int param) {
#ifdef RM_HAS_MQTT
  if (shouldThrottleCommand(m_lastCommonCommandSentMs, 100)) {
    qWarning() << "NetworkManager: CommonCommand throttled to <= 10Hz"
               << "cmdType=" << cmdType << "param=" << param;
    return;
  }

  robomaster::CommonCommand message;
  message.set_cmd_type(static_cast<uint32_t>(cmdType));
  message.set_param(static_cast<uint32_t>(param));

  const QByteArray payload = serializeMessage(message);
  if (payload.isEmpty()) {
    qWarning() << "NetworkManager: Failed to serialize CommonCommand protobuf";
    return;
  }

  // 如果启用了 MQTT 支持，尝试通过 MQTT 发布；否则记录并丢弃命令
  if (m_mqttManager && m_mqttManager->isConnected()) {
    m_mqttManager->publish(QStringLiteral("CommonCommand"), payload, 1);
    qInfo() << "NetworkManager: Published CommonCommand via MQTT"
            << "cmdType=" << cmdType << "param=" << param
            << "bytes=" << payload.size();
    emit dataSent(payload);
    return;
  }

  qWarning() << "NetworkManager: MQTT not connected, dropping CommonCommand cmdType=" << cmdType << " param=" << param;
#else
  Q_UNUSED(cmdType);
  Q_UNUSED(param);
  qWarning() << "NetworkManager: MQTT not available in this build, dropping CommonCommand cmdType=" << cmdType << " param=" << param;
#endif
}

void NetworkManager::sendDartCommand(uint32_t targetId, bool open, bool launchConfirm) {
#ifdef RM_HAS_MQTT
  robomaster::DartCommand cmd;
  cmd.set_target_id(targetId);
  cmd.set_open(open);
  cmd.set_launch_confirm(launchConfirm);

  const QByteArray payload = serializeMessage(cmd);
  if (payload.isEmpty()) {
    qWarning() << "[DartDebug] NetworkManager: Failed to serialize DartCommand protobuf";
    return;
  }

  if (m_mqttManager && m_mqttManager->isConnected()) {
    m_mqttManager->publish(QStringLiteral("DartCommand"), payload, 1);
    qInfo() << "[DartDebug] NetworkManager: Published DartCommand via MQTT"
            << "targetId=" << targetId << "open=" << open << "launchConfirm=" << launchConfirm
            << "bytes=" << payload.size();
    emit dataSent(payload);
    return;
  }

  qWarning() << "[DartDebug] NetworkManager: MQTT not connected, dropping DartCommand"
             << "targetId=" << targetId << "open=" << open << "launchConfirm=" << launchConfirm;
#else
  Q_UNUSED(targetId);
  Q_UNUSED(open);
  Q_UNUSED(launchConfirm);
  qWarning() << "[DartDebug] NetworkManager: MQTT not available in this build, dropping DartCommand"
             << "targetId=" << targetId << "open=" << open << "launchConfirm=" << launchConfirm;
#endif
}

bool NetworkManager::sendKeyboardMouseControl(
    int mouseX, int mouseY, int mouseZ, bool leftButtonDown,
    bool rightButtonDown, bool midButtonDown, quint32 keyboardValue) {
#ifdef RM_HAS_MQTT
  constexpr qint64 kKeyboardMouseMinIntervalMs = 13; // 最高约 75 Hz。
  const bool isNeutralFrame =
      mouseX == 0 && mouseY == 0 && mouseZ == 0 && !leftButtonDown &&
      !rightButtonDown && !midButtonDown && keyboardValue == 0;
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  if (!isNeutralFrame && m_lastKeyboardMouseControlAttemptMs > 0 &&
      (nowMs - m_lastKeyboardMouseControlAttemptMs) <
          kKeyboardMouseMinIntervalMs) {
    if (qEnvironmentVariableIsSet("RM_KEYBOARD_MOUSE_TRACE")) {
      qInfo() << "[keyboard-mouse-trace] stage=network-throttled"
              << "elapsedMs="
              << (nowMs - m_lastKeyboardMouseControlAttemptMs)
              << "minimumMs=" << kKeyboardMouseMinIntervalMs
              << "mouseX=" << mouseX << "mouseY=" << mouseY
              << "keyboardValue=" << Qt::hex << keyboardValue << Qt::dec;
    }
    return false;
  }
  m_lastKeyboardMouseControlAttemptMs = nowMs;

  m_lastKeyboardMouseX = mouseX;
  m_lastKeyboardMouseY = mouseY;
  m_lastKeyboardMouseZ = mouseZ;
  m_lastKeyboardMouseLeftButtonDown = leftButtonDown;
  m_lastKeyboardMouseRightButtonDown = rightButtonDown;
  m_lastKeyboardMouseMidButtonDown = midButtonDown;
  m_lastKeyboardMouseKeyboardValue = keyboardValue;

  robomaster::KeyboardMouseControl message;
  message.set_mouse_x(mouseX);
  message.set_mouse_y(mouseY);
  message.set_mouse_z(mouseZ);
  message.set_left_button_down(leftButtonDown);
  message.set_right_button_down(rightButtonDown);
  message.set_mid_button_down(midButtonDown);
  message.set_keyboard_value(keyboardValue);

  std::string buffer;
  if (!message.SerializeToString(&buffer)) {
    ++m_totalKeyboardMouseControlDropped;
    m_lastKeyboardMouseControlPublishOk = false;
    qWarning() << "NetworkManager: Failed to serialize KeyboardMouseControl protobuf";
    return false;
  }
  const QByteArray payload(buffer.data(), static_cast<int>(buffer.size()));

  if (m_mqttManager && m_mqttManager->isConnected()) {
    // KeyboardMouseControl 发送频率较高；在 UI 线程等待 QoS 完成会增加延迟并造成卡顿。
    const bool published =
        m_mqttManager->publish(QStringLiteral("KeyboardMouseControl"), payload,
                               1, 0);
    if (qEnvironmentVariableIsSet("RM_KEYBOARD_MOUSE_TRACE")) {
      qInfo() << "[keyboard-mouse-trace] stage=mqtt-publish"
              << "topic=KeyboardMouseControl"
              << "connected=true"
              << "published=" << published
              << "payloadBytes=" << payload.size()
              << "mouseX=" << mouseX << "mouseY=" << mouseY
              << "mouseZ=" << mouseZ
              << "left=" << leftButtonDown << "right=" << rightButtonDown
              << "middle=" << midButtonDown
              << "keyboardValue=" << Qt::hex << keyboardValue << Qt::dec;
    }
    m_lastKeyboardMouseControlPublishOk = published;
    if (published) {
      ++m_totalKeyboardMouseControlSent;
      m_lastKeyboardMouseControlSentMs = nowMs;
      emit dataSent(payload);
      return true;
    }
  }

  ++m_totalKeyboardMouseControlDropped;
  m_lastKeyboardMouseControlPublishOk = false;
  if (qEnvironmentVariableIsSet("RM_KEYBOARD_MOUSE_TRACE")) {
    qInfo() << "[keyboard-mouse-trace] stage=mqtt-drop"
            << "connected="
            << (m_mqttManager && m_mqttManager->isConnected())
            << "mouseX=" << mouseX << "mouseY=" << mouseY
            << "mouseZ=" << mouseZ
            << "keyboardValue=" << Qt::hex << keyboardValue << Qt::dec;
  }
  if (nowMs - m_lastKeyboardMouseControlDropLogMs > 1000) {
    m_lastKeyboardMouseControlDropLogMs = nowMs;
    qWarning() << "NetworkManager: MQTT not connected, dropping KeyboardMouseControl"
               << "keyboardValue=" << keyboardValue;
  }
  return false;
#else
  Q_UNUSED(mouseX);
  Q_UNUSED(mouseY);
  Q_UNUSED(mouseZ);
  Q_UNUSED(leftButtonDown);
  Q_UNUSED(rightButtonDown);
  Q_UNUSED(midButtonDown);
  Q_UNUSED(keyboardValue);
  qWarning() << "NetworkManager: MQTT not available in this build, dropping KeyboardMouseControl";
  return false;
#endif
}

void NetworkManager::sendMapClickInfo(quint32 targetRobotId, float x, float y,
                                      int type, int ascii, int enemyId,
                                      bool sendAll) {
#ifdef RM_HAS_MQTT
  if (shouldThrottleCommand(m_lastMapClickInfoSentMs, 500)) {
    qWarning() << "NetworkManager: MapClickInfoNotify throttled to <= 2Hz"
               << "targetRobotId=" << targetRobotId << "x=" << x << "y=" << y;
    return;
  }

  robomaster::MapClickInfoNotify notify;
  notify.set_is_send_all((sendAll || targetRobotId == 0) ? 1 : 0);

  // robot_id 是 7 字节目标 ID 列表，不是位图。红方写 1..7，蓝方写
  // 101..107，未使用位置补 0。
  QByteArray robotIds(7, 0);
  if (!sendAll && targetRobotId > 0 && targetRobotId <= 107) {
    robotIds[0] = static_cast<char>(targetRobotId & 0xff);
  }
  notify.set_robot_id(robotIds.constData(), robotIds.size());
  notify.set_mode(enemyId > 0 ? 2 : 1);
  notify.set_enemy_id(static_cast<uint32_t>(std::max(0, enemyId)));
  notify.set_ascii(static_cast<uint32_t>(std::max(0, ascii)));
  notify.set_type(static_cast<uint32_t>(std::clamp(type, 1, 4)));
  notify.set_map_x(x);
  notify.set_map_y(y);

  const QByteArray payload = serializeMessage(notify);
  if (payload.isEmpty()) {
    qWarning() << "NetworkManager: Failed to serialize MapClickInfoNotify protobuf";
    return;
  }

  if (m_mqttManager && m_mqttManager->isConnected()) {
    m_mqttManager->publish(QStringLiteral("MapClickInfoNotify"), payload, 1);
    emit dataSent(payload);
    return;
  }

  qWarning() << "NetworkManager: MQTT not connected, dropping MapClickInfoNotify";
#else
  Q_UNUSED(targetRobotId);
  Q_UNUSED(x);
  Q_UNUSED(y);
  Q_UNUSED(type);
  Q_UNUSED(ascii);
  Q_UNUSED(enemyId);
  Q_UNUSED(sendAll);
  qWarning() << "NetworkManager: MQTT not available in this build, dropping MapClickInfoNotify";
#endif
}

void NetworkManager::sendAssemblyCommand(int operation, int difficulty) {
#ifdef RM_HAS_MQTT
  const QString topic = QStringLiteral("AssemblyCommand");
  const QString operationName = assemblyOperationName(operation);

  if (shouldThrottleCommand(m_lastAssemblyCommandSentMs, 100)) {
    qWarning() << "NetworkManager: AssemblyCommand throttled to <= 10Hz"
               << "topic=" << topic << "operation=" << operation
               << "operationName=" << operationName
               << "difficulty=" << difficulty;
    return;
  }

  robomaster::AssemblyCommand cmd;
  cmd.set_operation(operation);
  cmd.set_difficulty(difficulty);

  const QByteArray payload = serializeMessage(cmd);
  if (payload.isEmpty()) {
    qWarning() << "NetworkManager: Failed to serialize AssemblyCommand protobuf"
               << "topic=" << topic << "operation=" << operation
               << "operationName=" << operationName
               << "difficulty=" << difficulty;
    return;
  }

  if (m_mqttManager && m_mqttManager->isConnected()) {
    qInfo() << "NetworkManager: Publishing AssemblyCommand"
            << "topic=" << topic << "operation=" << operation
            << "operationName=" << operationName
            << "difficulty=" << difficulty << "bytes=" << payload.size();
    m_mqttManager->publish(topic, payload, 1);
    emit dataSent(payload);
    qInfo() << "NetworkManager: AssemblyCommand publish queued"
            << "topic=" << topic << "operation=" << operation
            << "operationName=" << operationName
            << "difficulty=" << difficulty << "bytes=" << payload.size();
    return;
  }

  qWarning() << "NetworkManager: MQTT not connected, dropping AssemblyCommand"
             << "topic=" << topic << "operation=" << operation
             << "operationName=" << operationName << "difficulty=" << difficulty
             << "bytes=" << payload.size();
#else
  Q_UNUSED(operation);
  Q_UNUSED(difficulty);
  qWarning() << "NetworkManager: MQTT not available in this build, dropping AssemblyCommand";
#endif
}

bool NetworkManager::sendAirSupportCommand(int commandId) {
#ifdef RM_HAS_MQTT
  if (shouldThrottleCommand(m_lastAirSupportCommandSentMs, 1000)) {
    qWarning() << "NetworkManager: AirSupportCommand throttled to <= 1Hz"
               << "commandId=" << commandId;
    return false;
  }

  robomaster::AirSupportCommand cmd;
  cmd.set_command_id(commandId);

  const QByteArray payload = serializeMessage(cmd);
  // AirSupportCommand.command_id 使用显式存在性，因此中断命令 command_id=0
  // 也必须在传输数据中保留该字段。
  if (payload.isEmpty()) {
    qWarning() << "NetworkManager: Failed to serialize AirSupportCommand protobuf"
               << "commandId=" << commandId
               << "command=" << airSupportCommandName(commandId);
    return false;
  }

  if (m_mqttManager && m_mqttManager->isConnected()) {
    if (!m_mqttManager->publish(QStringLiteral("AirSupportCommand"), payload, 1)) {
      qWarning() << "NetworkManager: Failed to publish AirSupportCommand"
                 << "commandId=" << commandId
                 << "command=" << airSupportCommandName(commandId);
      return false;
    }
    const QByteArray debugLog =
        QStringLiteral("MQTT TX AirSupportCommand topic=AirSupportCommand "
                       "commandId=%1 command=%2 bytes=%3")
            .arg(commandId)
            .arg(airSupportCommandName(commandId))
            .arg(payload.size())
            .toUtf8();
    qInfo() << "NetworkManager: Published AirSupportCommand via MQTT"
            << "commandId=" << commandId
            << "command=" << airSupportCommandName(commandId)
            << "bytes=" << payload.size();
    emit dataSent(debugLog);
    return true;
  }

  qWarning() << "NetworkManager: MQTT not connected, dropping AirSupportCommand";
  return false;
#else
  Q_UNUSED(commandId);
  qWarning() << "NetworkManager: MQTT not available in this build, dropping AirSupportCommand";
  return false;
#endif
}

void NetworkManager::sendRobotPerformanceSelection(uint32_t shooter, uint32_t chassis, uint32_t sentryControl) {
#ifdef RM_HAS_MQTT
  robomaster::RobotPerformanceSelectionCommand cmd;
  cmd.set_shooter(shooter);
  cmd.set_chassis(chassis);
  cmd.set_sentry_control(sentryControl);

  const QByteArray payload = serializeMessage(cmd);
  if (payload.isEmpty()) {
    qWarning() << "NetworkManager: Failed to serialize RobotPerformanceSelectionCommand protobuf";
    return;
  }

  if (m_mqttManager && m_mqttManager->isConnected()) {
    m_mqttManager->publish(QStringLiteral("RobotPerformanceSelectionCommand"), payload, 1);
    emit dataSent(payload);
    qDebug() << "[NetworkManager] RobotPerformanceSelectionCommand sent:" << shooter << chassis << sentryControl;
    return;
  }

  qWarning() << "NetworkManager: MQTT not connected, dropping RobotPerformanceSelectionCommand";
#else
  Q_UNUSED(shooter);
  Q_UNUSED(chassis);
  Q_UNUSED(sentryControl);
  qWarning() << "NetworkManager: MQTT not available in this build, dropping RobotPerformanceSelectionCommand";
#endif
}

void NetworkManager::sendHeroDeployMode(uint32_t mode) {
#ifdef RM_HAS_MQTT
  robomaster::HeroDeployModeEventCommand cmd;
  cmd.set_mode(mode);

  // proto3 中标量字段取默认值 0 时，合法序列化结果可能是“空 payload”。
  // 英雄退出部署命令 mode=0 就属于这种情况，不能把空 payload 误判成失败。
  std::string buffer;
  if (!cmd.SerializeToString(&buffer)) {
    qWarning() << "NetworkManager: Failed to serialize HeroDeployModeEventCommand protobuf";
    return;
  }
  const QByteArray payload(buffer.data(), static_cast<int>(buffer.size()));

  if (m_mqttManager && m_mqttManager->isConnected()) {
    if (!m_mqttManager->publish(QStringLiteral("HeroDeployModeEventCommand"),
                                payload, 1)) {
      qWarning() << "NetworkManager: Failed to publish HeroDeployModeEventCommand"
                 << "mode=" << mode << "payloadSize=" << payload.size();
      return;
    }
    emit dataSent(payload);
    qInfo() << "NetworkManager: Published HeroDeployModeEventCommand via MQTT"
            << "mode=" << mode << "bytes=" << payload.size();
    return;
  }

  qWarning() << "NetworkManager: MQTT not connected, dropping HeroDeployModeEventCommand";
#else
  Q_UNUSED(mode);
  qWarning() << "NetworkManager: MQTT not available in this build, dropping HeroDeployModeEventCommand";
#endif
}

void NetworkManager::sendRuneActivate(uint32_t activate) {
#ifdef RM_HAS_MQTT
  robomaster::RuneActivateCommand cmd;
  cmd.set_activate(activate);

  const QByteArray payload = serializeMessage(cmd);
  if (payload.isEmpty()) {
    qWarning() << "NetworkManager: Failed to serialize RuneActivateCommand protobuf";
    return;
  }

  if (m_mqttManager && m_mqttManager->isConnected()) {
    m_mqttManager->publish(QStringLiteral("RuneActivateCommand"), payload, 1);
    emit dataSent(payload);
    qDebug() << "[NetworkManager] RuneActivateCommand sent:" << activate;
    return;
  }

  qWarning() << "NetworkManager: MQTT not connected, dropping RuneActivateCommand";
#else
  Q_UNUSED(activate);
  qWarning() << "NetworkManager: MQTT not available in this build, dropping RuneActivateCommand";
#endif
}

/**
 * @brief 发送客户端状态信息
 * @details 旧版 UDP 接口。
 *          官方 V1.3.0 MQTT 模式下不再使用 ClientStatus。
 *
 * @param volume     音量级别 (0-100)
 * @param resolution 分辨率字符串，如 "1920x1080"
 * @param fullscreen 是否全屏
 * @param crosshair  是否启用准星
 * @param minimap    是否启用小地图
 */
void NetworkManager::sendClientStatus(uint32_t volume,
                                      const QString &resolution,
                                      bool fullscreen, bool crosshair,
                                      bool minimap) {
#ifdef RM_HAS_MQTT
  if (m_mqttManager) {
    qWarning() << "NetworkManager: ClientStatus is a legacy UDP-only message and"
                  " is not part of the official MQTT custom client protocol";
    return;
  }
#endif

  // 创建消息对象
  robomaster::RoboMasterMessage message;

  // 设置 client_status 字段
  auto *status = message.mutable_client_status();
  status->set_volume(volume);
  status->set_resolution(resolution.toStdString());
  status->set_fullscreen(fullscreen);
  status->set_crosshair_enabled(crosshair);
  status->set_minimap_enabled(minimap);

  // 发送到服务器
  QString serverIp = ConfigManager::instance().getServerIP();
  quint16 serverPort = ConfigManager::instance().getServerPort();
  sendData(message, QHostAddress(serverIp), serverPort);
}

// --- 数据接收与处理 ---

/**
 * @brief UDP 数据就绪处理槽
 * @details 当 QUdpSocket 发出 readyRead 信号时调用。
 *          循环读取所有挂起的数据报直到队列为空。
 *
 *          处理流程：
 *          1. 检查是否有待处理的数据报
 *          2. 读取数据报到 QByteArray
 *          3. 同时获取发送方地址和端口
 *          4. 调用 processData 处理数据
 *          5. 发送 dataReceived 信号
 *          6. 继续循环直到队列为空
 */
void NetworkManager::onReadyRead() {
  // 循环处理所有挂起的数据报
  while (m_udpSocket->hasPendingDatagrams()) {
    // 准备接收缓冲区
    QByteArray datagram;
    datagram.resize(m_udpSocket->pendingDatagramSize());

    // 发送方信息
    QHostAddress sender;
    quint16 senderPort;

    // 读取数据报
    m_udpSocket->readDatagram(datagram.data(), datagram.size(), &sender,
                              &senderPort);
    ++m_totalUdpPacketsReceived;

            // 采样日志：记录接收时间戳与数据大小，便于链路延时分析
            qInfo() << "[RespawnDebug] NetworkManager: onReadyRead ts="
              << QDateTime::currentMSecsSinceEpoch() << "size=" << datagram.size();

    // 处理接收到的数据
    processData(datagram);

    // 发送信号通知外部
    emit dataReceived(datagram);
  }
}

/**
 * @brief 处理接收到的数据
 * @details 使用 Protocol 类解析数据包，并将解析后的消息
 *          传递给 GameData 进行状态更新。
 *
 *          处理流程：
 *          1. 创建 RoboMasterMessage 对象
 *          2. 调用 Protocol::parsePacket 解析数据
 *          3. 如果解析成功且 m_gameData 有效，调用其更新方法
 *          4. 解析失败则静默丢弃（不输出日志避免刷屏）
 *
 * @param data 原始字节数据
 *
 * @note 解析失败的数据包不会触发任何操作，这是正常现象
 *       （可能是其他应用的数据或损坏的数据包）
 */
void NetworkManager::processData(const QByteArray &data) {
  // 创建消息容器
  robomaster::RoboMasterMessage message;

  // 尝试解析数据包
  if (Protocol::parsePacket(data, message)) {
    // 解析成功，将数据传递给 GameData 处理
    if (m_gameData) {
            // 解析到有效消息时记录时间戳和 payload 类型，用于端到端延时对比
            qInfo() << "[RespawnDebug] NetworkManager: parsePacket OK ts=" << QDateTime::currentMSecsSinceEpoch()
              << "payload_case=" << static_cast<int>(message.payload_case());
      if (message.has_game_info()) {
        auto *gameInfo = message.mutable_game_info();
#ifdef RM_HAS_MQTT
        if (m_hasMqttGameStatus) {
          // MQTT GameStatus 存在时以 MQTT 为准，否则允许 UDP 兜底
          // 自己驱动阶段/计时变化。
          gameInfo->set_stage(m_lastMqttStage);
          gameInfo->set_time_remaining(m_lastMqttTimeRemaining);
          gameInfo->set_is_paused(m_lastMqttPaused);
        }
#endif
      }
      m_gameData->processProtocolData(message);
    }
  } else {
    // 解析失败时静默丢弃。
  }
}

// ============================================================================
// MQTT 相关实现
// ============================================================================

#ifdef RM_HAS_MQTT

void NetworkManager::startMqtt(const QString &brokerUri,
                               const QString &clientId) {
  // 创建 MQTT 管理器
  if (!m_mqttManager) {
    m_mqttManager = new MqttManager(this);

    // 连接信号
    connect(m_mqttManager, &MqttManager::messageObserved, this,
            &NetworkManager::mqttMessageObserved);
    connect(m_mqttManager, &MqttManager::messageReceived, this,
            &NetworkManager::processMqttMessage);
    connect(
        m_mqttManager, &MqttManager::connectionStateChanged, this,
        [this](bool connected) {
          if (connected) {
            qDebug()
                << "NetworkManager: MQTT connected, subscribing to topics...";
            subscribeToTopics();
            // 启动 MQTT 消息日志会话
            MqttLogger::instance()->startSession();
          } else {
            MqttLogger::instance()->stopSession();
          }
        });
    connect(m_mqttManager, &MqttManager::errorOccurred, this,
            &NetworkManager::errorOccurred);
    // 转发异步连接结果
    connect(m_mqttManager, &MqttManager::connectCompleted, this,
            [this](bool success, const QString &error) {
              emit mqttConnectCompleted(success, error);
            });
  }

  // 异步连接到 Broker（非阻塞，结果通过 mqttConnectCompleted 信号通知）
  m_mqttManager->connectToBrokerAsync(brokerUri, clientId);
}

void NetworkManager::stopMqtt() {
  if (m_mqttManager) {
    m_mqttManager->disconnect();
  }
}

bool NetworkManager::isMqttConnected() const {
  return m_mqttManager && m_mqttManager->isConnected();
}

void NetworkManager::subscribeToTopics() {
  if (!m_mqttManager)
    return;

  QStringList topics = {
      "GameStatus",             // 比赛全局状态
      "GlobalUnitStatus",       // 全局单位状态
      "GlobalLogisticsStatus",  // 全局后勤信息
      "GlobalSpecialMechanism", // 全局特殊机制
      "Event",                  // 事件通知
      "RobotInjuryStat",        // 伤害统计
      "RobotRespawnStatus",     // 复活状态
      "RobotStaticStatus",      // 机器人静态状态
      "RobotDynamicStatus",     // 机器人实时数据
      "RobotModuleStatus",      // 机器人模块状态
      "RobotPosition",          // 机器人位置
      "Buff",                   // Buff 信息
      "PenaltyInfo",            // 判罚信息同步
      "RobotPathPlanInfo",      // 哨兵轨迹规划信息
      "RadarInfoToClient",      // 雷达发送机器人位置信息
      "CustomByteBlock",        // 机器人自定义上传数据流
      "TechCoreMotionStateSync", // 科技核心运动状态同步
      "RobotPerformanceSelectionSync", // 性能体系状态同步
      "DeployModeStatusSync",   // 英雄部署模式状态同步
      "Event",                  // 事件通知
      "CustomRobotData",         // 自定义机器人数据
      "RuneStatusSync",         // 能量机关状态同步
      "SentryStatusSync",       // 哨兵姿态与弱化状态
      "DartSelectTargetStatusSync", // 飞镖目标选择状态同步
      "SentryCtrlResult",       // 哨兵控制结果
      "AirSupportStatusSync",   // 空中支援状态
  };

  for (const QString &topic : topics) {
    m_mqttManager->subscribe(topic, 1);
  }
}

void NetworkManager::processMqttMessage(const QString &topic,
                                        const QByteArray &payload) {
  if (!m_gameData)
    return;

  const bool detailedVideoTrace =
      !isHighRateVideoMessage(topic, payload) || verboseVideoLogEnabled();
  if (detailedVideoTrace) {
    qDebug() << "NetworkManager: Processing MQTT message from topic:" << topic;
    emit dataReceived(QStringLiteral("MQTT RX topic=%1 bytes=%2")
                          .arg(topic)
                          .arg(payload.size())
                          .toUtf8());
  }
  dumpMqttEvent(QStringLiteral("received"), topic, payload, m_gameData);

  // 根据 Topic 解析对应的 Protobuf 消息
  if (topic == "GameStatus") {
    robomaster::GameStatus msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      const auto stage =
          static_cast<robomaster::GameStage>(msg.current_stage());
      const quint16 timeRemaining = static_cast<quint16>(msg.stage_countdown_sec());
      const bool isPaused = msg.is_paused();

      m_hasMqttGameStatus = true;
      m_lastMqttStage = stage;
      m_lastMqttTimeRemaining = timeRemaining;
      m_lastMqttPaused = isPaused;

      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("GameStatus current_stage=%1 countdown=%2 paused=%3")
              .arg(msg.current_stage())
              .arg(msg.stage_countdown_sec())
              .arg(msg.is_paused()),
          QJsonObject{{QStringLiteral("current_stage"),
                       static_cast<int>(msg.current_stage())},
                      {QStringLiteral("stage_countdown_sec"),
                       static_cast<int>(msg.stage_countdown_sec())},
                      {QStringLiteral("stage_elapsed_sec"),
                       static_cast<int>(msg.stage_elapsed_sec())},
                      {QStringLiteral("is_paused"), msg.is_paused()}});
      m_gameData->updateGameStatus(msg);
    }
  } else if (topic == "GlobalUnitStatus") {
      robomaster::GlobalUnitStatus msg;
      if (msg.ParseFromArray(payload.data(), payload.size())) {
        dumpMqttEvent(
            QStringLiteral("parsed"), topic, payload, m_gameData,
            QStringLiteral("GlobalUnitStatus robot_health=%1 robot_bullets=%2")
                .arg(QString::fromUtf8(
                    QJsonDocument(repeatedFieldToJsonArray(msg.robot_health()))
                        .toJson(QJsonDocument::Compact)))
                .arg(QString::fromUtf8(
                    QJsonDocument(repeatedFieldToJsonArray(msg.robot_bullets()))
                        .toJson(QJsonDocument::Compact))),
            QJsonObject{{QStringLiteral("robot_health"),
                         repeatedFieldToJsonArray(msg.robot_health())},
                        {QStringLiteral("robot_bullets"),
                         repeatedFieldToJsonArray(msg.robot_bullets())},
                        {QStringLiteral("robot_health_size"),
                         msg.robot_health_size()},
                        {QStringLiteral("robot_bullets_size"),
                         msg.robot_bullets_size()},
                        {QStringLiteral("base_health"),
                         static_cast<int>(msg.base_health())},
                        {QStringLiteral("enemy_base_health"),
                         static_cast<int>(msg.enemy_base_health())},
                        {QStringLiteral("outpost_health"),
                         static_cast<int>(msg.outpost_health())},
                        {QStringLiteral("enemy_outpost_health"),
                         static_cast<int>(msg.enemy_outpost_health())},
                        {QStringLiteral("base_status"),
                         static_cast<int>(msg.base_status())},
                        {QStringLiteral("enemy_base_status"),
                         static_cast<int>(msg.enemy_base_status())},
                        {QStringLiteral("outpost_status"),
                         static_cast<int>(msg.outpost_status())},
                        {QStringLiteral("enemy_outpost_status"),
                         static_cast<int>(msg.enemy_outpost_status())},
                        {QStringLiteral("base_shield"),
                         static_cast<int>(msg.base_shield())},
                        {QStringLiteral("enemy_base_shield"),
                         static_cast<int>(msg.enemy_base_shield())},
                        {QStringLiteral("total_damage_ally"),
                         static_cast<int>(msg.total_damage_ally())},
                        {QStringLiteral("total_damage_enemy"),
                         static_cast<int>(msg.total_damage_enemy())}});
        // 详细明文日志：打印所有字段
        QStringList rHp, rBul;
        for (int i = 0; i < msg.robot_health_size(); ++i) rHp << QString::number(msg.robot_health(i));
        for (int i = 0; i < msg.robot_bullets_size(); ++i) rBul << QString::number(msg.robot_bullets(i));
        qInfo().noquote()
            << QStringLiteral("[GlobalUnitStatus] base_hp=%1 base_status=%2 base_shield=%3 "
                              "outpost_hp=%4 outpost_status=%5 "
                              "enemy_base_hp=%6 enemy_base_status=%7 enemy_base_shield=%8 "
                              "enemy_outpost_hp=%9 enemy_outpost_status=%10 "
                              "robot_hp=[%11] robot_bullets=[%12] "
                              "dmg_ally=%13 dmg_enemy=%14")
                   .arg(msg.base_health()).arg(msg.base_status())
                   .arg(msg.base_shield()).arg(msg.outpost_health())
                   .arg(msg.outpost_status()).arg(msg.enemy_base_health())
                   .arg(msg.enemy_base_status()).arg(msg.enemy_base_shield())
                   .arg(msg.enemy_outpost_health()).arg(msg.enemy_outpost_status())
                   .arg(rHp.join(QStringLiteral(",")))
                   .arg(rBul.join(QStringLiteral(",")))
                   .arg(msg.total_damage_ally()).arg(msg.total_damage_enemy());
        // 统一交给 GameData 的状态入口处理。
        m_gameData->updateGlobalUnitStatus(msg);
      }
  } else if (topic == "GlobalLogisticsStatus") {
    robomaster::GlobalLogisticsStatus msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("GlobalLogisticsStatus remaining=%1 tech=%2 encryption=%3")
              .arg(msg.remaining_economy())
              .arg(msg.tech_level())
              .arg(msg.encryption_level()),
          QJsonObject{{QStringLiteral("remaining_economy"),
                       static_cast<int>(msg.remaining_economy())},
                      {QStringLiteral("tech_level"),
                       static_cast<int>(msg.tech_level())},
                      {QStringLiteral("encryption_level"),
                       static_cast<int>(msg.encryption_level())}});
            robomaster::GameInfo gameInfo;
      const bool isBluePerspective = m_gameData->getMyRobotId() >= 100;
      gameInfo.set_stage(m_lastMqttStage);
      gameInfo.set_time_remaining(m_lastMqttTimeRemaining);
      gameInfo.set_is_paused(m_lastMqttPaused);
      gameInfo.set_current_round(static_cast<uint32_t>(m_gameData->currentRound()));
      gameInfo.set_red_score(static_cast<uint32_t>(m_gameData->redScore()));
      gameInfo.set_blue_score(static_cast<uint32_t>(m_gameData->blueScore()));
      gameInfo.set_red_economy(static_cast<uint32_t>(m_gameData->redEconomy()));
      gameInfo.set_blue_economy(static_cast<uint32_t>(m_gameData->blueEconomy()));
      if (isBluePerspective) {
        gameInfo.set_blue_economy(msg.remaining_economy());
      } else {
        gameInfo.set_red_economy(msg.remaining_economy());
      }
      m_gameData->updateGameState(gameInfo);
      m_gameData->updateGlobalLogisticsStatus(msg);
    }
  } else if (topic == "GlobalSpecialMechanism") {
    robomaster::GlobalSpecialMechanism msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      m_gameData->updateGlobalSpecialMechanism(msg);
    }
  } else if (topic == "RobotInjuryStat") {
    robomaster::RobotInjuryStat msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("RobotInjuryStat killer_id=%1 total_damage=%2")
              .arg(msg.killer_id())
              .arg(msg.total_damage()),
          QJsonObject{{QStringLiteral("killer_id"),
                       static_cast<int>(msg.killer_id())},
                      {QStringLiteral("total_damage"),
                       static_cast<int>(msg.total_damage())}});
      m_gameData->updateRobotInjury(msg);
    }
  } else if (topic == "RobotStaticStatus") {
    robomaster::RobotStaticStatus msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("RobotStaticStatus robot_id=%1 connection=%2 field=%3 alive=%4")
              .arg(msg.robot_id())
              .arg(msg.connection_state())
              .arg(msg.field_state())
              .arg(msg.alive_state()),
          QJsonObject{{QStringLiteral("robot_id"),
                       static_cast<int>(msg.robot_id())},
                      {QStringLiteral("robot_type"),
                       static_cast<int>(msg.robot_type())},
                      {QStringLiteral("connection_state"),
                       static_cast<int>(msg.connection_state())},
                      {QStringLiteral("field_state"),
                       static_cast<int>(msg.field_state())},
                      {QStringLiteral("alive_state"),
                       static_cast<int>(msg.alive_state())},
                      {QStringLiteral("level"),
                       static_cast<int>(msg.level())}});
      qInfo().noquote()
          << QStringLiteral("[RobotStaticStatus] robot_id=%1 type=%2 "
                            "conn=%3 field=%4 alive=%5 "
                            "perf_shooter=%6 perf_chassis=%7 "
                            "level=%8 max_hp=%9 max_heat=%10 cool_rate=%11 "
                            "max_power=%12 max_buf_energy=%13 max_chassis_energy=%14")
                 .arg(msg.robot_id()).arg(msg.robot_type())
                 .arg(msg.connection_state()).arg(msg.field_state())
                 .arg(msg.alive_state()).arg(msg.performance_system_shooter())
                 .arg(msg.performance_system_chassis()).arg(msg.level())
                 .arg(msg.max_health()).arg(msg.max_heat())
                 .arg(msg.heat_cooldown_rate()).arg(msg.max_power())
                 .arg(msg.max_buffer_energy()).arg(msg.max_chassis_energy());
      m_gameData->updateRobotStaticStatus(msg);
    }
  } else if (topic == "RobotDynamicStatus") {
    robomaster::RobotDynamicStatus msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("RobotDynamicStatus has_no_robot_id current_health=%1 remaining_ammo=%2")
              .arg(msg.current_health())
              .arg(msg.remaining_ammo()),
          QJsonObject{{QStringLiteral("protocol_has_robot_id"), false},
                      {QStringLiteral("current_health"),
                       static_cast<int>(msg.current_health())},
                      {QStringLiteral("current_heat"), msg.current_heat()},
                      {QStringLiteral("remaining_ammo"),
                       static_cast<int>(msg.remaining_ammo())},
                      {QStringLiteral("can_remote_heal"),
                       msg.can_remote_heal()},
                      {QStringLiteral("can_remote_ammo"),
                       msg.can_remote_ammo()}});
      qInfo().noquote()
          << QStringLiteral("[RobotDynamicStatus] hp=%1 heat=%2 fire_rate=%3 "
                            "chassis_energy=%4 buf_energy=%5 "
                            "exp=%6 exp_up=%7 fired=%8 ammo=%9 "
                            "out_of_combat=%10 combat_countdown=%11 "
                            "remote_heal=%12 remote_ammo=%13")
                 .arg(msg.current_health()).arg(msg.current_heat())
                 .arg(msg.last_projectile_fire_rate())
                 .arg(msg.current_chassis_energy())
                 .arg(msg.current_buffer_energy())
                 .arg(msg.current_experience()).arg(msg.experience_for_upgrade())
                 .arg(msg.total_projectiles_fired()).arg(msg.remaining_ammo())
                 .arg(msg.is_out_of_combat())
                 .arg(msg.out_of_combat_countdown())
                 .arg(msg.can_remote_heal()).arg(msg.can_remote_ammo());
      m_gameData->updateRobotDynamicStatus(msg);
    }
  } else if (topic == "RobotPosition") {
    robomaster::RobotPosition msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("RobotPosition robot_id=%1 x=%2 y=%3 yaw=%4")
              .arg(msg.robot_id())
              .arg(msg.x(), 0, 'f', 2)
              .arg(msg.y(), 0, 'f', 2)
              .arg(msg.yaw(), 0, 'f', 2),
          QJsonObject{{QStringLiteral("robot_id"),
                       static_cast<int>(msg.robot_id())},
                      {QStringLiteral("x"), msg.x()},
                      {QStringLiteral("y"), msg.y()},
                      {QStringLiteral("z"), msg.z()},
                      {QStringLiteral("yaw"), msg.yaw()}});
      m_gameData->updateRobotPosition(msg);
    }
  } else if (topic == "Event") {
    robomaster::Event msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("Event event_id=%1 param=%2")
              .arg(msg.event_id())
              .arg(QString::fromStdString(msg.param())),
          QJsonObject{{QStringLiteral("event_id"),
                       static_cast<int>(msg.event_id())},
                      {QStringLiteral("param"),
                       QString::fromStdString(msg.param())}});
      robomaster::RoboMasterMessage wrapper;
      *wrapper.mutable_event() = msg;
      m_gameData->processProtocolData(wrapper);
    }
  } else if (topic == "Buff") {
    robomaster::Buff msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("Buff robot_id=%1 buff_type=%2 buff_level=%3")
              .arg(msg.robot_id())
              .arg(msg.buff_type())
              .arg(msg.buff_level()),
          QJsonObject{{QStringLiteral("robot_id"),
                       static_cast<int>(msg.robot_id())},
                      {QStringLiteral("buff_type"),
                       static_cast<int>(msg.buff_type())},
                      {QStringLiteral("buff_level"),
                       static_cast<int>(msg.buff_level())}});
      m_gameData->updateBuff(msg);
    }
  } else if (topic == "RobotModuleStatus") {
    robomaster::RobotModuleStatus msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("RobotModuleStatus main_controller=%1 video=%2")
              .arg(msg.main_controller())
              .arg(msg.video_transmission()),
          QJsonObject{{QStringLiteral("main_controller"),
                       static_cast<int>(msg.main_controller())},
                      {QStringLiteral("video_transmission"),
                       static_cast<int>(msg.video_transmission())},
                      {QStringLiteral("rfid"), static_cast<int>(msg.rfid())},
                      {QStringLiteral("uwb"), static_cast<int>(msg.uwb())}});
      m_gameData->updateRobotModuleStatus(msg);
    }
  } else if (topic == "RuneStatusSync") {
    robomaster::RuneStatusSync msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("RuneStatusSync status=%1 arms=%2")
              .arg(msg.rune_status())
              .arg(msg.activated_arms()),
          QJsonObject{{QStringLiteral("rune_status"),
                       static_cast<int>(msg.rune_status())},
                      {QStringLiteral("activated_arms"),
                       static_cast<int>(msg.activated_arms())},
                      {QStringLiteral("average_rings"), msg.average_rings()}});
      m_gameData->updateRuneStatusSync(msg);
    }
  } else if (topic == "DartSelectTargetStatusSync") {
    robomaster::DartSelectTargetStatusSync msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("DartSelectTargetStatusSync target_id=%1 open=%2")
              .arg(msg.target_id())
              .arg(msg.open()),
          QJsonObject{{QStringLiteral("target_id"),
                       static_cast<int>(msg.target_id())},
                      {QStringLiteral("open"), static_cast<int>(msg.open())}});
      qInfo() << "[DartDebug] NetworkManager: RX DartSelectTargetStatusSync"
              << "targetId=" << msg.target_id()
              << "open=" << msg.open()
              << "bytes=" << payload.size();
      m_gameData->updateSiloStatusFromSync(static_cast<int>(msg.target_id()),
                                           static_cast<int>(msg.open()));
    } else {
      qWarning() << "[DartDebug] NetworkManager: Failed to parse DartSelectTargetStatusSync"
                 << "bytes=" << payload.size();
    }
  } else if (topic == "RadarInfoToClient") {
    robomaster::RadarInfoToClient msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("RadarInfoToClient robot_info_size=%1")
              .arg(msg.robot_info_size()),
          QJsonObject{{QStringLiteral("robot_info_size"),
                       msg.robot_info_size()}});
      m_gameData->updateRadarInfo(msg);
    }
  } else if (topic == "TechCoreMotionStateSync") {
    robomaster::TechCoreMotionStateSync msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("TechCoreMotionStateSync max=%1 basic=%2 putin=%3 move=%4 rotate=%5 enemy=%6 remain_all=%7 remain_step=%8")
              .arg(msg.maximum_difficulty_level())
              .arg(msg.basic_state())
              .arg(msg.putin_state())
              .arg(msg.move_state())
              .arg(msg.rotate_state())
              .arg(msg.enemy_core_status())
              .arg(msg.remain_time_all())
              .arg(msg.remain_time_step()),
          QJsonObject{
              {QStringLiteral("maximum_difficulty_level"),
               static_cast<int>(msg.maximum_difficulty_level())},
              {QStringLiteral("basic_state"), static_cast<int>(msg.basic_state())},
              {QStringLiteral("putin_state"), static_cast<int>(msg.putin_state())},
              {QStringLiteral("move_state"), static_cast<int>(msg.move_state())},
              {QStringLiteral("rotate_state"), static_cast<int>(msg.rotate_state())},
              {QStringLiteral("enemy_core_status"),
               static_cast<int>(msg.enemy_core_status())},
              {QStringLiteral("remain_time_all"),
               static_cast<int>(msg.remain_time_all())},
              {QStringLiteral("remain_time_step"),
               static_cast<int>(msg.remain_time_step())}});
      emit dataReceived(QStringLiteral("MQTT TechCoreMotionStateSync max=%1 basic=%2 putin=%3 move=%4 rotate=%5 enemy=%6 remain_all=%7 remain_step=%8")
                            .arg(msg.maximum_difficulty_level())
                            .arg(msg.basic_state())
                            .arg(msg.putin_state())
                            .arg(msg.move_state())
                            .arg(msg.rotate_state())
                            .arg(msg.enemy_core_status())
                            .arg(msg.remain_time_all())
                            .arg(msg.remain_time_step())
                            .toUtf8());
      m_gameData->updateTechCoreMotionStateSync(msg);
    }
  } else if (topic == "CustomByteBlock") {
    // 简化处理：不做视频/非视频检测，始终将 payload 作为视频片段转发给解码链。
    // 如果 payload 为 Protobuf 封装（CustomByteBlock），则使用其中的 data 字段；否则直接使用原始 payload。
    robomaster::CustomByteBlock msg;
    QByteArray data;

    if (msg.ParseFromArray(payload.data(), payload.size())) {
      data = QByteArray(msg.data().data(), static_cast<int>(msg.data().size()));
      if (detailedVideoTrace) {
        dumpMqttEvent(
            QStringLiteral("parsed"), topic, payload, m_gameData,
            QStringLiteral("CustomByteBlock wrapper_bytes=%1 data_bytes=%2")
                .arg(payload.size())
                .arg(data.size()),
            QJsonObject{{QStringLiteral("wrapper_parsed"), true},
                        {QStringLiteral("data_bytes"), data.size()},
                        {QStringLiteral("data_prefix_hex"),
                         QString::fromLatin1(data.left(24).toHex())}});
        qDebug() << "[custombyte] NetworkManager: protobuf payload parsed, data size="
                 << data.size();
      }
    } else {
      dumpMqttEvent(
          QStringLiteral("parse_failed"), topic, payload, m_gameData,
          QStringLiteral("CustomByteBlock payload is not valid protobuf"));
      qWarning() << "[custombyte] NetworkManager: payload is not valid protobuf";
		  return;
    }

    // 发送端对 300B 视频片段额外包装 3 字节前缀 (0x0a 0xac 0x02)
    // 若接收到的 data 为 303B 且以该前缀开头，剥离前三字节后再交给解码链
    const QByteArray wrapperPrefix("\x0a\xac\x02", 3);
    if (data.size() == 303 && data.startsWith(wrapperPrefix)) {
      data = data.mid(3);
    }

    if (!data.isEmpty()) {
      if (detailedVideoTrace) {
        const QByteArray prefix = data.left(8).toHex();
        qDebug() << "[custombyte] NetworkManager: forwarding payload bytes="
                 << data.size() << "prefix(hex)=" << prefix;
        emit dataReceived(
            QStringLiteral("[custombyte] MQTT RX forwarded bytes=%1 prefix=%2")
                .arg(data.size())
                .arg(QString::fromLatin1(prefix))
                .toUtf8());
      }
      emit customVideoPayloadReceived(data);

      // 同时尝试解析为 0x0301 机器人自定义状态数据 (28字节, 非H264视频)
      // H264视频帧为300+字节，不会进入此分支
      if (data.size() >= static_cast<int>(sizeof(RobotCustomStatus)) &&
          data.size() <= 100) {
        parseCustomByteBlock(data);
      }
    } else {
      qWarning() << "[custombyte] NetworkManager: empty payload after unwrap";
    }
    return;
  } else if (topic == "RobotPathPlanInfo") {
    robomaster::RobotPathPlanInfo msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("RobotPathPlanInfo sender_id=%1 intention=%2")
              .arg(msg.sender_id())
              .arg(msg.intention()),
          QJsonObject{{QStringLiteral("sender_id"),
                       static_cast<int>(msg.sender_id())},
                      {QStringLiteral("intention"),
                       static_cast<int>(msg.intention())}});
      m_gameData->updateSentryPath(msg);
    }
  } else if (topic == "PenaltyInfo") {
    robomaster::PenaltyInfo msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("PenaltyInfo type=%1 effect_sec=%2 total_penalty_num=%3")
              .arg(msg.penalty_type())
              .arg(msg.penalty_effect_sec())
              .arg(msg.total_penalty_num()),
          QJsonObject{{QStringLiteral("penalty_type"),
                       static_cast<int>(msg.penalty_type())},
                      {QStringLiteral("penalty_effect_sec"),
                       static_cast<int>(msg.penalty_effect_sec())},
                      {QStringLiteral("total_penalty_num"),
                       static_cast<int>(msg.total_penalty_num())}});
      const QByteArray debugLog =
          QStringLiteral("MQTT RX PenaltyInfo penaltyType=%1 "
                         "effectSec=%2 totalPenaltyNum=%3 currentRobotId=%4")
              .arg(msg.penalty_type())
              .arg(msg.penalty_effect_sec())
              .arg(msg.total_penalty_num())
              .arg(m_gameData ? m_gameData->getMyRobotId() : 0)
              .toUtf8();
      qInfo() << "NetworkManager:" << debugLog;
      emit dataReceived(debugLog);
      m_gameData->updatePenalty(msg);
    }
  } else if (topic == "AirSupportStatusSync") {
    robomaster::AirSupportStatusSync msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("AirSupportStatusSync status=%1 left_time=%2 cost=%3 "
                         "is_being_targeted=%4 shooter_status=%5")
              .arg(msg.airsupport_status())
              .arg(msg.left_time())
              .arg(msg.cost_coins())
              .arg(msg.is_being_targeted())
              .arg(msg.shooter_status()),
          QJsonObject{{QStringLiteral("airsupport_status"),
                       static_cast<int>(msg.airsupport_status())},
                      {QStringLiteral("left_time"),
                       static_cast<int>(msg.left_time())},
                      {QStringLiteral("cost_coins"),
                       static_cast<int>(msg.cost_coins())},
                      {QStringLiteral("is_being_targeted"),
                       static_cast<int>(msg.is_being_targeted())},
                      {QStringLiteral("shooter_status"),
                       static_cast<int>(msg.shooter_status())}});
      m_gameData->updateAirSupportStatusSync(msg);
    }
  } else if (topic == "DeployModeStatusSync") {
    robomaster::DeployModeStatusSync msg;
    if (msg.ParseFromArray(payload.data(), payload.size())) {
      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("DeployModeStatusSync status=%1").arg(msg.status()),
          QJsonObject{{QStringLiteral("status"),
                       static_cast<int>(msg.status())}});
      m_gameData->updateDeployModeStatusSync(msg);
    }
  }

  // 其他 Topic 可以后续添加处理逻辑

  // 处理 RobotRespawnStatus（单一 topic）
  if (topic.startsWith("RobotRespawnStatus")) {
    qDebug() << "NetworkManager: RobotRespawnStatus payload size" << payload.size() << "topic" << topic;

    robomaster::RobotRespawnStatus respMsg;
    if (respMsg.ParseFromArray(payload.data(), payload.size())) {
      dumpMqttEvent(
          QStringLiteral("parsed"), topic, payload, m_gameData,
          QStringLiteral("RobotRespawnStatus pending=%1 current=%2 total=%3")
              .arg(respMsg.is_pending_respawn())
              .arg(respMsg.current_respawn_progress())
              .arg(respMsg.total_respawn_progress()),
          QJsonObject{{QStringLiteral("is_pending_respawn"),
                       respMsg.is_pending_respawn()},
                      {QStringLiteral("current_respawn_progress"),
                       static_cast<int>(respMsg.current_respawn_progress())},
                      {QStringLiteral("total_respawn_progress"),
                       static_cast<int>(respMsg.total_respawn_progress())}});
      qDebug() << "NetworkManager: RobotRespawnStatus parsed as protobuf";
      QVariantMap map;
      // 始终传递 is_pending_respawn（用于显示/隐藏复活读条）
      map["is_pending_respawn"] = respMsg.is_pending_respawn();

      // 仅在 protobuf 中包含非零进度或处于 pending 状态时才传递进度字段，避免将缺省 0 值当作有效更新覆盖本地进度
      // 否则让 GameData::processRobotRespawnStatusMap 使用上次缓存的值
      if (respMsg.total_respawn_progress() > 0 || respMsg.is_pending_respawn()) {
        map["total_respawn_progress"] = static_cast<uint32_t>(respMsg.total_respawn_progress());
      }
      if (respMsg.current_respawn_progress() > 0 || respMsg.is_pending_respawn()) {
        map["current_respawn_progress"] = static_cast<uint32_t>(respMsg.current_respawn_progress());
      }

      // 布尔/费用字段始终透传，确保客户端状态严格跟随上游消息。
      map["can_free_respawn"] = respMsg.can_free_respawn();
      map["gold_cost_for_respawn"] = static_cast<uint32_t>(respMsg.gold_cost_for_respawn());
      map["can_pay_for_respawn"] = respMsg.can_pay_for_respawn();

      // robot_id 优先使用本地已知 myRobotId（避免 protobuf 默认 0 覆盖）
      int myRobotId = m_gameData ? m_gameData->getMyRobotId() : 0;
      if (myRobotId > 0) map["robot_id"] = QString::number(myRobotId);
      m_gameData->processRobotRespawnStatusMap(map);
    } else {
        dumpMqttEvent(
            QStringLiteral("parse_failed"), topic, payload, m_gameData,
            QStringLiteral("RobotRespawnStatus parse failed"));
        qWarning() << "NetworkManager: Failed to parse RobotRespawnStatus as protobuf";
    }
  }

  dumpMqttEvent(QStringLiteral("completed"), topic, payload, m_gameData,
                QStringLiteral("Topic processing finished"));
}

// ==================== 自定义 UI 数据解析 (CustomByteBlock) ====================

void NetworkManager::parseCustomByteBlock(const QByteArray &data) {
    if (data.size() < sizeof(RobotCustomStatus)) {
        qWarning() << "NetworkManager: CustomByteBlock data too small:" << data.size()
                   << "expected at least" << sizeof(RobotCustomStatus);
        return;
    }

    RobotCustomStatus status;
    // 使用 memcpy 拷贝到本地结构以避免对齐/别名问题
    std::memcpy(&status, data.constData(), sizeof(RobotCustomStatus));

    if (!m_gameData) {
        return;
    }

    // 获取当前机器人ID
    quint8 robotId = m_gameData->getMyRobotId();
    if (robotId == 0 && !m_gameData->getAllRobots().isEmpty()) {
        robotId = m_gameData->getAllRobots().first().robotId;
    }
    if (robotId == 0) {
        return;
    }

    RobotData& robot = m_gameData->getRobotDataRef(robotId);

    // 解析摩擦轮/拨弹轮状态
    robot.fricEnabled = status.friction_status.bits.fric_enabled;
    robot.rammerEnabled = status.friction_status.bits.rammer_enabled;

    // 解析底盘状态
    robot.chassisMode = status.chassis_status.bits.chassis_mode;
    robot.spinMode = status.chassis_status.bits.spin_mode;
    robot.followMode = status.chassis_status.bits.follow_mode;
    robot.chassisProtect = status.chassis_status.bits.chassis_protect;
    robot.chassisWarning = status.chassis_status.bits.chassis_warning;

    // 解析超级电容 (0.1% 精度)
    if (status.super_cap_max > 0) {
      robot.superCapEnergyPercent =
        (status.super_cap_energy * 100.0f) / status.super_cap_max;
    } else {
        robot.superCapEnergyPercent = 0.0f;
    }

    // 解析云台底盘角度 (0.1度精度)
    robot.gimbalChassisAngle = status.gimbal_chassis_angle / 10.0f;

    // 解析目标数据
    robot.targetDistance = status.target_distance_dm;
    robot.ballisticCompensation = status.ballistic_compensation;

    // 发射信号通知 UI 更新
    emit m_gameData->robotCustomDataUpdated(robotId);

    qDebug() << "NetworkManager: Parsed custom data for robot" << robotId
             << "fric=" << robot.fricEnabled
             << "cap=" << robot.superCapEnergyPercent << "%"
             << "angle=" << robot.gimbalChassisAngle;
}

#endif // RM_HAS_MQTT

} // namespace RM
