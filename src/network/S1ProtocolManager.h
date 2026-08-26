// SPDX-License-Identifier: MIT
/**
 * @file S1ProtocolManager.h
 * @brief S1 引擎协议管理器
 * @details 实现与 S1 引擎的通信协议，用于接收游戏状态数据。
 *          这是一个可选功能，通过 RM_S1_ENGINE_HOST 环境变量启用。
 * @author Clear
 * @date 2026-04-11
 * @copyright Copyright (c) 2026 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#ifndef S1PROTOCOLMANAGER_H
#define S1PROTOCOLMANAGER_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QTcpSocket>
#include <QTimer>
#include "../core/GameData.h"

namespace RM {

/**
 * @class S1ProtocolManager
 * @brief S1 引擎协议管理器
 * @details 处理与 S1 引擎的连接、登录和游戏状态同步
 */
class S1ProtocolManager : public QObject {
  Q_OBJECT

public:
  /**
   * @brief 构造函数
   * @param gameData 游戏数据对象
   * @param parent 父对象
   */
  explicit S1ProtocolManager(::GameData *gameData, QObject *parent = nullptr);

  /**
   * @brief 析构函数
   */
  ~S1ProtocolManager();

  /**
   * @brief 设置登录凭据
   * @param account 账号
   * @param password 密码
   */
  void setCredentials(const QString &account, const QString &password);

  /**
   * @brief 设置 TID
   * @param tid 赛事ID
   */
  void setTid(int tid);

  /**
   * @brief 设置队伍ID
   * @param teamId 队伍ID
   */
  void setTeamId(int teamId);

  /**
   * @brief 连接到 S1 引擎
   * @param host 主机地址
   * @param port 端口号
   * @return bool 是否成功启动连接
   */
  bool connectToEngine(const QString &host, quint16 port);

  /**
   * @brief 断开连接
   */
  void disconnect();

  /**
   * @brief 检查是否已连接
   * @return bool 连接状态
   */
  bool isConnected() const;

signals:
  /**
   * @brief 已连接信号
   */
  void connected();

  /**
   * @brief 登录成功信号
   */
  void loginSucceeded();

  /**
   * @brief 登录失败信号
   * @param reason 失败原因
   */
  void loginFailed(const QString &reason);

  /**
   * @brief 游戏状态接收信号
   * @param state 游戏状态数据
   */
  void gameStateReceived(const QVariantMap &state);

  /**
   * @brief 错误发生信号
   * @param error 错误信息
   */
  void errorOccurred(const QString &error);

private slots:
  /**
   * @brief 处理连接成功
   */
  void onConnected();

  /**
   * @brief 处理断开连接
   */
  void onDisconnected();

  /**
   * @brief 处理数据接收
   */
  void onReadyRead();

  /**
   * @brief 处理错误
   */
  void onError(QAbstractSocket::SocketError socketError);

  /**
   * @brief 发送心跳包
   */
  void sendHeartbeat();

private:
  /**
   * @brief 发送登录请求
   */
  void sendLoginRequest();

  /**
   * @brief 处理接收到的消息
   * @param data 消息数据
   */
  void processMessage(const QByteArray &data);

private:
  // --- 网络连接 ---
  QTcpSocket *m_socket = nullptr;  // TCP 连接
  QTimer *m_heartbeatTimer = nullptr;  // 心跳定时器

  // --- 配置 ---
  ::GameData *m_gameData = nullptr;  // 游戏数据
  QString m_account;               // 账号
  QString m_password;              // 密码
  int m_tid = 0;                   // 赛事ID
  int m_teamId = 0;                // 队伍ID

  // --- 状态 ---
  bool m_loggedIn = false;         // 是否已登录
  int m_reconnectAttempts = 0;     // 重连尝试次数
  static constexpr int MAX_RECONNECT_ATTEMPTS = 5;
  static constexpr int HEARTBEAT_INTERVAL_MS = 30000;  // 30秒心跳
};

} // namespace RM

#endif // S1PROTOCOLMANAGER_H
