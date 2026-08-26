/**
 * @file ClientDevHooks.cpp
 * @brief 可选开发 Hook 层实现
 */

#include "ClientDevHooks.h"

#include "../config/ConfigManager.h"
#include "../core/GameData.h"
#include "../network/HevcDecoder.h"
#include "../network/NetworkManager.h"
#include "../network/VideoReceiver.h"
#include "../widgets/VideoBackgroundWidget.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QHttpHeaders>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QTcpServer>
#include <QWidget>
#include <QtGlobal>

namespace {

QHttpServerResponse jsonResponse(const QJsonObject &obj, int statusCode = 200) {
  const QByteArray data =
      QJsonDocument(obj).toJson(QJsonDocument::Compact);
  QHttpServerResponse response(
      data, QHttpServerResponder::StatusCode(statusCode));
  QHttpHeaders headers;
  headers.append(QHttpHeaders::WellKnownHeader::ContentType,
                 "application/json");
  response.setHeaders(headers);
  return response;
}

QHttpServerResponse jsonError(const QString &message, int statusCode = 400) {
  QJsonObject obj;
  obj["error"] = message;
  return jsonResponse(obj, statusCode);
}

QJsonObject parseJsonBody(const QHttpServerRequest &request, bool *ok) {
  const QJsonDocument doc = QJsonDocument::fromJson(request.body());
  *ok = doc.isObject();
  return *ok ? doc.object() : QJsonObject();
}

QString stageName(GameStage stage) {
  switch (stage) {
  case GameStage::NOT_STARTED:
    return QStringLiteral("NOT_STARTED");
  case GameStage::PREPARATION:
    return QStringLiteral("PREPARATION");
  case GameStage::SELF_CHECK:
    return QStringLiteral("SELF_CHECK");
  case GameStage::COUNTDOWN:
    return QStringLiteral("COUNTDOWN");
  case GameStage::BATTLE:
    return QStringLiteral("BATTLE");
  case GameStage::SETTLEMENT:
    return QStringLiteral("SETTLEMENT");
  default:
    return QStringLiteral("UNKNOWN");
  }
}

} // namespace

namespace RM {

ClientDevHooks::ClientDevHooks(GameData *gameData,
                               NetworkManager *networkManager,
                               VideoReceiver *videoReceiver,
                               VideoBackgroundWidget *videoWidget,
                               HevcDecoder *hevcDecoder, QWidget *window,
                               QObject *parent)
    : QObject(parent), m_gameData(gameData), m_networkManager(networkManager),
      m_videoReceiver(videoReceiver), m_videoWidget(videoWidget),
  m_hevcDecoder(hevcDecoder),
      m_window(window), m_httpServer(new QHttpServer(this)) {
  const QString portStr = QString::fromUtf8(qgetenv("RM_DEVTOOLS_PORT"));
  bool ok = false;
  if (!portStr.isEmpty()) {
    const quint16 port = portStr.toUShort(&ok);
    if (ok) {
      m_port = port;
    }
  }

  m_runDir = QString::fromUtf8(qgetenv("RM_DEVTOOLS_RUN_DIR"));
  if (m_runDir.isEmpty()) {
    m_runDir = QDir::currentPath() + QStringLiteral("/logs/devhooks");
  }

  setupRoutes();
}

ClientDevHooks::~ClientDevHooks() { stop(); }

bool ClientDevHooks::isEnabled() {
  const QByteArray devtoolsEnv = qgetenv("RM_DEVTOOLS");
  return !devtoolsEnv.isEmpty() && devtoolsEnv != "0" &&
         devtoolsEnv.toLower() != "false";
}

bool ClientDevHooks::start() {
  if (m_running) {
    return true;
  }

  auto *tcpServer = new QTcpServer(this);
  if (!tcpServer->listen(QHostAddress::LocalHost, m_port)) {
    qWarning() << "[ClientDevHooks] Failed to listen on port" << m_port;
    delete tcpServer;
    return false;
  }

  if (!m_httpServer->bind(tcpServer)) {
    qWarning() << "[ClientDevHooks] Failed to bind HTTP server";
    delete tcpServer;
    return false;
  }

  m_running = true;
  recordTraceEvent(QStringLiteral("devhooks_started"),
                   QJsonObject{{"port", int(m_port)}});
  qInfo() << "[ClientDevHooks] Started on port" << m_port;
  return true;
}

void ClientDevHooks::stop() {
  if (!m_running) {
    return;
  }
  m_running = false;
  recordTraceEvent(QStringLiteral("devhooks_stopped"));
}

void ClientDevHooks::recordTraceEvent(const QString &event,
                                      const QJsonObject &payload) {
  QJsonObject entry;
  entry["event"] = event;
  entry["timestamp"] = QDateTime::currentMSecsSinceEpoch();
  entry["payload"] = payload;
  m_traceBuffer.append(entry);
  constexpr int kMaxTraceEntries = 200;
  while (m_traceBuffer.size() > kMaxTraceEntries) {
    m_traceBuffer.removeFirst();
  }
}

void ClientDevHooks::setupRoutes() {
  m_httpServer->route("/health", QHttpServerRequest::Method::Get,
                      [this]() { return handleHealth(); });
  m_httpServer->route("/state", QHttpServerRequest::Method::Get,
                      [this]() { return handleState(); });
  m_httpServer->route("/actions/common-command",
                      QHttpServerRequest::Method::Post,
                      [this](const QHttpServerRequest &request) {
                        return handleCommonCommand(request);
                      });
  m_httpServer->route("/actions/key-press", QHttpServerRequest::Method::Post,
                      [this](const QHttpServerRequest &request) {
                        return handleKeyPress(request);
                      });
  m_httpServer->route("/actions/key-release", QHttpServerRequest::Method::Post,
                      [this](const QHttpServerRequest &request) {
                        return handleKeyRelease(request);
                      });
  m_httpServer->route("/actions/screenshot",
                      QHttpServerRequest::Method::Post,
                      [this](const QHttpServerRequest &request) {
                        return handleScreenshot(request);
                      });
  m_httpServer->route("/actions/flush-trace",
                      QHttpServerRequest::Method::Post,
                      [this]() { return handleFlushTrace(); });
}

QJsonObject ClientDevHooks::collectGameState() const {
  QJsonObject gameState;
  if (!m_gameData) {
    return gameState;
  }
  const GameStage stage = m_gameData->getCurrentStage();
  gameState["stage"] = stageName(stage);
  gameState["stage_code"] = static_cast<int>(stage);
  gameState["remaining_time"] = m_gameData->getGameTime();
  gameState["remaining_time_text"] = m_gameData->getFormattedGameTime();
  gameState["red_score"] = m_gameData->redScore();
  gameState["blue_score"] = m_gameData->blueScore();
  gameState["current_round"] = m_gameData->currentRound();
  gameState["is_paused"] = m_gameData->is_paused();
  gameState["current_hp"] = m_gameData->currentHealth();
  return gameState;
}

QJsonObject ClientDevHooks::collectTopBarState() const {
  QJsonObject topBar;
  if (!m_gameData) {
    return topBar;
  }

  auto buildTeamState = [this](bool isBlue, const QList<int> &displayOrder) {
    QJsonObject team;
    team["team_name"] = isBlue ? m_gameData->blueTeamName()
                               : m_gameData->redTeamName();
    team["base_hp"] =
        isBlue ? m_gameData->blueBaseHealth() : m_gameData->redBaseHealth();
    team["base_max_hp"] = isBlue ? m_gameData->blueBaseMaxHealth()
                                 : m_gameData->redBaseMaxHealth();
    team["outpost_hp"] = isBlue ? m_gameData->blueOutpostHealth()
                                : m_gameData->redOutpostHealth();
    team["outpost_max_hp"] = isBlue ? m_gameData->blueOutpostMaxHealth()
                                    : m_gameData->redOutpostMaxHealth();
    team["base_invincible"] =
        isBlue ? m_gameData->blueBaseInvincible()
               : m_gameData->redBaseInvincible();
    team["defense_bonus"] =
        isBlue ? m_gameData->blueDefenseBonus()
               : m_gameData->redDefenseBonus();

    QJsonArray robots;
    for (int robotId : displayOrder) {
      QJsonObject robot = QJsonObject::fromVariantMap(m_gameData->getRobotInfo(robotId));
      robot["robot_id"] = robotId;
      robots.append(robot);
    }
    team["robots"] = robots;
    return team;
  };

  topBar["game_phase"] = m_gameData->gamePhaseString();
  topBar["is_paused"] = m_gameData->is_paused();
  topBar["red"] = buildTeamState(false, {7, 6, 4, 3, 2, 1});
  topBar["blue"] = buildTeamState(true, {101, 102, 103, 104, 106, 107});
  return topBar;
}

QJsonObject ClientDevHooks::collectNetworkState() const {
  QJsonObject networkState;
  networkState["server_ip"] = ConfigManager::instance().getServerIP();
  networkState["server_port"] = int(ConfigManager::instance().getServerPort());
  networkState["mqtt_broker"] = ConfigManager::instance().getMqttBroker();
  networkState["mqtt_port"] = int(ConfigManager::instance().getMqttPort());
  if (m_networkManager) {
#ifdef RM_HAS_MQTT
    networkState["mqtt_connected"] = m_networkManager->isMqttConnected();
#else
    networkState["mqtt_connected"] = false;
#endif
    networkState["udp_listening"] = m_networkManager->isUdpListening();
    networkState["udp_port"] = int(m_networkManager->listeningPort());
    networkState["packets_received"] =
        static_cast<qint64>(m_networkManager->totalUdpPacketsReceived());
    QJsonObject keyboardMouse;
    keyboardMouse["sent"] = static_cast<qint64>(
        m_networkManager->totalKeyboardMouseControlSent());
    keyboardMouse["dropped"] = static_cast<qint64>(
        m_networkManager->totalKeyboardMouseControlDropped());
    keyboardMouse["last_attempt_ms"] =
        m_networkManager->lastKeyboardMouseControlAttemptMs();
    keyboardMouse["last_sent_ms"] =
        m_networkManager->lastKeyboardMouseControlSentMs();
    keyboardMouse["last_publish_ok"] =
        m_networkManager->lastKeyboardMouseControlPublishOk();
    keyboardMouse["mouse_x"] = m_networkManager->lastKeyboardMouseX();
    keyboardMouse["mouse_y"] = m_networkManager->lastKeyboardMouseY();
    keyboardMouse["mouse_z"] = m_networkManager->lastKeyboardMouseZ();
    keyboardMouse["left_button_down"] =
        m_networkManager->lastKeyboardMouseLeftButtonDown();
    keyboardMouse["right_button_down"] =
        m_networkManager->lastKeyboardMouseRightButtonDown();
    keyboardMouse["mid_button_down"] =
        m_networkManager->lastKeyboardMouseMidButtonDown();
    keyboardMouse["keyboard_value"] =
        static_cast<int>(m_networkManager->lastKeyboardMouseKeyboardValue());
    networkState["keyboard_mouse_control"] = keyboardMouse;
  }
  return networkState;
}

QJsonObject ClientDevHooks::collectVideoStats() const {
  QJsonObject videoStats;
  videoStats["source_url"] = ConfigManager::instance().getVideoStreamUrl();
  videoStats["resolution"] = ConfigManager::instance().getVideoResolution();
  videoStats["fps"] = 0.0;
  videoStats["presented_fps"] = 0.0;
  videoStats["packets_received"] = 0;
  videoStats["frames_decoded"] = 0;
  videoStats["frames_assembled"] = 0;
  videoStats["frames_presented"] = 0;
  videoStats["frames_dropped"] = 0;
  videoStats["last_frame_ms"] = 0;
  videoStats["last_packet_ms"] = 0;
  videoStats["last_assembled_ms"] = 0;
  videoStats["last_received_frame_ms"] = 0;
  videoStats["last_presented_frame_ms"] = 0;
  videoStats["last_render_complete_ms"] = 0;
  videoStats["last_receive_to_present_ms"] = 0.0;
  videoStats["avg_receive_to_present_ms"] = 0.0;
  videoStats["last_assembled_to_present_ms"] = 0.0;
  videoStats["avg_assembled_to_present_ms"] = 0.0;
  videoStats["last_packet_to_present_ms"] = 0.0;
  videoStats["avg_packet_to_present_ms"] = 0.0;
  videoStats["gap_events"] = 0;
  videoStats["timeout_frames"] = 0;
  videoStats["waiting_for_keyframe_recovery"] = false;
  videoStats["decoder_last_decode_ms"] = 0.0;
  videoStats["decoder_avg_decode_ms"] = 0.0;
  videoStats["decoder_state"] = QStringLiteral("IDLE");

  if (!m_videoReceiver) {
    return videoStats;
  }

  videoStats["listening"] = m_videoReceiver->isListening();
  videoStats["listening_port"] = int(m_videoReceiver->listeningPort());
  videoStats["packets_received"] =
      static_cast<qint64>(m_videoReceiver->totalPacketsReceived());
  videoStats["bytes_received"] =
      static_cast<qint64>(m_videoReceiver->totalBytesReceived());
  videoStats["frames_assembled"] =
      static_cast<qint64>(m_videoReceiver->totalFramesAssembled());
  videoStats["last_frame_ms"] = m_videoReceiver->lastFrameMs();
  videoStats["last_packet_ms"] = m_videoReceiver->lastPacketMs();
  videoStats["last_assembled_ms"] = m_videoReceiver->lastAssembledFrameMs();
  videoStats["fps"] = m_videoReceiver->lastFps();
  videoStats["gap_events"] =
      static_cast<qint64>(m_videoReceiver->totalGapEvents());
  videoStats["timeout_frames"] =
      static_cast<qint64>(m_videoReceiver->totalTimeoutFrames());
  videoStats["waiting_for_keyframe_recovery"] =
      m_videoReceiver->waitingForKeyframeRecovery();
  if (m_videoWidget) {
    videoStats["frames_presented"] =
        static_cast<qint64>(m_videoWidget->presentedFrameCount());
    videoStats["frames_dropped"] =
        static_cast<qint64>(m_videoWidget->droppedFrameCount());
    videoStats["presented_fps"] = m_videoWidget->presentedFps();
    videoStats["last_received_frame_ms"] =
        m_videoWidget->lastReceivedFrameMs();
    videoStats["last_presented_frame_ms"] =
        m_videoWidget->lastPresentedFrameMs();
    videoStats["last_render_complete_ms"] =
        m_videoWidget->lastRenderCompleteMs();
    videoStats["last_receive_to_present_ms"] =
        m_videoWidget->lastReceiveToPresentMs();
    videoStats["avg_receive_to_present_ms"] =
        m_videoWidget->avgReceiveToPresentMs();
    videoStats["last_assembled_to_present_ms"] =
        m_videoWidget->lastAssembledToPresentMs();
    videoStats["avg_assembled_to_present_ms"] =
        m_videoWidget->avgAssembledToPresentMs();
    videoStats["last_packet_to_present_ms"] =
        m_videoWidget->lastPacketToPresentMs();
    videoStats["avg_packet_to_present_ms"] =
        m_videoWidget->avgPacketToPresentMs();
  }
  if (m_hevcDecoder) {
    videoStats["frames_decoded"] = m_hevcDecoder->getDecodedFrameCount();
    videoStats["decoder_state"] =
        m_hevcDecoder->isInitialized() ? QStringLiteral("READY")
                                       : QStringLiteral("IDLE");
    videoStats["decoder_last_decode_ms"] =
        m_hevcDecoder->getLastDecodeTimeMs();
    videoStats["decoder_avg_decode_ms"] =
        m_hevcDecoder->getAverageDecodeTimeMs();
  }
  return videoStats;
}

QString ClientDevHooks::detectCurrentPanel() const {
  if (!m_gameData) {
    return QStringLiteral("NONE");
  }

  const QVariantList activePopups = m_gameData->activePopups();
  for (const QVariant &entry : activePopups) {
    const QVariantMap map = entry.toMap();
    const QString type = map.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("RobotRespawn")) {
      return QStringLiteral("RESPAWN");
    }
    if (type == QStringLiteral("BattlePause")) {
      return QStringLiteral("BATTLE_PAUSE");
    }
    if (type == QStringLiteral("PrepPhase")) {
      return QStringLiteral("PREP_PHASE");
    }
    if (type == QStringLiteral("Countdown")) {
      return QStringLiteral("COUNTDOWN");
    }
    if (type == QStringLiteral("Out")) {
      return QStringLiteral("OUT");
    }
  }

  return QStringLiteral("NONE");
}

QJsonObject ClientDevHooks::collectUiState() const {
  QJsonObject uiState;
  uiState["current_robot_id"] =
      m_gameData ? m_gameData->getMyRobotId() : 0;
  uiState["window_focused"] = m_window ? m_window->isActiveWindow() : false;
  uiState["current_panel"] = detectCurrentPanel();
  uiState["tactical_large_map_mode"] =
      m_window ? m_window->property("tacticalLargeMapMode").toBool() : false;
  uiState["tactical_large_map_rendered"] =
      m_window ? m_window->property("tacticalLargeMapRendered").toBool()
               : false;

  if (m_gameData) {
    QJsonArray popups;
    const QVariantList activePopups = m_gameData->activePopups();
    for (const QVariant &entry : activePopups) {
      popups.append(QJsonObject::fromVariantMap(entry.toMap()));
    }
    uiState["active_popups"] = popups;
  }
  return uiState;
}

QJsonObject ClientDevHooks::collectRespawnState() const {
  if (!m_gameData) {
    return QJsonObject();
  }
  return QJsonObject::fromVariantMap(m_gameData->getLastRespawnStatus());
}

QJsonObject ClientDevHooks::collectState() const {
  QJsonObject root;
  root["version"] = QStringLiteral("devhooks-v1");
  root["timestamp"] = QDateTime::currentMSecsSinceEpoch();
  root["game_state"] = collectGameState();
  root["top_bar"] = collectTopBarState();
  root["network_state"] = collectNetworkState();
  root["video_stats"] = collectVideoStats();
  root["ui_state"] = collectUiState();
  root["respawn_state"] = collectRespawnState();
  return root;
}

QHttpServerResponse ClientDevHooks::handleHealth() const {
  QJsonObject obj;
  obj["status"] = QStringLiteral("ok");
  obj["port"] = int(m_port);
  obj["timestamp"] = QDateTime::currentMSecsSinceEpoch();
  obj["enabled"] = true;
  return jsonResponse(obj);
}

QHttpServerResponse ClientDevHooks::handleState() const {
  return jsonResponse(collectState());
}

QHttpServerResponse
ClientDevHooks::handleCommonCommand(const QHttpServerRequest &request) {
  bool ok = false;
  const QJsonObject obj = parseJsonBody(request, &ok);
  if (!ok) {
    return jsonError(QStringLiteral("Invalid JSON body"));
  }
  if (!m_networkManager) {
    return jsonError(QStringLiteral("Network manager unavailable"), 503);
  }

  const int cmdType = obj.value(QStringLiteral("cmd_type")).toInt(-1);
  const int param = obj.value(QStringLiteral("param")).toInt(0);
  if (cmdType < 0) {
    return jsonError(QStringLiteral("Missing or invalid cmd_type"));
  }

  m_networkManager->sendCommonCommand(cmdType, param);
  recordTraceEvent(QStringLiteral("common_command_dispatched"),
                   QJsonObject{{"cmd_type", cmdType}, {"param", param}});

  return jsonResponse(QJsonObject{{"emitted", true},
                                  {"cmd_type", cmdType},
                                  {"param", param}});
}

bool ClientDevHooks::dispatchKeyEvent(int key, bool pressed,
                                      Qt::KeyboardModifiers modifiers) {
  if (!m_window) {
    return false;
  }

  QWidget *target = QApplication::focusWidget();
  if (!target) {
    target = m_window;
  }

  const auto type = pressed ? QEvent::KeyPress : QEvent::KeyRelease;
  QKeyEvent event(type, key, modifiers);
  return QApplication::sendEvent(target, &event);
}

QHttpServerResponse
ClientDevHooks::handleKeyPress(const QHttpServerRequest &request) {
  bool ok = false;
  const QJsonObject obj = parseJsonBody(request, &ok);
  if (!ok) {
    return jsonError(QStringLiteral("Invalid JSON body"));
  }
  const int key = obj.value(QStringLiteral("key")).toInt(-1);
  if (key < 0) {
    return jsonError(QStringLiteral("Missing or invalid key"));
  }
  const auto modifiers = static_cast<Qt::KeyboardModifiers>(
      obj.value(QStringLiteral("modifiers")).toInt(0));
  const bool emitted = dispatchKeyEvent(key, true, modifiers);
  recordTraceEvent(QStringLiteral("key_pressed"),
                   QJsonObject{{"key", key},
                               {"modifiers", static_cast<int>(modifiers)}});
  return jsonResponse(QJsonObject{{"emitted", emitted},
                                  {"key", key},
                                  {"modifiers", static_cast<int>(modifiers)}});
}

QHttpServerResponse
ClientDevHooks::handleKeyRelease(const QHttpServerRequest &request) {
  bool ok = false;
  const QJsonObject obj = parseJsonBody(request, &ok);
  if (!ok) {
    return jsonError(QStringLiteral("Invalid JSON body"));
  }
  const int key = obj.value(QStringLiteral("key")).toInt(-1);
  if (key < 0) {
    return jsonError(QStringLiteral("Missing or invalid key"));
  }
  const auto modifiers = static_cast<Qt::KeyboardModifiers>(
      obj.value(QStringLiteral("modifiers")).toInt(0));
  const bool emitted = dispatchKeyEvent(key, false, modifiers);
  recordTraceEvent(QStringLiteral("key_released"),
                   QJsonObject{{"key", key},
                               {"modifiers", static_cast<int>(modifiers)}});
  return jsonResponse(QJsonObject{{"emitted", emitted},
                                  {"key", key},
                                  {"modifiers", static_cast<int>(modifiers)}});
}

QHttpServerResponse
ClientDevHooks::handleScreenshot(const QHttpServerRequest &request) {
  bool ok = false;
  const QJsonObject obj = parseJsonBody(request, &ok);
  if (!ok) {
    return jsonError(QStringLiteral("Invalid JSON body"));
  }
  if (!m_window) {
    return jsonError(QStringLiteral("Window unavailable"), 503);
  }

  QString filename = obj.value(QStringLiteral("filename")).toString();
  if (filename.isEmpty()) {
    filename = QStringLiteral("screenshot-%1.png")
                   .arg(QDateTime::currentDateTime().toString(
                       QStringLiteral("yyyyMMdd-HHmmsszzz")));
  }

  QFileInfo fileInfo(filename);
  QString absolutePath = fileInfo.isAbsolute()
                             ? fileInfo.absoluteFilePath()
                             : QDir(m_runDir).filePath(filename);
  QDir().mkpath(QFileInfo(absolutePath).absolutePath());

  const bool saved = m_window->grab().save(absolutePath);
  recordTraceEvent(QStringLiteral("screenshot_requested"),
                   QJsonObject{{"path", absolutePath}, {"saved", saved}});
  return jsonResponse(
      QJsonObject{{"saved", saved}, {"path", absolutePath}},
      saved ? 200 : 500);
}

QHttpServerResponse ClientDevHooks::handleFlushTrace() {
  QJsonObject response;
  response["events"] = m_traceBuffer;
  response["count"] = m_traceBuffer.size();
  m_traceBuffer = QJsonArray();
  return jsonResponse(response);
}

} // namespace RM
