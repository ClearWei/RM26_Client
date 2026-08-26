// SPDX-License-Identifier: MIT
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/**
 * @file MainWindow.h
 * @brief RoboMaster 2026 自定义客户端主界面定义
 * @details
 * 定义了客户端的主窗口类，包括UI布局初始化、事件处理、面板管理和核心业务逻辑的集成。
 * @author Clear
 * @date 2025-11-16
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 */

#include "LayoutConstants.h"
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QPoint>
#include <QPointer>
#include <QQueue>
#include <QSize>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <QVariant>

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QProgressBar;
class QTimer;
class QMediaPlayer;
class QAudioOutput;
class QQuickWidget; // QML 嵌入组件
class QMouseEvent;
class QWheelEvent;
QT_END_NAMESPACE

// 全局命名空间的前向声明
class HealthBarWidget;
class CrosshairWidget;
class HeatRingWidget;
class RobotStatusWidget;

#include "../core/GameData.h"
#include "../network/NetworkManager.h"

// 战术分析前向声明 (指针成员，避免拉入 Q_GADGET 静态链)
namespace RM {
class TacticalAnalyzer;
class DataFreshnessGuard;
class MapCoordinateMapper;
class ThreatRanker;
class ExecutionFusion;
}
#include "../widgets/VideoBackgroundWidget.h"
#include "../widgets/GameResultWidget.h"

namespace RM {

// RM命名空间的前向声明
class RobotStatusPanel;
class InfoPanel;
class OperationsPanel;
class HelpOverlayWidget;
class DebugLogWidget;
class AROverlayManager;
class AROverlayWidget;
class HeroVideoWidget;
class MqttClient; // MQTT 客户端前向声明
class ClientDevHooks; // 开发钩子前向声明
class S1ProtocolManager;   // S1 协议管理器前向声明

class KillFeedWidget;

/**
 * @brief RoboMaster 2026 主界面类
 * @details
 * 基于官方客户端和2026界面规范重新设计，负责管理所有子界面和核心交互逻辑。
 */
class MainWindow : public QMainWindow {
  Q_OBJECT
  Q_PROPERTY(bool tacticalLargeMapMode READ tacticalLargeMapMode NOTIFY
                 tacticalLargeMapModeChanged)
  Q_PROPERTY(bool tacticalLargeMapRendered READ tacticalLargeMapRendered)

public:
  /**
   * @brief 构造函数
   * @param parent 父窗口指针
   */
  explicit MainWindow(QWidget *parent = nullptr);

  Q_INVOKABLE void toggleSettingsPanel();

  /**
   * @brief 析构函数
   */
  ~MainWindow();

  /**
   * @brief 初始化界面布局
   * @details 严格按照2026规范进行像素级布局，设置各区域的大小和位置。
   */
  void initializeLayout();

  static QString officialEventSoundFileName(int eventId);
  static QString allyBaseArmorOpenedSoundFileName();
  static int allyBaseArmorOpenedSoundLoopCount();
  static QString paidRespawnSoundFileName(int robotId);
  bool tacticalLargeMapMode() const { return m_tacticalLargeMapMode; }
  bool tacticalLargeMapRendered() const;

signals:
  void tacticalLargeMapModeChanged();

protected:
  /**
   * @brief 事件过滤器
   * @details 用于全局拦截特定按键事件（如Tab键），确保在任何焦点状态下都能响应。
   */
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  /**
   * @brief 定时器更新槽函数
   * @details 用于周期性更新UI显示，如倒计时、帧率等。
   */
  void onTimerUpdate();

  /**
   * @brief 比赛开始事件处理
   * @details 处理比赛开始时的逻辑，如切换背景音乐。
   */
  void onGameStart();



  /**
   * @brief 比赛状态更新槽函数
   */
  void onGameStateUpdated();

  /**
   * @brief 处理模拟器生成的协议数据
   * @param type 数据包类型
   * @param data 原始数据
   */
  /**
   * @brief 处理模拟器生成的协议数据
   * @param type 数据包类型
   * @param data 原始数据
   */
  void onSimulatorDataReceived(PacketType type, const QByteArray &data);

  /**
   * @brief 处理设置面板的登录请求
   * @param robotType 机器人类型字符串
   */
  void onSettingsLoginRequested(const QString &robotType);
  void onSettingsLogoutRequested();
  void onCommonCommandRequested(int commandType, int param);

  // --- AR 系统槽函数 ---
  void onARInitializationCompleted(bool success, const QString &message);
  void onARSettingsChanged();

  // --- 由 Lambda 拆出的内部私有槽函数 ---
  void onVideoSourceChanged(const QString &url, bool isPlaying); // 视频源切换
  void onSystemMessageReceived(const QString &message); // 系统消息
  /**
   * @brief 显示模式变更槽函数
   * @param mode 模式名称 ("窗口化" / "全屏")
   */
  void onDisplayModeChanged(const QString &mode);
  void onVideoSourceTypeChanged(const QString &vtType);

   /**
   * @brief 准心是否可见变更槽函数
   * @param visible 是否可见
   */
    void onCrosshairVisibilityChanged(bool visible);
    void onHeroVisibilityChanged(bool visible);
  /**
   * @brief 系统音量变更槽函数
   * @param volume 音量值 (0-100)
   */
  void onSystemVolumeChanged(int volume);


  /**
   * @brief 背景音乐音量变更槽函数
   * @param volume 音量值 (0-100)
   */
  void onMusicVolumeChanged(int volume);

  /**
   * @brief 控制灵敏度变更槽函数
   * @param value 灵敏度值 (0-100)
   */
  void onSensitivityChanged(int value);
  /**
   * @brief 帧率设置变更槽函数
   * @param fpsText 设置面板中的帧率文本 (例如 "60 FPS")
   */
  void onFpsChanged(const QString &fpsText);
  /**
   * @brief 设置面板性能体系选择变更槽函数
   * @param shooter 发射机构性能体系，协议值 0-4（0 表示初始设置）
   * @param chassis 底盘性能体系，协议值 0-4（0 表示初始设置）
   * @param sentryControl 哨兵控制方式，协议值 0-1
   */
  void onPerformanceSelectionChanged(int shooter, int chassis,
                                     int sentryControl);
  void onRunePanelActivateRequested();
  void onOfficialEventPopupRequested(int eventId, const QString &message);
  void setOfficialEventPopupDuration(int durationMs);
  void showOfficialEventPopup(const QString &eventTitle,
                              const QString &eventMessage,
                              const QString &borderColor,
                              int durationMs);

  // 比赛结果接收槽
  void onGameResultReceived(quint8 winner);

  Q_INVOKABLE void handleMiniMapClick(qreal x, qreal y);

  // --- MQTT 异步连接状态机 ---
  /**
   * @brief MQTT 启动阶段连接完成回调
   * @param success 连接是否成功
   * @param error 错误描述
   */
  void onMqttBootstrapCompleted(bool success, const QString &error);

  /**
   * @brief MQTT 切换阶段连接完成回调
   * @param success 连接是否成功
   * @param error 错误描述
   */
  void onMqttSwitchCompleted(bool success, const QString &error);

private:
  // 初始化方法 (按调用顺序排列)
  void initializeCore(); // 初始化核心模块 (GameData, NetworkManager, Simulator)
  void setupConnections(); // 集中绑定所有信号-槽连接
  void startServices();    // 启动网络监听、模拟器等服务
  void finishServicesStartup(bool mqttModeEnabled); // 完成网络/视频/模拟器启动
  void setupDevHooks();    // 开发态 hooks 初始化
  void scheduleMqttReconnect(const QString &error);
  void cancelScheduledMqttReconnect();
  void triggerScheduledMqttReconnect();

  // --- 界面创建方法 ---
  void createTopArea();    // 创建顶部信息区域（胜利点、经济、比分）
  void createScoreArea();  // 创建中央计分板区域
  void createSideAreas();  // 创建左右机器人监控区域
  void createCenterArea(); // 创建中心战场区域

  // --- 面板管理方法 ---
  void setupKeyPanels();          // 初始化快捷键面板
  void showPanel(QWidget *panel);         // 显示指定面板
  void hideAllPanels();                   // 隐藏所有面板
  void hideAllPanelsExcept(QWidget *keep); // 隐藏除 keep 外的所有面板
  void updateSiloPanelVisibility(); // 统一同步飞镖面板的角色/阶段/模式可见性

  // --- 音频管理方法 ---
  void setupAudio();                                 // 初始化音频系统
  void playBackgroundMusic(const QString &filePath); // 播放背景音乐
  void playSecondarySound(const QString &filePath,
                          int loops = -1); // 播放并列音效（不影响BGM）
  void playSoundFromResourceFolder(const QString &fileName); // 从 resources/sounds 播放提示音，空则不播放
  void playRuneVoicePrompt(int runeType, int remainingChances);
  void updateDartCanOpenSoundOnOutpostHealthChange(TeamColor team);
  void updateOutpostReviveReminder(bool isRedOutpost, int status);
  void playOutpostReviveReminder();
  void startNextQueuedSecondarySound();

  // --- 辅助方法 ---
  void updateUI();                // 更新所有UI组件 (已弃用，保留兼容)
  void updateCurrentRobotPanel(); // 更新当前选中机器人的详细面板
  bool shouldShowDeployModePanel() const; // 仅英雄且非 H 面板打开时显示部署模式
  void updateExchangePanelGeometry(); // 更新 H 兑换面板的位置与尺寸
  int exchangePanelVisualTop() const; // 获取兑换面板可视顶部（用于提示文字定位）
  void updateDeployModePanelGeometry(); // 更新 K/L 部署提示面板位置
  void updateOfficialEventPopupGeometry(); // 更新官方事件弹窗位置
  void updateTacticalTimedEventPopupGeometry(); // 更新战术定时事件弹窗位置
  void updateTacticalTimedEventPopupState(); // 同步战术定时事件弹窗显示状态
  void ensureTacticalTimedEventPopupRaisedIfActive(); // 战术事件弹窗置顶
  void scheduleOverlayLayerRestack(); // 统一延迟刷新战术页/阶段弹窗/事件弹窗层级
  void restackOverlayLayers(); // 固定比赛阶段弹窗、战术全屏地图与事件弹窗顺序
  bool shouldRaiseTacticalPageAboveEventPopups() const; // 战术全屏地图是否应压过事件弹窗
  void showTacticalTimedEventPopupForTest(); // 手动测试战术定时事件弹窗
  void ensureInteractivePanelsRaisedIfVisible(); // 交互浮层保持在战术页之上
  QWidget *topInteractivePanel() const; // 当前最应该保持可交互的浮层
  bool shouldCaptureSettingsPanelShortcut(QKeyEvent *event) const;
  bool tryHandleSettingsPanelShortcut(QKeyEvent *event);
  bool tryHandleEngineerExitShortcut(); // 统一处理 engineer 的 L 退出逻辑
  bool tryHandleEngineerConfirmShortcut(QKeyEvent *event);
  void refreshHeroVideoStream();
  void checkDartHitViewSwitch(); // 检查飞镖命中视图切回时机（基于协议时间）


  bool tryHandleAerialAirSupportInterruptShortcut(); // 空中机器人 N 键中断空中支援
  bool tryHandleAerialAirSupportShortcut(int action); // 空中机器人 Ctrl+1/2 空中支援
  void updateRunePanelVisibility(); // 根据比赛阶段与机器人类型同步能量机关面板可见性
  bool tryHandleRuneHoldShortcut(); // 步兵长按 F 显示并触发大能量机关交互
  bool tryHandleSiloOpenShortcut(QKeyEvent *event); // F 上报后同步进入飞镖二次确认
  void endRuneHoldShortcut();       // F 松开后结束大能量机关交互显示
  bool canOpenRemoteExchangePanel() const; // 普通远程兑换面板是否满足协议许可
  void applyStyles();             // 应用全局样式表
  void ensureOverlayRaisedIfActive(); // 如果有激活弹窗则确保 Overlay 置顶
  void ensureGameResultRaisedIfVisible(); // 结算面板可见时保持最上层
  void updateGameResultWidgetGeometry(); // 结算面板与 MainWindow 同层时统一维护居中
  void setupTimer();              // 初始化定时器
  void setupFonts();              // 初始化字体
  void updateExchangeHintText(const QString &robotType);
  void applyExchangeHintVisualState(bool unavailable);
  void updateExchangeHintOverlayGeometry();
  void syncSettingsPanelState();
  void applySettingsRobotSelection(const QString &robotType, quint8 robotId);
  quint8 resolveRobotIdFromSettingsSelection(const QString &robotType) const;
  QString settingsRobotTypeLabel(quint8 robotId) const;
  int baseRobotIdFromSettingsType(const QString &robotType) const;  //获取settingpannel中的机器人基础id（不分红蓝方）
  quint8 robotIdFromSettingsType(const QString &robotType) const;   //获取settingpannel中的机器人id（分红蓝方）
  QString mqttClientIdForRobotType(const QString &robotType) const; //获取协议clientId
  QStringList mqttClientIdCandidatesForRobotType(const QString &robotType) const; // 协议ID + 适配回退ID
  void restartMqttForRobotType(const QString &robotType);           //重启 MQTT 连接并切换 clientId

  /**
   * @brief 获取自适应字体
   * @param baseSize 基础字号
   * @param bold 是否加粗
   * @return 配置好的字体对象
   */
  QFont getAdaptiveFont(int baseSize, bool bold = false);

  /**
   * @brief 获取自适应尺寸
   * @param baseSize 基础尺寸
   * @return 适配屏幕后的尺寸
   */
  int getAdaptiveSize(int baseSize);
  double getPanelScaleFactor() const;        //获取比例
  QSize getScaledPanelSize(int baseWidth, int baseHeight) const;    //获取面板比例缩放后的大小
  void applyPanelResolutionScaling();          //将面板大小应用到客户端中
  QSize popupResolutionViewport() const;
  void updatePopupResolutionScaling();

  void toggleLargeMiniMap();
  void applyTacticalMode(bool enabled);
  void setTacticalLargeMapMode(bool enabled);
  void setTacticalLayoutMode(const QString &mode);
  void toggleTacticalLayoutMode();
  void updateTacticalLoginButtonGeometry();
  void updateHeroVideoWidgetGeometry();
  void setMiniMapCommandMode(Qt::Key key);
  void clearMiniMapCommandMode();
  void syncMiniMapCommandState();
  void updateMiniMapLegendPanel();
  void updateMiniMapLegendSelection();
  int currentMiniMapTargetRobotId() const;
  bool canBroadcastMiniMapCommand() const;

  // 结算面板可见期间刷新统计数据
  void refreshGameResultWidgetStats();

  // --- 事件处理 ---
  bool event(QEvent *event) override; // 拦截 Tab 键（焦点导航）
  void keyPressEvent(QKeyEvent *event) override;
  void keyReleaseEvent(QKeyEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

  void setupKeyboardMouseControlTimer();
  bool canUseKeyboardMouseControl() const;
  bool isKeyboardMouseControlBlocked() const;
  bool isInputEventFromClientWindow(QObject *obj) const;
  int keyboardMouseBitForKey(int key) const;
  bool tryHandleRespawnShortcut(QKeyEvent *event);
  bool handleKeyboardMouseControlKey(QKeyEvent *event, bool pressed);
  void handleKeyboardMouseButton(Qt::MouseButton button, bool pressed);
  void handleKeyboardMouseMove(const QPoint &globalPos);
  void handleKeyboardMouseWheel(int angleDeltaY);
  void scheduleKeyboardMouseControlFlush();
  void flushKeyboardMouseControlFrame();
  void resetKeyboardMouseControlState(bool sendNeutral);

  // 前哨站状态转发给机器人 (CustomControl 0x0311)
  void setupOutpostForwardTimer();
  void scheduleOutpostForward();
  void flushOutpostForward();

  // --- 成员变量 (C++11 类内初始化) ---

  // --- 基础容器 ---
  QWidget *m_centralWidget = nullptr;  // 主窗口中心部件
  QVBoxLayout *m_mainLayout = nullptr; // 主布局管理器

  // --- 顶部区域 ---
  QWidget *m_topArea = nullptr;       // 顶部容器部件
  QVBoxLayout *m_topLayout = nullptr; // 顶部布局
  QQuickWidget *m_qmlTopInfoBar =
      nullptr; // QML实现的顶部信息条 (TopInfoBar.qml)

  // --- 侧边栏区域 ---
  QWidget *m_leftSideArea = nullptr;              // 左侧机器人状态区
  QWidget *m_rightSideArea = nullptr;             // 右侧机器人状态区
  QVBoxLayout *m_leftSideLayout = nullptr;        // 左侧布局
  QVBoxLayout *m_rightSideLayout = nullptr;       // 右侧布局
  QVector<class RobotStatusPanel *> m_redRobots;  // 红方机器人面板列表
  QVector<class RobotStatusPanel *> m_blueRobots; // 蓝方机器人面板列表

  // --- 中心区域 ---
  QWidget *m_centerArea = nullptr;              // 中心战场显示区
  class CrosshairWidget *m_crosshair = nullptr; // 准星组件
  class HeatRingWidget *m_heatRing = nullptr;   // 热量环组件

  // --- 视频与地图 ---
  class VideoBackgroundWidget *m_videoBackground = nullptr; // 视频背景组件
  class BattleMessageWidget *m_battleMessage = nullptr;     // 战场消息提示
  // class KillFeedWidget *m_killFeed = nullptr;               // 击杀播报列表 (右侧)
  QWidget *m_aimingContainer = nullptr;     // 准星容器 (绝对定位)

  // --- QML 小地图 ---
  QQuickWidget *m_miniMap = nullptr; // 小地图组件 (QML)
  QQuickWidget *m_miniMapLarge = nullptr; // 小地图放大组件 (QML)
  HeroVideoWidget *m_heroVideoWidget = nullptr; // 英雄吊射画面显示
  QList<QQuickWidget*> m_miniMaps; // 统一管理所有地图实例 (QML)
  QWidget *m_miniMapLegendPanel = nullptr;
  QWidget *m_miniMapLegendAttackRow = nullptr;
  QWidget *m_miniMapLegendWarningRow = nullptr;
  QWidget *m_miniMapLegendDefenseRow = nullptr;
  qint64 m_miniMapMarkCooldownUntilMs = 0;
  QWidget *m_rightBottomContainer = nullptr; // 右下角小地图容器


  // --- 底部面板 ---
  QQuickWidget *m_leftBottomPanel = nullptr; // 左下角面板 (QML)

  // --- 浮动信息 ---
  QLabel *m_fpsLabel = nullptr;     // FPS帧率标签
  QLabel *m_pingLabel = nullptr;    // Ping延迟标签
  QLabel *m_networkLabel = nullptr; // 网络状态标签
  QLabel *m_serverLabel = nullptr;  // 服务器连接状态标签
  QLabel *m_exchangeHint = nullptr;
  QLabel *m_hKeyHint = nullptr;     // 底部“H”按键图标标签
  QLabel *m_exchangeHintBox = nullptr; // 底部提示外框

  // --- 控制组件 ---
  QPushButton *m_quitBtn = nullptr;     // 退出按钮
  QProgressBar *m_triggerBar = nullptr; // 扳机/射击力度条
  QProgressBar *m_controlBar = nullptr; // 控制/输入反馈条

  // --- C++ Widget 面板 (按键触发) ---
  class InfoPanel *m_infoPanel = nullptr;             // 信息面板
  class OperationsPanel *m_operationsPanel = nullptr; // 操作指令面板
  class HelpOverlayWidget *m_helpOverlay = nullptr;   // F12 帮助浮层
  class DebugLogWidget *m_debugLogWidget = nullptr;   // 调试日志窗口
  bool m_helpOverlayHotkeyActive = false;             // F12 是否正在按住

  // --- QML 面板 (按键触发) ---
  QQuickWidget *m_qmlTabPanel = nullptr;       // [Tab] 机器人数据统计面板
  QQuickWidget *m_qmlDamagePanel = nullptr;    // [~] 伤害数据统计面板
  QQuickWidget *m_qmlSettingsPanel = nullptr;  // 设置面板
  QPointer<QObject> m_settingsPanelRoot;        // 安全缓存 rootObject
  bool m_settingsPanelVisible = false;          // 面板显隐状态
  bool m_handlingTabKey = false;                // 防止 Tab 面板 show/hide 递归触发
  QQuickWidget *m_qmlExchangePanel = nullptr;  // [H] 兑换商店面板
  QQuickWidget *m_qmlAmmoSupply17Panel = nullptr;  // [O] 买17mm弹面板
  QQuickWidget *m_qmlAmmoSupply42Panel = nullptr;  // [I] 买42mm弹面板
  QQuickWidget *m_qmlDeployModePanel = nullptr; // K/L 部署提示面板
  QQuickWidget *m_qmlSiloPanel = nullptr; // 飞镖操控面板 (QML)
  QQuickWidget *m_qmlGunnerOverlay = nullptr; // 云台手副屏自定义数据叠加层 (QML)
  QQuickWidget *m_qmlTacticalPage = nullptr;   // 战术指挥屏 (QML)
  QPushButton *m_tacticalLoginButton = nullptr; // 战术屏登录按钮
  QVector<QQuickWidget *> m_tacticalTimedEventPopups; // 战术定时事件提示弹窗池 (QML)
  bool m_tacticalMode = false;                  // 是否处于指挥屏模式
  bool m_tacticalLargeMapMode = false;          // 战术屏全屏地图子状态
  QQuickWidget *m_qmlRunePanel = nullptr; // 大能量机关交互面板 (QML)
  QString m_exchangeQmlSource;
  QString m_selectedRobotTypeFromSettings =
      QStringLiteral("R3 - Standard"); // SettingsPanel 最近一次选择的机器人类型

  bool m_exchangeHintLoginActive = false; // 仅登录后显示 H 提示区
  bool m_exchangeHintVisualInitialized = false; // H 提示视觉状态缓存是否已初始化
  bool m_exchangeHintVisualUnavailable = false; // 缓存上次不可用状态，避免重复刷样式
  int m_exchangeHintVisualScaleKey = -1; // 缓存上次缩放键，避免重复刷样式

  // --- MQTT 启动状态 ---
  QStringList m_mqttCandidates;      // MQTT clientId 候选列表
  int m_mqttCandidateIndex = 0;      // 当前 MQTT clientId 候选索引
  QString m_mqttUri;                 // MQTT Broker URI
  QString m_lastMqttConnectError;     // 最近一次 MQTT 连接错误，用于最终失败日志
  bool m_mqttIsBootstrapMode = false; // 是否处于首次启动引导模式
  bool m_servicesStarted = false;     // 初始网络/视频/模拟器服务是否已完成启动

  QString m_tryingMqttClientId;
  QString m_activeMqttClientId;       // 当前 MQTT clientId
  // 统一弹窗容器 (PopupOverlay.qml)
  // 说明：已收敛为单一路径，移除旧的分散实例以避免双轨实现带来的层级竞争。
  QQuickWidget *m_qmlPopupOverlay = nullptr;
  QQuickWidget *m_qmlAuxiliaryShootingPanel =
      nullptr; // 辅助射击面板 (准星周边) - 备用
  QQuickWidget *m_qmlCentralAimingHUD =
      nullptr; // 中央瞄准 HUD (准星 + 热量环 + 射击信息)
  QQuickWidget *m_qmlEventMessagePanel = nullptr; // 事件消息提示面板
  QQuickWidget *m_qmlOfficialEventPopupPanel = nullptr; // 空中机器人官方事件弹窗 (QML)
  int m_officialEventPopupDurationMs = 5000; // 官方事件弹窗默认显示时长 (毫秒)
  QVector<QString> m_tacticalTimedEventRuleKeys; // 每个弹窗当前显示的规则键
  QVector<int> m_tacticalTimedEventCountdowns;   // 每个弹窗当前显示的倒计时
  bool m_tacticalTimedEventManualOverride = false; // 战术定时事件手动测试覆盖状态
  qint64 m_tacticalTimedEventManualOverrideUntilMs = 0; // 战术定时事件手动覆盖结束时间 (毫秒)

  // --- 侧边浮动组件 ---
  class RobotStatusWidget *m_detailedRobotStatus =
      nullptr; // 选中机器人的详细状态悬浮窗
  GameResultWidget *m_gameResultWidget = nullptr; // 比赛结算界面

  // 结算面板可见期间使用的临时连接
  QList<QMetaObject::Connection> m_gameResultUpdateConns;

  // --- 系统核心对象 ---
  QTimer *m_updateTimer = nullptr;            // 主循环刷新定时器
  QTimer *m_videoConnectionTimer = nullptr;   // 图传连接超时检测定时器
  QTimer *m_popupRaiseTimer = nullptr;        // 弹窗抬升防抖定时器
  QTimer *m_mqttReconnectTimer = nullptr;     // MQTT 首次连接失败后的后台重试定时器
  GameData *m_gameData = nullptr;             // 比赛数据模型单例
  NetworkManager *m_networkManager = nullptr; // 网络通信管理器
  RM::TacticalAnalyzer *m_tacticalAnalyzer = nullptr; // 战术分析引擎
  MqttClient *m_mqttClient = nullptr;          // MQTT 客户端（可选，取决于是否找到 Qt6::Mqtt）
  QMediaPlayer *m_bgmPlayer = nullptr;        // 背景音乐播放器
  QAudioOutput *m_audioOutput = nullptr;      // 音频输出控制器
  QMediaPlayer *m_secondaryPlayer = nullptr;   // 并列音效播放器
  QAudioOutput *m_secondaryOutput = nullptr;   // 并列音频输出
  struct SecondarySoundRequest {
    QString filePath;
    int loops = 1;
  };
  QQueue<SecondarySoundRequest> m_secondarySoundQueue;
  bool m_secondarySoundPlaying = false;
  QTimer *m_outpostReviveReminderTimer = nullptr;
  int m_lastEnemyOutpostHealthForDartSound = -1;
  bool m_dartCanOpenDropSoundPlayed = false;
  bool m_dartCanOpenLateSoundPlayed = false;

  // 飞镖命中自动切换图传界面（基于协议时间计时，不使用本地定时器）
  bool m_dartHitViewPending = false;     // 是否等待切回战术指挥屏
  int m_dartHitRemainingAtHit = -1;      // 命中时比赛剩余时间（来自 protocol GameStatus.stage_countdown_sec）
  int m_dartHitTargetId = 0;             // 命中目标 ID，用于查询遮挡时长
  int m_dartHitOcclusionDurationSec = 0; // 本次命中的遮挡时长（按规则和命中序号计算）
  int m_dartHitDisplayToken = 0;         // 面板显示令牌 (用于去重)
  bool m_dartHitHeroDeployCamActive = false;  // 英雄部署模式下飞镖命中全屏显示工业相机
  bool m_secondaryPlayedAtFive = false;        // BATTLE 阶段剩余5s播放标记

  // --- 比赛状态数据 ---
  int m_gameTime = 0;                               // 当前比赛剩余时间 (秒)
  int m_redScore = 0;                               // 红方得分
  int m_blueScore = 0;                              // 蓝方得分
  int m_currentRound = 1;                           // 当前回合数
  QString m_gamePhase = QStringLiteral("准备阶段"); // 当前比赛阶段描述
  GameStage m_lastGameStage = GameStage::NOT_STARTED; // 上一次比赛阶段
  int m_screenWidth = 0;                            // 屏幕宽度
  int m_screenHeight = 0;                           // 屏幕高度


  // --- AR 与增强现实 ---
  AROverlayManager *m_arManager = nullptr; // AR覆盖层管理器
  AROverlayWidget *m_arWidget = nullptr;   // AR渲染窗口

  // --- 调试与模拟 ---
  class ProtocolSimulator *m_simulator = nullptr; // 协议数据模拟器

  // --- 可选开发 Hook 层 ---
  ClientDevHooks *m_devHooks = nullptr; // 可选开发 Hook（由 RM26_ENABLE_DEVTOOLS + RM_DEVTOOLS 控制）
  S1ProtocolManager *m_s1ProtocolManager = nullptr; // S1 协议管理器（可选，由 RM_S1_ENGINE_HOST 环境变量控制）

  // 控制参数
  int m_controlSensitivity = 50; // 控制灵敏度 (0-100)
  QPoint m_lastMousePos;
  bool m_hasLastMousePos = false;
  QTimer *m_keyboardMouseControlTimer = nullptr;
  QTimer *m_outpostForwardTimer = nullptr;
  quint32 m_keyboardMouseMask = 0;
  int m_pendingMouseX = 0;
  int m_pendingMouseY = 0;
  int m_pendingMouseZ = 0;
  bool m_keyboardMouseLeftDown = false;
  bool m_keyboardMouseRightDown = false;
  bool m_keyboardMouseMidDown = false;
  bool m_keyboardMouseHadActiveState = false;
  bool m_keyboardMouseInternalMove = false;

  int m_pendingMiniMapMarkType = -1;
  QString m_pendingMiniMapCommandLabel;
};

} // namespace RM

#endif // MAINWINDOW_H
