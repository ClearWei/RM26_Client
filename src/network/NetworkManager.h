// SPDX-License-Identifier: MIT
/**
 * @file NetworkManager.h
 * @brief 网络通信管理器
 * @details 本文件定义了 NetworkManager 类，负责处理客户端与裁判系统服务器之间的
 *          UDP 与 MQTT 通信。主要功能包括：
 *          - 绑定端口监听传入 UDP 数据包
 *          - 发送原始字节数据和 Protobuf 消息
 *          - 连接官方自定义客户端 MQTT Broker 并处理 Topic 收发
 *          - 接收并解析服务器下发的比赛数据
 *          - 将解析后的数据分发到 GameData 进行状态更新
 *
 *          通信架构：
 *          ┌─────────────┐ UDP/MQTT ┌─────────────────┐
 *          │ 裁判系统服务器 │ ◄──────► │ NetworkManager  │
 *          └─────────────┘          └────────┬────────┘
 *                                            │
 *                                   ┌────────▼────────┐
 *                                   │    GameData     │
 *                                   │  (状态更新)      │
 *                                   └─────────────────┘
 *
 * @author Clear
 * @date 2025-11-29
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

// --- 头文件包含 ---
#include "../core/GameData.h" // 比赛数据中心，用于存储解析后的比赛状态
#include "Protocol.h"         // 协议处理类，提供 Protobuf 序列化/反序列化
#include <QHostAddress>       // Qt 网络地址类
#include <QObject>            // Qt 基础对象类
#include <QUdpSocket>         // Qt UDP 套接字类

#ifdef RM_HAS_MQTT
#include "MqttManager.h" // MQTT 客户端管理器
#endif

namespace RM {

/**
 * @class NetworkManager
 * @brief UDP 网络通信管理类
 * @details 封装了 QUdpSocket，提供简洁的数据收发接口。
 *
 *          使用方式：
 *          1. 创建实例并传入 GameData 指针
 *          2. 调用 startListening() 开始监听
 *          3. 使用 sendData() 或专用方法发送数据
 *          4. 接收的数据会自动解析并更新 GameData
 *
 *          线程安全性：
 *          - 该类设计为在主线程中使用
 *          - 信号槽机制确保数据处理的线程安全
 */
class NetworkManager : public QObject {
  Q_OBJECT

public:
  // --- 构造与析构 ---

  /**
   * @brief 构造函数
   * @details 初始化 UDP 套接字并建立信号槽连接。
   *          套接字的 readyRead 信号连接到 onReadyRead 槽函数。
   *
   * @param gameData 比赛数据指针，用于存储解析后的数据
   * @param parent   Qt 父对象，用于内存管理
   *
   * @note gameData 不能为 nullptr，否则数据无法更新
   */
  explicit NetworkManager(GameData *gameData, QObject *parent = nullptr);

  /**
   * @brief 析构函数
   * @details 调用 stopListening() 关闭套接字，释放资源
   */
  ~NetworkManager();

  // --- 连接控制 ---

  /**
   * @brief 启动 UDP 监听
   * @details 将 UDP 套接字绑定到指定端口，开始接收传入的数据包。
   *          使用 QHostAddress::Any 表示监听所有网络接口。
   *
   * @param port 监听端口号，默认 10000
   * @return bool true = 绑定成功，false = 绑定失败
   *
   * @note 如果端口已被占用，会发送 errorOccurred 信号
   *
   * @example
   * @code
   * NetworkManager manager(gameData);
   * if (manager.startListening(10000)) {
   *     qDebug() << "开始监听端口 10000";
   * }
   * @endcode
   */
  bool startListening(quint16 port = 10000);

  /**
   * @brief 停止 UDP 监听
   * @details 关闭 UDP 套接字，停止接收数据。
   *          如果套接字已经处于未连接状态，则不执行任何操作。
   */
  void stopListening();

  bool isUdpListening() const;
  quint16 listeningPort() const;
  quint64 totalUdpPacketsReceived() const { return m_totalUdpPacketsReceived; }
  quint64 totalKeyboardMouseControlSent() const {
    return m_totalKeyboardMouseControlSent;
  }
  quint64 totalKeyboardMouseControlDropped() const {
    return m_totalKeyboardMouseControlDropped;
  }
  qint64 lastKeyboardMouseControlAttemptMs() const {
    return m_lastKeyboardMouseControlAttemptMs;
  }
  qint64 lastKeyboardMouseControlSentMs() const {
    return m_lastKeyboardMouseControlSentMs;
  }
  bool lastKeyboardMouseControlPublishOk() const {
    return m_lastKeyboardMouseControlPublishOk;
  }
  int lastKeyboardMouseX() const { return m_lastKeyboardMouseX; }
  int lastKeyboardMouseY() const { return m_lastKeyboardMouseY; }
  int lastKeyboardMouseZ() const { return m_lastKeyboardMouseZ; }
  bool lastKeyboardMouseLeftButtonDown() const {
    return m_lastKeyboardMouseLeftButtonDown;
  }
  bool lastKeyboardMouseRightButtonDown() const {
    return m_lastKeyboardMouseRightButtonDown;
  }
  bool lastKeyboardMouseMidButtonDown() const {
    return m_lastKeyboardMouseMidButtonDown;
  }
  quint32 lastKeyboardMouseKeyboardValue() const {
    return m_lastKeyboardMouseKeyboardValue;
  }

  // --- MQTT 连接控制 ---

#ifdef RM_HAS_MQTT
  /**
   * @brief 启动 MQTT 连接（异步，非阻塞）
   * @details 在后台线程连接 MQTT Broker，主线程不会阻塞。
   *          连接结果通过 mqttConnectCompleted 信号通知。
   * @param brokerUri Broker 地址，格式 "tcp://ip:port"
   * @param clientId 客户端 ID
   */
  void startMqtt(const QString &brokerUri, const QString &clientId);

  /**
   * @brief 停止 MQTT 连接
   */
  void stopMqtt();

  /**
   * @brief 检查 MQTT 是否已连接
   */
  bool isMqttConnected() const;
#endif

  // --- 数据发送 ---

  /**
   * @brief 发送原始字节数据
   * @details 直接发送 QByteArray 到指定目标地址。
   *          适用于已经序列化好的数据或自定义协议数据。
   *
   * @param data    待发送的字节数组
   * @param address 目标 IP 地址
   * @param port    目标端口号
   *
   * @note 发送后会触发 dataSent 信号（用于调试追踪）
   */
  void sendData(const QByteArray &data, const QHostAddress &address,
                quint16 port);

  /**
   * @brief 发送 Protobuf 消息
   * @details 将 RoboMasterMessage 对象序列化后发送到服务器。
   *          目标地址从 ConfigManager 读取。
   *
   * @param message 待发送的 Protobuf 消息对象
   * @param address 目标 IP 地址（可选，实际会使用 ConfigManager 的配置）
   * @param port    目标端口号（可选，实际会使用 ConfigManager 的配置）
   *
   * @warning 如果序列化失败，会输出警告日志但不会触发错误信号
   */
  void sendData(const robomaster::RoboMasterMessage &message,
                const QHostAddress &address, quint16 port);

  /**
   * @brief 发送客户端状态
   * @details 旧版 UDP 接口，仅用于历史兼容。
   *          该消息不属于官方 V1.3.0 自定义客户端 MQTT 协议。
   *
   * @param volume     音量级别 (0-100)
   * @param resolution 分辨率字符串，如 "1920x1080"
   * @param fullscreen 是否全屏模式
   * @param crosshair  是否启用准星显示
   * @param minimap    是否启用小地图显示
   */
  void sendClientStatus(uint32_t volume, const QString &resolution,
                        bool fullscreen, bool crosshair, bool minimap);

  /**
   * @brief 发送地图标记
   * @details 旧版 UDP 接口，仅用于历史兼容。
   *          官方 MQTT 模式下应使用 `MapClickInfoNotify`。
   *
   * @param x    标记点 X 坐标 (地图坐标系)
   * @param y    标记点 Y 坐标 (地图坐标系)
   * @param type 标记类型（如攻击点、防守点等）
   */
  void sendMapMarking(float x, float y, int type);

  /**
   * @brief 发送地图点击指令
   * @details 通过 MQTT 向机器人发送地图点击坐标。
   *
   * @param targetRobotId 目标机器人 ID，0 表示广播
   * @param x             地图 X 坐标（米）
   * @param y             地图 Y 坐标（米）
   */
  void sendMapClickInfo(quint32 targetRobotId, float x, float y, int type = 1,
                        int ascii = 0, int enemyId = 0,
                        bool sendAll = false);

  /**
   * @brief 发送机器人指令
   * @details 旧版 UDP 接口，仅用于历史兼容。
   *          该消息不属于官方 V1.3.0 自定义客户端 MQTT 协议。
   *
   * @param cmdType  指令类型（如移动、攻击等）
   * @param targetId 目标机器人 ID
   */
  void sendRobotCommand(int cmdType, int targetId);

  /**
   * @brief 发送飞镖控制指令（DartCommand）
   * @param targetId 目标 ID（1~5）
   * @param open 是否开启闸门
   * @param launchConfirm 是否确认发射
   */
  void sendDartCommand(uint32_t targetId, bool open, bool launchConfirm);

  /**
   * @brief 发送通用 CommonCommand（供 QML 调用）
    * @param cmdType 指令类型（如 3=确认复活，4=兑换立即复活）
    * @param param   额外参数含义由协议定义
    * @note 该方法对 QML 暴露，呼叫方需遵循频率限制（不高于 10Hz）。
   *       参数语义应与 `CommonCommand` protobuf 定义保持一致。
   */
  Q_INVOKABLE void sendCommonCommand(int cmdType, int param);

  /**
   * @brief 发送官方键鼠控制指令 KeyboardMouseControl（供 UI / dev hook 调用）
   * @details 官方 V1.3.0 上行 topic 为 "KeyboardMouseControl"，频率上限 75Hz。
   * @return true = 已排入 MQTT 发布，false = 被限频 / 未连接 / 序列化失败
   */
  Q_INVOKABLE bool sendKeyboardMouseControl(
      int mouseX, int mouseY, int mouseZ, bool leftButtonDown,
      bool rightButtonDown, bool midButtonDown, quint32 keyboardValue);

  /**
   * @brief 发送工程装配指令 AssemblyCommand（供 QML 调用）
   * @param operation 装配操作（1=确认装配，2=取消装配）
   * @param difficulty 装配难度（通常为 1~4）
   * @note 协议频率限制为 1Hz，本方法内置 1Hz 限频保护。
   */
  Q_INVOKABLE void sendAssemblyCommand(int operation, int difficulty);

  /**
   * @brief 发送空中支援指令 AirSupportCommand（供 QML 调用）
   * @param commandId 指令类型（0=中断/结束空中支援，1=免费呼叫，2=花费金币呼叫）
   * @note 协议频率限制为 1Hz，本方法内置 1Hz 限频保护。
   */
  Q_INVOKABLE bool sendAirSupportCommand(int commandId);

  /**
   * @brief 发送机器人性能体系选择指令 RobotPerformanceSelectionCommand
   * @param shooter 射击性能选择
   * @param chassis 底盘性能选择
   * @param sentryControl 哨兵控制性能选择
   * @note 官方 V1.3.0 协议，Topic: "RobotPerformanceSelectionCommand"
   */
  Q_INVOKABLE void sendRobotPerformanceSelection(uint32_t shooter, uint32_t chassis, uint32_t sentryControl);

  /**
   * @brief 发送英雄部署模式指令 HeroDeployModeEventCommand
   * @param mode 部署模式
   * @note 官方 V1.3.0 协议，Topic: "HeroDeployModeEventCommand"
   */
  Q_INVOKABLE void sendHeroDeployMode(uint32_t mode);

  /**
   * @brief 发送能量机关激活指令 RuneActivateCommand
   * @param activate 激活标志（1=激活）
   * @note 官方 V1.3.0 协议，Topic: "RuneActivateCommand"
   */
  Q_INVOKABLE void sendRuneActivate(uint32_t activate);

signals:
  // --- 信号定义 ---

  /**
   * @brief 数据接收信号
   * @details 当收到 UDP 数据包时发出。
   *          可用于调试或原始数据记录。
   *
   * @param data 接收到的原始字节数据
   */
  void dataReceived(const QByteArray &data);

  /**
   * @brief 错误发生信号
   * @details 当网络操作失败时发出（如端口绑定失败）。
   *
   * @param error 错误描述字符串
   */
  void errorOccurred(const QString &error);

  /**
   * @brief 数据发送信号
   * @details 当数据被发送后发出。
   *          主要用于调试和数据追踪。
   *
   * @param data 已发送的原始字节数据
   */
  void dataSent(const QByteArray &data);

  /**
   * @brief 自定义视频负载信号
   * @details 转发 MQTT CustomByteBlock 中提取出的视频负载。
   *          供视频接收链订阅，避免在 NetworkManager 内直接依赖解码器。
   *
   * @param data 需要交给视频解码链的原始负载
   */
  void customVideoPayloadReceived(const QByteArray &data);

#ifdef RM_HAS_MQTT
  /**
   * @brief MQTT 消息被客户端本地观测到
   * @details 该时间戳来自客户端本机 MQTT 回调，不代表赛事引擎发布时刻。
   *          高频 CustomByteBlock 视频默认最多每秒采样一次；普通 topic
   *          仍逐条转发。
   */
  void mqttMessageObserved(const QString &topic, int payloadSize,
                           const QString &payloadSha1, qint64 receivedMs);

  /**
   * @brief MQTT 异步连接完成信号
   * @param success 是否连接成功
   * @param error 失败时的错误描述
   */
  void mqttConnectCompleted(bool success, const QString &error);
#endif

private slots:
  // --- 私有槽函数 ---

  /**
   * @brief UDP 数据就绪处理槽
   * @details 当 QUdpSocket 有待读取的数据时自动调用。
   *          循环读取所有挂起的数据报并调用 processData 处理。
   */
  void onReadyRead();

private:
  // --- 私有成员 ---

  QUdpSocket *m_udpSocket; ///< UDP 套接字实例
  GameData *m_gameData;    ///< 比赛数据中心指针（不拥有所有权）

  /**
   * @brief 处理接收到的数据
   * @details 内部方法，使用 Protocol 类解析数据包，
   *          成功后将消息传递给 GameData 更新状态。
   *
   * @param data 接收到的原始字节数据
   *
   * @note 解析失败的数据包会被静默丢弃（不输出日志以避免刷屏）
   */
  void processData(const QByteArray &data);

#ifdef RM_HAS_MQTT
  // --- MQTT 相关私有方法 ---

  /**
   * @brief 处理 MQTT 消息
   * @param topic 主题名称
   * @param payload 消息载荷
   */
  void processMqttMessage(const QString &topic, const QByteArray &payload);

  /**
   * @brief 解析 CustomByteBlock 数据
   * @param data 原始字节数据
   * @details 解析机器人自定义状态并更新 GameData
   */
  void parseCustomByteBlock(const QByteArray &data);

  /**
   * @brief 订阅所有需要的 MQTT Topic
   */
  void subscribeToTopics();

  // --- MQTT 成员变量 ---
  MqttManager *m_mqttManager = nullptr; ///< MQTT 客户端管理器
  bool m_hasMqttGameStatus = false; ///< 是否已有 MQTT 比赛全局状态
  robomaster::GameStage m_lastMqttStage =
      robomaster::STAGE_NOT_STARTED; ///< 最近一次 MQTT 比赛阶段
  quint16 m_lastMqttTimeRemaining = 420; ///< 最近一次 MQTT 阶段剩余时间 (秒)
  bool m_lastMqttPaused = false;         ///< 最近一次 MQTT 暂停状态
#endif

  qint64 m_lastCommonCommandSentMs = 0;  ///< CommonCommand 最近发送时间戳（ms）
  qint64 m_lastAssemblyCommandSentMs = 0; ///< AssemblyCommand 最近发送时间戳（ms）
  qint64 m_lastAirSupportCommandSentMs = 0; ///< AirSupportCommand 最近发送时间戳（ms）
  qint64 m_lastMapClickInfoSentMs = 0; ///< MapClickInfoNotify 最近发送时间戳（ms）
  qint64 m_lastKeyboardMouseControlAttemptMs = 0; ///< KeyboardMouseControl 最近尝试发送时间戳（ms）
  qint64 m_lastKeyboardMouseControlSentMs = 0; ///< KeyboardMouseControl 最近成功发布排队时间戳（ms）
  qint64 m_lastKeyboardMouseControlDropLogMs = 0; ///< KeyboardMouseControl 最近丢弃日志时间戳（ms）
  quint64 m_totalKeyboardMouseControlSent = 0; ///< KeyboardMouseControl 成功排队数量
  quint64 m_totalKeyboardMouseControlDropped = 0; ///< KeyboardMouseControl 丢弃数量
  bool m_lastKeyboardMouseControlPublishOk = false; ///< 最近一次 KeyboardMouseControl 发布结果
  int m_lastKeyboardMouseX = 0;
  int m_lastKeyboardMouseY = 0;
  int m_lastKeyboardMouseZ = 0;
  bool m_lastKeyboardMouseLeftButtonDown = false;
  bool m_lastKeyboardMouseRightButtonDown = false;
  bool m_lastKeyboardMouseMidButtonDown = false;
  quint32 m_lastKeyboardMouseKeyboardValue = 0;
  quint64 m_totalUdpPacketsReceived = 0; ///< UDP 已接收的数据报数量
};

} // namespace RM

#endif // NETWORKMANAGER_H
