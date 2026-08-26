// SPDX-License-Identifier: MIT
/**
 * @file S1ProtocolManager.cpp
 * @brief S1 引擎协议管理器实现
 * @author Clear
 * @date 2026-04-11
 * @copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#include "S1ProtocolManager.h"
#include "../core/GameData.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

using namespace RM;

S1ProtocolManager::S1ProtocolManager(GameData *gameData, QObject *parent)
    : QObject(parent), m_gameData(gameData) {
  m_socket = new QTcpSocket(this);
  m_heartbeatTimer = new QTimer(this);

  // 连接信号槽
  connect(m_socket, &QTcpSocket::connected, this, &S1ProtocolManager::onConnected);
  connect(m_socket, &QTcpSocket::disconnected, this, &S1ProtocolManager::onDisconnected);
  connect(m_socket, &QTcpSocket::readyRead, this, &S1ProtocolManager::onReadyRead);
  connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
          this, &S1ProtocolManager::onError);
  connect(m_heartbeatTimer, &QTimer::timeout, this, &S1ProtocolManager::sendHeartbeat);
}

S1ProtocolManager::~S1ProtocolManager() {
  disconnect();
}

void S1ProtocolManager::setCredentials(const QString &account, const QString &password) {
  m_account = account;
  m_password = password;
}

void S1ProtocolManager::setTid(int tid) {
  m_tid = tid;
}

void S1ProtocolManager::setTeamId(int teamId) {
  m_teamId = teamId;
}

bool S1ProtocolManager::connectToEngine(const QString &host, quint16 port) {
  if (m_socket->state() != QAbstractSocket::UnconnectedState) {
    qWarning() << "S1ProtocolManager: Already connected or connecting";
    return false;
  }

  qInfo() << "S1ProtocolManager: Connecting to" << host << ":" << port;
  m_socket->connectToHost(host, port);
  return true;
}

void S1ProtocolManager::disconnect() {
  m_heartbeatTimer->stop();
  if (m_socket->state() != QAbstractSocket::UnconnectedState) {
    m_socket->disconnectFromHost();
  }
  m_loggedIn = false;
}

bool S1ProtocolManager::isConnected() const {
  return m_socket->state() == QAbstractSocket::ConnectedState && m_loggedIn;
}

void S1ProtocolManager::onConnected() {
  qInfo() << "S1ProtocolManager: Connected to engine";
  m_reconnectAttempts = 0;
  emit connected();

  // 发送登录请求
  sendLoginRequest();
}

void S1ProtocolManager::onDisconnected() {
  qInfo() << "S1ProtocolManager: Disconnected from engine";
  m_heartbeatTimer->stop();
  m_loggedIn = false;
}

void S1ProtocolManager::onReadyRead() {
  while (m_socket->canReadLine()) {
    QByteArray data = m_socket->readLine();
    processMessage(data);
  }
}

void S1ProtocolManager::onError(QAbstractSocket::SocketError socketError) {
  Q_UNUSED(socketError)
  QString errorMsg = m_socket->errorString();
  qWarning() << "S1ProtocolManager: Socket error:" << errorMsg;
  emit errorOccurred(errorMsg);
}

void S1ProtocolManager::sendHeartbeat() {
  if (!m_loggedIn || m_socket->state() != QAbstractSocket::ConnectedState) {
    return;
  }

  // 发送心跳包
  QJsonObject heartbeat;
  heartbeat["type"] = "heartbeat";
  heartbeat["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

  QByteArray data = QJsonDocument(heartbeat).toJson(QJsonDocument::Compact);
  data.append('\n');
  m_socket->write(data);
}

void S1ProtocolManager::sendLoginRequest() {
  QJsonObject login;
  login["type"] = "login";
  login["account"] = m_account.isEmpty() ? "anonymous" : m_account;
  login["password"] = m_password;
  if (m_tid > 0) {
    login["tid"] = m_tid;
  }
  if (m_teamId > 0) {
    login["teamId"] = m_teamId;
  }

  QByteArray data = QJsonDocument(login).toJson(QJsonDocument::Compact);
  data.append('\n');
  m_socket->write(data);
  qInfo() << "S1ProtocolManager: Login request sent";
}

void S1ProtocolManager::processMessage(const QByteArray &data) {
  QJsonDocument doc = QJsonDocument::fromJson(data);
  if (doc.isNull() || !doc.isObject()) {
    qWarning() << "S1ProtocolManager: Invalid JSON received:" << data;
    return;
  }

  QJsonObject msg = doc.object();
  QString type = msg["type"].toString();

  if (type == "login_response") {
    bool success = msg["success"].toBool();
    if (success) {
      qInfo() << "S1ProtocolManager: Login successful";
      m_loggedIn = true;
      emit loginSucceeded();
      // 启动心跳
      m_heartbeatTimer->start(HEARTBEAT_INTERVAL_MS);
    } else {
      QString reason = msg["reason"].toString("Unknown error");
      qWarning() << "S1ProtocolManager: Login failed:" << reason;
      emit loginFailed(reason);
    }
  }
  else if (type == "game_state") {
    // 转换 JSON 对象到 QVariantMap
    QVariantMap state = msg.toVariantMap();
    emit gameStateReceived(state);
  }
  else if (type == "heartbeat_response") {
    // 心跳响应，可以记录延迟等
    qDebug() << "S1ProtocolManager: Heartbeat acknowledged";
  }
  else if (type == "error") {
    QString errorMsg = msg["message"].toString();
    qWarning() << "S1ProtocolManager: Server error:" << errorMsg;
    emit errorOccurred(errorMsg);
  }
  else {
    qDebug() << "S1ProtocolManager: Unknown message type:" << type;
  }
}
