/**
 * @file MqttClient.cpp
 * @brief MQTT 客户端实现
 * @author
 * @date 2026-02-04
 */

#include "MqttClient.h"
#include "MqttLogger.h"
#include <QDateTime>
#include <QDebug>

namespace RM {

MqttClient::MqttClient(GameData *gameData, QObject *parent)
    : QObject(parent), m_gameData(gameData)
#ifdef RM_HAS_QT_MQTT
      , m_client(new QMqttClient(this))
      , m_processTimer(new QTimer(this))
#endif
{
#ifdef RM_HAS_QT_MQTT
  m_processTimer->setSingleShot(true);
  m_processTimer->setInterval(33); // 批量间隔 33ms (~30Hz 上限)
  connect(m_processTimer, &QTimer::timeout, this, &MqttClient::processPendingMessages);
#endif
}

void MqttClient::setClientId(const QString &clientId) {
  m_clientId = clientId.trimmed();
}

void MqttClient::start() {
#ifdef RM_HAS_QT_MQTT
  // 从配置读取 broker 地址与端口（外部化配置，避免硬编码）
  const QString broker = ConfigManager::instance().getMqttBroker();
  const quint16 port = ConfigManager::instance().getMqttPort();

  // 设置客户端参数
  if (m_client) {
    m_client->setHostname(broker);
    m_client->setPort(port);
    const QString effectiveClientId =
        m_clientId.isEmpty()
            ? QString("rm26_client_%1").arg(QCoreApplication::applicationPid())
            : m_clientId;
    m_client->setClientId(effectiveClientId);

    // 集中绑定信号-槽
    connect(m_client, &QMqttClient::connected, this,
            &MqttClient::onMqttConnected, Qt::UniqueConnection);
    connect(m_client, &QMqttClient::disconnected, this,
            &MqttClient::onMqttDisconnected, Qt::UniqueConnection);
    connect(m_client, &QMqttClient::messageReceived, this,
            &MqttClient::onMessageReceived, Qt::UniqueConnection);

    // 发起连接
    m_client->connectToHost();
  }
#else
  Q_UNUSED(m_gameData);
  qWarning() << "MQTT 未启用：未找到 Qt6::Mqtt 或未定义 RM_HAS_QT_MQTT";
#endif
}

void MqttClient::stop() {
#ifdef RM_HAS_QT_MQTT
  if (m_client) {
    m_client->disconnectFromHost();
  }
#endif
}

void MqttClient::subscribeDefaultTopics() {
#ifdef RM_HAS_QT_MQTT
  if (!m_client)
    return;
  for (const QString &t : m_defaultTopics) {
    auto sub = m_client->subscribe(QMqttTopicFilter(t));
    if (!sub) {
      qWarning() << "MQTT: 订阅失败:" << t;
    } else {
      qDebug() << "MQTT: 已订阅主题:" << t;
    }
  }
#endif
}

void MqttClient::handlePayload(const QString &topic,
                               const QByteArray &payload) {
  if (!m_gameData)
    return;

  if (topic == QStringLiteral("GameStatus")) {
    robomaster::GameStatus msg;
    if (msg.ParseFromArray(payload.constData(), payload.size())) {
      // 保留 GameStatus 的完整语义，包括 stage_elapsed_sec。
      qDebug() << "MqttClient: GameStatus received current_stage=" << msg.current_stage()
           << "countdown_sec=" << msg.stage_countdown_sec()
           << "elapsed_sec=" << msg.stage_elapsed_sec();
      m_gameData->updateGameStatus(msg);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("GlobalUnitStatus")) {
    robomaster::GlobalUnitStatus msg;
    if (msg.ParseFromArray(payload.constData(), payload.size())) {
      m_gameData->updateGlobalUnitStatus(msg);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("GlobalLogisticsStatus")) {
    robomaster::GlobalLogisticsStatus msg;
    if (msg.ParseFromArray(payload.constData(), payload.size())) {
      m_gameData->updateGlobalLogisticsStatus(msg);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("GlobalSpecialMechanism")) {
    robomaster::GlobalSpecialMechanism msg;
    if (msg.ParseFromArray(payload.constData(), payload.size())) {
      m_gameData->updateGlobalSpecialMechanism(msg);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("AirSupportStatusSync")) {
    robomaster::AirSupportStatusSync msg;
    if (msg.ParseFromArray(payload.constData(), payload.size())) {
      m_gameData->updateAirSupportStatusSync(msg);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("TechCoreMotionStateSync")) {
    robomaster::TechCoreMotionStateSync msg;
    if (msg.ParseFromArray(payload.constData(), payload.size())) {
      m_gameData->updateTechCoreMotionStateSync(msg);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("Event")) {
    robomaster::Event msg;
    if (msg.ParseFromArray(payload.constData(), payload.size())) {
      robomaster::RoboMasterMessage wrapper;
      *wrapper.mutable_event() = msg;
      m_gameData->processProtocolData(wrapper);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("DeployModeStatusSync")) {
    robomaster::DeployModeStatusSync msg;
    if (msg.ParseFromArray(payload.constData(), payload.size())) {
      m_gameData->updateDeployModeStatusSync(msg);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("RobotInjuryStat")) {
    robomaster::RobotInjuryStat msg;
    if (msg.ParseFromArray(payload.constData(), payload.size())) {
      m_gameData->updateRobotInjury(msg);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("Buff")) {
    robomaster::Buff msg;
    if (msg.ParseFromArray(payload.constData(), payload.size())) {
      m_gameData->updateBuff(msg);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("RobotModuleStatus")) {
    robomaster::RobotModuleStatus modules;
    if (modules.ParseFromArray(payload.constData(), payload.size())) {
      m_gameData->updateRobotModuleStatus(modules);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("RobotStaticStatus")) {
    robomaster::RobotStaticStatus msg;
    if (msg.ParseFromArray(payload.constData(), payload.size())) {
      m_gameData->updateRobotStaticStatus(msg);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("RobotDynamicStatus")) {
    robomaster::RobotDynamicStatus msg;
    if (msg.ParseFromArray(payload.constData(), payload.size())) {
      m_gameData->updateRobotDynamicStatus(msg);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("RobotPosition")) {
    robomaster::RobotPosition msg;
    if (msg.ParseFromArray(payload.constData(), payload.size())) {
      m_gameData->updateRobotPosition(msg);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("RuneStatusSync")) {
    robomaster::RuneStatusSync msg;
    if (msg.ParseFromArray(payload.constData(), payload.size())) {
      m_gameData->updateRuneStatusSync(msg);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("RadarInfoToClient")) {
    robomaster::RadarInfoToClient msg;
    if (msg.ParseFromArray(payload.constData(), payload.size())) {
      m_gameData->updateRadarInfo(msg);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("TechCoreMotionStateSync")) {
    robomaster::TechCoreMotionStateSync msg;
    if (msg.ParseFromArray(payload.constData(), payload.size())) {
      m_gameData->updateTechCoreMotionStateSync(msg);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("CustomByteBlock")) {
    emit messageReceived(topic, payload.size());
    return;
  }

  if (topic == QStringLiteral("PenaltyInfo")) {
    robomaster::PenaltyInfo penalty;
    if (penalty.ParseFromArray(payload.constData(), payload.size())) {
      m_gameData->updatePenalty(penalty);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  if (topic == QStringLiteral("RobotRespawnStatus")) {
    robomaster::RobotRespawnStatus respawn;
    if (respawn.ParseFromArray(payload.constData(), payload.size())) {
      QVariantMap map;
      map["is_pending_respawn"] = respawn.is_pending_respawn();
      map["total_respawn_progress"] = static_cast<uint32_t>(respawn.total_respawn_progress());
      map["current_respawn_progress"] = static_cast<uint32_t>(respawn.current_respawn_progress());
      map["can_free_respawn"] = respawn.can_free_respawn();
      map["gold_cost_for_respawn"] = static_cast<uint32_t>(respawn.gold_cost_for_respawn());
      map["can_pay_for_respawn"] = respawn.can_pay_for_respawn();
      m_gameData->processRobotRespawnStatusMap(map);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  // 情况A：统一封装的 RoboMasterMessage（包含 payload_case）
  {
    robomaster::RoboMasterMessage message;
    if (message.ParseFromArray(payload.constData(), payload.size())) {
      qDebug() << "MqttClient: Parsed RoboMasterMessage wrapper from topic" << topic.c_str() << ", payload_case=" << message.payload_case();
      m_gameData->processProtocolData(message);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  // 情况B：无帧头，直接发送单体结构（按 Topic 或尝试解析）
  // 先尝试 RobotInjuryStat（~ 面板伤害统计）
  {
    robomaster::RobotInjuryStat injury;
    if (injury.ParseFromArray(payload.constData(), payload.size())) {
      robomaster::RoboMasterMessage wrapper;
      *wrapper.mutable_robot_injury() = injury;
      m_gameData->processProtocolData(wrapper);
      emit messageReceived(topic, payload.size());
      return;
    }
  }
  // 再尝试 RobotModuleStatus（~ 面板模块在线状态）
  {
    robomaster::RobotModuleStatus modules;
    if (modules.ParseFromArray(payload.constData(), payload.size())) {
      robomaster::RoboMasterMessage wrapper;
      *wrapper.mutable_robot_module_status() = modules;
      m_gameData->processProtocolData(wrapper);
      emit messageReceived(topic, payload.size());
      return;
    }
  }
  // 再尝试 GlobalUnitStatus（包含总伤害字段 8/9）
  {
    robomaster::GlobalUnitStatus gus;
    if (gus.ParseFromArray(payload.constData(), payload.size())) {
      robomaster::RoboMasterMessage wrapper;
      *wrapper.mutable_global_unit_status() = gus;
      m_gameData->processProtocolData(wrapper);
      emit messageReceived(topic, payload.size());
      return;
    }
  }
  // 再尝试 Event（飞镖命中等事件，无帧头直发）
  {
    robomaster::Event evt;
    if (evt.ParseFromArray(payload.constData(), payload.size())) {
      robomaster::RoboMasterMessage wrapper;
      *wrapper.mutable_event() = evt;
      m_gameData->processProtocolData(wrapper);
      emit messageReceived(topic, payload.size());
      return;
    }
  }

  // 未识别的负载格式
  qWarning() << "MQTT: 未识别的 Protobuf 负载（无帧头或类型不匹配）, topic =" << topic
             << ", size =" << payload.size();
}

#ifdef RM_HAS_QT_MQTT
void MqttClient::onMqttConnected() {
  qDebug() << "MQTT: 已连接到 Broker";
  subscribeDefaultTopics();
  // 启动 MQTT 消息日志会话（如果尚未启动）
  MqttLogger::instance()->startSession();
  emit connected();
}

void MqttClient::onMqttDisconnected() {
  qWarning() << "MQTT: 与 Broker 断开";
  emit disconnected();
}

void MqttClient::onMessageReceived(const QByteArray &payload,
                                   const QMqttTopicName &topic) {
  // 记录完整的 MQTT 消息到日志文件（排除视频帧）
  MqttLogger::instance()->logRx(topic.name(), payload);

  // 批量处理：将消息缓存在待处理队列，由定时器统一触发处理
  // 避免每个 MQTT 消息都同步阻塞 UI 线程进行 Protobuf 解析
  m_pendingMessages.append({topic.name(), payload});
  if (!m_processTimer->isActive()) {
    m_processTimer->start();
  }
}

void MqttClient::processPendingMessages() {
  auto batch = std::move(m_pendingMessages);
  m_pendingMessages.clear();
  for (const auto &[topic, payload] : batch) {
    handlePayload(topic, payload);
  }
  // 如果处理过程中有新消息到达，再次启动定时器
  if (!m_pendingMessages.isEmpty()) {
    m_processTimer->start();
  }
}
#endif

} // namespace RM
