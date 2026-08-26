// SPDX-License-Identifier: MIT
/**
 * @file MainWindow.cpp
 * @brief RoboMaster 2026 主窗口实现文件
 * @author Clear
 * @date 2025-11-16
 * @copyright Copyright (c) 2025 Fudan University EGA Team (复旦大学星云 EGA 机器人战队).
 *
 * 主窗口界面
 * 包含顶部HUD区域（基地血条、轮次时间、比分经济）、中心信息区域和底部状态栏
 * 采用2行布局设计，符合官方界面规范
 */

// --- Qt 头文件 ---
#include <algorithm>
#include <QApplication>
#include <QAudioOutput>
#include <QColor>
#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QNetworkProxy>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickWidget>
#include <QRegularExpression>
#include <QScreen>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>
#include <QWheelEvent>
#include <cmath>

// --- 项目头文件 ---
#include "../config/ConfigManager.h"
#include "../core/GameData.h"
#include "../core/GameConstants.h"
#include "../core/TimedEventRules.h"
#include "../core/TacticalAnalyzer.h"
#include "../core/DataFreshnessGuard.h"
#include "../core/ExecutionFusion.h"
#include "../core/MapCoordinateMapper.h"
#include "../core/ThreatRanker.h"
#include "../devhooks/ClientDevHooks.h"
#include "../network/MqttClient.h"
#include "../simulator/ProtocolSimulator.h"
#include "../network/S1ProtocolManager.h"
#include "../widgets/BattleMessageWidget.h"
// #include "../widgets/KillFeedWidget.h"
#include "../widgets/CrosshairWidget.h"
#include "../widgets/DebugLogWidget.h"
#include "../widgets/HealthBarWidget.h"
#include "../widgets/HeatRingWidget.h"
#include "../widgets/HeroVideoWidget.h"
#include "../widgets/HelpOverlayWidget.h"
#include "../widgets/InfoPanel.h"
#include "../widgets/MiniMapWidget.h"
#include "../widgets/OperationsPanel.h"
#include "../widgets/RobotStatusPanel.h"
#include "../widgets/RobotStatusWidget.h"
#include "InputHotkeyPolicy.h"
#include "MainWindowStatePolicy.h"
#include "ExchangeCommandPolicy.h"
#include "PopupOverlayPolicy.h"
#ifdef RM_HAS_AR_OVERLAY
#include "../widgets/AROverlayManager.h"
#include "../widgets/AROverlayWidget.h"
#endif
#include "MainWindow.h"
#include <QFontDatabase>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickView>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QVariantMap>

#include "robomaster.pb.h"

// RuneData 由 GameData.h 提供，已通过 MainWindow.h 间接包含。

namespace RM {

namespace {

constexpr int kMqttReconnectIntervalMs = 5000;

bool keyboardMouseTraceEnabled() {
  return qEnvironmentVariableIsSet("RM_KEYBOARD_MOUSE_TRACE");
}

bool shouldLogKeyboardMouseMoveTrace() {
  if (!keyboardMouseTraceEnabled()) {
    return false;
  }
  static qint64 lastTraceMs = 0;
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  if (lastTraceMs > 0 && nowMs - lastTraceMs < 100) {
    return false;
  }
  lastTraceMs = nowMs;
  return true;
}

template <typename WidgetT>
void setVisibleIfNeeded(WidgetT *widget, bool visible) {
  if (!widget) {
    return;
  }
  if (widget->isVisible() == visible) {
    return;
  }
  visible ? widget->show() : widget->hide();
}

int normalizedRobotIdFromSelection(const QString &robotType) {
  static const QRegularExpression kRobotPattern(QStringLiteral("[RB](\\d+)"),
                                                QRegularExpression::CaseInsensitiveOption);
  const QRegularExpressionMatch match = kRobotPattern.match(robotType);
  if (!match.hasMatch()) {
    return 0;
  }
  bool ok = false;
  const int robotId = match.captured(1).toInt(&ok);
  if (!ok || robotId <= 0) {
    return 0;
  }
  return robotId;
}

QString settingsRobotTypeLabelForNormalizedId(int robotId) {
  switch (robotId) {
  case 1:
    return QStringLiteral("R1 - Hero");
  case 2:
    return QStringLiteral("R2 - Engineer");
  case 3:
    return QStringLiteral("R3 - Standard");
  case 4:
    return QStringLiteral("R4 - Standard");
  case 5:
    return QStringLiteral("R5 - Standard");
  case 6:
    return QStringLiteral("R6 - Aerial");
  case 7:
    return QStringLiteral("R7 - Sentry");
  default:
    return QStringLiteral("R1 - Hero");
  }
}

bool isInfantryRobotId(int robotId) {
  return robotId == 3 || robotId == 4 || robotId == 5 || robotId == 103 ||
         robotId == 104 || robotId == 105;
}

// 将协议数字转化为底盘类型文本
QString chassisSelectionTextForValue(quint8 selection) {
  switch (selection) {
  case 1:
    return QStringLiteral("血量优先");
  case 2:
    return QStringLiteral("功率优先");
  case 3:
    return QStringLiteral("英雄近战优先");
  case 4:
    return QStringLiteral("英雄远程优先");
  default:
    return QStringLiteral("初始设置");
  }
}

QString shooterSelectionTextForValue(quint8 selection) {
  switch (selection) {
  case 1:
    return QStringLiteral("冷却优先");
  case 2:
    return QStringLiteral("爆发优先");
  case 3:
    return QStringLiteral("英雄近战优先");
  case 4:
    return QStringLiteral("英雄远程优先");
  default:
    return QStringLiteral("初始设置");
  }
}

int clampKeyboardMouseAxis(qint64 value) {
  return static_cast<int>(std::clamp<qint64>(value, -32767, 32767));
}

bool isTacticalLayoutSwitchShortcut(const QKeyEvent *event) {
  if (!event || event->isAutoRepeat()) {
    return false;
  }
  const Qt::KeyboardModifiers expectedModifier = Qt::ControlModifier;
  return event->key() == Qt::Key_Y && event->modifiers() == expectedModifier;
}
} // namespace

static RobotType toUiRobotType(::RobotType t) {
  switch (t) {
  case ::RobotType::HERO:
    return RobotType::Hero;
  case ::RobotType::ENGINEER:
    return RobotType::Engineer;
  case ::RobotType::AERIAL:
    return RobotType::Aerial;
  case ::RobotType::SENTRY:
    return RobotType::Sentry;
  case ::RobotType::DART:
    return RobotType::Dart;
  case ::RobotType::RADAR:
    return RobotType::Radar;
  case ::RobotType::INFANTRY_3:
  case ::RobotType::INFANTRY_4:
  case ::RobotType::INFANTRY_5:
  default:
    return RobotType::Standard;
  }
}

// --- 辅助函数：设置 QQuickWidget 透明背景 (macOS 兼容) ---
static void makeQQuickWidgetTransparent(QQuickWidget *widget) {
  if (!widget)
    return;

  // 1. 设置 Qt 窗口属性
  widget->setAttribute(Qt::WA_TranslucentBackground);
  widget->setAttribute(Qt::WA_NoSystemBackground);
  widget->setAttribute(Qt::WA_OpaquePaintEvent, false);

  // 2. 设置 QQuickWidget 清除颜色
  widget->setClearColor(Qt::transparent);

  // 3. 禁用自动填充背景
  widget->setAutoFillBackground(false);

  // 4. 设置透明调色板
  QPalette pal = widget->palette();
  pal.setColor(QPalette::Window, Qt::transparent);
  pal.setColor(QPalette::Base, Qt::transparent);
  widget->setPalette(pal);

  // 5. 设置样式表
  widget->setStyleSheet("background: transparent; border: none;");

  // 6. [关键] 设置底层 QQuickWindow 的背景色为透明
  // 这是 macOS 上 QQuickWidget 透明度的关键修复
  if (QQuickWindow *qw = widget->quickWindow()) {
    qw->setColor(Qt::transparent);
  }
}

static void makeQQuickWidgetOpaque(QQuickWidget *widget,
                                   const QColor &backgroundColor) {
  if (!widget)
    return;

  widget->setAttribute(Qt::WA_TranslucentBackground, false);
  widget->setAttribute(Qt::WA_NoSystemBackground, false);
  widget->setAttribute(Qt::WA_OpaquePaintEvent, true);
  widget->setAutoFillBackground(true);
  widget->setClearColor(backgroundColor);

  QPalette pal = widget->palette();
  pal.setColor(QPalette::Window, backgroundColor);
  pal.setColor(QPalette::Base, backgroundColor);
  widget->setPalette(pal);
  widget->setStyleSheet(
      QStringLiteral("background: rgb(%1, %2, %3); border: none;")
          .arg(backgroundColor.red())
          .arg(backgroundColor.green())
          .arg(backgroundColor.blue()));

  if (QQuickWindow *qw = widget->quickWindow()) {
    qw->setColor(backgroundColor);
  }
}

class HeroVideoImageProvider final : public QQuickImageProvider {
public:
  explicit HeroVideoImageProvider(GameData *gameData)
      : QQuickImageProvider(QQuickImageProvider::Image), m_gameData(gameData) {}

  QImage requestImage(const QString &id, QSize *size,
                      const QSize &requestedSize) override {
    Q_UNUSED(id);
    if (!m_gameData) {
      if (size) {
        *size = QSize();
      }
      return QImage();
    }

    QImage frame = m_gameData->heroFrame();
    if (size) {
      *size = frame.size();
    }
    if (!frame.isNull() && requestedSize.isValid()) {
      frame = frame.scaled(requestedSize, Qt::KeepAspectRatio,
                           Qt::SmoothTransformation);
    }
    return frame;
  }

private:
  QPointer<GameData> m_gameData;
};

static void setQmlRootScale(QQuickWidget *widget, qreal scale) {
  if (!widget) {
    return;
  }
  auto *rootItem = qobject_cast<QQuickItem *>(widget->rootObject());
  if (!rootItem) {
    return;
  }
  rootItem->setTransformOrigin(QQuickItem::TopLeft);
  rootItem->setScale(scale);
}

// --- QML 小地图桥接：将 C++ 调用转发到 MiniMap.qml 的 JS 函数 ---
static QObject *miniMapRootObject(QQuickWidget *map) {
  if (!map) {
    return nullptr;
  }
  return map->rootObject();
}


static void miniMapSetInteractionEnabled(QQuickWidget *map, bool enabled) {
  QObject *root = miniMapRootObject(map);
  if (!root) {
    return;
  }
  root->setProperty("clickEnabled", enabled);
}

static void miniMapSetPendingMarkMode(QQuickWidget *map, int markType,
                                      const QString &label) {
  QObject *root = miniMapRootObject(map);
  if (!root) {
    return;
  }
  QMetaObject::invokeMethod(root, "setPendingMarkMode",
                            Q_ARG(QVariant, QVariant(markType)),
                            Q_ARG(QVariant, QVariant(label)));
}

static void miniMapClearPendingMarkMode(QQuickWidget *map) {
  QObject *root = miniMapRootObject(map);
  if (!root) {
    return;
  }
  QMetaObject::invokeMethod(root, "clearPendingMarkMode");
}

static void miniMapAddCommandMarker(QQuickWidget *map, qreal x, qreal y,
                                    int markType, const QString &label) {
  QObject *root = miniMapRootObject(map);
  if (!root) {
    return;
  }
  QMetaObject::invokeMethod(root, "addCommandMarker",
                            Q_ARG(QVariant, QVariant(x)),
                            Q_ARG(QVariant, QVariant(y)),
                            Q_ARG(QVariant, QVariant(markType)),
                            Q_ARG(QVariant, QVariant(label)));
}

// 下方 QQuickWidget 显隐辅助函数的前向声明。
static void safeQuickHide(QWidget *w);
static void safeQuickShow(QWidget *w);

static QObject *tacticalTimedEventPopupRootObject(QQuickWidget *popup) {
  if (!popup) {
    return nullptr;
  }
  return popup->rootObject();
}

static bool isMiniMapCommandKey(int key) {
  return key == Qt::Key_A || key == Qt::Key_B || key == Qt::Key_I;
}

//判断当前按键事件是否是~键（伤害面板）
static bool isOfficialEventPopupTestHotkey(const QKeyEvent *event) {
  if (!event) {
    return false;
  }

  // 事件提示弹窗临时调试键
  return event->key() == Qt::Key_F9 &&
         event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier);
}

static bool isTacticalTimedEventPopupTestHotkey(const QKeyEvent *event) {
  if (!event) {
    return false;
  }

  // 战术定时事件弹窗临时调试键
  return event->key() == Qt::Key_F8 &&
         event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier);
}

static int miniMapMarkTypeForKey(Qt::Key key) {
  switch (key) {
  case Qt::Key_A:
    return 1;
  case Qt::Key_B:
    return 3;
  case Qt::Key_I:
    return 2;
  default:
    return -1;
  }
}

static QString miniMapCommandLabelForKey(Qt::Key key) {
  switch (key) {
  case Qt::Key_A:
    return QStringLiteral("攻击");
  case Qt::Key_B:
    return QStringLiteral("警戒");
  case Qt::Key_I:
    return QStringLiteral("防御");
  default:
    return QString();
  }
}

static QString event8CounteredSoundFile(const QString &rawSelector) {
  const QString selector = rawSelector.trimmed().toLower();
  bool ok = false;
  const int remainingCounterCount = selector.toInt(&ok);

  // 协议约定：event=8 的 param 为“己方剩余可反制次数”。
  if (ok) {
    switch (remainingCounterCount) {
    case 4:
      return QStringLiteral("resources/sounds/反制无人机第一次.mp3");
    case 3:
      return QStringLiteral("resources/sounds/反制无人机第二次.mp3");
    case 2:
      return QStringLiteral("resources/sounds/反制无人机第三次.mp3");
    default:
      break;
    }
  }



  // 兜底仍给一个占位音效，避免参数异常时完全静音。
  return QStringLiteral("resources/sounds/反制无人机第一次.mp3");
}

QString MainWindow::allyBaseArmorOpenedSoundFileName() {
  return QStringLiteral("我方基地护甲展开.mp3");
}

int MainWindow::allyBaseArmorOpenedSoundLoopCount() {
  return 3;
}

QString MainWindow::paidRespawnSoundFileName(int robotId) {
  switch (robotId % 100) {
  case 1:
    return QStringLiteral("enemy_buyback_hero_revived.mp3");
  case 2:
    return QStringLiteral("enemy_buyback_engineer_revived.mp3");
  case 3:
    return QStringLiteral("enemy_buyback_infantry_3_revived.mp3");
  case 4:
    return QStringLiteral("enemy_buyback_infantry_4_revived.mp3");
  case 6:
    return QStringLiteral("enemy_buyback_aerial_revived.mp3");
  case 7:
    return QStringLiteral("enemy_buyback_sentry_revived.mp3");
  default:
    return QString();
  }
}

static void connectIfQmlSignalExists(QObject *sender,
                                     const char *signalSignature,
                                     QObject *receiver,
                                     const char *slotSignature) {
  if (!sender || !receiver) {
    return;
  }

  if (sender->metaObject()->indexOfSignal(signalSignature) < 0) {
    return;
  }

  const QByteArray signalSpec = "2" + QByteArray(signalSignature);
  const QByteArray slotSpec = "1" + QByteArray(slotSignature);
  QObject::connect(sender, signalSpec.constData(), receiver,
                   slotSpec.constData(), Qt::UniqueConnection);
}

static void connectMiniMapClickSignal(QObject *root, QObject *receiver,
                                      const char *slotSignature) {
  connectIfQmlSignalExists(root, "mapClicked(qreal,qreal)", receiver,
                           slotSignature);
  connectIfQmlSignalExists(root, "mapClicked(double,double)", receiver,
                           slotSignature);
}

// --- 辅助函数：使用 QQuickView 创建透明 QML 容器 (macOS 更可靠) ---
static QWidget *createTransparentQmlContainer(const QUrl &source,
                                              QQmlContext *context,
                                              QWidget *parent,
                                              QQuickView **outView = nullptr) {

  // 创建 QQuickView
  QQuickView *view = new QQuickView();

  // 设置透明背景 (关键!)
  view->setColor(Qt::transparent);

  // 设置上下文属性
  if (context) {
    // 复制上下文属性到 view 的 rootContext
    QQmlContext *viewContext = view->rootContext();
    // 注意: 需要手动设置上下文属性
  }

  // 设置调整大小模式
  view->setResizeMode(QQuickView::SizeRootObjectToView);

  // 加载 QML
  view->setSource(source);

  // 创建容器 widget
  QWidget *container = QWidget::createWindowContainer(view, parent);

  // 设置容器透明属性
  container->setAttribute(Qt::WA_TranslucentBackground);
  container->setAttribute(Qt::WA_NoSystemBackground);
  container->setAutoFillBackground(false);

  // 返回 view 指针以便后续使用
  if (outView) {
    *outView = view;
  }

  return container;
}

void ensureAmmoPanel(QQuickWidget *&panel, QWidget *parent, GameData *gameData,
                     const QString &title, const QString &ammoName,
                     int commandType, int batchSize, int batchPrice,
                     int maxCount, QObject *receiver,
                     const char *requestSlot) {
  if (!panel) {
    panel = new QQuickWidget(parent);
    panel->setResizeMode(QQuickWidget::SizeViewToRootObject);
    panel->setAttribute(Qt::WA_TranslucentBackground);
    panel->setClearColor(Qt::transparent);
    panel->rootContext()->setContextProperty("gameData", gameData);
    panel->setSource(QUrl("qrc:/qml/AmmoSupplyPanel.qml"));
    panel->setFixedSize(520, 360);
    if (QObject *root = panel->rootObject()) {
      connectIfQmlSignalExists(root, "commonCommandRequested(int,int)",
                               receiver, requestSlot);
    }
  }

  if (QObject *root = panel->rootObject()) {
    root->setProperty("panelTitle", title);
    root->setProperty("ammoName", ammoName);
    root->setProperty("commandType", commandType);
    root->setProperty("batchSize", batchSize);
    root->setProperty("batchPrice", batchPrice);
    root->setProperty("maxCount", maxCount);
  }
}

// --- 构造函数 - 使用 C++11 类内初始化，简化初始化列表 ---
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  // 1. 核心模块初始化 (必须最先执行，创建 GameData)
  initializeCore();
  setupKeyboardMouseControlTimer();
  setupOutpostForwardTimer();

  // 2. UI初始化 (依赖 GameData)
  setupFonts();  // 字体优先
  applyStyles(); // 样式优先
  initializeLayout();
  setupTimer();

  // 3. 信号-槽集中绑定
  setupConnections();
  // 先完成 TacticalCommandPage 构造，再启用 Bound Loader。这样既保留 M 键
  // 的原有加载路径，也只让首次进入应用时默认显示大地图。
  setTacticalLargeMapMode(true);
  applyTacticalMode(true);

  // 4. 启动服务（延迟到事件循环开始，避免构造函数中阻塞 UI 显示）
  QTimer::singleShot(0, this, [this]() { startServices(); });

  // 5. 安装全局事件过滤器 (用于拦截Tab键等全局快捷键)
  QApplication::instance()->installEventFilter(this);
  RM::InputHotkeyPolicy::enableMouseTrackingForWidgetTree(this);

  QTimer::singleShot(0, this, [this]() {
    if (QScreen *activeScreen = screen()) {
      connect(activeScreen, &QScreen::geometryChanged, this,
              [this](const QRect &geometry) {
                if (isFullScreen()) {
                  setGeometry(geometry);
                }
                updatePopupResolutionScaling();
              });
    }
    updatePopupResolutionScaling();
  });
}

MainWindow::~MainWindow() { resetKeyboardMouseControlState(true); }

void MainWindow::toggleSettingsPanel() {
  qInfo() << "[tactical-login] toggleSettingsPanel"
          << "settingsVisible="
          << (m_qmlSettingsPanel && m_qmlSettingsPanel->isVisible())
          << "status="
          << (m_qmlSettingsPanel ? static_cast<int>(m_qmlSettingsPanel->status())
                                 : -1)
          << "hasRoot="
          << (m_qmlSettingsPanel && m_qmlSettingsPanel->rootObject())
          << "focusWidget=" << QApplication::focusWidget();
  if (!m_qmlSettingsPanel) {
    qWarning() << "[tactical-login] settings panel is null";
    return;
  }

  const bool settingsVisible = m_qmlSettingsPanel->isVisible();
  if (settingsVisible) {
    safeQuickHide(m_qmlSettingsPanel);
    updateSiloPanelVisibility();
    setFocus();
    m_settingsPanelVisible = false;
    qDebug() << "Settings panel hidden";
    return;
  }

  showPanel(m_qmlSettingsPanel);
  m_settingsPanelVisible = true;
  ensureInteractivePanelsRaisedIfVisible();
  qDebug() << "Settings panel shown";
}

// --- 核心模块初始化 ---
void MainWindow::initializeCore() {
  // 初始化比赛数据中心
  m_gameData = new GameData(this);

  // 初始化网络管理器
  m_networkManager = new NetworkManager(m_gameData, this);

#ifdef RM_HAS_MQTT
  m_mqttReconnectTimer = new QTimer(this);
  m_mqttReconnectTimer->setSingleShot(true);
  m_mqttReconnectTimer->setInterval(kMqttReconnectIntervalMs);
  connect(m_mqttReconnectTimer, &QTimer::timeout, this,
          &MainWindow::triggerScheduledMqttReconnect);
#endif

  // 启动默认机器人身份以配置/环境变量为准，避免硬编码回落到 R3。
  m_selectedRobotTypeFromSettings = settingsRobotTypeLabel(
      static_cast<quint8>(ConfigManager::instance().getClientRobotId()));

  // 初始化调试日志窗口
  m_debugLogWidget = new DebugLogWidget(); // 独立窗口

  // 默认兑换面板
  m_exchangeQmlSource = "qrc:/qml/ExchangePanel.qml";

  // 设置按键面板
  setupKeyPanels();

  // 设置音频
  setupAudio();

#ifdef RM_HAS_AR_OVERLAY
  // 初始化 AR 系统
  m_arManager = new AROverlayManager(m_gameData, this);
  connect(m_arManager, &AROverlayManager::initializationCompleted, this,
          &MainWindow::onARInitializationCompleted);
#endif

  // 初始化协议模拟器
  m_simulator = new ProtocolSimulator(this);

  // devhooks 依赖 m_videoBackground，需在 initializeLayout() 创建视频控件后再初始化

  // 初始化 S1 协议管理器（可选，由 RM_S1_ENGINE_HOST 环境变量控制）
  if (qEnvironmentVariableIsSet("RM_S1_ENGINE_HOST")) {
    QString s1Host = QString::fromUtf8(qgetenv("RM_S1_ENGINE_HOST"));
    quint16 s1Port = qEnvironmentVariableIntValue("RM_S1_ENGINE_PORT");
    if (s1Port == 0) s1Port = 64998;

    qInfo() << "MainWindow: S1 protocol manager is enabled, connecting to" << s1Host << ":" << s1Port;

    m_s1ProtocolManager = new S1ProtocolManager(m_gameData, this);

    // 配置凭据（可选，使用环境变量或默认值）
    QString account = QString::fromUtf8(qgetenv("RM_S1_ACCOUNT"));
    QString password = QString::fromUtf8(qgetenv("RM_S1_PASSWORD"));
    if (!account.isEmpty() && !password.isEmpty()) {
      m_s1ProtocolManager->setCredentials(account, password);
    }
    if (qEnvironmentVariableIsSet("RM_S1_TID")) {
      m_s1ProtocolManager->setTid(qEnvironmentVariableIntValue("RM_S1_TID"));
    }
    if (qEnvironmentVariableIsSet("RM_S1_TEAMID")) {
      m_s1ProtocolManager->setTeamId(qEnvironmentVariableIntValue("RM_S1_TEAMID"));
    }

    // 连接信号
    connect(m_s1ProtocolManager, &S1ProtocolManager::connected, this, []() {
      qInfo() << "[MainWindow] S1 protocol connected";
    });
    connect(m_s1ProtocolManager, &S1ProtocolManager::loginSucceeded, this, []() {
      qInfo() << "[MainWindow] S1 protocol login succeeded";
    });
    connect(m_s1ProtocolManager, &S1ProtocolManager::loginFailed, this,
            [](const QString &reason) {
      qWarning() << "[MainWindow] S1 protocol login failed:" << reason;
    });
    connect(m_s1ProtocolManager, &S1ProtocolManager::gameStateReceived, this,
            [](const QVariantMap &state) {
      qDebug() << "[MainWindow] S1 game state received:" << state;
    });
    connect(m_s1ProtocolManager, &S1ProtocolManager::errorOccurred, this,
            [](const QString &error) {
      qWarning() << "[MainWindow] S1 protocol error:" << error;
    });

    // 开始连接
    if (!m_s1ProtocolManager->connectToEngine(s1Host, s1Port)) {
      qWarning() << "MainWindow: Failed to start S1 protocol manager";
      delete m_s1ProtocolManager;
      m_s1ProtocolManager = nullptr;
    }
  } else {
    qDebug() << "MainWindow: S1 protocol manager is disabled (set RM_S1_ENGINE_HOST to enable)";
  }
}

void MainWindow::setupDevHooks() {
#ifdef RM26_ENABLE_DEVTOOLS
  if (m_devHooks) {
    return;
  }

  if (!ClientDevHooks::isEnabled()) {
    qDebug() << "MainWindow: Dev hooks are disabled (set RM_DEVTOOLS=1 to enable)";
    return;
  }

  if (!m_gameData || !m_networkManager || !m_videoBackground) {
    qWarning() << "MainWindow: Dev hooks prerequisites are not ready";
    return;
  }

  VideoReceiver *videoReceiver = m_videoBackground->getVideoReceiver();
  HevcDecoder *hevcDecoder = videoReceiver ? videoReceiver->decoder() : nullptr;

  m_devHooks = new ClientDevHooks(m_gameData, m_networkManager, videoReceiver,
                                  m_videoBackground, hevcDecoder, this, this);

  if (!m_devHooks->start()) {
    qWarning() << "MainWindow: Failed to start dev hooks";
    delete m_devHooks;
    m_devHooks = nullptr;
    return;
  }

#ifdef RM_HAS_MQTT
  connect(m_networkManager, &NetworkManager::mqttMessageObserved, m_devHooks,
          [this](const QString &topic, int payloadSize,
                 const QString &payloadSha1, qint64 receivedMs) {
            if (!m_devHooks) {
              return;
            }
            m_devHooks->recordTraceEvent(
                QStringLiteral("mqtt_message_observed"),
                QJsonObject{{"topic", topic},
                            {"payload_size", payloadSize},
                            {"payload_sha1", payloadSha1},
                            {"received_ms", receivedMs}});
          });
#endif  // RM_HAS_MQTT

  qInfo() << "MainWindow: Dev hooks started on port" << m_devHooks->port();
#else
  if (qEnvironmentVariableIsSet("RM_DEVTOOLS")) {
    qWarning() << "MainWindow: RM26_ENABLE_DEVTOOLS is OFF, runtime dev hooks are not compiled";
  }
#endif  // RM26_ENABLE_DEVTOOLS

  // MQTT 连接完成信号必须在 DEVTOOLS 外面，否则非dev构建下收不到回调
#ifdef RM_HAS_MQTT
  connect(m_networkManager, &NetworkManager::mqttConnectCompleted, this,
          [this](bool success, const QString &error) {
            if (m_mqttIsBootstrapMode) {
              onMqttBootstrapCompleted(success, error);
            } else {
              onMqttSwitchCompleted(success, error);
            }
          });
#endif  // RM_HAS_MQTT
}

// --- 信号-槽集中绑定 ---
void MainWindow::setupConnections() {
  // QML 上下文属性改在 initializeLayout 中绑定，确保初始化顺序正确。

  // --- GameData 信号 ---
  connect(m_gameData, &GameData::outpostDestroyed, this,
          [this](bool isRed) {
            if (m_battleMessage) {
                m_battleMessage->showOutpostDestroyed(isRed);
      }
  });
  m_lastEnemyOutpostHealthForDartSound = m_gameData->enemyOutpostHealth();
  connect(m_gameData, &GameData::outpostHealthUpdated, this,
          [this](TeamColor team) {
            updateDartCanOpenSoundOnOutpostHealthChange(team);
          });
  connect(m_gameData, &GameData::baseDestroyed, this,
          [this](bool isRed) {
              if (m_battleMessage) {
                  m_battleMessage->showBaseDestroyed(isRed);
              }
          });
  connect(m_gameData, &GameData::outpostStatusChanged, this,
          [this](bool isRed, int status) {
            updateOutpostReviveReminder(isRed, status);
            if (m_battleMessage) {
            //1：存活，解除无敌，中部装甲旋转;2：存活，解除无敌，中部装甲停转
              if (status == 1 || status == 2) {
                m_battleMessage->showOutpostStatusChange(isRed, status);
              }
            }
          });
  connect(m_gameData, &GameData::gameStageChanged, this,
          [this](GameStage) {
            const bool allyIsRed = m_gameData->currentRobotId() < 100;
            const OutpostData &allyOutpost =
                m_gameData->getOutpostByTeam(allyIsRed ? TeamColor::RED
                                                       : TeamColor::BLUE);
            updateOutpostReviveReminder(allyIsRed, allyOutpost.status);
          });
  connect(m_gameData, &GameData::gameTimeUpdated, this, [this](quint16) {
    const bool allyIsRed = m_gameData->currentRobotId() < 100;
    const OutpostData &allyOutpost =
        m_gameData->getOutpostByTeam(allyIsRed ? TeamColor::RED
                                               : TeamColor::BLUE);
    updateOutpostReviveReminder(allyIsRed, allyOutpost.status);
  });
  // 前哨站数据变化 -> 转发给机器人 (CustomControl 0x0311)
  connect(m_gameData, &GameData::outpostStatusChanged, this,
          [this](bool, int) { scheduleOutpostForward(); });
  connect(m_gameData, &GameData::outpostHealthUpdated, this,
          [this](TeamColor) { scheduleOutpostForward(); });
  connect(m_gameData, &GameData::baseStatusChanged, this,
          [this](bool isRed, int status) {
            if (m_battleMessage) {
              m_battleMessage->showBaseStatusChange(isRed, status);
            }
          });
  connect(m_gameData, &GameData::gameStateUpdated, this,
          &MainWindow::onGameStateUpdated);
  connect(m_gameData, &GameData::runeVoicePromptRequested, this,
          [this](int runeType, int remainingChances) {
            playRuneVoicePrompt(runeType, remainingChances);
          });
  // 飞镖命中视图切回：需要跟随协议时间变化（gameTimeUpdated 每秒触发）而非仅
  // gameStateUpdated（仅在阶段/比分/暂停等变化时触发），确保基于协议时间的计时。
  connect(m_gameData, &GameData::gameTimeUpdated, this, [this](quint16) {
    if (m_dartHitViewPending) {
      checkDartHitViewSwitch();
    }
  });
  connect(m_gameData, &GameData::myRobotUpdated, this, [this]() {
    updateCurrentRobotPanel();
    updateExchangeHintText(m_exchangeQmlSource);
    syncSettingsPanelState();
  });
  connect(m_gameData, &GameData::gameResultReceived, this,
          &MainWindow::onGameResultReceived);
  // 击杀事件
  connect(m_gameData, &GameData::killEventOccurred, this,
          [this](const KillRecord &record) {
            if (m_battleMessage) {
              m_battleMessage->processKillEvent(record);
            }
          });
  // --- 战场消息 (裁判警告等) ---
  // 结构化判罚信息
  connect(m_gameData, &GameData::refereeWarningUpdated, this,
          [this](const robomaster::RefereeWarningData &data) {
            if (m_battleMessage) {
              m_battleMessage->showRefereeWarning(data.level(), data.offending_robot_id(),
                  data.penalty_effect_sec(), data.total_penalty_num());
            }
          });
  // --- 飞镖目标选择状态同步 ---
    connect(m_gameData, &GameData::siloCommandRequested, this,
          [this](int targetId, bool open, bool launchConfirm) {
            qDebug() << "[DartDebug] siloCommandRequested -> target:" << targetId
                     << "open:" << open << "launchConfirm:" << launchConfirm;
            if (!m_networkManager) {
              qDebug() << "[DartDebug] siloCommandRequested -> NetworkManager unavailable";
              return;
            }
            m_networkManager->sendDartCommand(static_cast<uint32_t>(targetId),
                                              open, launchConfirm);
          });
  // --- 视频源变化 ---
  connect(m_gameData, &GameData::videoSourceChanged, this,
          &MainWindow::onVideoSourceChanged);



  // --- 系统消息 ---
  connect(m_gameData, &GameData::systemMessageReceived, this,
          &MainWindow::onSystemMessageReceived);
    connect(m_gameData, &GameData::officialEventPopupRequested, this,
      &MainWindow::onOfficialEventPopupRequested);
  connect(m_gameData, &GameData::runeActivable, this,
          [this](int runeType) {
            if (m_battleMessage) {
              m_battleMessage->showRuneActivable(runeType);
            }
          });
  connect(m_gameData, &GameData::runeActived, this,
          [this](int runeType) {
            if (m_battleMessage) {
              m_battleMessage->showRuneActived(runeType);
            }
            if (m_gameData->airSupportIsBeingTargeted() == 1) {
              return;
            }
            if (!m_qmlEventMessagePanel) {
              return;
            }

            if (QObject *rootObject = m_qmlEventMessagePanel->rootObject()) {
              rootObject->setProperty("panelMode", "rune");
            }

            const int displayToken =
                m_qmlEventMessagePanel->property("displayToken").toInt() + 1;
            m_qmlEventMessagePanel->setProperty("displayToken", displayToken);
            m_qmlEventMessagePanel->show();
            m_qmlEventMessagePanel->raise();

            QTimer::singleShot(6000, this, [this, displayToken]() {
              if (m_qmlEventMessagePanel &&
                  m_qmlEventMessagePanel->property("displayToken")
                          .toInt() == displayToken) {
                m_qmlEventMessagePanel->hide();
              }
            });
          });
  connect(m_gameData, &GameData::dartMessageTriggered, this,
          [this]() {
            if (!m_qmlEventMessagePanel) {
              return;
            }

            const bool laserTargeted =
                m_gameData->airSupportIsBeingTargeted() == 1;
            int displayToken =
                m_qmlEventMessagePanel->property("displayToken").toInt();
            if (!laserTargeted) {
              if (QObject *rootObject = m_qmlEventMessagePanel->rootObject()) {
                rootObject->setProperty("panelMode", "dart");
              }

              // 显示面板
              ++displayToken;
              m_qmlEventMessagePanel->setProperty("displayToken", displayToken);
              m_qmlEventMessagePanel->show();
              m_qmlEventMessagePanel->raise();
            }

            // 飞镖命中自动切换图传界面（基于协议时间计时，不使用本地定时器）
            // 仅当「对方命中我方目标」时切换，己方命中对方目标时不切。
            const QVariantMap dartData = m_gameData->dartMessageData();
            const bool dartIsRedTeam =
                dartData.value("isRedTeam", false).toBool();
            const bool myIsRed = [this]() {
              const RobotData *r = m_gameData->getCurrentRobot();
              return r ? (r->team == TeamColor::RED)
                       : (m_gameData->getCurrentRobotId() < 100);
            }();
            if (dartIsRedTeam != myIsRed) {
              const bool shouldStartOrExtendOcclusion =
                  m_tacticalMode || m_dartHitViewPending;
              if (shouldStartOrExtendOcclusion) {
                const int currentGameTime = m_gameData->getGameTime();
                const int targetId = dartData.value("targetId", 0).toInt();
                const int hitOcclusionDurationSec =
                    dartData.value("occlusionDurationSec",
                                   RM::Dart::dartOcclusionDurationSeconds(
                                       targetId, 1))
                        .toInt();
                const int accumulatedOcclusionDurationSec =
                    MainWindowStatePolicy::accumulatedDartOcclusionSeconds(
                        m_dartHitRemainingAtHit, m_dartHitOcclusionDurationSec,
                        currentGameTime, hitOcclusionDurationSec);
                const bool extendingExistingOcclusion = m_dartHitViewPending;

                m_dartHitRemainingAtHit = currentGameTime;
                m_dartHitTargetId = targetId;
                m_dartHitOcclusionDurationSec =
                    accumulatedOcclusionDurationSec;
                m_dartHitDisplayToken = displayToken;

                if (!extendingExistingOcclusion) {
                  applyTacticalMode(false);
                  m_dartHitViewPending = true;

                  // 英雄部署模式下被飞镖命中，显示工业相机而非图传
                  {
                    const RobotData *r = m_gameData->getCurrentRobot();
                    const bool isHeroAndDeployed =
                        r && r->type == ::RobotType::HERO &&
                        m_gameData->deployModeStatus() ==
                            1; // 协议值 1 表示已部署
                    if (isHeroAndDeployed) {
                      m_dartHitHeroDeployCamActive = true;
                      if (m_videoBackground)
                        m_videoBackground->hide();
                      if (m_heroVideoWidget) {
                        m_heroVideoWidget->setForceVisible(true);
                        m_heroVideoWidget->setGeometry(
                            m_centralWidget->rect());
                        m_heroVideoWidget->show();
                        m_heroVideoWidget->raise();
                      }
                      qInfo() << "[DartHit] HERO deploy mode: showing "
                                 "industrial camera fullscreen";
                    }
                  }

                  qInfo() << "[DartHit] Switched to video feed, remaining="
                          << m_dartHitRemainingAtHit
                          << "targetId=" << m_dartHitTargetId
                          << "duration=" << m_dartHitOcclusionDurationSec;
                } else {
                  qInfo() << "[DartHit] Extended active occlusion, remaining="
                          << m_dartHitRemainingAtHit
                          << "targetId=" << m_dartHitTargetId
                          << "addedDuration=" << hitOcclusionDurationSec
                          << "totalRemainingDuration="
                          << m_dartHitOcclusionDurationSec;
                }
              } else {
                qInfo() << "[DartHit] Enemy dart hit received outside tactical"
                        << "viewPending=" << m_dartHitViewPending;
              }
            } else {
              qInfo() << "[DartHit] Own team dart hit, skip view switch"
                      << "dartIsRedTeam=" << dartIsRedTeam
                      << "myIsRed=" << myIsRed;
            }

            //6s自动关闭面板（非战术模式下生效；战术模式下由命中视图逻辑接管）
            if (!m_dartHitViewPending && !laserTargeted) {
              QTimer::singleShot(6000, this, [this, displayToken]() {
                if (m_qmlEventMessagePanel &&
                    m_qmlEventMessagePanel->property("displayToken")
                            .toInt() == displayToken) {
                  m_qmlEventMessagePanel->hide();
                }
              });
            }
          });

  // --- 网络调试日志 ---
  connect(m_gameData, &GameData::airSupportStarted, this,
          [this](bool isRedTeam) {
            if (m_battleMessage) {
              m_battleMessage->showAirSupportStarted(isRedTeam);
            }
          });
  connect(m_gameData, &GameData::airSupportTargetingStateChanged, this,
          [this](bool targeted) {
            if (targeted) {
              playSoundFromResourceFolder(
                  QStringLiteral("air_support_laser_targeted.mp3"));
            }

            if (!m_qmlEventMessagePanel) {
              return;
            }

            const int displayToken =
                m_qmlEventMessagePanel->property("displayToken").toInt() + 1;
            m_qmlEventMessagePanel->setProperty("displayToken", displayToken);

            QObject *rootObject = m_qmlEventMessagePanel->rootObject();
            if (targeted) {
              if (rootObject) {
                rootObject->setProperty("panelMode", "laserTargeted");
              }
              m_qmlEventMessagePanel->show();
              m_qmlEventMessagePanel->raise();
            } else if (!rootObject ||
                       rootObject->property("panelMode").toString() ==
                           QStringLiteral("laserTargeted")) {
              m_qmlEventMessagePanel->hide();
            }
          });
  connect(m_gameData, &GameData::airSupportCountered, this,
          [this](const QString &soundSelector) {
            playSecondarySound(event8CounteredSoundFile(soundSelector));
          });
  connect(m_gameData, &GameData::dartGateOpened, this,
          [this](bool isRedTeam, bool isEnemyTeam) {
            if (m_battleMessage) {
              m_battleMessage->showDartGateOpened(isRedTeam, isEnemyTeam);
            }
          });
  connect(m_gameData, &GameData::baseUnderAttackEvent, this,
          [this](bool isRedTeam) {
            if (m_battleMessage) {
              m_battleMessage->showBaseUnderAttack(isRedTeam);
            }
            if (m_gameData &&
                RM::isGunnerOperator(m_gameData->currentRobotId())) {
              playSecondarySound(
                  QStringLiteral("resources/sounds/basedrop.mov")); // 待替换资源
            }
          });
  connect(m_gameData, &GameData::enemyOutpostStoppedEvent, this,
          [this](bool isRedTeam) {
            if (m_battleMessage) {
              m_battleMessage->showEnemyOutpostStopped(isRedTeam);
            }
          });
  connect(m_gameData, &GameData::enemyBaseShieldOpenedEvent, this,
          [this](bool isRedTeam) {
            if (m_battleMessage) {
              m_battleMessage->showEnemyBaseShieldOpened(isRedTeam);
            }
          });
  connect(m_gameData, &GameData::allyOutpostHealthDropAlertTriggered, this,
          [this]() {
            if (m_gameData &&
                RM::isGunnerOperator(m_gameData->currentRobotId())) {
              playSecondarySound(
                  QStringLiteral("resources/sounds/outpostdrop.mov")); // 待替换资源
            }
          });
  connect(m_gameData, &GameData::allyBaseHealthDropAlertTriggered, this,
          [this]() {
            if (m_gameData &&
                RM::isGunnerOperator(m_gameData->currentRobotId())) {
              playSecondarySound(QStringLiteral("resources/sounds/basedrop.mov"));
            }
          });
  connect(m_gameData, &GameData::allyBaseArmorOpenedTriggered, this,
          [this]() {
            playSecondarySound(
                QStringLiteral("resources/sounds/%1")
                    .arg(allyBaseArmorOpenedSoundFileName()),
                allyBaseArmorOpenedSoundLoopCount());
          });
  connect(m_gameData, &GameData::allyFortressOccupationAlertTriggered, this,
          [this]() {
            playSecondarySound(QStringLiteral("resources/sounds/occupation.mov"));
          });
  connect(m_networkManager, &NetworkManager::dataSent, m_debugLogWidget,
          &DebugLogWidget::logSent);
  connect(m_networkManager, &NetworkManager::dataReceived, m_debugLogWidget,
          &DebugLogWidget::logReceived);

  //--- qml信号连接 ---
  //--- settingPanel(P / +-*/ 按键) ---
  if (m_qmlSettingsPanel && m_qmlSettingsPanel->rootObject()) {
    QObject *root = m_qmlSettingsPanel->rootObject();
    // 硬件/图形设置
    //  1. 控制灵敏度
    connect(root, SIGNAL(sensitivityChanged(int)), this,
            SLOT(onSensitivityChanged(int)));
    // 2. 系统音量调节 (主音量)
    connect(root, SIGNAL(volumeChanged(int)), this,
            SLOT(onSystemVolumeChanged(int)));
    onSystemVolumeChanged(50);

    // 3. 背景音乐调节 (BGM)
    connect(root, SIGNAL(bgmChanged(int)), this,
            SLOT(onMusicVolumeChanged(int)));

    // 1. 准星显示（绑定到 QML CentralAimingHUD）
    if (m_qmlCentralAimingHUD && m_qmlCentralAimingHUD->rootObject()) {
      connect(root, SIGNAL(crosshairVisibilityChanged(bool)), this,
              SLOT(onCrosshairVisibilityChanged(bool)));
      m_qmlCentralAimingHUD->rootObject()->setProperty("crosshairVisible",
                                                       true);
    }
    else {
      qWarning() << "MainWindow: no crosshair target found for settings bind";
    }
    // 2. 小地图显示
    connect(root, SIGNAL(miniMapVisibilityChanged(bool)), m_miniMap,
            SLOT(setVisible(bool)));
    // 3. 显示模式
    connect(root, SIGNAL(displayModeChanged(QString)), this,
            SLOT(onDisplayModeChanged(QString)));
    // 4. 帧率设置
    connect(root, SIGNAL(fpsChanged(QString)), this,
            SLOT(onFpsChanged(QString)));
    // 5. 图传源切换
    connect(root, SIGNAL(vtChanged(QString)), this,
            SLOT(onVideoSourceTypeChanged(QString)));
    // 6. 性能体系选择
    connect(root, SIGNAL(performanceSelectionChanged(int, int, int)), this,
            SLOT(onPerformanceSelectionChanged(int, int, int)));
  }
  // 若1.5s内没有新帧 则判断为图传连接失败
  if (!m_videoConnectionTimer) {
    m_videoConnectionTimer = new QTimer(this);
    m_videoConnectionTimer->setSingleShot(true);
    m_videoConnectionTimer->setInterval(1500);
    connect(m_videoConnectionTimer, &QTimer::timeout, this, [this]() {
      if (m_qmlSettingsPanel && m_qmlSettingsPanel->rootObject()) {
        m_qmlSettingsPanel->rootObject()->setProperty("isVideoConnected",
                                                      false);
      }
    });
  }

  // --- 视频统计 ---
  if (m_videoBackground && m_videoBackground->getVideoReceiver()) {
    connect(m_videoBackground->getVideoReceiver(), &VideoReceiver::statsUpdated,
            m_debugLogWidget, &DebugLogWidget::logVideoStats);

    // 只要收到并组装出合法 HEVC 帧，就认为图传链路在线（用于本地视频）。
    connect(m_videoBackground->getVideoReceiver(),
            &VideoReceiver::hevcDataReady, this,
            [this](quint16, const QByteArray &data) {
              if (!data.isEmpty() && m_qmlSettingsPanel &&
                  m_qmlSettingsPanel->rootObject()) {
                m_qmlSettingsPanel->rootObject()->setProperty(
                    "isVideoConnected", true);
              }
              if (m_videoConnectionTimer) {
                m_videoConnectionTimer->start();
              }
            });

    // 如果 NetworkManager 提供了 NetworkManager::customVideoPayloadReceived 信号，则把它连接到 VideoReceiver::feedH264Frame
    if (m_networkManager) {
      // 连接视频信号
      connect(m_networkManager, &NetworkManager::customVideoPayloadReceived,
              m_videoBackground->getVideoReceiver(), &VideoReceiver::feedH264Frame,
              Qt::UniqueConnection);

      qDebug() << "[herovideo] MainWindow: connected NetworkManager" << m_networkManager
               << "to VideoReceiver" << m_videoBackground->getVideoReceiver();
    }
    // 将 VideoReceiver 解析的帧转发到 GameData，以便 QML 绑定显示
    if (m_videoBackground->getVideoReceiver() && m_gameData) {
      connect(m_videoBackground->getVideoReceiver(), &VideoReceiver::imageReceivedH264,
              m_gameData, &GameData::onVideoFrameReceived, Qt::UniqueConnection);
    }
  }

  // 收到视频帧则将图传设置为已连接
  if (m_videoBackground) {
    connect(m_videoBackground, &VideoBackgroundWidget::frameUpdated, this,
            [this](const QImage &frame) {
              if (!frame.isNull() && m_qmlSettingsPanel &&
                  m_qmlSettingsPanel->rootObject()) {
                m_qmlSettingsPanel->rootObject()->setProperty(
                    "isVideoConnected", true);
              }
              if (m_videoConnectionTimer) {
                m_videoConnectionTimer->start();
              }
            });
  }

#ifdef RM_HAS_AR_OVERLAY
  // --- AR 系统连接 ---
  if (m_videoBackground && m_arManager) {
    // 视频帧传输: VideoBackgroundWidget -> AROverlayManager
    connect(m_videoBackground, &VideoBackgroundWidget::frameUpdated,
            m_arManager, &AROverlayManager::processFrame);
  }

  // 配置变更
  connect(&ConfigManager::instance(), &ConfigManager::configReloaded, this,
          &MainWindow::onARSettingsChanged);
#endif

  // --- 模拟器连接 ---
  if (m_simulator) {
    connect(m_simulator, &ProtocolSimulator::dataReceived, this,
            &MainWindow::onSimulatorDataReceived);
  }
}

// --- 启动服务 ---
void MainWindow::startServices() {
  const quint8 selectedRobotId =
      robotIdFromSettingsType(m_selectedRobotTypeFromSettings);
  const QStringList clientIdCandidates =
      mqttClientIdCandidatesForRobotType(m_selectedRobotTypeFromSettings);

  if (m_gameData) {
    m_gameData->setCurrentRobotId(selectedRobotId);
  }

#ifdef RM_HAS_MQTT
  // 如果使用 Paho MQTT（RM_HAS_MQTT），通过 NetworkManager 异步启动 MQTT 连接
  {
    const QString broker = ConfigManager::instance().getMqttBroker();
    const quint16 port = ConfigManager::instance().getMqttPort();
    const QString uri = QStringLiteral("tcp://%1:%2").arg(broker).arg(port);
    qInfo() << "MainWindow: MQTT bootstrap broker=" << uri
            << "clientId candidates=" << clientIdCandidates;
    if (m_networkManager && !clientIdCandidates.isEmpty()) {
      // 异步 MQTT 启动：逐个尝试候选 clientId，主线程不阻塞
      m_mqttCandidates = clientIdCandidates;
      m_mqttCandidateIndex = 0;
      m_mqttUri = uri;
      m_lastMqttConnectError.clear();
      m_mqttIsBootstrapMode = true;
      qInfo() << "MainWindow: async MQTT try clientId"
              << m_mqttCandidates.at(0) << "(1/" << m_mqttCandidates.size() << ")";
      m_networkManager->startMqtt(uri, m_mqttCandidates.at(0));
      // onMqttBootstrapCompleted 回调处理后续启动流程
      return;
    }
  }
#endif

#ifdef RM_HAS_QT_MQTT
#ifndef RM_HAS_MQTT
  // 启动 MQTT 客户端（如果已找到 Qt6::Mqtt）
  if (!m_mqttClient) {
    m_mqttClient = new MqttClient(m_gameData, this);
  }
  m_mqttClient->setClientId(clientIdCandidates.isEmpty()
                                ? QString()
                                : clientIdCandidates.first());
  m_mqttClient->start();
  m_activeMqttClientId =
      clientIdCandidates.isEmpty() ? QString() : clientIdCandidates.first();
  finishServicesStartup(true);
  return;
#else
  qDebug() << "MainWindow: Skip Qt MQTT client because Paho MQTT is enabled";
#endif
#endif

  // 无 MQTT：直接启动后续服务
  finishServicesStartup(false);
}

void MainWindow::finishServicesStartup(bool mqttModeEnabled) {
  if (m_servicesStarted) {
    qInfo() << "MainWindow: services already started; keep current network/video pipeline"
            << "requestedMqttMode=" << mqttModeEnabled;
    return;
  }
  m_servicesStarted = true;
  m_mqttIsBootstrapMode = false;

  if (!mqttModeEnabled) {
    int clientPort = ConfigManager::instance().getClientPort();
    m_networkManager->startListening(clientPort);
  } else {
    qDebug() << "MainWindow: MQTT mode enabled, UDP listener disabled";
  }

  // 视频流启动
  QString videoStreamUrl = ConfigManager::instance().getVideoStreamUrl();
  m_videoBackground->playUrl(videoStreamUrl);

  // 启动模拟器
  if (m_simulator) {
    m_simulator->startSimulation();
  }
}

void MainWindow::scheduleMqttReconnect(const QString &error) {
#ifdef RM_HAS_MQTT
  if (!m_mqttReconnectTimer || !m_networkManager) {
    return;
  }

  if (!RM::MainWindowStatePolicy::shouldScheduleMqttRetry(error)) {
    qInfo() << "MainWindow: skip MQTT auto-retry for non-transient failure:"
            << error;
    return;
  }

  if (m_networkManager->isMqttConnected()) {
    cancelScheduledMqttReconnect();
    return;
  }

  if (m_mqttReconnectTimer->isActive()) {
    qDebug() << "MainWindow: MQTT reconnect already scheduled in"
             << m_mqttReconnectTimer->remainingTime() << "ms";
    return;
  }

  qWarning() << "MainWindow: scheduling MQTT reconnect in"
             << m_mqttReconnectTimer->interval() << "ms"
             << "robotType=" << m_selectedRobotTypeFromSettings
             << "lastError=" << error;
  m_mqttReconnectTimer->start();
#else
  Q_UNUSED(error);
#endif
}

void MainWindow::cancelScheduledMqttReconnect() {
#ifdef RM_HAS_MQTT
  if (m_mqttReconnectTimer && m_mqttReconnectTimer->isActive()) {
    m_mqttReconnectTimer->stop();
  }
#endif
}

void MainWindow::triggerScheduledMqttReconnect() {
#ifdef RM_HAS_MQTT
  if (!m_networkManager) {
    return;
  }

  if (m_networkManager->isMqttConnected()) {
    cancelScheduledMqttReconnect();
    return;
  }

  qInfo() << "MainWindow: triggering scheduled MQTT reconnect"
          << "robotType=" << m_selectedRobotTypeFromSettings
          << "broker=" << m_mqttUri;
  restartMqttForRobotType(m_selectedRobotTypeFromSettings);
#endif
}

// --- 异步 MQTT 连接回调 ---

void MainWindow::onMqttBootstrapCompleted(bool success, const QString &error) {
  qInfo() << "MainWindow: onMqttBootstrapCompleted success=" << success << "error=" << error;
  if (m_servicesStarted) {
    qInfo() << "MainWindow: ignoring late MQTT bootstrap result after services startup;"
            << "video receiver will not be rebound";
    return;
  }
  if (m_mqttCandidates.isEmpty()) {
    qWarning() << "MainWindow: MQTT candidates list is empty, falling back to UDP";
    finishServicesStartup(false);
    return;
  }
  auto candidateAt = [this](int idx) {
    return (idx >= 0 && idx < m_mqttCandidates.size())
      ? m_mqttCandidates.at(idx) : QStringLiteral("?");
  };
  if (success) {
    cancelScheduledMqttReconnect();
    m_activeMqttClientId = candidateAt(m_mqttCandidateIndex);
    m_lastMqttConnectError.clear();
    qInfo() << "MainWindow: MQTT bootstrap success, clientId=" << m_activeMqttClientId;
    finishServicesStartup(true);
  } else {
    m_lastMqttConnectError = error;
    qWarning() << "MainWindow: MQTT bootstrap failed for clientId"
               << candidateAt(m_mqttCandidateIndex) << ":" << error;
    m_networkManager->stopMqtt();
    m_mqttCandidateIndex++;
    if (m_mqttCandidateIndex < m_mqttCandidates.size()) {
      qInfo() << "MainWindow: async MQTT try next clientId"
              << candidateAt(m_mqttCandidateIndex)
              << "(" << (m_mqttCandidateIndex + 1) << "/" << m_mqttCandidates.size() << ")";
      m_networkManager->startMqtt(m_mqttUri, candidateAt(m_mqttCandidateIndex));
    } else {
      qWarning() << "MainWindow: All MQTT candidates failed, falling back to UDP"
                 << "broker=" << m_mqttUri
                 << "candidates=" << m_mqttCandidates
                 << "lastError=" << m_lastMqttConnectError;
      scheduleMqttReconnect(m_lastMqttConnectError);
      finishServicesStartup(false);
    }
  }
}

void MainWindow::onMqttSwitchCompleted(bool success, const QString &error) {
  if (success) {
    cancelScheduledMqttReconnect();
    // 显式身份切换时采用正在尝试的 clientId；自动重连没有 switch
    // clientId，必须保留当前身份，不能把活动身份覆盖为空。
    m_activeMqttClientId = MainWindowStatePolicy::resolvedActiveMqttClientId(
        m_activeMqttClientId, m_tryingMqttClientId);
    m_lastMqttConnectError.clear();
    qDebug() << "MainWindow: MQTT switch success, clientId=" << m_activeMqttClientId;
  } else {
    m_lastMqttConnectError = error;
    qWarning() << "MainWindow: MQTT switch failed for clientId"
               << m_tryingMqttClientId << ":" << error;
    m_networkManager->stopMqtt();
    m_mqttCandidateIndex++;
    if (m_mqttCandidateIndex < m_mqttCandidates.size()) {
      // 保存下一个要尝试的 clientId
      m_tryingMqttClientId = m_mqttCandidates.at(m_mqttCandidateIndex);
      qInfo() << "MainWindow: async MQTT switch try next clientId"
              << m_tryingMqttClientId
              << "(" << (m_mqttCandidateIndex + 1) << "/" << m_mqttCandidates.size() << ")";
      m_networkManager->startMqtt(m_mqttUri, m_tryingMqttClientId);
    } else {
      qWarning() << "MainWindow: All MQTT switch candidates failed:"
                 << m_mqttCandidates
                 << "broker=" << m_mqttUri
                 << "lastError=" << m_lastMqttConnectError;
      scheduleMqttReconnect(m_lastMqttConnectError);
    }
  }
}

// 将 SettingsPanel 的机器人类型转换为基础 ID
int MainWindow::baseRobotIdFromSettingsType(const QString &robotType) const {
  // 读取 SettingsPanel 当前选择的机器人类型
  static const QRegularExpression re(QStringLiteral("[RB](\\d+)"),
                                     QRegularExpression::CaseInsensitiveOption);
  const QRegularExpressionMatch match = re.match(robotType);
  //转化为数字
  if (match.hasMatch()) {
    bool ok = false;
    const int id = match.captured(1).toInt(&ok);
    if (ok) {
      return id;
    }
  }
  return 3;
}

// 将 SettingsPanel 的机器人类型转换为带阵营的机器人 ID
quint8 MainWindow::robotIdFromSettingsType(const QString &robotType) const {
  int baseId = baseRobotIdFromSettingsType(robotType);
  if (baseId < 1 || baseId > 9) {
    baseId = 3;
  }

  const bool isBlue = robotType.startsWith("B", Qt::CaseInsensitive);
  return static_cast<quint8>(isBlue ? (100 + baseId) : baseId);
}

// 根据 SettingsPanel 选择的 robotType 生成协议定义的 MQTT clientId。
// V1.3 文档定义：红方 1~7，蓝方 101~107。
QString MainWindow::mqttClientIdForRobotType(const QString &robotType) const {
  return QString::number(robotIdFromSettingsType(robotType));
}

// 返回可尝试的 clientId 列表。
// Settings 登录/切换时必须让选中的机器人身份成为唯一 MQTT 身份；
// 否则 broker 拒绝 R4/R5 后回退到 config 里的 R3，会导致服务器仍显示 3 在线。
QStringList MainWindow::mqttClientIdCandidatesForRobotType(
    const QString &robotType) const {
  QStringList candidates;

  const QString protocolId = mqttClientIdForRobotType(robotType).trimmed();
  if (!protocolId.isEmpty()) {
    candidates << protocolId;
    return candidates;
  }

  const int configuredId = ConfigManager::instance().getClientRobotId();
  if (configuredId > 0) {
    candidates << QString::number(configuredId);
  }

  return candidates;
}

// 根据当前 SettingsPanel 选择的机器人，重启 MQTT 连接并切换 clientId
void MainWindow::restartMqttForRobotType(const QString &robotType) {
  const QStringList candidates = mqttClientIdCandidatesForRobotType(robotType);
  if (candidates.isEmpty()) {
    return;
  }
  cancelScheduledMqttReconnect();
  if (candidates.first() == m_activeMqttClientId && m_networkManager &&
      m_networkManager->isMqttConnected()) {
    qDebug() << "MainWindow: MQTT clientId unchanged:" << m_activeMqttClientId;
    return;
  }
  qInfo() << "MainWindow: switching MQTT identity, robotType=" << robotType
          << "candidates=" << candidates;

#ifdef RM_HAS_MQTT
  if (m_networkManager) {
    const QString broker = ConfigManager::instance().getMqttBroker();
    const quint16 port = ConfigManager::instance().getMqttPort();
    const QString uri = QStringLiteral("tcp://%1:%2").arg(broker).arg(port);

    // 断开当前 MQTT 连接
    m_networkManager->stopMqtt();

    // 异步切换：逐个尝试候选 clientId，主线程不阻塞
    m_mqttCandidates = candidates;
    m_mqttCandidateIndex = 0;
    m_mqttUri = uri;
    m_lastMqttConnectError.clear();
    m_mqttIsBootstrapMode = false;
    // 保存正在尝试连接的 clientId，以便回调时直接使用
    m_tryingMqttClientId = candidates.first();
    if (!m_mqttCandidates.isEmpty()) {
      qInfo() << "MainWindow: async MQTT switch try clientId"
              << m_mqttCandidates.at(0) << "(1/" << m_mqttCandidates.size() << ")";
      m_networkManager->startMqtt(uri, m_mqttCandidates.at(0));
    } else {
      qWarning() << "MainWindow: MQTT switch candidates list is empty";
    }
  }
#elif defined(RM_HAS_QT_MQTT)
  if (!m_mqttClient) {
    m_mqttClient = new MqttClient(m_gameData, this);
  } else {
    m_mqttClient->stop();
  }
  m_mqttClient->setClientId(candidates.first());
  m_mqttClient->start();
  m_activeMqttClientId = candidates.first();
#else
  Q_UNUSED(robotType);
#endif
}

// --- 显示模式 ---

void MainWindow::initializeLayout() {
  // 设置窗口属性
  setWindowTitle(ConfigManager::instance().getUIText("window_title"));

  // 设置焦点策略以接收键盘事件
  setFocusPolicy(Qt::StrongFocus);
  setFocus();
  setMouseTracking(true);

  // 获取屏幕分辨率并自动设置窗口大小
  QScreen *screen = QApplication::primaryScreen();
  QRect screenGeometry = screen->geometry();
  int screenWidth = screenGeometry.width();
  int screenHeight = screenGeometry.height();

  // 根据屏幕分辨率计算最佳窗口大小（使用85%的屏幕尺寸）
  int windowWidth = MainLayout::getCanvasWidth(screenWidth);
  int windowHeight = MainLayout::getCanvasHeight(screenHeight);

  // 设置窗口大小和最小尺寸限制 (从配置读取)
  int minWidth = ConfigManager::instance().getWindowMinWidth();
  int minHeight = ConfigManager::instance().getWindowMinHeight();
  setMinimumSize(minWidth, minHeight);

  // 存储屏幕信息供后续布局使用
  m_screenWidth = screenWidth;
  m_screenHeight = screenHeight;

  // 创建中央部件
  m_centralWidget = new QWidget(this);
  m_centralWidget->setMouseTracking(true);
  m_centralWidget->setAttribute(Qt::WA_TranslucentBackground);
  m_centralWidget->setStyleSheet("background-color: transparent;");
  setCentralWidget(m_centralWidget);

  // 创建视频背景 (位于最底层)
  m_videoBackground = new VideoBackgroundWidget(m_centralWidget);
  m_videoBackground->lower();
  if (m_videoBackground && m_gameData) {
    m_videoBackground->setCurrentRobotId(m_gameData->currentRobotId());
    connect(m_gameData, &GameData::myRobotUpdated, this, [this]() {
      if (!m_videoBackground || !m_gameData) {
        return;
      }
      m_videoBackground->setCurrentRobotId(m_gameData->currentRobotId());
    });
  }

  setupDevHooks();

  // 创建主布局
  m_mainLayout = new QVBoxLayout(m_centralWidget);
  m_mainLayout->setContentsMargins(0, 0, 0, 0);
  m_mainLayout->setSpacing(0);

  // 创建各区域
  createTopArea();    // 顶部2行布局
  createCenterArea(); // 中心战区信息

  // 创建比赛结算界面。挂到 MainWindow 这一层，避免被挂在 MainWindow
  // 下的 H/K 等 overlay 盖住。
  m_gameResultWidget = new GameResultWidget(this);
  m_gameResultWidget->hide(); // 默认隐藏

  // === RobotRespawn / Out QML 弹窗 ===
  // 使用统一的 PopupOverlay.qml 驱动所有弹窗显示；不再单独创建和控制分散的
  // QQuickWidget 实例。
  // 如果保留单独实例用于调试，可在短期内恢复，但生产分支应仅使用
  // m_qmlPopupOverlay。

  // === 统一弹窗 Overlay （PopupOverlay.qml） ===
  m_qmlPopupOverlay = new QQuickWidget(m_centralWidget);
  m_qmlPopupOverlay->setObjectName(QStringLiteral("popupOverlayWidget"));
  m_qmlPopupOverlay->setResizeMode(QQuickWidget::SizeRootObjectToView);
  m_qmlPopupOverlay->setFocusPolicy(Qt::StrongFocus);
  makeQQuickWidgetTransparent(m_qmlPopupOverlay);
  m_qmlPopupOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  if (m_gameData) {
    m_qmlPopupOverlay->rootContext()->setContextProperty("gameData",
                                                         m_gameData);
  }
  // 注入 network 对象到 PopupOverlay，使内部 QML 能调用 sendCommonCommand
  if (m_networkManager) {
    m_qmlPopupOverlay->rootContext()->setContextProperty("network",
                                                         m_networkManager);
  }
  m_qmlPopupOverlay->setSource(QUrl("qrc:/qml/PopupOverlay.qml"));
  m_qmlPopupOverlay->setGeometry(m_centralWidget->rect());
  m_qmlPopupOverlay->move(0, 0);
  m_qmlPopupOverlay->hide();
    // 防抖抬升——高频信号（activePopupsChanged / robotRespawnStatusUpdated）
    // 每秒触发数十次，避免每次都 raise() 导致闪烁。用单次 timer 合并请求。
    auto schedulePopupRaise = [this]() { scheduleOverlayLayerRestack(); };
    m_popupRaiseTimer = new QTimer(this);
    m_popupRaiseTimer->setSingleShot(true);
    connect(m_popupRaiseTimer, &QTimer::timeout, this,
            &MainWindow::restackOverlayLayers);

    // 检查是否需要显示 Overlay：activePopups 非空 或 复活弹窗活跃
    auto updateOverlayVisibility = [this, schedulePopupRaise]() {
      if (!m_qmlPopupOverlay) return;
      const bool hasActivePopups =
          m_gameData && !m_gameData->activePopups().isEmpty();
      const bool hasRespawnPending =
          m_gameData &&
          m_gameData->robotRespawnStatus()
              .value("is_pending_respawn", false)
              .toBool();
      const bool shouldShow =
          RM::PopupOverlayPolicy::shouldActivateOverlay(hasActivePopups,
                                                        hasRespawnPending);
      const bool wasShown = !m_qmlPopupOverlay->isHidden();
      m_qmlPopupOverlay->setAttribute(Qt::WA_TransparentForMouseEvents,
                                      !shouldShow);
      if (shouldShow) {
        if (!wasShown) {
          m_qmlPopupOverlay->show();
        }
        if (hasRespawnPending) {
          m_qmlPopupOverlay->setFocus(Qt::PopupFocusReason);
        }
        schedulePopupRaise();
      } else {
        if (wasShown) {
          m_qmlPopupOverlay->hide();
        }
      }
    };
    // 监听 activePopups 变化（PrepPhase / Countdown / BattlePause / Out）
    if (m_gameData) {
      connect(m_gameData, &GameData::activePopupsChanged, this,
              updateOverlayVisibility);
    }
    // 监听复活状态变化（RobotRespawn 已从 activePopups 解耦）
    if (m_gameData) {
      connect(m_gameData, &GameData::robotRespawnStatusUpdated, this,
              updateOverlayVisibility);
    }
  if (m_qmlPopupOverlay->status() == QQuickWidget::Error) {
    qWarning() << "PopupOverlay QML Errors:";
    for (const auto &error : m_qmlPopupOverlay->errors()) {
      qWarning() << "  " << error.toString();
    }
  }
}

// 有活动弹窗时保持 PopupOverlay 位于顶层。
void MainWindow::ensureOverlayRaisedIfActive() {
  if (!m_qmlPopupOverlay || !m_gameData)
    return;
  const bool hasActivePopups = !m_gameData->activePopups().isEmpty();
  const bool hasRespawnPending =
      m_gameData->robotRespawnStatus()
          .value("is_pending_respawn", false)
          .toBool();
  if (!RM::PopupOverlayPolicy::shouldActivateOverlay(hasActivePopups,
                                                     hasRespawnPending)) {
    m_qmlPopupOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_qmlPopupOverlay->hide();
    ensureGameResultRaisedIfVisible();
    ensureInteractivePanelsRaisedIfVisible();
    return;
  }
  m_qmlPopupOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, false);
  m_qmlPopupOverlay->show();
  if (hasRespawnPending) {
    m_qmlPopupOverlay->setFocus(Qt::PopupFocusReason);
  }
  scheduleOverlayLayerRestack();
}

void MainWindow::ensureGameResultRaisedIfVisible() {
  if (m_gameResultWidget && m_gameResultWidget->isVisible()) {
    m_gameResultWidget->raise();
  }
}

void MainWindow::updateGameResultWidgetGeometry() {
  if (!m_gameResultWidget) {
    return;
  }

  m_gameResultWidget->setResolutionViewport(popupResolutionViewport());
  const int x = (width() - m_gameResultWidget->width()) / 2;
  const int y = (height() - m_gameResultWidget->height()) / 2;
  m_gameResultWidget->move(x, y);
  ensureGameResultRaisedIfVisible();
}

void MainWindow::updateOfficialEventPopupGeometry() {
  if (!m_qmlOfficialEventPopupPanel || !m_centralWidget) {
    return;
  }

  const QRect anchorRect = (m_tacticalMode && m_qmlTacticalPage)
                               ? m_qmlTacticalPage->geometry()
                               : m_centralWidget->rect();
  m_qmlOfficialEventPopupPanel->setFixedSize(
      RM::PopupOverlayPolicy::scaledSize(QSize(640, 180),
                                         popupResolutionViewport()));
  const int panelX = anchorRect.center().x() -
                     m_qmlOfficialEventPopupPanel->width() / 2;
  const int panelY = anchorRect.center().y() -
                     m_qmlOfficialEventPopupPanel->height() / 2;

  m_qmlOfficialEventPopupPanel->move(panelX, panelY);
}

void MainWindow::updateTacticalTimedEventPopupGeometry() {
  if (m_tacticalTimedEventPopups.isEmpty() || !m_centralWidget) {
    return;
  }

  QVector<int> visibleIdx;
  for (int i = 0; i < m_tacticalTimedEventPopups.size(); ++i) {
    if (m_tacticalTimedEventPopups[i]->isVisible()) {
      visibleIdx.append(i);
    }
  }
  if (visibleIdx.isEmpty()) {
    return;
  }

  const QRect anchorRect = (m_tacticalMode && m_qmlTacticalPage)
                               ? m_qmlTacticalPage->geometry()
                               : m_centralWidget->rect();
  const QSize resolutionViewport = popupResolutionViewport();
  const double popupScale =
      RM::PopupOverlayPolicy::resolutionScale(resolutionViewport);
  const QSize popupSize = RM::PopupOverlayPolicy::scaledSize(
      QSize(540, 132), resolutionViewport);
  const int popupWidth = popupSize.width();
  const int popupHeight = popupSize.height();
  const int popupY = qRound(151.0 * popupScale);

  const int n = visibleIdx.size();
  const int gap = qMax(1, qRound(12.0 * popupScale));
  const int totalWidth = n * popupWidth + (n - 1) * gap;
  const int startX = anchorRect.center().x() - totalWidth / 2;

  for (int j = 0; j < n; ++j) {
    const int x = startX + j * (popupWidth + gap);
    m_tacticalTimedEventPopups[visibleIdx[j]]->setGeometry(x, popupY, popupWidth,
                                                           popupHeight);
  }
}

void MainWindow::ensureTacticalTimedEventPopupRaisedIfActive() {
  bool anyVisible = false;
  for (auto *popup : m_tacticalTimedEventPopups) {
    if (popup && popup->isVisible()) {
      anyVisible = true;
      break;
    }
  }
  if (!anyVisible) {
    return;
  }

  scheduleOverlayLayerRestack();
}

void MainWindow::scheduleOverlayLayerRestack() {
  if (m_popupRaiseTimer) {
    m_popupRaiseTimer->start(50);
  }
}

bool MainWindow::shouldRaiseTacticalPageAboveEventPopups() const {
  return m_tacticalMode && m_tacticalLargeMapMode && m_qmlTacticalPage &&
         m_qmlTacticalPage->isVisible();
}

void MainWindow::restackOverlayLayers() {
  const bool tacticalPageVisible =
      m_qmlTacticalPage && m_qmlTacticalPage->isVisible();
  const bool tacticalPageAboveEvents =
      shouldRaiseTacticalPageAboveEventPopups();

  if (tacticalPageVisible && !tacticalPageAboveEvents) {
    m_qmlTacticalPage->raise();
  }

  if (m_qmlOfficialEventPopupPanel && m_qmlOfficialEventPopupPanel->isVisible()) {
    m_qmlOfficialEventPopupPanel->raise();
  }
  for (auto *popup : m_tacticalTimedEventPopups) {
    if (popup && popup->isVisible()) {
      popup->raise();
    }
  }

  if (tacticalPageVisible && tacticalPageAboveEvents) {
    m_qmlTacticalPage->raise();
  }

  if (m_qmlPopupOverlay && m_qmlPopupOverlay->isVisible()) {
    m_qmlPopupOverlay->raise();
  }
  if (m_qmlRunePanel && m_qmlRunePanel->isVisible()) {
    m_qmlRunePanel->raise();
  }
  ensureGameResultRaisedIfVisible();
  ensureInteractivePanelsRaisedIfVisible();
}

QWidget *MainWindow::topInteractivePanel() const {
  const auto firstVisible = [](const std::initializer_list<QWidget *> &widgets)
      -> QWidget * {
    for (QWidget *widget : widgets) {
      if (widget && widget->isVisible()) {
        return widget;
      }
    }
    return nullptr;
  };

  return firstVisible({
      m_helpOverlay,
      m_qmlSettingsPanel,
      m_qmlDamagePanel,
      m_qmlTabPanel,
      m_qmlExchangePanel,
      m_qmlAmmoSupply17Panel,
      m_qmlAmmoSupply42Panel,
      m_miniMapLarge,
      m_miniMapLegendPanel,
      m_infoPanel,
      m_operationsPanel,
  });
}

void MainWindow::ensureInteractivePanelsRaisedIfVisible() {
  QWidget *panel = topInteractivePanel();
  if (!panel) {
    return;
  }

  QTimer::singleShot(0, this, [this, panel]() {
    if (!panel || !panel->isVisible()) {
      return;
    }
    panel->raise();
    if (panel == m_qmlSettingsPanel || panel == m_qmlExchangePanel ||
        panel == m_qmlAmmoSupply17Panel || panel == m_qmlAmmoSupply42Panel ||
        panel == m_qmlTabPanel ||
        panel == m_miniMapLarge ||
        panel == m_infoPanel || panel == m_operationsPanel) {
      panel->setFocus(Qt::PopupFocusReason);
    }
  });
}

void MainWindow::showTacticalTimedEventPopupForTest() {
  if (m_tacticalTimedEventPopups.isEmpty()) {
    return;
  }
  auto *popup = m_tacticalTimedEventPopups.first();

  m_tacticalTimedEventManualOverride = true;
  m_tacticalTimedEventManualOverrideUntilMs =
      QDateTime::currentMSecsSinceEpoch() + 6000;

  if (QObject *rootObject = tacticalTimedEventPopupRootObject(popup)) {
    rootObject->setProperty("eventTitle", QStringLiteral("战术事件预告"));
    rootObject->setProperty("triggerTimeText", QStringLiteral("6:30"));
    rootObject->setProperty("countdownSeconds", 5);
    rootObject->setProperty("eventLabel", QStringLiteral("可开启飞镖闸门"));
    rootObject->setProperty("countdownPrefix", QStringLiteral("还有"));
    const int displayToken =
        popup->property("displayToken").toInt() + 1;
    popup->setProperty("displayToken", displayToken);
  }

  updateTacticalTimedEventPopupGeometry();
  popup->show();
  ensureTacticalTimedEventPopupRaisedIfActive();

  const int displayToken =
      popup->property("displayToken").toInt();
  QTimer::singleShot(6000, this, [this, displayToken]() {
    if (!m_tacticalTimedEventPopups.isEmpty()) {
      auto *p = m_tacticalTimedEventPopups.first();
      if (p->property("displayToken").toInt() == displayToken) {
        p->hide();
        m_tacticalTimedEventManualOverride = false;
        m_tacticalTimedEventManualOverrideUntilMs = 0;
      }
    }
  });
}

void MainWindow::updateTacticalTimedEventPopupState() {
  if (m_tacticalTimedEventPopups.isEmpty() || !m_gameData) {
    return;
  }

  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  if (m_tacticalTimedEventManualOverride) {
    if (nowMs < m_tacticalTimedEventManualOverrideUntilMs) {
      updateTacticalTimedEventPopupGeometry();
      auto *popup = m_tacticalTimedEventPopups.first();
      if (!popup->isVisible()) {
        popup->show();
      }
      ensureTacticalTimedEventPopupRaisedIfActive();
      return;
    }
    m_tacticalTimedEventManualOverride = false;
    m_tacticalTimedEventManualOverrideUntilMs = 0;
  }

  const bool tacticalReady = m_tacticalMode &&
                             m_qmlTacticalPage &&
                             m_qmlTacticalPage->isVisible() &&
                             m_gameData->getCurrentStage() == GameStage::BATTLE;
  if (!tacticalReady) {
    for (int i = 0; i < m_tacticalTimedEventPopups.size(); ++i) {
      m_tacticalTimedEventPopups[i]->hide();
      m_tacticalTimedEventRuleKeys[i].clear();
      m_tacticalTimedEventCountdowns[i] = -1;
    }
    return;
  }

  const int currentGameTime = m_gameData->getGameTime();
  const int enemyOutpostHp = m_gameData->enemyOutpostHealth();
  const QVector<RM::TimedEventHit> hits =
      RM::tryMatchTimedEvents(currentGameTime, enemyOutpostHp);

  const int popupCount = m_tacticalTimedEventPopups.size();
  const int hitCount = qMin(hits.size(), popupCount);

  for (int i = 0; i < popupCount; ++i) {
    if (i < hitCount) {
      const auto &hit = hits[i];
      const bool enteringRule =
          !m_tacticalTimedEventPopups[i]->isVisible() ||
          m_tacticalTimedEventRuleKeys[i] != hit.key;
      const bool needsUpdate =
          enteringRule ||
          m_tacticalTimedEventCountdowns[i] != hit.countdownSeconds;
      if (needsUpdate) {
        if (QObject *rootObject = tacticalTimedEventPopupRootObject(
                m_tacticalTimedEventPopups[i])) {
          rootObject->setProperty("eventTitle", QStringLiteral("战术事件预告"));
          rootObject->setProperty("triggerTimeText", hit.triggerTimeText);
          rootObject->setProperty("countdownSeconds", hit.countdownSeconds);
          rootObject->setProperty("eventLabel", hit.label);
          rootObject->setProperty("countdownPrefix", QStringLiteral("还有"));
        }
        m_tacticalTimedEventRuleKeys[i] = hit.key;
        m_tacticalTimedEventCountdowns[i] = hit.countdownSeconds;
        if (enteringRule) {
          playSoundFromResourceFolder(hit.soundFileName);
        }
      }
      if (!m_tacticalTimedEventPopups[i]->isVisible()) {
        m_tacticalTimedEventPopups[i]->show();
      }
    } else {
      m_tacticalTimedEventPopups[i]->hide();
      m_tacticalTimedEventRuleKeys[i].clear();
      m_tacticalTimedEventCountdowns[i] = -1;
    }
  }

  updateTacticalTimedEventPopupGeometry();
  ensureTacticalTimedEventPopupRaisedIfActive();
}

void MainWindow::updateRunePanelVisibility() {
  if (!m_qmlRunePanel || !m_gameData || m_qmlRunePanel->status() != QQuickWidget::Ready) {
    return;
  }

  const bool shouldBeVisible =
      m_gameData->getCurrentStage() == GameStage::BATTLE &&
      isInfantryRobotId(m_gameData->currentRobotId());

  if (shouldBeVisible) {
    if (!m_qmlRunePanel->isVisible()) {
      m_qmlRunePanel->show();
    }
    m_qmlRunePanel->raise();
  } else {
    m_qmlRunePanel->hide();
  }
}

void MainWindow::onRunePanelActivateRequested() {
  if (!m_gameData || !m_networkManager) {
    return;
  }

  if (m_gameData->getCurrentStage() != GameStage::BATTLE) {
    return;
  }

  if (!isInfantryRobotId(m_gameData->currentRobotId())) {
    return;
  }

  m_networkManager->sendRuneActivate(1);

  if (m_qmlRunePanel && m_qmlRunePanel->status() == QQuickWidget::Ready) {
    if (QObject *rootObj = m_qmlRunePanel->rootObject()) {
      QMetaObject::invokeMethod(rootObj, "markActivated");
    }
  }
}

/**
 * @brief 创建顶部HUD区域
 *
 * 实现官方RoboMaster 2025标准的2行布局：
 * 第1行：基地血条 + 轮次/时间 + 比分 + 经济
 * 第2行：机器人缩略图
 *
 * 布局特点：
 * - 红方在左，蓝方在右，中央为比赛信息
 * - 基地血条显示5000血量，带校徽标识
 * - 中间区域包含轮次、倒计时、比分、经济资源
 * - 机器人缩略图显示5个机器人状态（红方#1-5，蓝方#101-105）
 */
void MainWindow::createTopArea() {
  // 顶部区域 - 使用封装好的 TopInfoBar
  m_topArea = new QWidget(m_centralWidget);
  m_topArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  int topAreaHeight = MainLayout::getTopAreaHeight(m_screenHeight);
  m_topArea->setFixedHeight(topAreaHeight);

  // 移除背景色设置，由 TopInfoBar 处理
  m_topArea->setStyleSheet("background-color: transparent;");
  m_topArea->setAttribute(Qt::WA_TranslucentBackground);
  m_topArea->setAttribute(Qt::WA_NoSystemBackground);

  QVBoxLayout *topMainLayout = new QVBoxLayout(m_topArea);
  topMainLayout->setContentsMargins(0, 0, 0, 0);
  topMainLayout->setSpacing(0);

  // --- QML TopInfoBar (V2.0) ---
  m_qmlTopInfoBar = new QQuickWidget(m_topArea);
  m_qmlTopInfoBar->setResizeMode(QQuickWidget::SizeRootObjectToView);
  makeQQuickWidgetTransparent(m_qmlTopInfoBar);

  // 设置 gameData 上下文属性用于数据绑定
  if (m_gameData) {
    m_qmlTopInfoBar->rootContext()->setContextProperty("gameData", m_gameData);
  }

  // 加载 QML 源文件
  m_qmlTopInfoBar->setSource(QUrl("qrc:/qml/TopInfoBar.qml"));

  // 检查加载错误
  if (m_qmlTopInfoBar->status() == QQuickWidget::Error) {
    qWarning() << "TopInfoBar QML Errors:";
    for (const auto &error : m_qmlTopInfoBar->errors()) {
      qWarning() << "  " << error.toString();
    }
  }

  topMainLayout->addWidget(m_qmlTopInfoBar);

  // 调试按钮 (覆盖层)
  QPushButton *debugBtn = new QPushButton(m_topArea);
  debugBtn->setIcon(QIcon(":/icons/debug.png"));
  debugBtn->move(m_screenWidth - 40, 10);

  connect(debugBtn, &QPushButton::clicked, [this]() {
    if (m_debugLogWidget->isVisible()) {
      m_debugLogWidget->hide();
    } else {
      m_debugLogWidget->show();
      m_debugLogWidget->raise();
      ensureOverlayRaisedIfActive();
    }
  });

  m_mainLayout->addWidget(m_topArea);
}

/**
 * @brief 创建中心信息区域
 *
 * 实现三列布局，显示比赛的关键信息：
 * 左列：系统信息 - 显示空投时间、系统状态等重要通知
 * 中列：战斗状态 - 显示当前战局状态，如堡垒区占领情况
 * 右列：机器人状态 - 显示当前选中机器人的详细信息
 *
 * 设计原则：
 * - 左右两列固定宽度350px，中列自适应
 * - 使用半透明黑色背景，确保信息清晰可读
 * - 黄色标题，白色内容，红色警告信息
 */
void MainWindow::createCenterArea() {
  // 中心区域 - 包含中央增益点、准星和热量环
  m_centerArea = new QWidget(m_centralWidget);
  m_centerArea->setAttribute(Qt::WA_TranslucentBackground);
  m_centerArea->setStyleSheet("background: transparent;");
  m_centerArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  // 背景透明，由VideoBackgroundWidget提供视觉背景
  m_centerArea->setStyleSheet("background-color: transparent;");
  m_centerArea->setAttribute(Qt::WA_TranslucentBackground);

  // 使用网格布局来叠加控件
  QGridLayout *centerLayout = new QGridLayout(m_centerArea);
  centerLayout->setContentsMargins(0, 0, 0, 0);
  centerLayout->setSpacing(0);
  // 配置行列拉伸，确保中心区域居中
  centerLayout->setRowStretch(0, 1);    // 顶部留白
  centerLayout->setRowStretch(1, 0);    // 中心行 (不拉伸)
  centerLayout->setRowStretch(2, 1);    // 底部留白
  centerLayout->setColumnStretch(0, 1); // 左侧留白
  centerLayout->setColumnStretch(1, 0); // 中心列 (不拉伸)
  centerLayout->setColumnStretch(2, 1); // 右侧留白

  // 0. 视频背景已在 initializeLayout 中移动，以覆盖全屏

#ifdef RM_HAS_AR_OVERLAY
  // 0.5 AR 叠加层 (在视频之上，其他控件之下)
  if (m_arManager) {
    m_arWidget = new AROverlayWidget(m_centerArea);
    // 初始大小跟随 centerArea，后续由 resizeEvent 维护
    m_arWidget->resize(m_centerArea->size());
    // 绑定管理器
    m_arManager->setOverlayWidget(m_arWidget);
    // 默认隐藏，等待初始化成功且配置开启
    m_arWidget->hide();

    // 模型不随源码提供；只有配置了明确路径时才初始化可选 AR 能力。
    const QString modelPath = ConfigManager::instance().getARModelPath().trimmed();
    if (!modelPath.isEmpty()) {
      m_arManager->initialize(modelPath);
    } else {
      qInfo() << "AR overlay model is not configured; initialization skipped";
    }

    // 应用配置
    onARSettingsChanged();
  }
#endif

  // 2. 准星和热量环容器 (位于中心)
  // 创建一个容器来容纳准星和热量环，确保它们重叠
  QWidget *aimingContainer = new QWidget(m_centerArea);
  aimingContainer->setFixedSize(860, 600); // 额外宽度用于外环右侧射击信息
  aimingContainer->setStyleSheet("background: transparent;");

  QGridLayout *aimingLayout = new QGridLayout(aimingContainer);
  aimingLayout->setContentsMargins(0, 0, 0, 0);
  aimingLayout->setSpacing(0);

  // FPS 和 Ping 标签 (浮动，无背景，小字体) - 默认隐藏

  m_pingLabel = new QLabel("Ping: 15ms", this);
  m_pingLabel->setStyleSheet("color: #00FF00; font-size: 10px; background: "
                             "transparent; font-weight: bold;");
  m_pingLabel->setGeometry(80, 5, 100, 15); // 移至左上角
  m_pingLabel->setVisible(false);           // 隐藏

  m_networkLabel = new QLabel("NET: OK", this);
  m_networkLabel->setStyleSheet("color: #00FF00; font-size: 10px; background: "
                                "transparent; font-weight: bold;");
  m_networkLabel->setGeometry(160, 5, 100, 15);
  m_networkLabel->setVisible(false); // 隐藏

  m_serverLabel = new QLabel("Server: Local", this);
  m_serverLabel->setStyleSheet("color: #CCCCCC; font-size: 10px; background: "
                               "transparent; font-weight: bold;");
  m_serverLabel->setGeometry(240, 5, 150, 15);
  m_serverLabel->setVisible(false); // 隐藏
  // 中央瞄准 HUD (统一 QML 组件) - 包含准星、热量环、射击信息
  m_qmlCentralAimingHUD = new QQuickWidget(aimingContainer);
  m_qmlCentralAimingHUD->setResizeMode(QQuickWidget::SizeRootObjectToView);
  m_qmlCentralAimingHUD->setAttribute(Qt::WA_TranslucentBackground);
  m_qmlCentralAimingHUD->setClearColor(Qt::transparent);
  m_qmlCentralAimingHUD->rootContext()->setContextProperty("gameData",
                                                           m_gameData);
  m_qmlCentralAimingHUD->setSource(QUrl("qrc:/qml/CentralAimingHUD.qml"));
  m_qmlCentralAimingHUD->setFixedSize(860, 600);
  aimingLayout->addWidget(m_qmlCentralAimingHUD, 0, 0, Qt::AlignCenter);

  // 使用绝对定位确保准星区域在屏幕正中央
  // 不再依赖 grid layout，避免被其他元素影响
  // 初始位置将在 resizeEvent 中更新
  aimingContainer->setParent(m_centerArea);
  m_aimingContainer = aimingContainer; // 保存引用用于 resizeEvent

  // 2.5 战场消息提示（位于准星上方）
  m_battleMessage = new BattleMessageWidget(m_gameData, m_centerArea);
  m_battleMessage->raise(); // 确保在最上层
  ensureOverlayRaisedIfActive();

  // 2.6 事件消息提示（大能量机关激活 / 飞镖命中时显示）
  m_qmlEventMessagePanel = new QQuickWidget(m_centerArea);
  m_qmlEventMessagePanel->setResizeMode(
      QQuickWidget::SizeRootObjectToView);
  makeQQuickWidgetTransparent(m_qmlEventMessagePanel);
  if (m_gameData && m_qmlEventMessagePanel->rootContext()) {
    m_qmlEventMessagePanel->rootContext()->setContextProperty(
        "gameData", m_gameData);
  }
  m_qmlEventMessagePanel->setSource(QUrl("qrc:/qml/EventMessagePanel.qml"));
  makeQQuickWidgetTransparent(m_qmlEventMessagePanel);
  m_qmlEventMessagePanel->setFixedSize(420, 96);

  if (m_qmlEventMessagePanel->status() == QQuickWidget::Error) {
    qWarning() << "EventMessagePanel QML Errors:";
    for (const auto &error : m_qmlEventMessagePanel->errors()) {
      qWarning() << "  " << error.toString();
    }
  }
  m_qmlEventMessagePanel->hide();

  // 2.6.1 空中机器人官方事件提示（战术屏风格）
  m_qmlOfficialEventPopupPanel = new QQuickWidget(m_centralWidget);
  m_qmlOfficialEventPopupPanel->setObjectName(
      QStringLiteral("officialEventPopupWidget"));
  m_qmlOfficialEventPopupPanel->setResizeMode(
      QQuickWidget::SizeRootObjectToView);
  m_qmlOfficialEventPopupPanel->setAttribute(Qt::WA_TransparentForMouseEvents);
  makeQQuickWidgetTransparent(m_qmlOfficialEventPopupPanel);
  if (m_gameData && m_qmlOfficialEventPopupPanel->rootContext()) {
    m_qmlOfficialEventPopupPanel->rootContext()->setContextProperty(
        "gameData", m_gameData);
  }
  m_qmlOfficialEventPopupPanel->setSource(
      QUrl("qrc:/qml/OfficialEventPopup.qml"));
  makeQQuickWidgetTransparent(m_qmlOfficialEventPopupPanel);
  m_qmlOfficialEventPopupPanel->setFixedSize(640, 180);
  if (m_qmlOfficialEventPopupPanel->status() == QQuickWidget::Error) {
    qWarning() << "OfficialEventPopup QML Errors:";
    for (const auto &error : m_qmlOfficialEventPopupPanel->errors()) {
      qWarning() << "  " << error.toString();
    }
  }
  m_qmlOfficialEventPopupPanel->hide();
  updateOfficialEventPopupGeometry();

  // 2.7 云台手副屏自定义数据叠加层 (GunnerOverlay)
  m_qmlGunnerOverlay = new QQuickWidget(m_centerArea);
  m_qmlGunnerOverlay->setResizeMode(QQuickWidget::SizeRootObjectToView);
  m_qmlGunnerOverlay->setAttribute(Qt::WA_TranslucentBackground);
  m_qmlGunnerOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
  m_qmlGunnerOverlay->setClearColor(Qt::transparent);
  m_qmlGunnerOverlay->setStyleSheet("background: transparent;");
  if (m_gameData && m_qmlGunnerOverlay->rootContext()) {
    m_qmlGunnerOverlay->rootContext()->setContextProperty("gameData", m_gameData);
  }
  m_qmlGunnerOverlay->setSource(QUrl("qrc:/qml/CustomUI/GunnerOverlay.qml"));
  if (m_centerArea) {
    m_qmlGunnerOverlay->setGeometry(m_centerArea->rect());
  }
  m_qmlGunnerOverlay->lower();

  // 2.8 战术指挥屏 (Ctrl+T 切换；内部布局用 Command+Y/Ctrl+Y 切换)
  if (!m_tacticalAnalyzer && m_gameData) {
    m_tacticalAnalyzer = new RM::TacticalAnalyzer(m_gameData, this);
    auto *freshness = new RM::DataFreshnessGuard(this);
    auto *mapper = new RM::MapCoordinateMapper(this);
    auto *ranker = new RM::ThreatRanker(this);
    auto *fusion = new RM::ExecutionFusion(m_gameData, this);
    m_tacticalAnalyzer->setFreshnessGuard(freshness);
    m_tacticalAnalyzer->setCoordMapper(mapper);
    m_tacticalAnalyzer->setThreatRanker(ranker);
    m_tacticalAnalyzer->setExecutionFusion(fusion);
    m_tacticalAnalyzer->setUseMockData(qEnvironmentVariableIsSet("RM_TACTICAL_MOCK"));
    connect(m_tacticalAnalyzer, &RM::TacticalAnalyzer::enemyPaidRespawnDetected,
            this, [this](int robotId) {
              const QString fileName = paidRespawnSoundFileName(robotId);
              qInfo() << "[PaidRespawnVoice] enemy robot revived by buyback"
                      << "robotId=" << robotId << "file=" << fileName;
              playSoundFromResourceFolder(fileName);
            });
    m_tacticalAnalyzer->start(100);
  }

  m_qmlTacticalPage = new QQuickWidget(m_centralWidget);
  m_qmlTacticalPage->setObjectName(QStringLiteral("tacticalPageWidget"));
  m_qmlTacticalPage->setResizeMode(QQuickWidget::SizeRootObjectToView);
  makeQQuickWidgetOpaque(m_qmlTacticalPage, QColor(2, 7, 13));
  if (m_gameData && m_qmlTacticalPage->rootContext()) {
    m_qmlTacticalPage->rootContext()->setContextProperty("gameData", m_gameData);
  }
  if (m_gameData && m_qmlTacticalPage->engine()) {
    m_qmlTacticalPage->engine()->addImageProvider(
        QStringLiteral("herovideo"), new HeroVideoImageProvider(m_gameData));
  }
  if (m_tacticalAnalyzer && m_qmlTacticalPage->rootContext()) {
    m_qmlTacticalPage->rootContext()->setContextProperty("tacticalAnalyzer",
                                                         m_tacticalAnalyzer);
  }
  if (m_qmlTacticalPage->rootContext()) {
    m_qmlTacticalPage->rootContext()->setContextProperty("mainWindow", this);
  }
  m_qmlTacticalPage->setSource(QUrl("qrc:/qml/Tactical/TacticalCommandPage.qml"));
  if (QObject *tacticalRoot = m_qmlTacticalPage->rootObject()) {
    tacticalRoot->setProperty(
        "heroCameraGridVisible",
        ConfigManager::instance().getIndustrialCameraGridEnabled());
  }
  connect(&ConfigManager::instance(), &ConfigManager::configReloaded, this,
          [this]() {
            if (m_qmlTacticalPage && m_qmlTacticalPage->rootObject()) {
              m_qmlTacticalPage->rootObject()->setProperty(
                  "heroCameraGridVisible",
                  ConfigManager::instance().getIndustrialCameraGridEnabled());
            }
          });
  if (m_centralWidget) {
    m_qmlTacticalPage->setGeometry(m_centralWidget->rect());
  }
  if (m_qmlTacticalPage->status() == QQuickWidget::Error) {
    qWarning() << "TacticalCommandPage QML Errors:";
    for (const auto &error : m_qmlTacticalPage->errors()) {
      qWarning() << "  " << error.toString();
    }
  }
  m_qmlTacticalPage->hide();  // 默认隐藏，Tab切换显示

  m_tacticalLoginButton =
      new QPushButton(QStringLiteral("登录"), m_centralWidget);
  m_tacticalLoginButton->setObjectName(QStringLiteral("tacticalLoginButton"));
  m_tacticalLoginButton->setCursor(Qt::PointingHandCursor);
  m_tacticalLoginButton->setFocusPolicy(Qt::StrongFocus);
#ifdef Q_OS_LINUX
  m_tacticalLoginButton->setAttribute(Qt::WA_TransparentForMouseEvents);
#endif
  m_tacticalLoginButton->setStyleSheet(QStringLiteral(
      "QPushButton {"
      "  color: #effbff;"
      "  background-color: #0d3248;"
      "  border: 1px solid #4fd8ff;"
      "  border-radius: 6px;"
      "  font-size: 11px;"
      "  font-weight: 600;"
      "  padding: 0 14px;"
      "}"
      "QPushButton:hover { background-color: #164f6d; }"
      "QPushButton:pressed { background-color: #1e7aa5; }"));
  connect(m_tacticalLoginButton, &QPushButton::pressed, this, [this]() {
    qInfo() << "[tactical-login] button pressed"
            << "geometry=" << m_tacticalLoginButton->geometry()
            << "visible=" << m_tacticalLoginButton->isVisible()
            << "enabled=" << m_tacticalLoginButton->isEnabled()
            << "focusWidget=" << QApplication::focusWidget();
  });
  connect(m_tacticalLoginButton, &QPushButton::released, this, [this]() {
    qInfo() << "[tactical-login] button released"
            << "focusWidget=" << QApplication::focusWidget();
  });
  connect(m_tacticalLoginButton, &QPushButton::clicked, this, [this]() {
    qInfo() << "[tactical-login] button clicked"
            << "focusWidget=" << QApplication::focusWidget();
  });
  connect(m_tacticalLoginButton, &QPushButton::clicked, this,
          &MainWindow::toggleSettingsPanel);
  updateTacticalLoginButtonGeometry();
  m_tacticalLoginButton->hide();

  constexpr int kMaxTacticalTimedEventPopups = 3;
  m_tacticalTimedEventPopups.reserve(kMaxTacticalTimedEventPopups);
  m_tacticalTimedEventRuleKeys.resize(kMaxTacticalTimedEventPopups);
  m_tacticalTimedEventCountdowns.resize(kMaxTacticalTimedEventPopups);
  for (int i = 0; i < kMaxTacticalTimedEventPopups; ++i) {
    auto *popup = new QQuickWidget(m_centralWidget);
    popup->setObjectName(QStringLiteral("tacticalTimedEventPopup%1").arg(i));
    popup->setResizeMode(QQuickWidget::SizeRootObjectToView);
    popup->setAttribute(Qt::WA_TransparentForMouseEvents);
    makeQQuickWidgetTransparent(popup);
    if (m_gameData && popup->rootContext()) {
      popup->rootContext()->setContextProperty("gameData", m_gameData);
    }
    popup->setSource(QUrl("qrc:/qml/Tactical/TacticalEventPopup.qml"));
    makeQQuickWidgetTransparent(popup);
    if (popup->status() == QQuickWidget::Error) {
      qWarning() << "TacticalEventPopup QML Errors (slot" << i << "):";
      for (const auto &error : popup->errors()) {
        qWarning() << "  " << error.toString();
      }
    }
    popup->hide();
    m_tacticalTimedEventPopups.append(popup);
    m_tacticalTimedEventRuleKeys[i].clear();
    m_tacticalTimedEventCountdowns[i] = -1;
  }

  // 3. 侧边栏区域
  createSideAreas();

  if (m_leftSideArea) {
    centerLayout->addWidget(m_leftSideArea, 0, 0, 3, 1,
                            Qt::AlignLeft | Qt::AlignTop);
  }

  if (m_rightSideArea) {
    centerLayout->addWidget(m_rightSideArea, 0, 2, 3, 1,
                            Qt::AlignRight | Qt::AlignVCenter);
  }

  // 4. 小地图区域 (右下角)
  m_rightBottomContainer = new QWidget(m_centerArea);
  m_rightBottomContainer->setAttribute(Qt::WA_TranslucentBackground);
  m_rightBottomContainer->setStyleSheet("background: transparent;");
  m_rightBottomContainer->setFixedWidth(300); // 稍宽以便显示信息
  QVBoxLayout *rightBottomLayout = new QVBoxLayout(m_rightBottomContainer);
  rightBottomLayout->setContentsMargins(0, 0, 20, 8);
  rightBottomLayout->setSpacing(5);

  // 小地图 (QML)
  m_miniMap = new QQuickWidget(m_rightBottomContainer);
  m_miniMap->setResizeMode(QQuickWidget::SizeRootObjectToView);
  m_miniMap->setClearColor(QColor(24, 28, 32));
  m_miniMap->setStyleSheet("border: none;");
  if (m_gameData && m_miniMap->rootContext()) {
    m_miniMap->rootContext()->setContextProperty("gameData", m_gameData);
    m_miniMap->rootContext()->setContextProperty("mainWindow", this);
  }
  m_miniMap->setSource(QUrl("qrc:/qml/MiniMap.qml"));
  if (m_miniMap->status() == QQuickWidget::Error) {
    qWarning() << "MiniMap QML Errors:";
    for (const auto &error : m_miniMap->errors()) {
      qWarning() << "  " << error.toString();
    }
  }
  m_miniMap->setFixedSize(270, 160);
  m_miniMap->setFocusPolicy(Qt::StrongFocus);
  m_miniMap->setFocus();
  rightBottomLayout->addWidget(m_miniMap, 0, Qt::AlignRight);

  // 英雄视频浮层：直接绘制最近一帧，避免 QML Image 每帧重载 data URL 导致闪动。
  m_heroVideoWidget = new HeroVideoWidget(m_centralWidget);
  m_heroVideoWidget->setFixedSize(300, 300);
  m_heroVideoWidget->setStyleSheet("border: none;");
  m_heroVideoWidget->hide();
  m_heroVideoWidget->raise();

  // 大地图 (QML, 预先初始化但隐藏)
  m_miniMapLarge = new QQuickWidget(this);
  m_miniMapLarge->setResizeMode(QQuickWidget::SizeRootObjectToView);
  m_miniMapLarge->setClearColor(QColor(24, 28, 32));
  m_miniMapLarge->setStyleSheet("border: none;");
  if (m_gameData && m_miniMapLarge->rootContext()) {
    m_miniMapLarge->rootContext()->setContextProperty("gameData", m_gameData);
    m_miniMapLarge->rootContext()->setContextProperty("mainWindow", this);
  }
  m_miniMapLarge->setSource(QUrl("qrc:/qml/MiniMap.qml"));
  if (m_miniMapLarge->status() == QQuickWidget::Error) {
    qWarning() << "MiniMapLarge QML Errors:";
    for (const auto &error : m_miniMapLarge->errors()) {
      qWarning() << "  " << error.toString();
    }
  }
  m_miniMapLarge->setFixedSize(810, 480); // 放大显示，保持原始比例
  m_miniMapLarge->setFocusPolicy(Qt::StrongFocus);
  m_miniMapLarge->setFocus();
  m_miniMapLarge->hide();
  m_miniMapLarge->raise();
  ensureOverlayRaisedIfActive();
  if (QObject *root = m_miniMapLarge->rootObject()) {
    connectMiniMapClickSignal(root, this, "handleMiniMapClick(qreal,qreal)");
  }

  // 将所有地图加入列表统一管理
  m_miniMaps << m_miniMap << m_miniMapLarge;
  miniMapSetInteractionEnabled(m_miniMap, false);
  miniMapSetInteractionEnabled(m_miniMapLarge, true);
  syncMiniMapCommandState();

  if (m_heroVideoWidget && m_gameData) {
    m_heroVideoWidget->setCurrentRobotId(m_gameData->currentRobotId());
    connect(m_gameData, &GameData::robotDataUpdated, this, [this](int) {
      if (!m_heroVideoWidget || !m_gameData) {
        return;
      }
      if (m_tacticalMode && !m_dartHitHeroDeployCamActive) {
        m_heroVideoWidget->hide();
        return;
      }
      m_heroVideoWidget->setCurrentRobotId(m_gameData->currentRobotId());
    });
    connect(m_gameData, &GameData::heroFrameUpdated, this, [this]() {
      if (!m_heroVideoWidget || !m_gameData) {
        return;
      }
      // 战术指挥屏由 QML image provider 直接显示最新帧。不要同时刷新一个
      // 被压在 QML 页面下方的 QWidget，避免同一帧重复绘制。
      if (m_tacticalMode && !m_dartHitHeroDeployCamActive) {
        return;
      }
      m_heroVideoWidget->setFrame(m_gameData->heroFrame());
    });
    connect(m_heroVideoWidget, &HeroVideoWidget::visibilityChanged, this,
            &MainWindow::onHeroVisibilityChanged, Qt::UniqueConnection);
  }

  m_miniMapLegendPanel = new QWidget(this);
  m_miniMapLegendPanel->setObjectName("miniMapLegendPanel");
  m_miniMapLegendPanel->setAttribute(Qt::WA_TranslucentBackground);
  m_miniMapLegendPanel->setAttribute(Qt::WA_StyledBackground, true);
  m_miniMapLegendPanel->setFixedSize(148, 98);
  m_miniMapLegendPanel->hide();
  m_miniMapLegendPanel->setStyleSheet(
      "#miniMapLegendPanel { background: rgba(15, 28, 34, 220);"
      " border: 1px solid rgba(90, 116, 126, 220); border-radius: 8px; }");
  auto *legendShadow = new QGraphicsDropShadowEffect(m_miniMapLegendPanel);
  legendShadow->setBlurRadius(18);
  legendShadow->setOffset(0, 6);
  legendShadow->setColor(QColor(0, 0, 0, 120));
  m_miniMapLegendPanel->setGraphicsEffect(legendShadow);

  auto createLegendRow = [this](const QString &iconPath, const QString &text,
                                QWidget *parent) -> QWidget * {
    QWidget *row = new QWidget(parent);
    row->setFixedHeight(24);
    QHBoxLayout *layout = new QHBoxLayout(row);
    layout->setContentsMargins(8, 2, 8, 2);
    layout->setSpacing(6);

    QLabel *iconLabel = new QLabel(row);
    iconLabel->setFixedSize(16, 16);
    iconLabel->setPixmap(QPixmap(iconPath).scaled(
        iconLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QLabel *textLabel = new QLabel(text, row);
    textLabel->setStyleSheet(
        "background: transparent; border: none; color: #EAF5FA;"
        " font-family: 'Microsoft YaHei'; font-size: 12px; font-weight: 700;");

    layout->addWidget(iconLabel);
    layout->addWidget(textLabel, 1);
    return row;
  };

  QVBoxLayout *legendLayout = new QVBoxLayout(m_miniMapLegendPanel);
  legendLayout->setContentsMargins(6, 8, 6, 8);
  legendLayout->setSpacing(4);
  m_miniMapLegendAttackRow = createLegendRow(
      ":/images/minimap/ic_fpv_map_cursor_attack_1.png", "A: 攻击",
      m_miniMapLegendPanel);
  m_miniMapLegendWarningRow = createLegendRow(
      ":/images/minimap/ic_fpv_map_cursor_warning_1.png", "B: 警戒",
      m_miniMapLegendPanel);
  m_miniMapLegendDefenseRow = createLegendRow(
      ":/images/minimap/ic_fpv_map_cursor_defense_1.png", "I: 防御",
      m_miniMapLegendPanel);
  legendLayout->addWidget(m_miniMapLegendAttackRow);
  legendLayout->addWidget(m_miniMapLegendWarningRow);
  legendLayout->addWidget(m_miniMapLegendDefenseRow);
  updateMiniMapLegendSelection();
  updateMiniMapLegendPanel();

  // 键鼠信息 (触发/控制)
  auto createInputBar = [](const QString &label, int value,
                           QWidget *parent) -> QProgressBar * {
    QWidget *w = new QWidget(parent);
    w->setAttribute(Qt::WA_TranslucentBackground);
    w->setStyleSheet("background: transparent;");
    QHBoxLayout *l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(5);

    QLabel *lbl = new QLabel(label, w);
    lbl->setFixedWidth(30);
    lbl->setStyleSheet("color: #AAAAAA; font-size: 10px;");

    QProgressBar *bar = new QProgressBar(w);
    bar->setFixedHeight(6);
    bar->setTextVisible(false);
    bar->setRange(0, 100);
    bar->setValue(value);
    bar->setStyleSheet(
        "QProgressBar { background: #333; border: none; border-radius: 3px; } "
        "QProgressBar::chunk { background: #00AAFF; border-radius: 3px; }");

    l->addWidget(lbl);
    l->addWidget(bar);
    return bar;
  };

  m_triggerBar = createInputBar("触发", 0, m_rightBottomContainer);
  rightBottomLayout->addWidget(m_triggerBar->parentWidget());

  m_controlBar = createInputBar("控制", 0, m_rightBottomContainer);
  rightBottomLayout->addWidget(m_controlBar->parentWidget());

  centerLayout->addWidget(m_rightBottomContainer, 2, 2,
                          Qt::AlignRight | Qt::AlignBottom);

  m_miniMap->show();


  // 启动时按 SettingsPanel 当前选择同步视角机器人，避免固定写死红方 1 号。
  m_gameData->setCurrentRobotId(
      robotIdFromSettingsType(m_selectedRobotTypeFromSettings));
  // 触发面板与小地图的刷新，读取 GameData 并同步高亮
  updateCurrentRobotPanel();

  // 5. 布局伸缩设置
  centerLayout->setColumnStretch(0, 1);
  centerLayout->setColumnStretch(2, 1);
  centerLayout->setRowStretch(1, 1);

  // 左下角复合面板
  // 左下角复合面板
  // 使用 QML 实现 LeftBottomPanel
  m_leftBottomPanel = new QQuickWidget(m_centerArea);
  m_leftBottomPanel->setResizeMode(QQuickWidget::SizeViewToRootObject);
  makeQQuickWidgetTransparent(m_leftBottomPanel);

  // 绑定 GameData context - 必须在 setSource 之前设置
  if (m_gameData && m_leftBottomPanel->rootContext()) {
    m_leftBottomPanel->rootContext()->setContextProperty("gameData",
                                                         m_gameData);
  }

  m_leftBottomPanel->setSource(QUrl("qrc:/qml/LeftBottomPanel.qml"));

  // [关键] setSource 后再次设置透明度 (quickWindow 现已可用)
  makeQQuickWidgetTransparent(m_leftBottomPanel);

  if (m_leftBottomPanel->status() == QQuickWidget::Error) {
    qWarning() << "LeftBottomPanel QML Errors:";
    for (const auto &error : m_leftBottomPanel->errors()) {
      qWarning() << "  " << error.toString();
    }
  }
  // 通过 contextProperty 绑定 GameData，无需手动调用 updateCurrentRobotPanel。
  // 绑定 GameData context 移至 initializeCore
  // 上下文属性稍后设置

  // 设置左下角机器人面板的最小尺寸和外边距
  m_leftBottomPanel->setMinimumSize(400, 200);
  m_leftBottomPanel->setContentsMargins(0, 0, 0, 0);

  // 设置 row 2 的最小高度以确保面板可见
  centerLayout->setRowMinimumHeight(2, 200); // 300
  centerLayout->addWidget(m_leftBottomPanel, 2, 0,
                          Qt::AlignLeft | Qt::AlignBottom);

  // 消息通知面板 (LeftBottomPanel 上方)
  QQuickWidget *messagePanel = new QQuickWidget(m_centerArea);
  messagePanel->setResizeMode(QQuickWidget::SizeViewToRootObject);
  makeQQuickWidgetTransparent(messagePanel);

  if (m_gameData && messagePanel->rootContext()) {
    messagePanel->rootContext()->setContextProperty("gameData", m_gameData);
  }

  messagePanel->setSource(QUrl("qrc:/qml/MessageNotificationPanel.qml"));

  // [关键] setSource 后再次设置透明度
  makeQQuickWidgetTransparent(messagePanel);

  if (messagePanel->status() == QQuickWidget::Error) {
    qWarning() << "MessageNotificationPanel QML Errors:";
    for (const auto &error : messagePanel->errors()) {
      qWarning() << "  " << error.toString();
    }
  }

  // 添加到布局第 1 行第 0 列 (LeftBottomPanel 上方)
  // 设置底部边距让消息面板紧靠机器人区域
  centerLayout->addWidget(messagePanel, 1, 0, Qt::AlignLeft | Qt::AlignBottom);

  // 准备阶段/倒计时弹窗由统一的 PopupOverlay 管理，不在此处单独创建
  // QQuickWidget 实例


  // 默认隐藏，通过 PopupOverlay 管理显示

  // 原有的 lambda 连接已移除，逻辑移至 onGameStateUpdated
  if (m_gameData) {
    // 初始状态刷新
    onGameStateUpdated();
  }

  // 初始化状态
  if (m_gameData) {
    QString current = m_gameData->gamePhaseString();
    // 弹窗显示由统一的 PopupOverlay.qml（绑定 `gameData.activePopups`）驱动。
    // 初始化时不再直接控制单个 PrepPhase 弹窗的可见性/大小。
  }
  // PrepPhase 弹窗由 PopupOverlay.qml
  // 管理；如果需要临时恢复单独实例，请在此添加（调试用）

  // 兑换提示 (底部中央)
  m_exchangeHint = new QLabel("按'H'打开远程兑换面板", m_centralWidget);
  m_exchangeHint->setObjectName("exchangeHint");
  m_exchangeHint->resize(300, 36);
  m_exchangeHint->setAlignment(Qt::AlignCenter);
  m_exchangeHint->setStyleSheet(R"(
    QLabel {
        background-color: rgba(75, 77, 80, 200);
        border: 1px solid rgba(120, 240, 255, 180);
        border-radius: 8px;
        color: #BFFBFF;
        padding: 6px 18px;
        font-family: "Microsoft YaHei";
        font-size: 15px;
        font-weight: bold;
    }
  )");

  if (m_hKeyHint) {
    m_hKeyHint->setStyleSheet(R"(
        QLabel {
          background-color: rgba(75, 77, 80, 200);
          border: 2px solid rgba(140, 255, 240, 200);
          border-radius: 8px;
          color: white;
          font-family: "Microsoft YaHei";
          font-size: 18px;
          font-weight: bold;
        }
      )");
    if (m_hKeyHint->graphicsEffect())
      m_hKeyHint->graphicsEffect()->setEnabled(true);
  }

  if (m_exchangeHintBox) {
    m_exchangeHintBox->setStyleSheet(R"(
        QLabel {
          background-color: rgba(75, 77, 80, 200);
          border: 2px solid rgba(140, 255, 240, 200);
          border-radius: 8px;
        }
      )");
    if (m_exchangeHintBox->graphicsEffect())
      m_exchangeHintBox->graphicsEffect()->setEnabled(true);
  }

  // 添加一些底部边距
  centerLayout->setContentsMargins(0, 0, 0, 20); // 全局底部边距

  // 底部“H”按键图标标签
  m_hKeyHint = new QLabel("H", m_centralWidget);
  m_hKeyHint->setFixedSize(40, 40);
  m_hKeyHint->setAlignment(Qt::AlignCenter);
  m_hKeyHint->setStyleSheet(R"(
    QLabel {
      background-color: rgba(75, 77, 80, 200);
      border: 2px solid rgba(160, 210, 210, 180);
      border-radius: 8px;
      color: white;
      font-family: "Microsoft YaHei";
      font-size: 18px;
      font-weight: bold;
    }
  )");
  // 真实发光效果：外发光 + 内发光叠加
  auto *outerGlow = new QGraphicsDropShadowEffect(m_hKeyHint);
  outerGlow->setBlurRadius(15);
  outerGlow->setColor(QColor(160, 210, 210, 150));
  outerGlow->setOffset(0, 0);
  m_hKeyHint->setGraphicsEffect(outerGlow);

  // 内部高光：在标签内部再叠加一层半透明内阴影（通过透明子标签实现）
  QLabel *innerHighlight = new QLabel(m_hKeyHint);
  innerHighlight->setAttribute(Qt::WA_TransparentForMouseEvents);
  innerHighlight->setStyleSheet(
      "QLabel { background: rgba(255,255,255,0.05); border-radius: 8px; }");
  innerHighlight->resize(m_hKeyHint->size());
  innerHighlight->move(0, 0);
  auto *innerGlow = new QGraphicsDropShadowEffect(innerHighlight);
  innerGlow->setBlurRadius(10);
  innerGlow->setColor(QColor(160, 210, 210, 120));
  innerGlow->setOffset(0, 0);
  innerHighlight->setGraphicsEffect(innerGlow);
  innerHighlight->show();

  m_hKeyHint->raise();
  ensureOverlayRaisedIfActive();
  m_hKeyHint->show();

  m_exchangeHintBox = new QLabel(m_centralWidget);
  m_exchangeHintBox->setAttribute(Qt::WA_TransparentForMouseEvents);
  m_exchangeHintBox->setStyleSheet(R"(
    QLabel {
      background-color: rgba(75, 77, 80, 200);
      border: 1px solid rgba(160, 210, 210, 120);
      border-radius: 8px;
    }
  )");
  m_exchangeHintBox->show();
  m_exchangeHintBox->stackUnder(m_exchangeHint);
  m_hKeyHint->raise();
  updateExchangeHintText(m_exchangeQmlSource);

  // 常驻 K/L 部署状态提示（按键逻辑在 QML 内实现）
  if (!m_qmlDeployModePanel) {
    m_qmlDeployModePanel = new QQuickWidget(this);
    m_qmlDeployModePanel->setResizeMode(QQuickWidget::SizeRootObjectToView);
    makeQQuickWidgetTransparent(m_qmlDeployModePanel);
    m_qmlDeployModePanel->rootContext()->setContextProperty("gameData",
                                                            m_gameData);

    //将network注入到DeployModePanel中
    if (m_networkManager) {
      m_qmlDeployModePanel->rootContext()->setContextProperty("network",
                                                              m_networkManager);
    }
    m_qmlDeployModePanel->setSource(QUrl("qrc:/qml/DeployModePanel.qml"));
    if (m_qmlDeployModePanel->status() == QQuickWidget::Error) {
      qWarning() << "DeployModePanel QML Load Errors:";
      for (const auto &error : m_qmlDeployModePanel->errors()) {
        qWarning() << "  " << error.toString();
      }
    }
    m_qmlDeployModePanel->show();
    ensureOverlayRaisedIfActive();
  }

  //面板大小自适应
  applyPanelResolutionScaling();

  m_mainLayout->addWidget(m_centerArea);
}

/**
 * @brief 创建侧边栏区域
 *
 * 左侧：系统模块状态 (ModuleStatusWidget)
 * 右侧：详细机器人状态 (RobotStatusWidget)
 */
void MainWindow::createSideAreas() {
  // 左侧区域
  m_leftSideArea = new QWidget(m_centerArea);
  m_leftSideArea->setFixedWidth(360);
  m_leftSideArea->setStyleSheet("background-color: transparent;");
  m_leftSideArea->setAttribute(Qt::WA_TranslucentBackground);
  m_leftSideLayout = new QVBoxLayout(m_leftSideArea);
  m_leftSideLayout->setContentsMargins(MainLayout::LAYOUT_MARGIN, 0, 0, 0);

  // --- SiloControlPanel QML (按主窗口顶端定位) ---
  m_qmlSiloPanel = new QQuickWidget(m_centralWidget);
  m_qmlSiloPanel->setResizeMode(QQuickWidget::SizeRootObjectToView);
  m_qmlSiloPanel->setGeometry(m_centralWidget->rect());
  makeQQuickWidgetTransparent(m_qmlSiloPanel);
  if (m_gameData) {
    m_qmlSiloPanel->rootContext()->setContextProperty("gameData", m_gameData);
  }
  m_qmlSiloPanel->setSource(QUrl("qrc:/qml/SiloControlPanel.qml"));
  if (m_qmlSiloPanel->status() == QQuickWidget::Error) {
    qWarning() << "SiloControlPanel QML Errors:";
    for (const auto &error : m_qmlSiloPanel->errors()) {
      qWarning() << "  " << error.toString();
    }
  }
  m_qmlSiloPanel->raise();
  ensureOverlayRaisedIfActive();
  updateSiloPanelVisibility();

  m_leftSideLayout->addStretch();

  // 右侧区域
  m_rightSideArea = new QWidget(m_centerArea);
  m_rightSideArea->setFixedWidth(MainLayout::SIDE_WIDTH);
  m_rightSideArea->setStyleSheet("background-color: transparent;");
  m_rightSideArea->setAttribute(Qt::WA_TranslucentBackground);
  m_rightSideLayout = new QVBoxLayout(m_rightSideArea);
  m_rightSideLayout->setContentsMargins(0, 0, MainLayout::LAYOUT_MARGIN, 0);

  // 创建详细机器人状态控件 (默认显示红方1号步兵)
  m_detailedRobotStatus =
      new RobotStatusWidget(RobotStatusWidget::Red, 1, m_rightSideArea);
  m_detailedRobotStatus->setRobotType(RobotStatusWidget::Infantry);
  m_detailedRobotStatus->setHealth(100, 100);
  m_detailedRobotStatus->setLevel(1);

  m_rightSideLayout->addWidget(m_detailedRobotStatus);

  m_rightSideLayout->addStretch();
}

/**
 * @brief 应用界面样式
 *
 * 设置应用程序的整体视觉风格：
 * - 主窗口背景色：深蓝色调（#0a0e1a）
 * - 默认文字颜色：白色
 * - 创建现代化的比赛界面氛围
 */
void MainWindow::applyStyles() {
  // 设置整体样式 先简单设置，后续慢慢细化
  setStyleSheet(R"(
        QMainWindow {
            background-color: #0a0e1a;
        }

        QWidget {
            color: white;
        }
    )");
}

/**
 * @brief 设置定时器
 *
 * 创建并启动界面更新定时器：
 * - 更新频率：1秒/次
 * - 用于更新比赛时间、阶段等动态信息
 * - 连接到onTimerUpdate槽函数处理更新逻辑
 */
void MainWindow::setupTimer() {
  // 定时器仅用于客户端本地倒计时同步和FPS统计
  // UI 数据更新由信号驱动（增量更新模式）
  m_updateTimer = new QTimer(this);
  connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::onTimerUpdate);
  m_updateTimer->start(1000); // 1Hz: 倒计时同步、FPS统计
}

/**
 * @brief 定时器更新处理函数
 *
 * 每秒执行一次的更新逻辑：
 * - 递增比赛时间计数器
 * - 根据时间判断当前比赛阶段
 * - 可扩展：更新界面显示、检查比赛状态等
 *
 * 比赛阶段划分：
 * - 0-60秒：准备阶段
 * - 60-420秒：比赛进行中（6分钟）
 * - 420秒后：比赛结束
 */
// --- 槽函数实现 (信号驱动增量更新模式) ---



/**
 * @brief 比赛状态更新槽 - 增量更新
 * @details 只更新比赛状态相关 UI（比分、阶段），不刷新机器人数据
 */
void MainWindow::onGameStateUpdated() {
  if (!m_gameData)
    return;

  const auto &gameState = m_gameData->getGameState();
  GameStage stage = m_gameData->getCurrentStage();
  const bool enteredCountdown = (m_lastGameStage != GameStage::COUNTDOWN &&
                                 stage == GameStage::COUNTDOWN);
  const bool enteredBattle = (m_lastGameStage != GameStage::BATTLE &&
                              stage == GameStage::BATTLE);
  const bool enteredSettlement = (m_lastGameStage != GameStage::SETTLEMENT &&
                                  stage == GameStage::SETTLEMENT);
  const bool scoreChangedForUi =
      (m_redScore != gameState.redScore || m_blueScore != gameState.blueScore);

  // 更新比赛阶段文本
  switch (stage) {
  case GameStage::NOT_STARTED:
    m_gamePhase = "未开始";
    break;
  case GameStage::PREPARATION:
    m_gamePhase = "准备阶段";
    break;
  case GameStage::SELF_CHECK:
    m_gamePhase = "自检阶段";
    break;
  case GameStage::COUNTDOWN:
    m_gamePhase = "倒计时";
    break;
  case GameStage::BATTLE:
    m_gamePhase = "战斗阶段";
    break;
  case GameStage::SETTLEMENT:
    m_gamePhase = "结算阶段";
    break;
  default:
    m_gamePhase = "未知阶段";
    break;
  }

  if (enteredCountdown) {
    // 可选语音包存在时播放倒计时提示；缺失时保持静默，不影响阶段切换。
    playSecondarySound(QString::fromUtf8("resources/sounds/3-1比赛开始.mp3"));
  }

  if (enteredBattle && m_bgmPlayer) {
    // 比赛 BGM 属于可选资源，默认源码包不携带历史音频。
    playBackgroundMusic("resources/sounds/gameBg.mp3");
  }

  updateRunePanelVisibility();

  if (enteredSettlement && m_bgmPlayer) {
    // 进入结算：如果双方基地血量均大于0 且 比赛时间不为0，视为比赛异常终止，播放异常终止音效
    bool bothBasesAlive = (m_gameData->redBaseHealth() > 0) && (m_gameData->blueBaseHealth() > 0);
    bool timeNotZero = (m_gameData->getGameTime() != 0);
    if (bothBasesAlive && timeNotZero) {
      qDebug() << "[MainWindow] Abnormal termination detected.";
      // 异常终止和正常结算暂沿用同一个可选提示文件。
      playBackgroundMusic("resources/sounds/20game_finish.mp3");
    } else {
      // 播放结算音效，一次性，不循环
      playBackgroundMusic("resources/sounds/20game_finish.mp3");
    }
  }

  // 准备和自检阶段只在可选语音包存在时播放背景音。
  if (m_bgmPlayer) {
    if (stage == GameStage::PREPARATION || stage == GameStage::SELF_CHECK) {
      const QString currentSource =
          m_bgmPlayer->source().toString(QUrl::FullyDecoded);
      if (!currentSource.contains("min3bgm.mp3")) {
        playBackgroundMusic("resources/sounds/min3bgm.mp3");
      }
    }
  }

  m_redScore = gameState.redScore;
  m_blueScore = gameState.blueScore;
  m_currentRound = gameState.currentRound;

  // 更新左下角面板阶段显示
  if (m_leftBottomPanel && m_leftBottomPanel->rootObject()) {
    m_leftBottomPanel->rootObject()->setProperty("gamePhase", m_gamePhase);
  }

  // 准备阶段与倒计时等流程弹窗的显示现在由统一的 PopupOverlay.qml 驱动，
  // 该 QML 通过绑定 `gameData.activePopups` 来决定显示哪些弹窗，避免分散的
  // setVisible/raise/resizing 导致层级抢占或闪烁。保留旧实例以便回归兼容。

  // 倒计时显示同上，由 PopupOverlay.qml/`activePopups` 驱动。

  // 结算阶段之外隐藏结果界面
  if (m_gameResultWidget && m_gameResultWidget->isVisible() &&
      stage != GameStage::SETTLEMENT) {
    // 隐藏结算面板时断开临时更新连接
    if (!m_gameResultUpdateConns.isEmpty()) {
      for (auto &c : m_gameResultUpdateConns) {
        QObject::disconnect(c);
      }
      m_gameResultUpdateConns.clear();
    }
    m_gameResultWidget->hide();
  }

  updateTacticalTimedEventPopupState();

  // 进入结算阶段时自动显示结算面板（即使没有收到 gameResultReceived 信号）
  if (enteredSettlement && m_gameResultWidget) {
    qInfo() << "[MainWindow] Entered settlement stage, showing result widget";

    // 确定胜负结果：统一使用 GameData 的战斗阶段到结算阶段比分增量判定。
    GameResultWidget::GameResult result = GameResultWidget::Draw;
    const quint8 winner = m_gameData->determineWinner();
    if (winner == 1) {
      result = GameResultWidget::RedWin;
    } else if (winner == 2) {
      result = GameResultWidget::BlueWin;
    }

    // 填充当前数据
    m_gameResultWidget->setPlayVictoryOnShow(true);
    m_gameResultWidget->setGameData(
      m_gameData->redRoundScoreDelta(), m_gameData->blueRoundScoreDelta(),
      m_gameData->currentRound(), m_gameData->getFormattedStageElapsedTime());
       // 补充增强统计，确保自动进入结算时也填充基地/前哨/哨兵等数据
    {
      int redSentryHP = 0;
      int blueSentryHP = 0;
      const auto &gameState = m_gameData->getGameState();
      for (const auto &r : m_gameData->getRobotsByTeam(TeamColor::RED)) {
        if (r.type != ::RobotType::SENTRY) continue;
        if (r.lastUpdateTime < gameState.lastUpdateTime) continue;
        redSentryHP += r.currentHP;
      }
      for (const auto &r : m_gameData->getRobotsByTeam(TeamColor::BLUE)) {
        if (r.type != ::RobotType::SENTRY) continue;
        if (r.lastUpdateTime < gameState.lastUpdateTime) continue;
        blueSentryHP += r.currentHP;
      }
      int activated = m_gameData->activatedRuneArms();
      int redEnergyActivations = m_gameData->isRedRuneActive() ? activated : 0;
      int blueEnergyActivations = m_gameData->isBlueRuneActive() ? activated : 0;

      m_gameResultWidget->setEnhancedGameStats(
          m_gameData->redBaseHealth(), m_gameData->blueBaseHealth(),
          m_gameData->redOutpostHealth(), m_gameData->blueOutpostHealth(),
          redSentryHP, blueSentryHP, redEnergyActivations, blueEnergyActivations,
          QString());
    }

    // 同步填充单兵统计（确保自动进入结算时也显示单兵伤害）
    {
      QMap<int, GameResultWidget::RobotStats> redMap;
      QMap<int, GameResultWidget::RobotStats> blueMap;
      const QMap<int, quint32> damageMap = m_gameData->getDamageByRobot();
      for (const auto &r : m_gameData->getRobotsByTeam(TeamColor::RED)) {
        GameResultWidget::RobotStats s;
        s.name = r.name.isEmpty() ? QStringLiteral("R%1").arg(r.robotId) : r.name;
        s.currentHP = r.currentHP;
        s.maxHP = r.maxHP;
        s.damageDealt = static_cast<int>(damageMap.value(r.robotId, 0));
        s.damageTaken = 0;
        s.kills = 0;
        s.deaths = (r.currentHP <= 0) ? 1 : 0;
        s.isAlive = (r.currentHP > 0);
        redMap.insert(r.robotId, s);
      }
      for (const auto &r : m_gameData->getRobotsByTeam(TeamColor::BLUE)) {
        GameResultWidget::RobotStats s;
        s.name = r.name.isEmpty() ? QStringLiteral("B%1").arg(r.robotId - 100) : r.name;
        s.currentHP = r.currentHP;
        s.maxHP = r.maxHP;
        s.damageDealt = static_cast<int>(damageMap.value(r.robotId, 0));
        s.damageTaken = 0;
        s.kills = 0;
        s.deaths = (r.currentHP <= 0) ? 1 : 0;
        s.isAlive = (r.currentHP > 0);
        blueMap.insert(r.robotId, s);
      }
      m_gameResultWidget->setRobotStats(redMap, blueMap);
    }

    // 显示结果（带失败/胜利动画）
    qDebug() << "[MainWindow] Entered settlement -> showResult (auto) result=" << static_cast<int>(result) << "reason=" << static_cast<int>(GameResultWidget::TimeUp);
    m_gameResultWidget->showResult(result, GameResultWidget::TimeUp);
    m_gameResultWidget->show();
    updateGameResultWidgetGeometry();
    ensureOverlayRaisedIfActive();

    // 结算面板可见期间订阅相关 GameData 更新
    if (m_gameResultUpdateConns.isEmpty() && m_gameData) {
      m_gameResultUpdateConns.append(connect(m_gameData, &GameData::baseHealthUpdated, this, [this](TeamColor){
        QMetaObject::invokeMethod(this, [this]() { refreshGameResultWidgetStats(); }, Qt::QueuedConnection);
      }));
      m_gameResultUpdateConns.append(connect(m_gameData, &GameData::outpostHealthUpdated, this, [this](TeamColor){
        refreshGameResultWidgetStats();
      }));
      m_gameResultUpdateConns.append(connect(m_gameData, &GameData::robotDataUpdated, this, [this](quint8){
        refreshGameResultWidgetStats();
      }));
      m_gameResultUpdateConns.append(connect(m_gameData, &GameData::damageEventOccurred, this, [this](quint8, quint8, quint16){
        refreshGameResultWidgetStats();
      }));
      m_gameResultUpdateConns.append(connect(m_gameData, &GameData::redRuneStatusUpdated, this, [this](){
        refreshGameResultWidgetStats();
      }));
      m_gameResultUpdateConns.append(connect(m_gameData, &GameData::blueRuneStatusUpdated, this, [this](){
        refreshGameResultWidgetStats();
      }));

      // 首次显示时立即刷新，确保数据最新
      refreshGameResultWidgetStats();
    }
  } else if (stage == GameStage::SETTLEMENT && scoreChangedForUi &&
             m_gameResultWidget && m_gameResultWidget->isVisible()) {
    GameResultWidget::GameResult result = GameResultWidget::Draw;
    const quint8 winner = m_gameData->determineWinner();
    if (winner == 1) {
      result = GameResultWidget::RedWin;
    } else if (winner == 2) {
      result = GameResultWidget::BlueWin;
    }

    m_gameResultWidget->setPlayVictoryOnShow(true);
    m_gameResultWidget->setGameData(
        m_gameData->redRoundScoreDelta(), m_gameData->blueRoundScoreDelta(),
        m_gameData->currentRound(), m_gameData->getFormattedStageElapsedTime());
    m_gameResultWidget->showResult(result, GameResultWidget::TimeUp);
    updateGameResultWidgetGeometry();
    ensureOverlayRaisedIfActive();
  }

  updateSiloPanelVisibility();

  // 飞镖命中图传界面自动切回战术指挥屏（基于协议时间，不使用本地定时器）
  checkDartHitViewSwitch();

  m_lastGameStage = stage;

  // 音频暂停/恢复处理：直接根据 GameData::is_paused() 设置播放状态
  if (m_gameData && m_bgmPlayer) {
    if (m_gameData->is_paused()) {
      m_bgmPlayer->pause();
    } else {
      m_bgmPlayer->play();
    }
  }

  // 离开比赛阶段时重置 5s 播放触发标记
  if (stage != GameStage::BATTLE) {
    m_secondaryPlayedAtFive = false;
  }
}

void MainWindow::checkDartHitViewSwitch() {
  if (!m_dartHitViewPending || !m_gameData) return;

  const GameStage stage = m_gameData->getCurrentStage();

  // 若比赛阶段已离开战斗阶段（如进入结算），取消等待并切回战术模式
  if (stage != GameStage::BATTLE || m_dartHitRemainingAtHit < 0) {
    // 恢复图传（若之前切换了工业相机）
    if (m_dartHitHeroDeployCamActive) {
      m_dartHitHeroDeployCamActive = false;
      if (m_videoBackground)
        m_videoBackground->show();
    }
    applyTacticalMode(true);
    // 飞镖遮挡结束后固定切回纯地图战术页；常规战术指挥页仍可用 M 键进入。
    setTacticalLargeMapMode(true);
    m_dartHitViewPending = false;
    m_dartHitRemainingAtHit = -1;
    m_dartHitTargetId = 0;
    m_dartHitOcclusionDurationSec = 0;
    qInfo() << "[DartHit] Stage changed, cancelled view switch";
    return;
  }

  const int currentGameTime = m_gameData->getGameTime();
  const int elapsedSinceHit = m_dartHitRemainingAtHit - currentGameTime;
  const int occlusionDuration = m_dartHitOcclusionDurationSec > 0
                                    ? m_dartHitOcclusionDurationSec
                                    : RM::Dart::dartOcclusionDurationSeconds(
                                          m_dartHitTargetId, 1);
  const int remainingOcclusionSeconds =
      MainWindowStatePolicy::remainingDartOcclusionSeconds(
          m_dartHitRemainingAtHit, occlusionDuration, currentGameTime);

  if (remainingOcclusionSeconds <= 0) {
    // 恢复图传（若之前切换了工业相机）
    if (m_dartHitHeroDeployCamActive) {
      m_dartHitHeroDeployCamActive = false;
      if (m_videoBackground)
        m_videoBackground->show();
    }
    applyTacticalMode(true);
    // 固定遮挡时间结束后恢复纯地图战术页，M 键仍可切回原战术指挥页。
    setTacticalLargeMapMode(true);
    m_dartHitViewPending = false;
    m_dartHitRemainingAtHit = -1;
    m_dartHitTargetId = 0;
    m_dartHitOcclusionDurationSec = 0;
    // 切回后自动隐藏事件消息面板
    if (m_qmlEventMessagePanel) {
      m_qmlEventMessagePanel->hide();
    }
    qInfo() << "[DartHit] Occlusion ended, switched back to tactical. "
            << "elapsed=" << elapsedSinceHit
            << "duration=" << occlusionDuration;
  }
}

void MainWindow::onGameResultReceived(quint8 winner) {
  if (!m_gameData || !m_gameResultWidget)
    return;

  // 仅在处于结算阶段时播放结算音效与展示结果，避免比赛从结算回到未开始时误触发
  if (m_gameData->getCurrentStage() != GameStage::SETTLEMENT) {
    qInfo() << "MainWindow: Received gameResult but current stage is not SETTLEMENT, ignoring result playback";
    return;
  }

  // 填充比分与时间（显示回合持续时间，来自 GameStatus.stage_elapsed_sec）
  m_gameResultWidget->setPlayVictoryOnShow(true);
  m_gameResultWidget->setGameData(
    m_gameData->redRoundScoreDelta(), m_gameData->blueRoundScoreDelta(),
    m_gameData->currentRound(), m_gameData->getFormattedStageElapsedTime());

  // 队伍信息
  m_gameResultWidget->setTeamData(m_gameData->redTeamName(),
                                  m_gameData->blueTeamName(), QString(),
                                  QString());

  // 构造机器人统计摘要
  QMap<int, GameResultWidget::RobotStats> redMap;
  QMap<int, GameResultWidget::RobotStats> blueMap;
  // 填充各机器人统计和 GameData 中的伤害数据
  const QMap<int, quint32> damageMap = m_gameData->getDamageByRobot();
  for (const auto &r : m_gameData->getRobotsByTeam(TeamColor::RED)) {
    GameResultWidget::RobotStats s;
    s.name = r.name.isEmpty() ? QStringLiteral("R%1").arg(r.robotId) : r.name;
    s.currentHP = r.currentHP;
    s.maxHP = r.maxHP;
    s.damageDealt = static_cast<int>(damageMap.value(r.robotId, 0));
    s.damageTaken = 0;
    s.kills = 0;
    s.deaths = (r.currentHP <= 0) ? 1 : 0;
    s.isAlive = (r.currentHP > 0);
    redMap.insert(r.robotId, s);
  }

  for (const auto &r : m_gameData->getRobotsByTeam(TeamColor::BLUE)) {
    GameResultWidget::RobotStats s;
    s.name = r.name.isEmpty() ? QStringLiteral("B%1").arg(r.robotId - 100) : r.name;
    s.currentHP = r.currentHP;
    s.maxHP = r.maxHP;
    s.damageDealt = static_cast<int>(damageMap.value(r.robotId, 0));
    s.damageTaken = 0;
    s.kills = 0;
    s.deaths = (r.currentHP <= 0) ? 1 : 0;
    s.isAlive = (r.currentHP > 0);
    blueMap.insert(r.robotId, s);
  }

  m_gameResultWidget->setRobotStats(redMap, blueMap);

  // 伤害事件无法提供分项统计时，使用协议中的队伍总量兜底，保证“伤害总量”
  // 与权威的 GlobalUnitStatus 一致。
  QMap<QString, QVariant> detailed;
  detailed.insert("redTotalDamage", QVariant(m_gameData->redTotalDamage()));
  detailed.insert("blueTotalDamage", QVariant(m_gameData->blueTotalDamage()));
  m_gameResultWidget->setDetailedStats(detailed);

  // 补充增强统计（来自 GameData / MQTT 转发的数据），用于结算面板展示
  int redSentryHP = 0;
  int blueSentryHP = 0;
  const auto &gameState = m_gameData->getGameState();
  // 只有在哨兵在本局内有更新时才把其血量计入结算面板，避免使用初始化默认值(600)
  for (const auto &r : m_gameData->getRobotsByTeam(TeamColor::RED)) {
    if (r.type != ::RobotType::SENTRY) continue;
    if (r.lastUpdateTime < gameState.lastUpdateTime) {
      continue; // 未在本局收到过更新，视为未上报
    }
    redSentryHP += r.currentHP;
  }
  for (const auto &r : m_gameData->getRobotsByTeam(TeamColor::BLUE)) {
    if (r.type != ::RobotType::SENTRY) continue;
    if (r.lastUpdateTime < gameState.lastUpdateTime) {
      continue;
    }
    blueSentryHP += r.currentHP;
  }

  int activated = m_gameData->activatedRuneArms();
  int redEnergyActivations = m_gameData->isRedRuneActive() ? activated : 0;
  int blueEnergyActivations = m_gameData->isBlueRuneActive() ? activated : 0;

  m_gameResultWidget->setEnhancedGameStats(
      m_gameData->redBaseHealth(), m_gameData->blueBaseHealth(),
      m_gameData->redOutpostHealth(), m_gameData->blueOutpostHealth(),
      redSentryHP, blueSentryHP, redEnergyActivations, blueEnergyActivations,
      QString());

  // 根据 winner 映射结果类型
  GameResultWidget::GameResult result = GameResultWidget::Draw;
  GameResultWidget::WinReason reason = GameResultWidget::TimeUp;
  if (winner == 1) {
    result = GameResultWidget::RedWin;
    reason = GameResultWidget::TimeUp;
  } else if (winner == 2) {
    result = GameResultWidget::BlueWin;
    reason = GameResultWidget::TimeUp;
  } else if (winner == 0) {
    // 平局（与 enteredSettlement 路径保持一致）
    result = GameResultWidget::Draw;
    reason = GameResultWidget::TimeUp;
  }

  qDebug() << "[MainWindow] onGameResultReceived: winner=" << static_cast<int>(winner)
           << "mapped result=" << static_cast<int>(result) << "reason=" << static_cast<int>(reason);
  m_gameResultWidget->showResult(result, reason);
  m_gameResultWidget->show();
  updateGameResultWidgetGeometry();
  ensureOverlayRaisedIfActive();
  m_gameResultWidget->raise();

  // 尚未订阅时，为可见的结算面板建立临时更新连接
  if (m_gameResultUpdateConns.isEmpty() && m_gameData) {
    m_gameResultUpdateConns.append(connect(m_gameData, &GameData::baseHealthUpdated, this, [this](TeamColor){
      QMetaObject::invokeMethod(this, [this]() { refreshGameResultWidgetStats(); }, Qt::QueuedConnection);
    }));
    m_gameResultUpdateConns.append(connect(m_gameData, &GameData::outpostHealthUpdated, this, [this](TeamColor){
      refreshGameResultWidgetStats();
    }));
    m_gameResultUpdateConns.append(connect(m_gameData, &GameData::robotDataUpdated, this, [this](quint8){
      refreshGameResultWidgetStats();
    }));
    m_gameResultUpdateConns.append(connect(m_gameData, &GameData::damageEventOccurred, this, [this](quint8, quint8, quint16){
      refreshGameResultWidgetStats();
    }));
    m_gameResultUpdateConns.append(connect(m_gameData, &GameData::redRuneStatusUpdated, this, [this](){
      refreshGameResultWidgetStats();
    }));
    m_gameResultUpdateConns.append(connect(m_gameData, &GameData::blueRuneStatusUpdated, this, [this](){
      refreshGameResultWidgetStats();
    }));

    // 首次刷新
    refreshGameResultWidgetStats();
  }
}


  void MainWindow::refreshGameResultWidgetStats() {
    if (!m_gameResultWidget || !m_gameData) return;

    int redSentryHP = 0;
    int blueSentryHP = 0;
    const auto &gameState = m_gameData->getGameState();
    for (const auto &r : m_gameData->getRobotsByTeam(TeamColor::RED)) {
      if (r.type != ::RobotType::SENTRY) continue;
      if (r.lastUpdateTime < gameState.lastUpdateTime) continue;
      redSentryHP += r.currentHP;
    }
    for (const auto &r : m_gameData->getRobotsByTeam(TeamColor::BLUE)) {
      if (r.type != ::RobotType::SENTRY) continue;
      if (r.lastUpdateTime < gameState.lastUpdateTime) continue;
      blueSentryHP += r.currentHP;
    }

    int activated = m_gameData->activatedRuneArms();
    int redEnergyActivations = m_gameData->isRedRuneActive() ? activated : 0;
    int blueEnergyActivations = m_gameData->isBlueRuneActive() ? activated : 0;

    m_gameResultWidget->setEnhancedGameStats(
        m_gameData->redBaseHealth(), m_gameData->blueBaseHealth(),
        m_gameData->redOutpostHealth(), m_gameData->blueOutpostHealth(),
        redSentryHP, blueSentryHP, redEnergyActivations, blueEnergyActivations,
        QString());

    // 更新机器人、伤害和协议总量
    QMap<int, GameResultWidget::RobotStats> redMap;
    QMap<int, GameResultWidget::RobotStats> blueMap;
    const QMap<int, quint32> damageMap = m_gameData->getDamageByRobot();
    for (const auto &r : m_gameData->getRobotsByTeam(TeamColor::RED)) {
      GameResultWidget::RobotStats s;
      s.name = r.name.isEmpty() ? QStringLiteral("R%1").arg(r.robotId) : r.name;
      s.currentHP = r.currentHP;
      s.maxHP = r.maxHP;
      s.damageDealt = static_cast<int>(damageMap.value(r.robotId, 0));
      s.damageTaken = 0;
      s.kills = 0;
      s.deaths = (r.currentHP <= 0) ? 1 : 0;
      s.isAlive = (r.currentHP > 0);
      redMap.insert(r.robotId, s);
    }
    for (const auto &r : m_gameData->getRobotsByTeam(TeamColor::BLUE)) {
      GameResultWidget::RobotStats s;
      s.name = r.name.isEmpty() ? QStringLiteral("B%1").arg(r.robotId - 100) : r.name;
      s.currentHP = r.currentHP;
      s.maxHP = r.maxHP;
      s.damageDealt = static_cast<int>(damageMap.value(r.robotId, 0));
      s.damageTaken = 0;
      s.kills = 0;
      s.deaths = (r.currentHP <= 0) ? 1 : 0;
      s.isAlive = (r.currentHP > 0);
      blueMap.insert(r.robotId, s);
    }
    m_gameResultWidget->setRobotStats(redMap, blueMap);

    QMap<QString, QVariant> detailed;
    detailed.insert("redTotalDamage", QVariant(m_gameData->redTotalDamage()));
    detailed.insert("blueTotalDamage", QVariant(m_gameData->blueTotalDamage()));
    m_gameResultWidget->setDetailedStats(detailed);
  }
// 视频源切换（切换时设置为未连接）
void MainWindow::onVideoSourceChanged(const QString &url, bool isPlaying) {
  if (m_qmlSettingsPanel && m_qmlSettingsPanel->rootObject()) {
    m_qmlSettingsPanel->rootObject()->setProperty("isVideoConnected", false);
  }
  if (m_videoConnectionTimer) {
    m_videoConnectionTimer->start();
  }

  if (m_videoBackground) {
    if (isPlaying) {
      m_videoBackground->playUrl(url);
    } else {
      m_videoBackground->stopVideo();
    }
  }
}

// 图传设置切换
void MainWindow::onVideoSourceTypeChanged(const QString &vtType) {
  if (!m_videoBackground) {
    return;
  }

  // 设置图传来源
  QString targetUrl;
  if (vtType == QStringLiteral("本地文件")) {
    targetUrl = ConfigManager::instance().getDefaultVideoPath();
  } else if (vtType == QStringLiteral("外部通道")) {
    targetUrl = ConfigManager::instance().getVideoStreamUrl();
  } else if (vtType == QStringLiteral("远程Mock")) {
    targetUrl = ConfigManager::instance().getVideoStreamUrl();
  } else { // 新图传
    targetUrl = ConfigManager::instance().getVideoStreamUrl();
  }

  if (m_qmlSettingsPanel && m_qmlSettingsPanel->rootObject()) {
    m_qmlSettingsPanel->rootObject()->setProperty("isVideoConnected", false);
  }
  if (m_videoConnectionTimer) {
    m_videoConnectionTimer->start();
  }

  if (targetUrl.isEmpty()) {
    m_videoBackground->stopVideo();
    return;
  }

  m_videoBackground->playUrl(targetUrl);
}



// 系统消息
void MainWindow::onSystemMessageReceived(const QString &message) {
  if (m_battleMessage) {
    m_battleMessage->showMessage(message);
  }
}

QString MainWindow::officialEventSoundFileName(int eventId) {
  switch (eventId) {
  case 10:
    return QStringLiteral("enemy_dart_open.mov");
  default:
    return QString();
  }
}

void MainWindow::onOfficialEventPopupRequested(int eventId, const QString &message) {
  if (message.isEmpty()) {
    return;
  }

  // 仅在正式战斗阶段播放事件音效；event 10（飞镖闸门）在倒计时/准备阶段
  // 由赛事引擎发送的初始化消息不应触发音效和弹窗。
  if (eventId == 10 && m_gameData->getCurrentStage() != GameStage::BATTLE) {
    return;
  }

  playSoundFromResourceFolder(officialEventSoundFileName(eventId));

  auto officialEventBorderColor = [](int id) -> QString {
    switch (id) {
    case 1:
    case 9:
    case 10:
      return QStringLiteral("#36d6ff");
    case 2:
    case 11:
    case 14:
      return QStringLiteral("#36d6ff");
    case 3:
    case 4:
    case 12:
    case 13:
      return QStringLiteral("#36d6ff");
    case 5:
    case 6:
      return QStringLiteral("#36d6ff");
    case 7:
    case 8:
      return QStringLiteral("#36d6ff");
    default:
      return QStringLiteral("#36d6ff");
    }
  };

  showOfficialEventPopup(QStringLiteral("官方事件"), message,
                         officialEventBorderColor(eventId),
                         m_officialEventPopupDurationMs);
}

void MainWindow::showOfficialEventPopup(const QString &eventTitle,
                                        const QString &eventMessage,
                                        const QString &borderColor,
                                        int durationMs) {
  if (!m_qmlOfficialEventPopupPanel || eventMessage.isEmpty()) {
    return;
  }

  const bool tacticalReady = m_tacticalMode &&
                             m_qmlTacticalPage &&
                             m_qmlTacticalPage->isVisible();
  if (!tacticalReady) {
    return;
  }

  if (QObject *rootObject = m_qmlOfficialEventPopupPanel->rootObject()) {
    rootObject->setProperty("eventTitle", eventTitle);
    rootObject->setProperty("eventMessage", eventMessage);
    rootObject->setProperty("borderColor", borderColor);
    rootObject->setProperty("accentColor", borderColor);
    rootObject->setProperty("displayDurationMs", durationMs);
  }

  const int displayToken =
      m_qmlOfficialEventPopupPanel->property("displayToken").toInt() + 1;
  m_qmlOfficialEventPopupPanel->setProperty("displayToken", displayToken);
  updateOfficialEventPopupGeometry();
  scheduleOverlayLayerRestack();
  ensureOverlayRaisedIfActive();
  ensureTacticalTimedEventPopupRaisedIfActive();

  const int displayDurationMs = durationMs > 0
                                    ? durationMs
                                    : 5000;
  QTimer::singleShot(displayDurationMs, this, [this, displayToken]() {
    if (m_qmlOfficialEventPopupPanel &&
        m_qmlOfficialEventPopupPanel->property("displayToken").toInt() == displayToken) {
      m_qmlOfficialEventPopupPanel->hide();
    }
  });
}

void MainWindow::setOfficialEventPopupDuration(int durationMs) {
  if (durationMs <= 0) {
    durationMs = 5000;
  }
  m_officialEventPopupDurationMs = durationMs;
  if (m_qmlOfficialEventPopupPanel) {
    m_qmlOfficialEventPopupPanel->setProperty("displayDurationMs", durationMs);
  }
}

/**
 * @brief 定时器更新 - 仅用于客户端本地计算
 * @details 1Hz 更新: 倒计时同步、FPS/Ping 统计
 *          机器人数据更新由信号驱动，不在此处理
 */
void MainWindow::onTimerUpdate() {
  if (!m_gameData)
    return;

  // 同步比赛时间（客户端本地倒计时）
  m_gameTime = m_gameData->getGameTime();

  // 剩余 5 秒时尝试播放可选提示音，不中断已有 BGM。
  if (m_gameData->getCurrentStage() == GameStage::BATTLE) {
    if (m_gameTime == 5 && !m_secondaryPlayedAtFive) {
      playSecondarySound(QString::fromUtf8("resources/sounds/2自检.mp3"));
      m_secondaryPlayedAtFive = true;
    }
    updateDartCanOpenSoundOnOutpostHealthChange(
        m_gameData->currentRobotId() < 100 ? TeamColor::BLUE : TeamColor::RED);
  } else {
    // 非比赛阶段重置标记
    m_secondaryPlayedAtFive = false;
    m_dartCanOpenDropSoundPlayed = false;
    m_dartCanOpenLateSoundPlayed = false;
    m_lastEnemyOutpostHealthForDartSound = m_gameData->enemyOutpostHealth();
  }

  updateTacticalTimedEventPopupState();

  // 更新网络统计标签
  if (m_pingLabel)
    m_pingLabel->setText("Ping: 15ms");
  if (m_networkLabel)
    m_networkLabel->setText("NET: Connected");
  // 复活状态更新现在由 GameData -> PopupStateMachine -> PopupOverlay.qml 驱动。
  // MainWindow 不再直接向单独的 QQuickWidget
  // 转发状态，保留此槽以便兼容（目前无操作）。
  if (m_serverLabel)
    m_serverLabel->setText("Server: Remote");
}

void MainWindow::updateUI() {
  // 顶部信息栏由 QML 数据绑定自动更新，此处无需处理

  // 更新左下角面板
  if (m_leftBottomPanel && m_leftBottomPanel->rootObject()) {
    QObject *root = m_leftBottomPanel->rootObject();
    root->setProperty("gamePhase", m_gamePhase);

    // 使用来自 GameData 的真实数据更新
    const RobotData *robot = m_gameData->getCurrentRobot();
    if (robot) {
      root->setProperty("currentHP", robot->currentHP);
      root->setProperty("maxHP", robot->maxHP);
      root->setProperty("currentHeat", robot->currentHeat);
      root->setProperty("heatLimit", robot->heatLimit);
      // 功率
      root->setProperty("currentPower", robot->power);
      root->setProperty("maxPower", robot->maxPower);
      root->setProperty("robotLevel", robot->level);
      root->setProperty("robotId", robot->robotId);
    }
  }

  const RobotData *robot = m_gameData->getCurrentRobot();

  if (robot && m_detailedRobotStatus) {
    // 更新详细机器人状态
    m_detailedRobotStatus->setHealth(robot->currentHP, robot->maxHP);
    m_detailedRobotStatus->setLevel(robot->level);
    m_detailedRobotStatus->setExperience(robot->experience,
                                         robot->maxExperience);
    m_detailedRobotStatus->setPower(robot->power, robot->maxPower);
    m_detailedRobotStatus->setHeat(robot->currentHeat, robot->heatLimit);
    m_detailedRobotStatus->setBuffMask(robot->buffMask);
  }

  // 更新网络统计
  if (m_pingLabel)
    m_pingLabel->setText("Ping: 15ms");
  if (m_networkLabel)
    m_networkLabel->setText("NET: Connected");
  if (m_serverLabel)
    m_serverLabel->setText("Server: Remote");
}

/**
 * @brief 更新当前选中机器人的详细面板 (增量更新)
 * @details 在机器人数据变化时调用
 */
void MainWindow::updateExchangeHintText(const QString &robotType) {
  if (!m_exchangeHint)
    return;

  if (m_tacticalMode) {
    m_exchangeHint->hide();
    if (m_hKeyHint) {
      m_hKeyHint->hide();
    }
    if (m_exchangeHintBox) {
      m_exchangeHintBox->hide();
    }
    updateDeployModePanelGeometry();
    return;
  }

  // 未登录时不显示任何 H 提示相关控件
  if (!m_exchangeHintLoginActive) {
    m_exchangeHint->hide();
    if (m_hKeyHint) {
      m_hKeyHint->hide();
    }
    if (m_exchangeHintBox) {
      m_exchangeHintBox->hide();
    }
    updateDeployModePanelGeometry();
    return;
  }

  // 判断面板是否打开
  bool isPanelOpen = m_qmlExchangePanel && m_qmlExchangePanel->isVisible();
  QString actionText = isPanelOpen ? "关闭" : "打开";

  // 判断机器人类型
  const QString effectiveRobotType =
      robotType.isEmpty() ? m_selectedRobotTypeFromSettings : robotType;
  const int baseRobotId = baseRobotIdFromSettingsType(effectiveRobotType);
  bool isEngineer = baseRobotId == 2 ||
                    m_exchangeQmlSource.contains("ExchangePanelEngineer.qml");
  bool isAerial = baseRobotId == 6 ||
                  m_exchangeQmlSource.contains("ExchangePanelAerial.qml");

  bool isSpecialRobot = isEngineer || isAerial;
  const bool canOpenRemoteExchange = canOpenRemoteExchangePanel();

  // V1.3.0 RobotDynamicStatus 已直接上报 can_remote_heal / can_remote_ammo。
  // 脱战只保留在协议侧或缺失数据时作为保护，不再由 UI 自行推导远程兑换许可。
  if (!isSpecialRobot && !canOpenRemoteExchange) {
    if (m_qmlExchangePanel) {
      m_qmlExchangePanel->hide();
      m_qmlExchangePanel->setEnabled(false);
    }
    const QString unavailableText = QStringLiteral("不满足远程兑换条件");
    if (m_exchangeHint->text() != unavailableText) {
      m_exchangeHint->setText(unavailableText);
    }
    applyExchangeHintVisualState(true);
    m_exchangeHint->show();
    updateExchangeHintOverlayGeometry();
    return;
  }

  if (m_qmlExchangePanel) {
    m_qmlExchangePanel->setEnabled(true);
  }

  QString text;
  if (isEngineer) {
    if (isPanelOpen) {
      text.clear();
    } else {
      text = "按\"H\"选择兑换难度";
    }
  } else if (isAerial) {
    text = QString("按\"H\"%1空中支援面板").arg(actionText);
  } else {
    text = QString("按\"H\"%1远程兑换面板").arg(actionText);
  }

  if (text.isEmpty()) {
    m_exchangeHint->hide();
    if (m_exchangeHintBox)
      m_exchangeHintBox->hide();
  } else {
    if (m_exchangeHint->text() != text) {
      m_exchangeHint->setText(text);
    }
    applyExchangeHintVisualState(false);
    m_exchangeHint->show(); // 始终显示文字提示
  }

  // H 图标和外框随面板可见性切换
  if (m_hKeyHint) {
    if (m_qmlExchangePanel && m_qmlExchangePanel->isVisible()) {
      m_hKeyHint->hide();
      if (m_exchangeHintBox)
        m_exchangeHintBox->hide();
    } else {
      m_hKeyHint->show();
      if (m_exchangeHintBox)
        m_exchangeHintBox->show();
    }
  }
  updateExchangeHintOverlayGeometry();
  updateDeployModePanelGeometry();
}

//是否可以打开远程面板
bool MainWindow::canOpenRemoteExchangePanel() const {
  if (!m_gameData) {
    return false;
  }

  const RobotData *robot = m_gameData->getCurrentRobot();
  if (!robot) {
    return false;
  }

  return RM::ExchangeCommandPolicy::canOpenRemoteExchange(
      robot->canRemoteHeal, robot->canRemoteAmmo);
}

//更新H按键提示的样式
void MainWindow::applyExchangeHintVisualState(bool unavailable) {
  if (!m_exchangeHint) {
    return;
  }

  const double scale = getPanelScaleFactor();
  const int scaleKey = qRound(scale * 1000.0);
  if (m_exchangeHintVisualInitialized &&
      m_exchangeHintVisualUnavailable == unavailable &&
      m_exchangeHintVisualScaleKey == scaleKey) {
    return;
  }
  const int hintBorder = qMax(1, qRound(1.0 * scale));    //文字框边框
  const int keyBorder = qMax(1, qRound(2.0 * scale));     //H按键小方块边框
  const int hintRadius = qMax(6, qRound(8.0 * scale));    //圆角半径
  const int boxRadius = hintRadius;                       //容器圆角半径
  const int unavailableBoxRadius = qMax(4, qRound(4.0 * scale));  //不可用容器圆角半径
  const int hintPadV = qMax(4, qRound(6.0 * scale));      //文字框上下间距
  const int hintPadH = qMax(12, qRound(18.0 * scale));    //文字框左右间距
  const int hintFont = qMax(12, qRound(15.0 * scale));    //H文字大小
  const int keyFont = qMax(14, qRound(18.0 * scale));     //提示字大小
  const int hintMinWidth = qMax(220, qRound(300.0 * scale));    //提示框最小宽度
  const int hintMinHeight = qMax(28, qRound(36.0 * scale));     //提示框最小高度
  const int keySize = qMax(30, qRound(40.0 * scale));            //H小方块尺寸

  //不满足远程兑换条件
  //1. 更新H按键提示文字
  if (unavailable) {
    m_exchangeHint->setStyleSheet(QString(R"(
        QLabel {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                              stop:0 rgba(45, 48, 52, 120),
                                              stop:1 rgba(30, 34, 38, 120));
            border: %1px solid rgba(90, 90, 90, 100);
            border-radius: %2px;
            color: rgba(180, 180, 180, 140);
            padding: %3px %4px;
            font-family: "Microsoft YaHei";
            font-size: %5px;
            font-weight: bold;
        }
      )")
                                     .arg(hintBorder)
                                     .arg(hintRadius)
                                     .arg(hintPadV)
                                     .arg(hintPadH)
                                     .arg(hintFont));
  } else {
    m_exchangeHint->setStyleSheet(QString(R"(
        QLabel {
            background-color: rgba(75, 77, 80, 200);
            border: %1px solid rgba(160, 210, 210, 150);
            border-radius: %2px;
            color: #A0C8C8;
            padding: %3px %4px;
            font-family: "Microsoft YaHei";
            font-size: %5px;
            font-weight: bold;
        }
      )")
                                     .arg(hintBorder)
                                     .arg(hintRadius)
                                     .arg(hintPadV)
                                     .arg(hintPadH)
                                     .arg(hintFont));
  }

  const QSize hintSize = m_exchangeHint->sizeHint().expandedTo(
      QSize(hintMinWidth, hintMinHeight));
  m_exchangeHint->resize(hintSize);

  //2. 更新H小方块
  if (m_hKeyHint) {
    if (unavailable) {
      m_hKeyHint->setStyleSheet(QString(R"(
          QLabel {
            background-color: rgba(60, 60, 60, 150);
            border: %1px solid rgba(100, 100, 100, 150);
            border-radius: %2px;
            color: rgba(180, 180, 180, 140);
            font-family: "Microsoft YaHei";
            font-size: %3px;
            font-weight: bold;
          }
        )")
                                 .arg(keyBorder)
                                 .arg(hintRadius)
                                 .arg(keyFont));
    } else {
      m_hKeyHint->setStyleSheet(QString(R"(
          QLabel {
            background-color: rgba(30, 45, 50, 200);
            border: %1px solid rgba(160, 210, 210, 180);
            border-radius: %2px;
            color: white;
            font-family: "Microsoft YaHei";
            font-size: %3px;
            font-weight: bold;
          }
        )")
                                 .arg(keyBorder)
                                 .arg(hintRadius)
                                 .arg(keyFont));
    }

    m_hKeyHint->setFixedSize(keySize, keySize);

    // 小方块发光效果变化
    if (auto *effect =
            qobject_cast<QGraphicsDropShadowEffect *>(m_hKeyHint->graphicsEffect())) {
      effect->setBlurRadius(qMax(10, qRound(15.0 * scale)));
      effect->setColor(unavailable ? QColor(100, 100, 100, 120)
                                   : QColor(160, 210, 210, 150));
      effect->setEnabled(!unavailable);
    }

    const auto overlays =
        m_hKeyHint->findChildren<QLabel *>(QString(), Qt::FindDirectChildrenOnly);
    for (QLabel *overlay : overlays) {
      overlay->setStyleSheet(QString(
          "QLabel { background: rgba(255,255,255,%1); border-radius: %2px; }")
                                 .arg(unavailable ? 3 : 13)
                                 .arg(hintRadius));
      overlay->setGeometry(m_hKeyHint->rect());
      if (auto *effect = qobject_cast<QGraphicsDropShadowEffect *>(
              overlay->graphicsEffect())) {
        effect->setBlurRadius(qMax(8, qRound(10.0 * scale)));
        effect->setColor(QColor(160, 210, 210, 120));
        effect->setEnabled(!unavailable);
      }
    }
  }

  //3. 更新H按键提示背景
  if (m_exchangeHintBox) {
    if (unavailable) {
      m_exchangeHintBox->setStyleSheet(QString(R"(
          QLabel {
            background-color: rgba(40, 40, 40, 150);
            border: %1px solid rgba(100, 100, 100, 150);
            border-radius: %2px;
          }
        )")
                                           .arg(keyBorder)
                                           .arg(unavailableBoxRadius));
    } else {
      m_exchangeHintBox->setStyleSheet(QString(R"(
          QLabel {
            background-color: rgba(75, 77, 80, 200);
            border: %1px solid rgba(160, 210, 210, 120);
            border-radius: %2px;
          }
        )")
                                           .arg(hintBorder)
                                           .arg(boxRadius));
    }
  }

  m_exchangeHintVisualInitialized = true;
  m_exchangeHintVisualUnavailable = unavailable;
  m_exchangeHintVisualScaleKey = scaleKey; // 缓存本次 scale，用于下次样式短路
}

//更新H按键提示的位置
void MainWindow::updateExchangeHintOverlayGeometry() {
  if (!m_exchangeHint || !m_exchangeHint->isVisible()) {
    return;
  }
  if (m_tacticalMode) {
    m_exchangeHint->hide();
    if (m_hKeyHint) {
      m_hKeyHint->hide();
    }
    if (m_exchangeHintBox) {
      m_exchangeHintBox->hide();
    }
    return;
  }
  QWidget *overlayParent = m_exchangeHint->parentWidget();
  const int overlayWidth = overlayParent ? overlayParent->width() : width();
  const int overlayHeight = overlayParent ? overlayParent->height() : height();

  const double scale = getPanelScaleFactor();
  const int bottomMargin = qMax(6, qRound(5.0 * scale)); //外下间距
  const int panelSpacing = qMax(12, qRound(20.0 * scale));  //提示条内间距
  const int keyGap = qMax(20, qRound(35.0 * scale));      //H小方块与提示条垂直间距
  const int boxPadding = qMax(20, qRound(35.0 * scale));  //背景间距

  //设置提示条位置
  const int x = (overlayWidth - m_exchangeHint->width()) / 2;
  int y = overlayHeight - m_exchangeHint->height() - bottomMargin;

  m_exchangeHint->move(x, y);

  if (!m_hKeyHint) {
    return;
  }

  //H面板打开时关闭上部提示
  if (m_qmlExchangePanel && m_qmlExchangePanel->isVisible()) {
    m_hKeyHint->hide();
    if (m_exchangeHintBox) {
      m_exchangeHintBox->hide();
    }
    return;
  }

  //设置H小方块位置
  const int hx = x + (m_exchangeHint->width() - m_hKeyHint->width()) / 2;
  const int hy = y - m_hKeyHint->height() - keyGap;
  m_hKeyHint->move(hx, hy);
  if (!m_hKeyHint->isVisible()) {
    m_hKeyHint->show();
  }

  //设置提示背景位置
  if (m_exchangeHintBox) {
    const int boxWidth = m_exchangeHint->width();
    const int boxHeight =
        m_hKeyHint->height() + m_exchangeHint->height() + boxPadding * 2;
    const int boxX = x;
    const int boxY = y + m_exchangeHint->height() - boxHeight;

    m_exchangeHintBox->setGeometry(boxX, boxY, boxWidth, boxHeight);
    if (!m_exchangeHintBox->isVisible()) {
      m_exchangeHintBox->show();
    }
    m_exchangeHintBox->stackUnder(m_exchangeHint);
  }

  m_exchangeHint->raise();
  m_hKeyHint->raise();
}

//更新H按键面板大小和位置（随分辨率变化）
void MainWindow::updateExchangePanelGeometry() {
  if (!m_qmlExchangePanel) {
    return;
  }

  const bool isEngineer =
      m_exchangeQmlSource.contains("ExchangePanelEngineer.qml");

  // 工程面板使用全屏容器，QML 内部自行控制主卡片位置。
  if (isEngineer) {
    m_qmlExchangePanel->setMinimumSize(0, 0);
    m_qmlExchangePanel->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    m_qmlExchangePanel->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_qmlExchangePanel->resize(width(), height());
    m_qmlExchangePanel->move(0, 0);
    return;
  }

  // 普通/空中面板按缩放尺寸贴底居中显示。
  const QSize panelSize = getScaledPanelSize(300, 190);
  m_qmlExchangePanel->setFixedSize(panelSize);
  m_qmlExchangePanel->setResizeMode(QQuickWidget::SizeRootObjectToView);

  const int panelX = (width() - panelSize.width()) / 2;
  const int bottomMargin = qMax(8, qRound(10.0 * getPanelScaleFactor()));
  const int panelY = height() - panelSize.height() - bottomMargin;
  m_qmlExchangePanel->move(panelX, panelY);
}

//确定提示文字的位置
int MainWindow::exchangePanelVisualTop() const {
  if (!m_qmlExchangePanel) {
    return 0;
  }

  const bool isEngineer =
      m_exchangeQmlSource.contains("ExchangePanelEngineer.qml");
  if (!isEngineer) {
    return m_qmlExchangePanel->y();
  }

  int panelHeight = 370;
  if (QObject *root = m_qmlExchangePanel->rootObject()) {
    const QVariant h = root->property("mainpannelHeight");
    if (h.isValid()) {
      panelHeight = h.toInt();
    }
  }
  return m_qmlExchangePanel->y() +
         (m_qmlExchangePanel->height() - panelHeight) / 2;
}


// 处理工程机器人 L 键，避免快捷键冲突。
bool MainWindow::tryHandleEngineerExitShortcut() {
  if (!m_qmlExchangePanel || !m_qmlExchangePanel->isVisible()) {
    return false;
  }

  if (!m_exchangeQmlSource.contains("ExchangePanelEngineer.qml")) {
    return false;
  }

  QObject *rootObj = m_qmlExchangePanel->rootObject();
  if (!rootObj) {
    return false;
  }

  QVariant handled;
  const bool invoked = QMetaObject::invokeMethod(
      rootObj, "handleExitShortcut", Q_RETURN_ARG(QVariant, handled));
  if (invoked) {
    return handled.toBool();
  }

  return false;
}

bool MainWindow::tryHandleEngineerConfirmShortcut(QKeyEvent *event) {
  if (!RM::InputHotkeyPolicy::isEngineerConfirmHotkey(event) ||
      !m_qmlExchangePanel || !m_qmlExchangePanel->isVisible() ||
      !m_exchangeQmlSource.contains("ExchangePanelEngineer.qml") ||
      m_qmlExchangePanel->status() != QQuickWidget::Ready) {
    return false;
  }

  QObject *rootObj = m_qmlExchangePanel->rootObject();
  if (!rootObj || rootObj->property("viewState").toString() != "selection") {
    return false;
  }

  const bool invoked =
      QMetaObject::invokeMethod(rootObj, "confirmAssemblySelection");
  qInfo() << "MainWindow: engineer assembly selection confirm"
          << "key=" << event->key() << "invoked=" << invoked
          << "difficulty=" << rootObj->property("selectedIndex").toInt()
          << "maxDifficulty="
          << rootObj->property("maximumDifficultyLevel").toInt();
  return invoked;
}

void MainWindow::refreshHeroVideoStream() {
  if (!m_videoBackground || !m_gameData) {
    qWarning() << "[herovideo] MainWindow: cannot refresh hero video stream,"
               << " videoBackground=" << m_videoBackground
               << " gameData=" << m_gameData;
    return;
  }

  VideoReceiver *videoReceiver = m_videoBackground->getVideoReceiver();
  if (!videoReceiver) {
    qWarning() << "[herovideo] MainWindow: cannot refresh hero video stream,"
               << " VideoReceiver unavailable";
    return;
  }

  videoReceiver->resetHeroVideoStreamState();
  m_gameData->clearHeroFrame();
  qInfo() << "[herovideo] MainWindow: hero video stream manually refreshed";
}

void MainWindow::applyTacticalMode(bool enabled) {
  // 大地图只属于战术页视图状态；退出前先清除，保证飞镖命中和手动切换后
  // 都能恢复常规战术界面。
  if (!enabled) {
    setTacticalLargeMapMode(false);
  }

  if (m_tacticalMode == enabled) {
    if (enabled) {
      setTacticalLayoutMode(QStringLiteral("map_primary"));
      updateHeroVideoWidgetGeometry();
    }
    return;
  }

  // 用户手动切回战术模式时，取消飞镖命中视图等待
  if (enabled && m_dartHitViewPending) {
    if (m_dartHitHeroDeployCamActive) {
      m_dartHitHeroDeployCamActive = false;
      if (m_videoBackground)
        m_videoBackground->show();
    }
    m_dartHitViewPending = false;
    m_dartHitRemainingAtHit = -1;
    m_dartHitTargetId = 0;
    m_dartHitOcclusionDurationSec = 0;
    qInfo() << "[DartHit] Manual tactical mode toggle, cancelled pending view switch";
  }

  m_tacticalMode = enabled;

  // KeyboardMouseControl 只服务操作界面；切换 Ctrl+T 时立即清除按住状态，
  // 避免输入状态带入战术页。
  if (m_tacticalMode) {
    resetKeyboardMouseControlState(true);
  }

  // 首次进入兜底创建分析引擎，正常路径已在 QML 页面创建前预热。
  if (!m_tacticalAnalyzer) {
    m_tacticalAnalyzer = new RM::TacticalAnalyzer(m_gameData, this);
    auto *freshness = new RM::DataFreshnessGuard(this);
    auto *mapper = new RM::MapCoordinateMapper(this);
    auto *ranker = new RM::ThreatRanker(this);
    auto *fusion = new RM::ExecutionFusion(m_gameData, this);
    m_tacticalAnalyzer->setFreshnessGuard(freshness);
    m_tacticalAnalyzer->setCoordMapper(mapper);
    m_tacticalAnalyzer->setThreatRanker(ranker);
    m_tacticalAnalyzer->setExecutionFusion(fusion);
    m_tacticalAnalyzer->setUseMockData(qEnvironmentVariableIsSet("RM_TACTICAL_MOCK"));
    connect(m_tacticalAnalyzer, &RM::TacticalAnalyzer::enemyPaidRespawnDetected,
            this, [this](int robotId) {
              const QString fileName = paidRespawnSoundFileName(robotId);
              qInfo() << "[PaidRespawnVoice] enemy robot revived by buyback"
                      << "robotId=" << robotId << "file=" << fileName;
              playSoundFromResourceFolder(fileName);
            });
  }
  if (m_tacticalAnalyzer && !m_tacticalAnalyzer->isRunning()) {
    m_tacticalAnalyzer->start(100);
  }

  if (m_tacticalMode) {
    setTacticalLayoutMode(QStringLiteral("map_primary"));
    setVisibleIfNeeded(m_topArea, false);
    setVisibleIfNeeded(m_leftSideArea, false);
    setVisibleIfNeeded(m_rightSideArea, false);
    setVisibleIfNeeded(m_battleMessage, false);
    setVisibleIfNeeded(m_helpOverlay, false);
    setVisibleIfNeeded(m_gameResultWidget, false);
    setVisibleIfNeeded(m_qmlDeployModePanel, false);
    setVisibleIfNeeded(m_qmlExchangePanel, false);
    setVisibleIfNeeded(m_qmlSiloPanel, false);
    setVisibleIfNeeded(m_hKeyHint, false);
    setVisibleIfNeeded(m_exchangeHint, false);
    setVisibleIfNeeded(m_exchangeHintBox, false);
  } else {
    setVisibleIfNeeded(m_topArea, true);
    setVisibleIfNeeded(m_leftSideArea, true);
    setVisibleIfNeeded(m_rightSideArea, true);
    setVisibleIfNeeded(m_battleMessage, true);
    setVisibleIfNeeded(m_helpOverlay, m_helpOverlayHotkeyActive);
    if (m_gameResultWidget && m_gameData &&
        m_gameData->getCurrentStage() == GameStage::SETTLEMENT) {
      setVisibleIfNeeded(m_gameResultWidget, true);
    }
    updateExchangeHintText(m_selectedRobotTypeFromSettings);
    updateSiloPanelVisibility();
  }

  if (m_qmlTacticalPage) {
    if (QObject *tacticalRoot = m_qmlTacticalPage->rootObject()) {
      tacticalRoot->setProperty("cameraRefreshEnabled", m_tacticalMode);
    }
    if (m_tacticalMode) {
      if (m_centralWidget) {
        m_qmlTacticalPage->setGeometry(m_centralWidget->rect());
      }
      m_qmlTacticalPage->show();
      if (m_tacticalLoginButton) {
        updateTacticalLoginButtonGeometry();
        // TacticalCommandPage.qml 已提供战术登录入口，隐藏旧原生悬浮按钮，
        // 避免不同平台出现重复的“登录”按钮。
        m_tacticalLoginButton->hide();
      }
      if (m_qmlOfficialEventPopupPanel && m_qmlOfficialEventPopupPanel->isVisible()) {
        updateOfficialEventPopupGeometry();
      }
      updateTacticalTimedEventPopupState();
      updateHeroVideoWidgetGeometry();
    } else {
      m_qmlTacticalPage->hide();
      if (m_tacticalLoginButton) {
        m_tacticalLoginButton->hide();
      }
      for (auto *popup : m_tacticalTimedEventPopups) {
        if (popup) popup->hide();
      }
      updateTacticalTimedEventPopupState();
      updateHeroVideoWidgetGeometry();
    }
  }
  scheduleOverlayLayerRestack();

  QTimer::singleShot(0, this, [this, enabled]() {
    if (m_tacticalMode != enabled) {
      return;
    }
    updateDeployModePanelGeometry();
    scheduleOverlayLayerRestack();
  });
  if (m_tacticalMode && m_tacticalLoginButton) {
    QTimer::singleShot(0, this, [this]() {
#ifndef Q_OS_LINUX
      if (m_tacticalMode && m_tacticalLoginButton &&
          m_tacticalLoginButton->isVisible()) {
        m_tacticalLoginButton->raise();
      }
#endif
    });
  }
  qInfo() << "MainWindow: Tactical" << (m_tacticalMode ? "ENABLED" : "DISABLED");
}

void MainWindow::setTacticalLargeMapMode(bool enabled) {
  if (m_tacticalLargeMapMode == enabled) {
    return;
  }

  m_tacticalLargeMapMode = enabled;
  emit tacticalLargeMapModeChanged();
  scheduleOverlayLayerRestack();
  qInfo() << "MainWindow: Tactical large map"
          << (m_tacticalLargeMapMode ? "ENABLED" : "DISABLED");
}

bool MainWindow::tacticalLargeMapRendered() const {
  if (!m_qmlTacticalPage || !m_qmlTacticalPage->rootObject()) {
    return false;
  }
  return m_qmlTacticalPage->rootObject()
      ->property("largeMapOverlayReady")
      .toBool();
}

void MainWindow::setTacticalLayoutMode(const QString &mode) {
  if (!m_tacticalAnalyzer) {
    return;
  }

  const QString normalized =
      (mode == QStringLiteral("video_primary")) ? QStringLiteral("video_primary")
                                                : QStringLiteral("map_primary");
  m_tacticalAnalyzer->setLayoutMode(normalized);
  updateHeroVideoWidgetGeometry();
  qInfo() << "MainWindow: Tactical layout" << normalized;
}

void MainWindow::toggleTacticalLayoutMode() {
  const QString current =
      m_tacticalAnalyzer ? m_tacticalAnalyzer->layoutMode()
                         : QStringLiteral("map_primary");
  setTacticalLayoutMode(current == QStringLiteral("video_primary")
                            ? QStringLiteral("map_primary")
                            : QStringLiteral("video_primary"));
}

void MainWindow::updateTacticalLoginButtonGeometry() {
  if (!m_tacticalLoginButton || !m_centralWidget) {
    return;
  }

  const int buttonWidth = 80;
  const int buttonHeight = 30;
  const int rightMargin = 66;
  const int topOffset = 108;
  const int x =
      qMax(24, m_centralWidget->width() - buttonWidth - rightMargin);

  m_tacticalLoginButton->setGeometry(x, topOffset, buttonWidth, buttonHeight);
}

void MainWindow::updateHeroVideoWidgetGeometry() {
  if (!m_heroVideoWidget || !m_centralWidget) {
    return;
  }

  // 英雄部署模式飞镖命中期间，保持工业相机全屏
  if (m_dartHitHeroDeployCamActive) {
    m_heroVideoWidget->setForceVisible(true);
    m_heroVideoWidget->setGeometry(m_centralWidget->rect());
    m_heroVideoWidget->show();
    m_heroVideoWidget->raise();
    return;
  }

  if (m_tacticalMode) {
    m_heroVideoWidget->setForceVisible(false);
    m_heroVideoWidget->hide();
    return;
  }

  m_heroVideoWidget->setForceVisible(false);
  if (m_gameData) {
    m_heroVideoWidget->setCurrentRobotId(m_gameData->currentRobotId());
  }
  if (m_gameData && m_gameData->hasHeroFrame()) {
    m_heroVideoWidget->setFrame(m_gameData->heroFrame());
  }
  if (m_miniMap) {
    QPoint miniPos = m_miniMap->mapTo(m_centralWidget, QPoint(0, 0));
    const int x = miniPos.x() + (m_miniMap->width() - m_heroVideoWidget->width()) / 2;
    int y = miniPos.y() - m_heroVideoWidget->height() - 100;
    if (y < 0) {
      y = 0;
    }
    m_heroVideoWidget->move(x, y);
    m_heroVideoWidget->raise();
  }
}

//处理空中支援中断快捷键
bool MainWindow::tryHandleAerialAirSupportInterruptShortcut() {
  return tryHandleAerialAirSupportShortcut(0);
}

bool MainWindow::tryHandleAerialAirSupportShortcut(int action) {
  if (!m_qmlExchangePanel || !m_qmlExchangePanel->isVisible()) {
    return false;
  }

  if (!m_exchangeQmlSource.contains("ExchangePanelAerial.qml",
                                    Qt::CaseInsensitive)) {
    return false;
  }

  QObject *rootObj = m_qmlExchangePanel->rootObject();
  if (!rootObj) {
    qWarning() << "MainWindow: Aerial exchange panel has no root object,"
                  " cannot dispatch AirSupportCommand"
               << "action=" << action;
    return false;
  }

  const bool invoked = QMetaObject::invokeMethod(
      rootObj, "actionSelected", Q_ARG(QVariant, QVariant(action)));
  if (!invoked) {
    qWarning() << "MainWindow: Failed to invoke aerial actionSelected,"
                  " AirSupportCommand was not dispatched"
               << "action=" << action;
    return false;
  }

  qInfo() << "MainWindow: Dispatched aerial shortcut to QML"
          << "action=" << action;
  return true;
}

bool MainWindow::tryHandleRuneHoldShortcut() {
  if (!m_qmlRunePanel || !m_gameData) {
    return false;
  }

  if (m_gameData->getCurrentStage() != GameStage::BATTLE) {
    return false;
  }

  if (!isInfantryRobotId(m_gameData->currentRobotId())) {
    return false;
  }

  if (m_qmlRunePanel->status() != QQuickWidget::Ready) {
    return false;
  }

  if (QObject *rootObj = m_qmlRunePanel->rootObject()) {
    QMetaObject::invokeMethod(rootObj, "beginHold");
  }

  updateRunePanelVisibility();
  ensureOverlayRaisedIfActive();
  return true;
}

bool MainWindow::tryHandleSiloOpenShortcut(QKeyEvent *event) {
  if (!event || !m_gameData || !m_qmlSiloPanel) {
    return false;
  }

  const bool panelReady = m_qmlSiloPanel->status() == QQuickWidget::Ready &&
                          m_qmlSiloPanel->rootObject();
  const bool isUnmodifiedF = event->key() == Qt::Key_F &&
                             event->modifiers() == Qt::NoModifier;
  if (!RM::MainWindowStatePolicy::shouldRequestSiloOpen(
          m_gameData->getCurrentRobotId(), m_gameData->getCurrentStage(),
          m_tacticalMode, isUnmodifiedF, event->isAutoRepeat(), panelReady)) {
    return false;
  }

  return QMetaObject::invokeMethod(m_qmlSiloPanel->rootObject(), "requestOpen");
}

void MainWindow::endRuneHoldShortcut() {
  if (!m_qmlRunePanel || m_qmlRunePanel->status() != QQuickWidget::Ready) {
    return;
  }

  if (QObject *rootObj = m_qmlRunePanel->rootObject()) {
    QMetaObject::invokeMethod(rootObj, "endHold");
  }
  updateRunePanelVisibility();
}

bool MainWindow::shouldShowDeployModePanel() const {
  if (!m_qmlDeployModePanel) {
    return false;
  }

  // 战术指挥屏是全屏 QML 页面，正式主界面的 H/K 部署 overlay
  // 不能叠到战术面板上，否则会遮挡底部战术分析标题。
  if (m_tacticalMode) {
    return false;
  }

  // H 兑换面板打开时，部署提示面板强制隐藏
  if (m_qmlExchangePanel && m_qmlExchangePanel->isVisible()) {
    return false;
  }

  const bool isHeroSelected =
      baseRobotIdFromSettingsType(m_selectedRobotTypeFromSettings) == 1 ||
      m_selectedRobotTypeFromSettings.contains("Hero", Qt::CaseInsensitive);
  return isHeroSelected;
}

void MainWindow::updateDeployModePanelGeometry() {
  if (!m_qmlDeployModePanel) {
    return;
  }

  const double scale = getPanelScaleFactor();
  const QSize panelSize = getScaledPanelSize(200, 96);
  if (m_qmlDeployModePanel->size() != panelSize) {
    m_qmlDeployModePanel->setFixedSize(panelSize);
  }

  if (QObject *rootObj = m_qmlDeployModePanel->rootObject()) {
    rootObj->setProperty("shortcutsEnabled", shouldShowDeployModePanel());
  }

  if (!shouldShowDeployModePanel()) {
    m_qmlDeployModePanel->hide();
    return;
  }

  if (!m_qmlDeployModePanel->isVisible()) {
    m_qmlDeployModePanel->show();
  }

  const int panelX = (width() - m_qmlDeployModePanel->width()) / 2;
  const int spacing = qMax(6, qRound(8.0 * scale));
  int panelY = height() - m_qmlDeployModePanel->height() - qMax(110, qRound(130.0 * scale));

  // 优先贴在 H 键提示上方；如果 H 键隐藏，则贴在文字提示上方。
  if (m_hKeyHint && m_hKeyHint->isVisible()) {
    panelY = m_hKeyHint->y() - m_qmlDeployModePanel->height() - spacing;
  } else if (m_exchangeHint && m_exchangeHint->isVisible()) {
    panelY = m_exchangeHint->y() - m_qmlDeployModePanel->height() -
             qMax(36, qRound(44.0 * scale));
  }

  panelY = qMax(4, panelY);
  m_qmlDeployModePanel->move(panelX, panelY);
  ensureOverlayRaisedIfActive();
}

void MainWindow::updateCurrentRobotPanel() {
  if (!m_gameData)
    return;

  const RobotData *robot = m_gameData->getCurrentRobot();
  if (!robot)
    return;

  // 更新左下角面板
  if (m_leftBottomPanel && m_leftBottomPanel->rootObject()) {
    QObject *root = m_leftBottomPanel->rootObject();
    root->setProperty("currentHP", robot->currentHP);
    root->setProperty("maxHP", robot->maxHP);
    root->setProperty("currentHeat", robot->currentHeat);
    root->setProperty(
        "maxHeat", robot->heatLimit); // QML 属性名为 maxHeat，数据源为 heatLimit
    root->setProperty("heatLimit", robot->heatLimit);

    // V4/V3 数据绑定
    root->setProperty("currentPower", robot->power);
    root->setProperty("maxPower", robot->maxPower);
    root->setProperty("chassisEnergy", robot->power); // 青色状态条跟随功率
    root->setProperty("maxChassisEnergy", robot->maxPower);
    root->setProperty("bufferEnergy", robot->bufferEnergy);
    root->setProperty("maxBufferEnergy", robot->maxBufferEnergy);

    // 增益状态
    root->setProperty("hasHpBuff", (robot->buffMask & 0x01) != 0);
    root->setProperty("hasCoolBuff", (robot->buffMask & 0x02) != 0);

    root->setProperty("robotLevel", robot->level);
    root->setProperty("robotId", robot->robotId);
    root->setProperty("currentExp", robot->experience);
    root->setProperty("maxExp", robot->maxExperience);
    root->setProperty("isRed", robot->team == TeamColor::RED);

  }

  // 更新详细机器人状态面板
  if (m_detailedRobotStatus) {
    m_detailedRobotStatus->setHealth(robot->currentHP, robot->maxHP);
    m_detailedRobotStatus->setLevel(robot->level);
    m_detailedRobotStatus->setExperience(robot->experience,
                                         robot->maxExperience);
    m_detailedRobotStatus->setPower(robot->power, robot->maxPower);
    m_detailedRobotStatus->setHeat(robot->currentHeat, robot->heatLimit);
    m_detailedRobotStatus->setBuffMask(robot->buffMask);
  }

  syncSettingsPanelState();

  // 机器人切换后立即刷新部署提示可见性（仅英雄显示）
  updateDeployModePanelGeometry();
  updateRunePanelVisibility();
}

//把真实状态返回settingpannel
void MainWindow::syncSettingsPanelState() {
  if (!m_qmlSettingsPanel || !m_qmlSettingsPanel->rootObject()) {
    return;
  }

  const quint8 currentRobotId =
      m_gameData ? m_gameData->getCurrentRobotId()
                 : static_cast<quint8>(ConfigManager::instance().getClientRobotId());
  QObject *root = m_qmlSettingsPanel->rootObject();
  root->setProperty("selectedRobotType", settingsRobotTypeLabel(currentRobotId));
  root->setProperty("activeRobotLabel",
                    m_exchangeHintLoginActive
                        ? settingsRobotTypeLabel(currentRobotId)
                        : QString());

  if (!m_gameData) {
    return;
  }

  const RobotData *currentRobot = m_gameData->getCurrentRobot();
  if (!currentRobot) {
    return;
  }
  //当比赛开始后才由系统设置性能
  const bool allowProtocolPerformanceSync =
      m_gameData->getCurrentStage() == GameStage::BATTLE;
  //获取底盘类型文本（协议）
  const QString protocolChassisText =
      chassisSelectionTextForValue(currentRobot->chassisPerformanceSelection);
  const QString protocolShooterText =
      shooterSelectionTextForValue(currentRobot->shooterPerformanceSelection);
  const bool hasProtocolChassis =
      currentRobot->chassisPerformanceSelection >= 1 &&
      currentRobot->chassisPerformanceSelection <= 4;
  const bool hasProtocolShooter =
      currentRobot->shooterPerformanceSelection >= 1 &&
      currentRobot->shooterPerformanceSelection <= 4;
  //当前下拉框底盘类型文本
  const QString currentChassisText =
      root->property("chassisSelectionText").toString();
  const QString currentShooterText =
      root->property("shooterSelectionText").toString();
  //未设置底盘类型
  const bool chassisPanelStillInitial =
      currentChassisText.isEmpty() ||
      currentChassisText == QStringLiteral("初始设置");
  const bool shooterPanelStillInitial =
      currentShooterText.isEmpty() ||
      currentShooterText == QStringLiteral("初始设置");
  //未设置底盘类型且比赛开始 ，系统系统自动设置
  if (allowProtocolPerformanceSync && hasProtocolChassis && chassisPanelStillInitial &&
      currentChassisText != protocolChassisText) {
    m_gameData->addSystemMessage(
        QStringLiteral("未选择底盘性能，已被系统设置为“%1”")
            .arg(protocolChassisText),
        QStringLiteral("#FFFFFF"));
  }
  if (allowProtocolPerformanceSync && hasProtocolShooter && shooterPanelStillInitial &&
      currentShooterText != protocolShooterText) {
    m_gameData->addSystemMessage(
        QStringLiteral("未选择发射机构类型，已被系统设置为“%1”")
            .arg(protocolShooterText),
        QStringLiteral("#FFFFFF"));
  }

  const QString nextChassisText =
      (allowProtocolPerformanceSync && hasProtocolChassis)
          ? protocolChassisText
          : (currentChassisText.isEmpty() ? QStringLiteral("初始设置")
                                          : currentChassisText);
  const QString nextShooterText =
      (allowProtocolPerformanceSync && hasProtocolShooter)
          ? protocolShooterText
          : (currentShooterText.isEmpty() ? QStringLiteral("初始设置")
                                          : currentShooterText);

  root->setProperty(
      "chassisSelectionText",
      nextChassisText);
  root->setProperty(
      "shooterSelectionText",
      nextShooterText);
}

quint8 MainWindow::resolveRobotIdFromSettingsSelection(
    const QString &robotType) const {
  const int normalizedRobotId = normalizedRobotIdFromSelection(robotType);
  if (normalizedRobotId <= 0) {
    return static_cast<quint8>(ConfigManager::instance().getClientRobotId());
  }

  if (robotType.startsWith("R", Qt::CaseInsensitive) ||
      robotType.startsWith("B", Qt::CaseInsensitive)) {
    return static_cast<quint8>(robotType.startsWith("B", Qt::CaseInsensitive)
                                   ? normalizedRobotId + 100
                                   : normalizedRobotId);
  }

  const quint8 referenceRobotId =
      m_gameData ? m_gameData->getCurrentRobotId()
                 : static_cast<quint8>(ConfigManager::instance().getClientRobotId());
  const bool isBluePerspective = referenceRobotId >= 100;
  return static_cast<quint8>(isBluePerspective ? normalizedRobotId + 100
                                               : normalizedRobotId);
}

QString MainWindow::settingsRobotTypeLabel(quint8 robotId) const {
  const bool isBlue = robotId >= 100;
  const int normalizedRobotId = isBlue ? (robotId - 100) : robotId;
  QString label = settingsRobotTypeLabelForNormalizedId(normalizedRobotId);
  if (isBlue && label.startsWith("R")) {
    label[0] = QLatin1Char('B');
  }
  return label;
}

void MainWindow::applySettingsRobotSelection(const QString &robotType,
                                             quint8 robotId) {
  if (m_gameData) {
    m_gameData->setMyRobotId(robotId);
  }

  m_selectedRobotTypeFromSettings = robotType;
  const int baseRobotId = baseRobotIdFromSettingsType(robotType);
  QString nextExchangeQmlSource;
  if (baseRobotId == 2) {
    nextExchangeQmlSource = "qrc:/qml/ExchangePanelEngineer.qml";
  } else if (baseRobotId == 6) {
    nextExchangeQmlSource = "qrc:/qml/ExchangePanelAerial.qml";
  } else {
    nextExchangeQmlSource = "qrc:/qml/ExchangePanel.qml";
  }

  const bool exchangePanelSourceChanged = (m_exchangeQmlSource != nextExchangeQmlSource);
  m_exchangeQmlSource = nextExchangeQmlSource;

  if (m_qmlExchangePanel) {
    if (exchangePanelSourceChanged) {
      m_qmlExchangePanel->setSource(QUrl(m_exchangeQmlSource));
    }
    if (m_qmlExchangePanel->rootObject()) {
      QObject *rootObj = m_qmlExchangePanel->rootObject();
      rootObj->setProperty("selectedRobotType", robotType);
      connectIfQmlSignalExists(rootObj, "commonCommandRequested(int,int)",
                               this,
                               "onCommonCommandRequested(int,int)");
      connectIfQmlSignalExists(rootObj, "exchangeRequested(int)", this,
                               "onExchangeRequested(int)");
      connectIfQmlSignalExists(rootObj, "exchangeValue(int,int)", this,
                               "onEngineerExchangeValue(int,int)");
      connectIfQmlSignalExists(rootObj, "ammoExchangeSucceeded(int)", this,
                               "onEngineerAmmoExchangeSucceeded(int)");
    }
  }

  updateSiloPanelVisibility();

  updateCurrentRobotPanel();
  updateExchangeHintText(robotType);
  syncSettingsPanelState();
}

void MainWindow::toggleLargeMiniMap() {
  if (!m_miniMapLarge) {
    return;
  }

  if (m_miniMapLarge->isVisible()) {
    safeQuickHide(m_miniMapLarge);
    if (m_miniMapLegendPanel)
      m_miniMapLegendPanel->hide();
    clearMiniMapCommandMode();
    return;
  }

  applyPanelResolutionScaling();
  m_miniMapLarge->move((width() - m_miniMapLarge->width()) / 2,
                       (height() - m_miniMapLarge->height()) / 2);
  safeQuickShow(m_miniMapLarge);
  m_miniMapLarge->raise();
  ensureOverlayRaisedIfActive();
  ensureInteractivePanelsRaisedIfVisible();
  m_miniMapLarge->setFocus();
  syncMiniMapCommandState();
  updateMiniMapLegendPanel();

  if (m_battleMessage) {
    m_battleMessage->showMessage(
        QStringLiteral("大地图已打开，按 A/B/I 选择攻击、警戒或防御"));
  }
}

void MainWindow::setMiniMapCommandMode(Qt::Key key) {
  const int markType = miniMapMarkTypeForKey(key);
  const QString label = miniMapCommandLabelForKey(key);
  if (markType < 0 || label.isEmpty()) {
    return;
  }

  m_pendingMiniMapMarkType = markType;
  m_pendingMiniMapCommandLabel = label;
  syncMiniMapCommandState();

  if (m_battleMessage) {
    m_battleMessage->showMessage(
        QStringLiteral("地图指令模式：%1").arg(m_pendingMiniMapCommandLabel));
  }
}

void MainWindow::clearMiniMapCommandMode() {
  m_pendingMiniMapMarkType = -1;
  m_pendingMiniMapCommandLabel.clear();
  syncMiniMapCommandState();
}

void MainWindow::syncMiniMapCommandState() {
  for (auto *map : m_miniMaps) {
    if (!map) {
      continue;
    }
    if (m_pendingMiniMapMarkType >= 0) {
      miniMapSetPendingMarkMode(map, m_pendingMiniMapMarkType,
                                m_pendingMiniMapCommandLabel);
    } else {
      miniMapClearPendingMarkMode(map);
    }
  }
  updateMiniMapLegendSelection();
}

void MainWindow::updateMiniMapLegendSelection() {
  auto applyRowStyle = [](QWidget *row, bool active, const QString &color) {
    if (!row) {
      return;
    }
    row->setStyleSheet(active
                           ? QString("background: %1; border: none; border-radius: 4px;")
                                 .arg(color)
                           : QStringLiteral(
                                 "background: transparent; border: none;"));
  };

  applyRowStyle(m_miniMapLegendAttackRow, m_pendingMiniMapMarkType == 1,
                QStringLiteral("rgba(24, 210, 110, 0.18)"));
  applyRowStyle(m_miniMapLegendWarningRow, m_pendingMiniMapMarkType == 3,
                QStringLiteral("rgba(255, 179, 64, 0.18)"));
  applyRowStyle(m_miniMapLegendDefenseRow, m_pendingMiniMapMarkType == 2,
                QStringLiteral("rgba(21, 215, 199, 0.18)"));
}

void MainWindow::updateMiniMapLegendPanel() {
  if (!m_miniMapLegendPanel || !m_miniMapLarge) {
    return;
  }

  if (!m_miniMapLarge->isVisible()) {
    m_miniMapLegendPanel->hide();
    return;
  }

  const int gap = 12;
  const int x = m_miniMapLarge->x() - m_miniMapLegendPanel->width() - gap;
  const int y = m_miniMapLarge->y();
  m_miniMapLegendPanel->move(std::max(8, x), std::max(8, y));
  m_miniMapLegendPanel->raise();
  ensureOverlayRaisedIfActive();
  m_miniMapLegendPanel->show();
}

bool MainWindow::canBroadcastMiniMapCommand() const {
  if (!m_gameData) {
    return false;
  }

  const RobotData *robot = m_gameData->getCurrentRobot();
  return robot && robot->type == ::RobotType::RADAR;
}

int MainWindow::currentMiniMapTargetRobotId() const {
  if (!m_gameData) {
    return 0;
  }

  if (canBroadcastMiniMapCommand()) {
    return 0;
  }

  const RobotData *robot = m_gameData->getCurrentRobot();
  return robot ? robot->robotId : m_gameData->getCurrentRobotId();
}

void MainWindow::handleMiniMapClick(qreal x, qreal y) {
  if (!m_miniMapLarge || !m_miniMapLarge->isVisible()) {
    return;
  }

  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  if (nowMs < m_miniMapMarkCooldownUntilMs) {
    if (m_battleMessage) {
      const qreal remainSec =
          qMax<qint64>(0, m_miniMapMarkCooldownUntilMs - nowMs) / 1000.0;
      m_battleMessage->showMessage(
          QStringLiteral("上一条标记未消失，请 %.1f 秒后再试").arg(remainSec, 0, 'f', 1));
    }
    return;
  }

  const qreal normX = std::clamp(x, 0.0, 1.0);
  const qreal normY = std::clamp(y, 0.0, 1.0);

  if (m_pendingMiniMapMarkType < 0) {
    if (m_battleMessage) {
      m_battleMessage->showMessage(QStringLiteral("请先按 A/B/I 选择地图指令"));
    }
    return;
  }

  for (auto *map : m_miniMaps) {
    miniMapAddCommandMarker(map, normX, normY, m_pendingMiniMapMarkType,
                            m_pendingMiniMapCommandLabel);
  }
  m_miniMapMarkCooldownUntilMs = nowMs + 1500;

  if (m_networkManager) {
#ifndef RM_HAS_MQTT
    m_networkManager->sendMapMarking(static_cast<float>(normX),
                                     static_cast<float>(normY),
                                     m_pendingMiniMapMarkType);
#endif
#ifdef RM_HAS_MQTT
    if (m_networkManager->isMqttConnected()) {
      m_networkManager->sendMapClickInfo(
          static_cast<quint32>(currentMiniMapTargetRobotId()),
          static_cast<float>(normX * 28.0), static_cast<float>(normY * 15.0),
          m_pendingMiniMapMarkType, 0, 0, canBroadcastMiniMapCommand());
    }
#endif
  }

  if (m_battleMessage) {
    const QString scope =
        canBroadcastMiniMapCommand() ? QStringLiteral("全队")
                                     : QStringLiteral("当前机器人");
    m_battleMessage->showMessage(QStringLiteral("%1 -> %2 (%3m, %4m)")
                                     .arg(m_pendingMiniMapCommandLabel, scope)
                                     .arg(normX * 28.0, 0, 'f', 1)
                                     .arg(normY * 15.0, 0, 'f', 1));
  }
}

void MainWindow::setupKeyboardMouseControlTimer() {
  if (m_keyboardMouseControlTimer) {
    return;
  }
  m_keyboardMouseControlTimer = new QTimer(this);
  m_keyboardMouseControlTimer->setTimerType(Qt::PreciseTimer);
  m_keyboardMouseControlTimer->setInterval(13);
  connect(m_keyboardMouseControlTimer, &QTimer::timeout, this,
          &MainWindow::flushKeyboardMouseControlFrame);
}

bool MainWindow::canUseKeyboardMouseControl() const {
  return m_networkManager && RM::InputHotkeyPolicy::canUseKeyboardMouseControl(
                                 m_exchangeHintLoginActive, m_tacticalMode);
}

bool MainWindow::isKeyboardMouseControlBlocked() const {
  auto visible = [](const QWidget *widget) {
    return widget && widget->isVisible();
  };

  if (visible(m_qmlSettingsPanel) || visible(m_qmlExchangePanel) ||
      visible(m_qmlAmmoSupply17Panel) || visible(m_qmlAmmoSupply42Panel) ||
      visible(m_qmlTabPanel) ||
      visible(m_qmlDamagePanel) || visible(m_miniMapLarge) ||
      visible(m_helpOverlay)) {
    return true;
  }

  return m_gameData && !m_gameData->activePopups().isEmpty();
}

bool MainWindow::isInputEventFromClientWindow(QObject *obj) const {
  const QWidget *widget = qobject_cast<QWidget *>(obj);
  if (!widget) {
    return false;
  }
  return widget == this || isAncestorOf(widget);
}

bool MainWindow::tryHandleRespawnShortcut(QKeyEvent *event) {
  if (!event || event->isAutoRepeat() || !m_gameData || !m_networkManager) {
    return false;
  }

  const int key = event->key();
  if (key != Qt::Key_F1 && key != Qt::Key_Y) {
    return false;
  }

  const QEvent::Type eventType = event->type();
  if (event->modifiers() &
      (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
    qInfo() << "MainWindow: respawn shortcut ignored"
            << "eventType=" << eventType << "key=" << key
            << "modifiers=" << event->modifiers()
            << "reason=modified-shortcut";
    return false;
  }

  const QVariantMap status = m_gameData->robotRespawnStatus();
  const bool isPending =
      status.value(QStringLiteral("is_pending_respawn"), false).toBool();
  const bool canPay =
      status.value(QStringLiteral("can_pay_for_respawn"), false).toBool();
  const bool canFree =
      status.value(QStringLiteral("can_free_respawn"), false).toBool();
  const uint current =
      status.value(QStringLiteral("current_respawn_progress"), 0).toUInt();
  const uint total =
      status.value(QStringLiteral("total_respawn_progress"), 0).toUInt();
  const bool isReadComplete = total > 0 && current >= total;
  const int currentRobotId = m_gameData->getCurrentRobotId();
  const bool robotOnline =
      currentRobotId <= 0 || m_gameData->isRobotConnected(currentRobotId);

  qInfo() << "MainWindow: respawn shortcut candidate"
          << "eventType=" << eventType << "key=" << key
          << "pending=" << isPending << "canPay=" << canPay
          << "canFree=" << canFree << "progress=" << current << "/" << total
          << "readComplete=" << isReadComplete
          << "currentRobotId=" << currentRobotId
          << "robotOnline=" << robotOnline;

  if (eventType != QEvent::KeyPress) {
    return isPending;
  }

  int cmdType = 0;
  if (key == Qt::Key_F1 &&
      RM::PopupOverlayPolicy::canSendPaidRespawn(isPending, canPay,
                                                 robotOnline)) {
    cmdType = 4;
  } else if (key == Qt::Key_Y &&
             RM::PopupOverlayPolicy::canSendFreeRespawn(
                 isPending, canFree, robotOnline)) {
    cmdType = 3;
  }

  if (cmdType == 0) {
    qInfo() << "MainWindow: respawn shortcut blocked"
            << "key=" << key << "pending=" << isPending
            << "canPay=" << canPay << "canFree=" << canFree
            << "readComplete=" << isReadComplete
            << "robotOnline=" << robotOnline;
    return false;
  }

  // 暂时与 RobotRespawn.qml 的默认 robotIndex 保持一致，待所有运行路径
  // 接入协议侧机器人索引后再移除该兜底。
  constexpr int kDefaultRespawnRobotIndex = 0;
  qInfo() << "MainWindow: respawn shortcut dispatch"
          << "key=" << key << "cmdType=" << cmdType
          << "param=" << kDefaultRespawnRobotIndex
          << "currentRobotId=" << currentRobotId;
  m_networkManager->sendCommonCommand(cmdType, kDefaultRespawnRobotIndex);
  return true;
}

int MainWindow::keyboardMouseBitForKey(int key) const {
  switch (key) {
  case Qt::Key_W:
    return 0;
  case Qt::Key_S:
    return 1;
  case Qt::Key_A:
    return 2;
  case Qt::Key_D:
    return 3;
  case Qt::Key_Shift:
    return 4;
  case Qt::Key_Control:
    return 5;
  case Qt::Key_Q:
    return 6;
  case Qt::Key_E:
    return 7;
  case Qt::Key_R:
    return 8;
  case Qt::Key_F:
    return 9;
  case Qt::Key_G:
    return 10;
  case Qt::Key_Z:
    return 11;
  case Qt::Key_X:
    return 12;
  case Qt::Key_C:
    return 13;
  case Qt::Key_V:
    return 14;
  case Qt::Key_B:
    return 15;
  default:
    return -1;
  }
}

bool MainWindow::handleKeyboardMouseControlKey(QKeyEvent *event,
                                               bool pressed) {
  if (!event || event->isAutoRepeat()) {
    return false;
  }

  const int bit = keyboardMouseBitForKey(event->key());
  if (bit < 0) {
    return false;
  }

  if (event->modifiers() & (Qt::AltModifier | Qt::MetaModifier)) {
    return false;
  }

  const quint32 keyMask = (quint32{1} << bit);
  const bool keyWasDown = (m_keyboardMouseMask & keyMask) != 0;
  const bool available =
      canUseKeyboardMouseControl() && !isKeyboardMouseControlBlocked();

  if (keyboardMouseTraceEnabled()) {
    qInfo() << "[keyboard-mouse-trace] stage=key"
            << "key=" << event->key() << "pressed=" << pressed
            << "available=" << available
            << "loginActive=" << m_exchangeHintLoginActive
            << "tacticalMode=" << m_tacticalMode
            << "inputBlocked=" << isKeyboardMouseControlBlocked()
            << "networkAvailable=" << (m_networkManager != nullptr)
            << "maskBefore=" << Qt::hex << m_keyboardMouseMask << Qt::dec;
  }

  if (!available) {
    if (!pressed && keyWasDown) {
      m_keyboardMouseMask &= ~keyMask;
      resetKeyboardMouseControlState(true);
      return true;
    }
    if (m_keyboardMouseHadActiveState) {
      resetKeyboardMouseControlState(true);
      return true;
    }
    return false;
  }

  if (pressed) {
    m_keyboardMouseMask |= keyMask;
  } else {
    m_keyboardMouseMask &= ~keyMask;
  }

  scheduleKeyboardMouseControlFlush();
  return true;
}

void MainWindow::handleKeyboardMouseButton(Qt::MouseButton button,
                                           bool pressed) {
  if (!canUseKeyboardMouseControl()) {
    return;
  }
  if (isKeyboardMouseControlBlocked()) {
    resetKeyboardMouseControlState(true);
    return;
  }

  bool handled = true;
  switch (button) {
  case Qt::LeftButton:
    m_keyboardMouseLeftDown = pressed;
    break;
  case Qt::RightButton:
    m_keyboardMouseRightDown = pressed;
    break;
  case Qt::MiddleButton:
    m_keyboardMouseMidDown = pressed;
    break;
  default:
    handled = false;
    break;
  }

  if (handled) {
    scheduleKeyboardMouseControlFlush();
  }
}

void MainWindow::handleKeyboardMouseMove(const QPoint &globalPos) {
  if (m_keyboardMouseInternalMove) {
    m_keyboardMouseInternalMove = false;
    m_lastMousePos = globalPos;
    m_hasLastMousePos = true;
    return;
  }

  if (!m_hasLastMousePos) {
    m_lastMousePos = globalPos;
    m_hasLastMousePos = true;
    if (keyboardMouseTraceEnabled()) {
      qInfo() << "[keyboard-mouse-trace] stage=mouse-baseline"
              << "globalPos=" << globalPos;
    }
    return;
  }

  const QPoint delta = globalPos - m_lastMousePos;
  if (delta.isNull()) {
    return;
  }

  const bool controlAvailable = canUseKeyboardMouseControl();
  const bool inputBlocked = isKeyboardMouseControlBlocked();
  if (!RM::InputHotkeyPolicy::shouldCapturePointerForRemoteControl(
          controlAvailable, inputBlocked)) {
    if (shouldLogKeyboardMouseMoveTrace()) {
      qInfo() << "[keyboard-mouse-trace] stage=mouse-blocked"
              << "globalPos=" << globalPos << "delta=" << delta
              << "controlAvailable=" << controlAvailable
              << "loginActive=" << m_exchangeHintLoginActive
              << "tacticalMode=" << m_tacticalMode
              << "inputBlocked=" << inputBlocked
              << "networkAvailable=" << (m_networkManager != nullptr);
    }
    if (controlAvailable && inputBlocked) {
      resetKeyboardMouseControlState(true);
    }
    // 战术模式和本地悬浮层保留系统指针，只同步基准位置，不应用操作界面的
    // 灵敏度，也不通过 QCursor::setPos 移动光标。
    m_lastMousePos = globalPos;
    return;
  }

  const qreal sensitivityGain =
      (m_controlSensitivity <= 50)
          ? (0.1 + (m_controlSensitivity / 50.0) * 0.9)
          : (1.0 + ((m_controlSensitivity - 50) / 50.0) * 2.0);

  m_pendingMouseX = clampKeyboardMouseAxis(
      m_pendingMouseX + qRound(delta.x() * sensitivityGain));
  m_pendingMouseY = clampKeyboardMouseAxis(
      m_pendingMouseY - qRound(delta.y() * sensitivityGain));
  if (shouldLogKeyboardMouseMoveTrace()) {
    qInfo() << "[keyboard-mouse-trace] stage=mouse-accepted"
            << "globalPos=" << globalPos << "delta=" << delta
            << "sensitivityGain=" << sensitivityGain
            << "pendingX=" << m_pendingMouseX
            << "pendingY=" << m_pendingMouseY;
  }
  scheduleKeyboardMouseControlFlush();

  const int targetGlobalX =
      globalPos.x() + qRound(delta.x() * (sensitivityGain - 1.0));
  const int targetGlobalY =
      globalPos.y() + qRound(delta.y() * (sensitivityGain - 1.0));
  m_lastMousePos = QPoint(targetGlobalX, targetGlobalY);

  if (qAbs(sensitivityGain - 1.0) > 0.01 && isActiveWindow()) {
    m_keyboardMouseInternalMove = true;
    QCursor::setPos(targetGlobalX, targetGlobalY);
  }
}

void MainWindow::handleKeyboardMouseWheel(int angleDeltaY) {
  if (!canUseKeyboardMouseControl()) {
    return;
  }
  if (isKeyboardMouseControlBlocked()) {
    resetKeyboardMouseControlState(true);
    return;
  }

  m_pendingMouseZ = clampKeyboardMouseAxis(m_pendingMouseZ + angleDeltaY);
  scheduleKeyboardMouseControlFlush();
}

void MainWindow::scheduleKeyboardMouseControlFlush() {
  if (!m_keyboardMouseControlTimer) {
    return;
  }
  if (!m_keyboardMouseControlTimer->isActive()) {
    m_keyboardMouseControlTimer->start();
  }
}

void MainWindow::flushKeyboardMouseControlFrame() {
  if (!m_networkManager) {
    return;
  }

  if (!canUseKeyboardMouseControl() || isKeyboardMouseControlBlocked()) {
    if (keyboardMouseTraceEnabled()) {
      qInfo() << "[keyboard-mouse-trace] stage=flush-blocked"
              << "loginActive=" << m_exchangeHintLoginActive
              << "tacticalMode=" << m_tacticalMode
              << "inputBlocked=" << isKeyboardMouseControlBlocked()
              << "networkAvailable=" << (m_networkManager != nullptr);
    }
    resetKeyboardMouseControlState(true);
    return;
  }

  const bool hasActiveState =
      m_keyboardMouseMask != 0 || m_keyboardMouseLeftDown ||
      m_keyboardMouseRightDown || m_keyboardMouseMidDown ||
      m_pendingMouseX != 0 || m_pendingMouseY != 0 || m_pendingMouseZ != 0;

  if (!hasActiveState && !m_keyboardMouseHadActiveState) {
    if (m_keyboardMouseControlTimer) {
      m_keyboardMouseControlTimer->stop();
    }
    return;
  }

  const int mouseX = m_pendingMouseX;
  const int mouseY = m_pendingMouseY;
  const int mouseZ = m_pendingMouseZ;
  m_pendingMouseX = 0;
  m_pendingMouseY = 0;
  m_pendingMouseZ = 0;

  const bool sent = m_networkManager->sendKeyboardMouseControl(
      mouseX, mouseY, mouseZ, m_keyboardMouseLeftDown,
      m_keyboardMouseRightDown, m_keyboardMouseMidDown, m_keyboardMouseMask);
  if (keyboardMouseTraceEnabled()) {
    qInfo() << "[keyboard-mouse-trace] stage=flush"
            << "mouseX=" << mouseX << "mouseY=" << mouseY
            << "mouseZ=" << mouseZ
            << "left=" << m_keyboardMouseLeftDown
            << "right=" << m_keyboardMouseRightDown
            << "middle=" << m_keyboardMouseMidDown
            << "keyboardMask=" << Qt::hex << m_keyboardMouseMask << Qt::dec
            << "sendReturned=" << sent;
  }

  m_keyboardMouseHadActiveState =
      m_keyboardMouseMask != 0 || m_keyboardMouseLeftDown ||
      m_keyboardMouseRightDown || m_keyboardMouseMidDown;
  if (!m_keyboardMouseHadActiveState && m_keyboardMouseControlTimer) {
    m_keyboardMouseControlTimer->stop();
  }
}

void MainWindow::resetKeyboardMouseControlState(bool sendNeutral) {
  const bool hadActiveState =
      m_keyboardMouseHadActiveState || m_keyboardMouseMask != 0 ||
      m_keyboardMouseLeftDown || m_keyboardMouseRightDown ||
      m_keyboardMouseMidDown || m_pendingMouseX != 0 ||
      m_pendingMouseY != 0 || m_pendingMouseZ != 0;

  m_keyboardMouseMask = 0;
  m_pendingMouseX = 0;
  m_pendingMouseY = 0;
  m_pendingMouseZ = 0;
  m_keyboardMouseLeftDown = false;
  m_keyboardMouseRightDown = false;
  m_keyboardMouseMidDown = false;

  if (sendNeutral && hadActiveState && m_networkManager) {
    m_networkManager->sendKeyboardMouseControl(0, 0, 0, false, false, false,
                                               0);
  }
  m_keyboardMouseHadActiveState = false;
  if (m_keyboardMouseControlTimer) {
    m_keyboardMouseControlTimer->stop();
  }
}

// ============================================================================
// 前哨站状态 -> CustomControl (0x0311) 转发给机器人
// CustomControl 载荷（10 字节，不超过协议规定的 30 字节上限）：
//   字节 0：    command_type（0x01 = outpost_status）
//   字节 1：    enemy_team（0=RED，1=BLUE）
//   字节 2-3：  outpost_hp（uint16，大端序）
//   字节 4：    outpost_status（0=正常，3=摧毁且不可重建，4=摧毁后可重建，5=重建中）
//   字节 5：    is_destroyed（0/1）
//   字节 6-9：  timestamp_sec（uint32，大端序，用于机器人端 TTL 校验）
// ============================================================================

void MainWindow::setupOutpostForwardTimer() {
  if (m_outpostForwardTimer) return;
  m_outpostForwardTimer = new QTimer(this);
  m_outpostForwardTimer->setTimerType(Qt::PreciseTimer);
  m_outpostForwardTimer->setSingleShot(true);
  // 限频为 10 Hz，与 GlobalUnitStatus 输入频率一致。
  m_outpostForwardTimer->setInterval(100);
  connect(m_outpostForwardTimer, &QTimer::timeout, this,
          &MainWindow::flushOutpostForward);
}

void MainWindow::scheduleOutpostForward() {
  if (!m_outpostForwardTimer || !m_gameData || !m_networkManager) return;
  // 任何阶段都转发，机器人端自行判断何时使用
  if (!m_outpostForwardTimer->isActive()) {
    m_outpostForwardTimer->start();
  }
}

void MainWindow::flushOutpostForward() {
  if (!m_gameData || !m_networkManager) return;

  // 判断敌方队伍
  const int robotId = m_gameData->currentRobotId();
  const bool isBluePerspective = robotId >= 100;
  const TeamColor enemyTeam = isBluePerspective ? TeamColor::RED : TeamColor::BLUE;

  // 获取敌方前哨站数据（通过公共 getter，类型为 private 内部结构体）
  const auto &outpost = m_gameData->getOutpostByTeam(enemyTeam);

  // 构建 CustomControl payload
  QByteArray payload;
  payload.resize(10);
  payload[0] = 0x01;  // 命令：前哨站状态
  payload[1] = static_cast<char>(enemyTeam == TeamColor::RED ? 0 : 1);
  // 血量（uint16，大端序）
  quint16 hp = outpost.currentHP;
  payload[2] = static_cast<char>((hp >> 8) & 0xFF);
  payload[3] = static_cast<char>(hp & 0xFF);
  // 状态
  payload[4] = static_cast<char>(outpost.status);
  // 摧毁标记
  payload[5] = static_cast<char>(outpost.isDestroyed ? 1 : 0);
  // 时间戳（uint32，大端序，单位为秒）
  quint32 ts = static_cast<quint32>(QDateTime::currentSecsSinceEpoch());
  payload[6] = static_cast<char>((ts >> 24) & 0xFF);
  payload[7] = static_cast<char>((ts >> 16) & 0xFF);
  payload[8] = static_cast<char>((ts >> 8) & 0xFF);
  payload[9] = static_cast<char>(ts & 0xFF);

  // 通过 CustomControl MQTT topic 发送
  m_networkManager->sendData(payload, QHostAddress::LocalHost, 0);

  qInfo().noquote()
      << QStringLiteral("RM26_OUTPOST_FORWARD enemy=%1 hp=%2/%3 status=%4 destroyed=%5")
             .arg(enemyTeam == TeamColor::RED ? "RED" : "BLUE")
             .arg(outpost.currentHP)
             .arg(outpost.maxHP)
             .arg(outpost.status)
             .arg(outpost.isDestroyed ? 1 : 0);
}

/**
 * @brief 设置字体
 *
 * 根据当前屏幕分辨率设置适配的字体大小和样式
 */
void MainWindow::setupFonts() {
  // 获取当前屏幕信息用于字体适配
  QScreen *screen = QApplication::primaryScreen();
  QRect screenGeometry = screen->geometry();
  int screenWidth = screenGeometry.width();
  int screenHeight = screenGeometry.height();

  // 计算字体缩放比例（基于1920x1080作为基准）
  double scaleFactor = qMin(static_cast<double>(screenWidth) / 1920.0,
                            static_cast<double>(screenHeight) / 1080.0);

  QString fontFamily = QStringLiteral("Noto Sans CJK SC");
  const QStringList preferredFamilies = {
      QStringLiteral("Noto Sans CJK SC"), QStringLiteral("WenQuanYi Zen Hei"),
      QStringLiteral("Microsoft YaHei"),  QStringLiteral("PingFang SC"),
      QStringLiteral("SimHei"),           QStringLiteral("Roboto")};
  const QStringList availableFamilies = QFontDatabase::families();
  for (const QString &family : preferredFamilies) {
    if (availableFamilies.contains(family)) {
      fontFamily = family;
      break;
    }
  }

  // 设置应用程序默认字体
  QFont defaultFont = getAdaptiveFont(10, false); // 基础字体大小10
  defaultFont.setFamily(fontFamily);
  QApplication::setFont(defaultFont);
}

/**
 * @brief 获取适配字体
 *
 * @param baseSize 基础字体大小
 * @param bold 是否粗体
 * @return QFont 适配后的字体
 */
QFont MainWindow::getAdaptiveFont(int baseSize, bool bold) {
  // 获取当前屏幕信息
  QScreen *screen = QApplication::primaryScreen();
  QRect screenGeometry = screen->geometry();
  int screenWidth = screenGeometry.width();
  int screenHeight = screenGeometry.height();

  // 计算字体缩放比例（基于1920x1080作为基准）
  double scaleFactor = qMin(static_cast<double>(screenWidth) / 1920.0,
                            static_cast<double>(screenHeight) / 1080.0);

  // 限制缩放范围（0.8到1.5之间）
  scaleFactor = qBound(0.8, scaleFactor, 1.5);

  // 计算适配后的字体大小
  int adaptiveSize = static_cast<int>(baseSize * scaleFactor);

  // 创建字体
  QFont font;
  font.setFamily(Fonts::FONT_FAMILY); // 使用微软雅黑字体
  font.setPointSize(adaptiveSize);
  font.setBold(bold);
  font.setStyleStrategy(QFont::PreferAntialias); // 启用抗锯齿

  return font;
}

/**
 * @brief 获取适配尺寸
 *
 * @param baseSize 基础尺寸
 * @return int 适配后的尺寸
 */
int MainWindow::getAdaptiveSize(int baseSize) {
  // 获取当前屏幕信息
  QScreen *screen = QApplication::primaryScreen();
  QRect screenGeometry = screen->geometry();
  int screenWidth = screenGeometry.width();
  int screenHeight = screenGeometry.height();

  // 计算缩放比例（基于1920x1080作为基准）
  double scaleFactor = qMin(static_cast<double>(screenWidth) / 1920.0,
                            static_cast<double>(screenHeight) / 1080.0);

  // 限制缩放范围（0.8到1.5之间）
  scaleFactor = qBound(0.8, scaleFactor, 1.5);

  // 计算适配后的尺寸
  return static_cast<int>(baseSize * scaleFactor);
}

//获取比例
double MainWindow::getPanelScaleFactor() const {
  //获取当前屏幕
  QScreen *screen = this->screen();
  if (!screen) {
    screen = QApplication::primaryScreen();
  }
  if (!screen) {
    return 1.0;
  }

  const QRect screenGeometry = screen->geometry();
  const int refWidth = screenGeometry.width();
  const int refHeight = screenGeometry.height();
  const double scale =
      qMin(static_cast<double>(refWidth) / 1530.0,
           static_cast<double>(refHeight) / 910.0);
  return qBound(0.75, scale, 2.0);
}

//获取处理后应显示的大小
QSize MainWindow::getScaledPanelSize(int baseWidth, int baseHeight) const {
  const double scale = getPanelScaleFactor();
  return QSize(qMax(1, qRound(baseWidth * scale)),
               qMax(1, qRound(baseHeight * scale)));
}

QSize MainWindow::popupResolutionViewport() const {
  if (QScreen *activeScreen = screen()) {
    const QSize screenSize = activeScreen->geometry().size();
    if (screenSize.isValid()) {
      return screenSize;
    }
  }
  return size().isValid() ? size() : QSize(1920, 1080);
}

void MainWindow::updatePopupResolutionScaling() {
  const QSize viewport = popupResolutionViewport();
  const double scale = RM::PopupOverlayPolicy::resolutionScale(viewport);

  if (m_qmlPopupOverlay && m_qmlPopupOverlay->rootObject()) {
    m_qmlPopupOverlay->rootObject()->setProperty("resolutionScale", scale);
  }
  updateGameResultWidgetGeometry();
  updateOfficialEventPopupGeometry();
  updateTacticalTimedEventPopupGeometry();
}

//把缩放应用到各面板
void MainWindow::applyPanelResolutionScaling() {
  const double scale = getPanelScaleFactor();

  if (m_qmlSettingsPanel) {
    m_qmlSettingsPanel->setFixedSize(getScaledPanelSize(1280, 450));
    setQmlRootScale(m_qmlSettingsPanel, scale);
  }

  if (m_qmlTabPanel) {
    m_qmlTabPanel->setFixedSize(getScaledPanelSize(1100, 650));
    setQmlRootScale(m_qmlTabPanel, scale);
  }

  if (m_qmlDamagePanel) {
    m_qmlDamagePanel->setFixedSize(getScaledPanelSize(260, 480));
    setQmlRootScale(m_qmlDamagePanel, scale);
  }

  if (m_qmlDeployModePanel) {
    updateDeployModePanelGeometry();
  }

  if (m_qmlExchangePanel) {
    updateExchangePanelGeometry();
  }

  if (m_leftBottomPanel) {
    const QSize leftBottomSize = getScaledPanelSize(460, 200);
    m_leftBottomPanel->setMinimumSize(leftBottomSize);
    m_leftBottomPanel->setFixedSize(leftBottomSize);
    setQmlRootScale(m_leftBottomPanel, scale);
  }

  if (m_miniMap) {
    m_miniMap->setFixedSize(getScaledPanelSize(270, 160));
  }

  if (m_miniMapLarge) {
    m_miniMapLarge->setFixedSize(getScaledPanelSize(675, 400));
  }

  if (m_heroVideoWidget) {
    m_heroVideoWidget->setFixedSize(getScaledPanelSize(300, 300));
  }

  if (m_rightBottomContainer && m_miniMap) {
    const int minContainerWidth = m_miniMap->width() + qRound(30.0 * scale);
    m_rightBottomContainer->setFixedWidth(
        qMax(getScaledPanelSize(300, 1).width(), minContainerWidth));
  }

  if (m_centerArea) {
    if (auto *centerLayout = qobject_cast<QGridLayout *>(m_centerArea->layout())) {
      centerLayout->setRowMinimumHeight(2, getScaledPanelSize(1, 200).height());
    }
  }

  //----------- 调整到对应位置 ------------------
  if (m_qmlSettingsPanel) {
    m_qmlSettingsPanel->move((width() - m_qmlSettingsPanel->width()) / 2,
                             (height() - m_qmlSettingsPanel->height()) / 2);
  }
  if (m_qmlTabPanel) {
    m_qmlTabPanel->move((width() - m_qmlTabPanel->width()) / 2,
                        (height() - m_qmlTabPanel->height()) / 2);
  }
  if (m_qmlDamagePanel && m_qmlDamagePanel->isVisible()) {
    const int leftMargin = qMax(8, qRound(10.0 * scale));
    m_qmlDamagePanel->move(leftMargin,
                           (height() - m_qmlDamagePanel->height()) / 2);
  }
  if (m_qmlDeployModePanel)
    updateDeployModePanelGeometry();
  if (m_qmlExchangePanel && m_qmlExchangePanel->isVisible()) {
    updateExchangePanelGeometry();
  }
  if (m_miniMapLarge) {
    m_miniMapLarge->move((width() - m_miniMapLarge->width()) / 2,
                         (height() - m_miniMapLarge->height()) / 2);
  }
  updateHeroVideoWidgetGeometry();

  updateExchangeHintText(m_exchangeQmlSource);
}

// 设置按键面板
void MainWindow::setupKeyPanels() {
  // InfoPanel 和 OperationsPanel 仍然使用 C++ Widget
  m_infoPanel = new InfoPanel(this);
  m_operationsPanel = new OperationsPanel(this);
  m_helpOverlay = new HelpOverlayWidget(this);
  m_helpOverlay->hide();
  m_helpOverlay->setGeometry(rect());

  auto centerPanel = [this](QWidget *panel) {
    // 如果是窗口，需要将其居中到屏幕或父窗口
    if (panel->isWindow()) {
      panel->move(geometry().center() - panel->rect().center());
    } else {
      panel->move((width() - panel->width()) / 2,
                  (height() - panel->height()) / 2);
    }
    panel->hide();
  };

  centerPanel(m_infoPanel);
  centerPanel(m_operationsPanel);

  // 初始化设置面板
  // ----------------P键-----------------------
  m_qmlSettingsPanel = new QQuickWidget(this);
  m_qmlSettingsPanel->setResizeMode(QQuickWidget::SizeViewToRootObject);
  m_qmlSettingsPanel->setFocusPolicy(Qt::StrongFocus);
  makeQQuickWidgetOpaque(m_qmlSettingsPanel, QColor(20, 20, 30));
  if (m_gameData && m_qmlSettingsPanel->rootContext()) {
    m_qmlSettingsPanel->rootContext()->setContextProperty("gameData",
                                                          m_gameData);
  }
  m_qmlSettingsPanel->setSource(QUrl("qrc:/qml/SettingsPanel.qml"));
  m_settingsPanelRoot = m_qmlSettingsPanel->rootObject();

  m_qmlTabPanel = new QQuickWidget(this);
  m_qmlTabPanel->setResizeMode(QQuickWidget::SizeViewToRootObject);
  makeQQuickWidgetOpaque(m_qmlTabPanel, QColor(20, 20, 30));
  if (m_gameData && m_qmlTabPanel->rootContext()) {
    m_qmlTabPanel->rootContext()->setContextProperty("gameData", m_gameData);
  }
  m_qmlTabPanel->setSource(QUrl("qrc:/qml/TabStatsPanel.qml"));
  m_qmlTabPanel->setFixedSize(1100, 650);

  // ----------------H键-----------------------

  // 检查QML加载错误
  if (m_qmlSettingsPanel->status() == QQuickWidget::Error) {
    qWarning() << "SettingsPanel QML Load Errors:";
    for (const auto &error : m_qmlSettingsPanel->errors()) {
      qWarning() << "  " << error.toString();
    }
  } else {
    // 连接登录请求信号
    QObject *root = m_qmlSettingsPanel->rootObject();
    if (root) {
      connect(root, SIGNAL(loginRequested(QString)), this,
              SLOT(onSettingsLoginRequested(QString)));
      connect(root, SIGNAL(logoutRequested()), this,
              SLOT(onSettingsLogoutRequested()));
      syncSettingsPanelState();
    }
  }
  if (m_qmlTabPanel) {
    if (m_qmlTabPanel->status() == QQuickWidget::Error) {
      qWarning() << "TabStatsPanel QML Load Errors:";
      for (const auto &error : m_qmlTabPanel->errors()) {
        qWarning() << "  " << error.toString();
      }
    }
  }
  centerPanel(m_qmlSettingsPanel);
  centerPanel(m_qmlTabPanel);
  //面板大小自适应
  applyPanelResolutionScaling();

  connect(m_infoPanel, &InfoPanel::closed, [this]() { m_infoPanel->hide(); });
  connect(m_operationsPanel, &OperationsPanel::closed,
          [this]() { m_operationsPanel->hide(); });

  // SettingsPanel 和 ExchangePanel 已迁移至 QML:
  // - m_qmlSettingsPanel (SettingsPanel.qml)
  // - m_qmlExchangePanel (ExchangePanel.qml)
  // 信号连接在各自 QML 文件内部处理
}

void MainWindow::setupAudio() {
  if (qEnvironmentVariableIntValue("RM_DISABLE_AUDIO") != 0) {
    qInfo() << "MainWindow: audio disabled by RM_DISABLE_AUDIO";
    return;
  }

  qDebug() << "MainWindow: initializing audio outputs and players";
  m_audioOutput = new QAudioOutput(this);
  m_bgmPlayer = new QMediaPlayer(this);
  m_bgmPlayer->setAudioOutput(m_audioOutput);
  m_audioOutput->setVolume(0.8);
  qDebug() << "MainWindow: BGM player and audio output created; volume=" << m_audioOutput->volume();

  // 并列音效播放器（用于在不打断 BGM 的情况下播放短音效）
  m_secondaryOutput = new QAudioOutput(this);
  m_secondaryPlayer = new QMediaPlayer(this);
  m_secondaryPlayer->setAudioOutput(m_secondaryOutput);
  m_secondaryOutput->setVolume(0.8);
  qDebug() << "MainWindow: Secondary player created; volume=" << m_secondaryOutput->volume();

  // 预热 secondaryPlayer 的音频管线（AVFoundation 首次播放有 1-3s 冷启动延迟）
  m_secondaryOutput->setVolume(0.0);
  const QString warmupPath = QCoreApplication::applicationDirPath()
      + QStringLiteral("/resources/sounds/basedrop.mov");
  if (QFile::exists(warmupPath)) {
    m_secondaryPlayer->setSource(QUrl::fromLocalFile(warmupPath));
    m_secondaryPlayer->play();
  } else {
    qWarning() << "[AudioQueue] skip missing warmup sound"
               << "resolvedPlayPath=" << warmupPath;
  }
  QTimer::singleShot(800, this, [this]() {
    if (m_secondaryPlayer) {
      m_secondaryPlayer->stop();
    }
    if (m_secondaryOutput) {
      m_secondaryOutput->setVolume(0.8);
    }
    if (!m_secondarySoundQueue.isEmpty()) {
      startNextQueuedSecondarySound();
    }
  });
  qDebug() << "MainWindow: secondaryPlayer warmup initiated";

  connect(m_secondaryPlayer, &QMediaPlayer::mediaStatusChanged, this,
          [this](QMediaPlayer::MediaStatus status) {
            const QString source =
                m_secondaryPlayer
                    ? m_secondaryPlayer->source().toString(QUrl::FullyDecoded)
                    : QString();
            if (source.contains(QStringLiteral("rune_"))) {
              qInfo() << "[RuneVoice] audio media status"
                      << "status=" << status << "source=" << source
                      << "queueSize=" << m_secondarySoundQueue.size();
              if (status == QMediaPlayer::EndOfMedia) {
                qInfo() << "[RuneVoice] audio finished"
                        << "source=" << source;
              } else if (status == QMediaPlayer::InvalidMedia) {
                qWarning() << "[RuneVoice] audio invalid media"
                           << "source=" << source;
              }
            }
            if (status != QMediaPlayer::EndOfMedia &&
                status != QMediaPlayer::InvalidMedia) {
              return;
            }
            if (!m_secondarySoundPlaying) {
              return;
            }
            m_secondarySoundPlaying = false;
            QTimer::singleShot(0, this,
                               &MainWindow::startNextQueuedSecondarySound);
          });
  connect(m_secondaryPlayer, &QMediaPlayer::playbackStateChanged, this,
          [this](QMediaPlayer::PlaybackState state) {
            const QString source =
                m_secondaryPlayer
                    ? m_secondaryPlayer->source().toString(QUrl::FullyDecoded)
                    : QString();
            if (source.contains(QStringLiteral("rune_"))) {
              qInfo() << "[RuneVoice] audio playback state"
                      << "state=" << state << "source=" << source
                      << "position=" << m_secondaryPlayer->position()
                      << "duration=" << m_secondaryPlayer->duration();
            }
          });
  connect(m_secondaryPlayer, &QMediaPlayer::errorOccurred, this,
          [this](QMediaPlayer::Error error, const QString &errorString) {
            const QString source =
                m_secondaryPlayer
                    ? m_secondaryPlayer->source().toString(QUrl::FullyDecoded)
                    : QString();
            qWarning() << "[AudioQueue] playback error"
                       << "error=" << error << "message=" << errorString
                       << "source=" << source;
            if (source.contains(QStringLiteral("rune_"))) {
              qWarning() << "[RuneVoice] audio error"
                         << "error=" << error << "message=" << errorString
                         << "source=" << source;
            }
            if (!m_secondarySoundPlaying) {
              return;
            }
            m_secondarySoundPlaying = false;
            m_secondaryPlayer->stop();
            QTimer::singleShot(0, this,
                               &MainWindow::startNextQueuedSecondarySound);
          });

  // 不在启动时强制播放 min3；min3 的播放由 onGameStateUpdated 决定。
  connect(m_bgmPlayer, &QMediaPlayer::mediaStatusChanged, this,
          [this](QMediaPlayer::MediaStatus status) {
            qDebug() << "MainWindow: mediaStatusChanged->status=" << status;
            if (status != QMediaPlayer::EndOfMedia || !m_bgmPlayer) {
                return;
              }

              const QString currentSource =
                  m_bgmPlayer->source().toString(QUrl::FullyDecoded);
              qDebug() << "MainWindow: media ended for source=" << currentSource;

            // 对于一次性语音（如倒计时/提示音），在结束后根据当前游戏阶段决定后续播放：
            // - 如果当前阶段为非(未开始/倒计时/战斗/结算)，播放 min3.mp3 循环
            // - 如果当前阶段为 BATTLE，切换到 gameBg.mp3 循环
            // - 否则不自动播放（保持静默）
            if (currentSource.contains("min7bgmyuyin.mp3") ||
                currentSource.contains(QString::fromUtf8("3-1比赛开始.mp3"))) {
              if (m_gameData) {
                GameStage stage = m_gameData->getCurrentStage();
                qDebug() << "MainWindow: current game stage=" << static_cast<int>(stage);
                // 如果当前为准备或自检阶段，则播放并循环 min3
                if (stage == GameStage::PREPARATION || stage == GameStage::SELF_CHECK) {
                  qDebug() << "MainWindow: switching to min3bgm for PREPARATION/SELF_CHECK";
                  playBackgroundMusic("resources/sounds/min3bgm.mp3");
                  return;
                }
                // 进入比赛则切换到循环 BGM
                if (stage == GameStage::BATTLE) {
                  qDebug() << "MainWindow: switching to gameBg for BATTLE";
                  playBackgroundMusic("resources/sounds/gameBg.mp3");
                  return;
                }
              }
              return;
            }

            // 其它资源：仅对常规循环 BGM 做自动循环，其它一次性语音（如结算）不循环
            const QString curLower = currentSource.toLower();
            if (curLower.contains("gamebg.mp3") || curLower.contains("min3bgm.mp3") || curLower.contains("min3.mp3")) {
              qDebug() << "MainWindow: auto-looping background source=" << currentSource;
              m_bgmPlayer->setPosition(0);
              m_bgmPlayer->play();
              qDebug() << "MainWindow: play() invoked for BGM";
            }
          });
}

void MainWindow::playBackgroundMusic(const QString &filePath) {
  if (m_bgmPlayer) {
    QString playPath = filePath;
    QFileInfo fileInfo(filePath);

    // 如果是相对路径，则相对于应用程序目录解析
    if (fileInfo.isRelative()) {
      playPath = QCoreApplication::applicationDirPath() + "/" + filePath;
    }

    // 优先尝试加载解析后的路径，如果失败则尝试原始路径（可能是URL或qrc）
    qDebug() << "MainWindow: playBackgroundMusic requested filePath=" << filePath << " resolvedPlayPath=" << playPath;
    bool exists = QFile::exists(playPath);
    qDebug() << "MainWindow: resolved file exists=" << exists;
    if (!exists) {
      qWarning() << "[AudioQueue] skip missing background sound"
                 << "filePath=" << filePath
                 << "resolvedPlayPath=" << playPath;
      return;
    }
    qDebug() << "MainWindow: setting BGM source to local file:" << playPath;
    m_bgmPlayer->setSource(QUrl::fromLocalFile(playPath));
    qDebug() << "MainWindow: invoking play() for background music";
    m_bgmPlayer->play();
  }
}

void MainWindow::playSecondarySound(const QString &filePath, int loops) {
  if (!m_secondaryPlayer) {
    return;
  }

  const int resolvedLoops = loops > 0 ? loops : 1;
  m_secondarySoundQueue.enqueue(SecondarySoundRequest{filePath, resolvedLoops});
  qDebug() << "MainWindow: queued secondary sound"
           << "filePath=" << filePath << "loops=" << resolvedLoops
           << "queueSize=" << m_secondarySoundQueue.size();

  if (!m_secondarySoundPlaying &&
      m_secondaryPlayer->playbackState() != QMediaPlayer::PlayingState) {
    startNextQueuedSecondarySound();
  }
}

void MainWindow::startNextQueuedSecondarySound() {
  if (!m_secondaryPlayer || m_secondarySoundPlaying) {
    return;
  }

  while (!m_secondarySoundQueue.isEmpty()) {
    const SecondarySoundRequest request = m_secondarySoundQueue.dequeue();
    QString playPath = request.filePath;
    const QFileInfo fileInfo(request.filePath);
    if (fileInfo.isRelative()) {
      playPath =
          QCoreApplication::applicationDirPath() + "/" + request.filePath;
    }

    const QFileInfo resolvedFileInfo(playPath);
    if (!resolvedFileInfo.exists() || !resolvedFileInfo.isFile()) {
      qWarning() << "[AudioQueue] skip missing secondary sound"
                 << "filePath=" << request.filePath
                 << "resolvedPlayPath=" << playPath
                 << "remainingQueue=" << m_secondarySoundQueue.size();
      if (request.filePath.contains(QStringLiteral("rune_"))) {
        qWarning() << "[RuneVoice] skip missing audio"
                   << "filePath=" << request.filePath
                   << "resolvedPlayPath=" << playPath;
      }
      continue;
    }

    m_secondarySoundPlaying = true;
    qDebug() << "MainWindow: play queued secondary sound"
             << "filePath=" << request.filePath
             << "resolvedPlayPath=" << playPath
             << "remainingQueue=" << m_secondarySoundQueue.size();
    if (request.filePath.contains(QStringLiteral("rune_"))) {
      qInfo() << "[RuneVoice] audio play requested"
              << "filePath=" << request.filePath
              << "resolvedPlayPath=" << playPath
              << "remainingQueue=" << m_secondarySoundQueue.size();
    }
    m_secondaryPlayer->setSource(QUrl::fromLocalFile(playPath));
    m_secondaryPlayer->setLoops(request.loops);
    m_secondaryPlayer->setPosition(0);
    m_secondaryPlayer->play();
    return;
  }
}

void MainWindow::playSoundFromResourceFolder(const QString &fileName) {
  const QString trimmed = fileName.trimmed();
  if (trimmed.isEmpty()) {
    return;
  }

  const QString normalizedName = QFileInfo(trimmed).fileName();
  if (normalizedName.isEmpty()) {
    return;
  }

  playSecondarySound(QStringLiteral("resources/sounds/%1").arg(normalizedName));
}

void MainWindow::playRuneVoicePrompt(int runeType, int remainingChances) {
  const auto type = runeType == static_cast<int>(RM::RuneVoiceType::Large)
                        ? RM::RuneVoiceType::Large
                        : RM::RuneVoiceType::Small;
  const QString fileName =
      RM::runeVoiceSoundFileName(type, remainingChances);
  if (fileName.isEmpty()) {
    qWarning() << "MainWindow: no rune voice mapping"
               << "runeType=" << runeType
               << "remainingChances=" << remainingChances;
    return;
  }

  const QString relativePath =
      QStringLiteral("resources/sounds/%1").arg(fileName);
  const QString resolvedPath =
      QCoreApplication::applicationDirPath() + QStringLiteral("/") +
      relativePath;
  if (!QFile::exists(resolvedPath)) {
    qWarning() << "MainWindow: rune voice placeholder has no audio asset:"
               << resolvedPath;
    return;
  }

  qInfo() << "[RuneVoice] audio enqueue"
          << "type=" << (type == RM::RuneVoiceType::Small ? "small" : "large")
          << "chances=" << remainingChances << "file=" << fileName
          << "resolvedPath=" << resolvedPath
          << "queueSizeBefore=" << m_secondarySoundQueue.size();
  playSecondarySound(relativePath, 1);
}

void MainWindow::updateDartCanOpenSoundOnOutpostHealthChange(TeamColor team) {
  if (!m_gameData) {
    return;
  }

  const int operatorRobotId =
      robotIdFromSettingsType(m_selectedRobotTypeFromSettings);
  if (!RM::isDartGateVoiceOperator(operatorRobotId)) {
    return;
  }

  const bool isBattleStage = m_gameData->getCurrentStage() == GameStage::BATTLE;
  const bool allyIsRed = m_gameData->currentRobotId() < 100;
  const TeamColor enemyTeam = allyIsRed ? TeamColor::BLUE : TeamColor::RED;
  if (team != enemyTeam) {
    return;
  }

  const int currentEnemyOutpostHp = m_gameData->enemyOutpostHealth();
  const int previousEnemyOutpostHp = m_lastEnemyOutpostHealthForDartSound;
  m_lastEnemyOutpostHealthForDartSound = currentEnemyOutpostHp;

  const int currentGameTime = m_gameData->getGameTime();
  const bool shouldPlayDropSound =
      !m_dartCanOpenDropSoundPlayed &&
      RM::shouldPlayDartCanOpenDropSound(currentGameTime,
                                         previousEnemyOutpostHp,
                                         currentEnemyOutpostHp,
                                         isBattleStage);
  const bool shouldPlayLateSound =
      !m_dartCanOpenLateSoundPlayed &&
      RM::shouldPlayDartCanOpenLateSound(currentGameTime,
                                         currentEnemyOutpostHp,
                                         isBattleStage);

  if (shouldPlayDropSound || shouldPlayLateSound) {
    if (shouldPlayDropSound) {
      m_dartCanOpenDropSoundPlayed = true;
    }
    if (shouldPlayLateSound) {
      m_dartCanOpenLateSoundPlayed = true;
    }
    playSoundFromResourceFolder(QStringLiteral("dart_can_open.mov"));
  }
}

void MainWindow::updateOutpostReviveReminder(bool isRedOutpost, int status) {
  if (!m_gameData) {
    return;
  }

  const bool allyIsRed = m_gameData->currentRobotId() < 100;
  const int currentGameTime = m_gameData->getGameTime();
  const bool shouldRun = RM::shouldEnableOutpostReviveReminder(
      isRedOutpost == allyIsRed, status,
      currentGameTime,
      m_gameData->getCurrentStage() == GameStage::BATTLE);

  if (!shouldRun) {
    if (isRedOutpost == allyIsRed && m_outpostReviveReminderTimer) {
      m_outpostReviveReminderTimer->stop();
    }
    return;
  }

  if (!m_outpostReviveReminderTimer) {
    m_outpostReviveReminderTimer = new QTimer(this);
    m_outpostReviveReminderTimer->setInterval(
        RM::kOutpostReviveReminderIntervalMs);
    connect(m_outpostReviveReminderTimer, &QTimer::timeout, this,
            &MainWindow::playOutpostReviveReminder);
  }

  if (!m_outpostReviveReminderTimer->isActive()) {
    playOutpostReviveReminder();
    m_outpostReviveReminderTimer->start();
  }
}


void MainWindow::playOutpostReviveReminder() {
  if (m_gameData) {
    const bool allyIsRed = m_gameData->currentRobotId() < 100;
    const OutpostData &allyOutpost =
        m_gameData->getOutpostByTeam(allyIsRed ? TeamColor::RED
                                               : TeamColor::BLUE);
    const bool shouldRun = RM::shouldEnableOutpostReviveReminder(
        true, allyOutpost.status, m_gameData->getGameTime(),
        m_gameData->getCurrentStage() == GameStage::BATTLE);
    if (!shouldRun) {
      if (m_outpostReviveReminderTimer) {
        m_outpostReviveReminderTimer->stop();
      }
      return;
    }
  }

  playSecondarySound(QStringLiteral("resources/sounds/outpost_revive.mp3"));
}

void MainWindow::onMusicVolumeChanged(int volume) {
  if (m_audioOutput) {
    m_audioOutput->setVolume(volume / 100.0);
  }
}

void MainWindow::onSensitivityChanged(int value) {
  m_controlSensitivity = qBound(0, value, 100);
  qDebug() << "Control Sensitivity Set to:" << m_controlSensitivity;
}

void MainWindow::onSystemVolumeChanged(int volume) {
  const qreal normalized = qBound(0, volume, 100) / 100.0;

  // 非背景音乐音效（击杀播报、结算音效等）
  if (m_battleMessage) {
    m_battleMessage->setSoundVolume(normalized);
  }
  if (m_gameResultWidget) {
    m_gameResultWidget->setSoundVolume(normalized);
  }
}

void MainWindow::onCrosshairVisibilityChanged(bool visible) {
  if (m_qmlCentralAimingHUD && m_qmlCentralAimingHUD->rootObject()) {
    m_qmlCentralAimingHUD->rootObject()->setProperty("crosshairVisible",
                                                     visible);
    return;
  }
  if (m_crosshair) {
    m_crosshair->setVisible(visible);
  }
}

void MainWindow::onFpsChanged(const QString &fpsText) {
  if (!m_videoBackground) {
    return;
  }

  static const QRegularExpression re("(\\d+)");
  const QRegularExpressionMatch match = re.match(fpsText);
  if (!match.hasMatch()) {
    qWarning() << "MainWindow: 无法解析帧率文本:" << fpsText;
    return;
  }

  bool ok = false;
  const int fps = match.captured(1).toInt(&ok);
  if (!ok || fps <= 0) {
    qWarning() << "MainWindow: 非法帧率值:" << fpsText;
    return;
  }

  m_videoBackground->setTargetFps(fps);
  qDebug() << "MainWindow: target FPS set to" << fps;
}

void MainWindow::onPerformanceSelectionChanged(int shooter, int chassis,
                                               int sentryControl) {
  // 安全保护：0 表示“初始设置/未选择”，允许与另一项已选值一起发送。
  if (shooter < 0 || shooter > 4 || chassis < 0 || chassis > 4 ||
      sentryControl < 0 || sentryControl > 1) {
    qWarning() << "MainWindow: invalid RobotPerformanceSelectionCommand values"
               << "shooter=" << shooter << "chassis=" << chassis
               << "sentryControl=" << sentryControl;
    return;
  }

  if (!m_networkManager) {
    qWarning() << "MainWindow: NetworkManager unavailable, cannot send "
                  "RobotPerformanceSelectionCommand";
    return;
  }

  qInfo() << "MainWindow: sending RobotPerformanceSelectionCommand"
          << "shooter=" << shooter << "chassis=" << chassis
          << "sentryControl=" << sentryControl;
  m_networkManager->sendRobotPerformanceSelection(
      static_cast<uint32_t>(shooter), static_cast<uint32_t>(chassis),
      static_cast<uint32_t>(sentryControl));
}

void MainWindow::onDisplayModeChanged(const QString &mode) {
  if (mode == "全屏") {
    showFullScreen();
  } else {
    showNormal();
  }
}

void MainWindow::onGameStart() {}

// QQuickWidget::hide() 在 Linux/X11 上销毁 OpenGL 上下文可能触发 SIGSEGV。
// 对 QQuickWidget 面板改用 QML 层 visible 属性控制显隐。
static void safeQuickHide(QWidget *w) {
  if (auto *qw = qobject_cast<QQuickWidget *>(w)) {
    if (QObject *root = qw->rootObject())
      root->setProperty("visible", false);
  }
  w->hide();
}

static void safeQuickShow(QWidget *w) {
  if (auto *qw = qobject_cast<QQuickWidget *>(w)) {
    if (QObject *root = qw->rootObject())
      root->setProperty("visible", true);
  }
  w->show();
}

static bool isQuickPanelEffectivelyVisible(QWidget *w) {
  if (!w) {
    return false;
  }
  bool rootVisible = true;
  if (auto *qw = qobject_cast<QQuickWidget *>(w)) {
    auto *root = qobject_cast<QQuickItem *>(qw->rootObject());
    rootVisible = (root != nullptr) && root->isVisible();
  }
  return MainWindowStatePolicy::isQuickPanelEffectivelyVisible(w->isVisible(),
                                                               rootVisible);
}

bool MainWindow::shouldCaptureSettingsPanelShortcut(QKeyEvent *event) const {
  if (!event || !m_qmlSettingsPanel ||
      m_qmlSettingsPanel->status() != QQuickWidget::Ready) {
    return false;
  }

  QObject *rootObj = m_settingsPanelRoot.data();
  if (!rootObj) {
    rootObj = m_qmlSettingsPanel->rootObject();
  }
  if (!rootObj) {
    return false;
  }

  const bool panelActive =
      isActiveWindow() && isQuickPanelEffectivelyVisible(m_qmlSettingsPanel);
  return RM::InputHotkeyPolicy::shouldCaptureSettingsPanelShortcut(
      event, panelActive,
      rootObj->property("robotShortcutPending").toBool());
}

bool MainWindow::tryHandleSettingsPanelShortcut(QKeyEvent *event) {
  if (!shouldCaptureSettingsPanelShortcut(event)) {
    return false;
  }

  // ShortcutOverride 和 KeyPress 的自动重复事件都在这里终止。
  if (event->isAutoRepeat()) {
    return true;
  }

  QObject *rootObj = m_settingsPanelRoot.data();
  if (!rootObj) {
    rootObj = m_qmlSettingsPanel->rootObject();
  }

  const int robotId =
      RM::InputHotkeyPolicy::settingsRobotIdForShortcut(event);
  if (robotId > 0) {
    const QString robotType =
        settingsRobotTypeLabel(static_cast<quint8>(robotId));
    QVariant handled;
    const bool invoked = QMetaObject::invokeMethod(
        rootObj, "selectRobotByShortcut", Q_RETURN_ARG(QVariant, handled),
        Q_ARG(QVariant, QVariant(robotType)));
    if (!invoked || !handled.toBool()) {
      qWarning() << "MainWindow: settings robot shortcut selection failed"
                 << "key=" << event->key() << "robotType=" << robotType
                 << "invoked=" << invoked;
    }
    return true;
  }

  QVariant handled;
  const bool invoked = QMetaObject::invokeMethod(
      rootObj, "confirmRobotShortcutLogin", Q_RETURN_ARG(QVariant, handled));
  if (!invoked || !handled.toBool()) {
    qWarning() << "MainWindow: settings robot shortcut login failed"
               << "key=" << event->key() << "invoked=" << invoked;
  }
  return true;
}

void MainWindow::showPanel(QWidget *panel) {
  if (panel) {
    if (auto *quickWidget = qobject_cast<QQuickWidget *>(panel)) {
      if (quickWidget->status() != QQuickWidget::Ready ||
          !quickWidget->rootObject()) {
        qWarning() << "MainWindow: refusing to show QML panel"
                   << quickWidget->source()
                   << "status=" << static_cast<int>(quickWidget->status())
                   << "hasRoot=" << (quickWidget->rootObject() != nullptr);
        for (const auto &error : quickWidget->errors()) {
          qWarning() << " " << error.toString();
        }
        return;
      }
      qDebug() << "MainWindow: showing QML panel" << quickWidget->source()
               << "size=" << quickWidget->size();
    }
    hideAllPanelsExcept(panel);
    panel->move(width() / 2 - panel->width() / 2,
                height() / 2 - panel->height() / 2);
    safeQuickShow(panel);
    panel->raise();
    ensureOverlayRaisedIfActive();
    ensureInteractivePanelsRaisedIfVisible();
    if (auto *quickWidget = qobject_cast<QQuickWidget *>(panel)) {
      panel->setFocus(Qt::PopupFocusReason);
      if (QObject *rootObject = quickWidget->rootObject()) {
        rootObject->setProperty("focus", true);
        if (auto *rootItem = qobject_cast<QQuickItem *>(rootObject)) {
          rootItem->forceActiveFocus(Qt::PopupFocusReason);
        }
      }
    } else {
      panel->setFocus(Qt::PopupFocusReason);
    }
  }
}

// 隐藏所有面板
void MainWindow::hideAllPanels() { hideAllPanelsExcept(nullptr); }

void MainWindow::hideAllPanelsExcept(QWidget *keep) {
  auto hideOne = [&](QWidget *w) { if (w && w != keep) safeQuickHide(w); };
  hideOne(m_qmlSettingsPanel);
  m_settingsPanelVisible = (keep == m_qmlSettingsPanel);
  hideOne(m_infoPanel);
  hideOne(m_qmlExchangePanel);
  hideOne(m_operationsPanel);
  hideOne(m_qmlTabPanel);
  hideOne(m_qmlDamagePanel);
  hideOne(m_helpOverlay);
  hideOne(m_miniMapLarge);
  hideOne(m_miniMapLegendPanel);
  clearMiniMapCommandMode();
  hideOne(m_qmlAmmoSupply17Panel);
  hideOne(m_qmlAmmoSupply42Panel);
  hideOne(m_qmlSiloPanel);
}

void MainWindow::updateSiloPanelVisibility() {
  if (!m_qmlSiloPanel) {
    return;
  }

  const int robotId =
      m_gameData ? m_gameData->getCurrentRobotId()
                 : ConfigManager::instance().getClientRobotId();
  const GameStage stage =
      m_gameData ? m_gameData->getCurrentStage() : GameStage::NOT_STARTED;
  if (MainWindowStatePolicy::shouldShowSiloPanel(robotId, stage,
                                                 m_tacticalMode)) {
    safeQuickShow(m_qmlSiloPanel);
    m_qmlSiloPanel->raise();
    ensureOverlayRaisedIfActive();
  } else {
    safeQuickHide(m_qmlSiloPanel);
  }
}

// --- 全局事件过滤器 - 替代 event() 和 keyPressEvent() 处理全局快捷键 ---
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::Show) {
    if (QWidget *widget = qobject_cast<QWidget *>(obj);
        widget && isInputEventFromClientWindow(widget)) {
      RM::InputHotkeyPolicy::enableMouseTrackingForWidgetTree(widget);
    }
  }

  if (m_tacticalMode && event->type() == QEvent::MouseButtonPress &&
      qEnvironmentVariableIsSet("RM_TACTICAL_INPUT_TRACE")) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    QWidget *widget = qobject_cast<QWidget *>(obj);
    QWidget *underCursor =
        m_centralWidget ? m_centralWidget->childAt(
                              m_centralWidget->mapFromGlobal(
                                  mouseEvent->globalPosition().toPoint()))
                        : nullptr;
    qInfo() << "[tactical-input-trace]"
            << "receiver=" << obj
            << "receiverClass=" << obj->metaObject()->className()
            << "widget=" << widget
            << "widgetName=" << (widget ? widget->objectName() : QString())
            << "underCursor=" << underCursor
            << "underCursorClass="
            << (underCursor ? underCursor->metaObject()->className() : "")
            << "underCursorName="
            << (underCursor ? underCursor->objectName() : QString())
            << "global=" << mouseEvent->globalPosition().toPoint();
  }

  if (obj == m_tacticalLoginButton && m_tacticalLoginButton) {
    switch (event->type()) {
    case QEvent::MouseButtonPress:
      qInfo() << "[tactical-login] eventFilter MouseButtonPress"
              << "buttonGeometry=" << m_tacticalLoginButton->geometry()
              << "visible=" << m_tacticalLoginButton->isVisible()
              << "enabled=" << m_tacticalLoginButton->isEnabled();
      break;
    case QEvent::MouseButtonRelease:
      qInfo() << "[tactical-login] eventFilter MouseButtonRelease";
      break;
    case QEvent::FocusIn:
      qInfo() << "[tactical-login] eventFilter FocusIn";
      break;
    case QEvent::FocusOut:
      qInfo() << "[tactical-login] eventFilter FocusOut";
      break;
    case QEvent::Show:
      qInfo() << "[tactical-login] eventFilter Show"
              << "geometry=" << m_tacticalLoginButton->geometry();
      break;
    case QEvent::Hide:
      qInfo() << "[tactical-login] eventFilter Hide";
      break;
    default:
      break;
    }
  }

  const auto hasBlockedModifiers = [](Qt::KeyboardModifiers mods) {
    return (mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
           != 0;
  };

  if (event->type() == QEvent::ShortcutOverride) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    if (shouldCaptureSettingsPanelShortcut(keyEvent)) {
      event->accept();
      return true;
    }
    if (m_tacticalMode &&
        RM::InputHotkeyPolicy::isTacticalLargeMapToggleHotkey(keyEvent) &&
        !isInputEventFromClientWindow(obj)) {
      return QMainWindow::eventFilter(obj, event);
    }
    if (RM::InputHotkeyPolicy::isEngineerConfirmHotkey(keyEvent) &&
        m_qmlExchangePanel && m_qmlExchangePanel->isVisible() &&
        m_exchangeQmlSource.contains("ExchangePanelEngineer.qml")) {
      event->accept();
      return true;
    }
    if (!keyEvent->isAutoRepeat() &&
        keyEvent->modifiers() == Qt::NoModifier &&
        keyEvent->key() == Qt::Key_N &&
        m_qmlExchangePanel && m_qmlExchangePanel->isVisible() &&
        m_exchangeQmlSource.contains("ExchangePanelAerial.qml",
                                     Qt::CaseInsensitive)) {
      event->accept();
      return true;
    }
    if ((keyEvent->modifiers() & Qt::ControlModifier) &&
        (keyEvent->key() == Qt::Key_1 || keyEvent->key() == Qt::Key_2) &&
        m_qmlExchangePanel && m_qmlExchangePanel->isVisible() &&
        m_exchangeQmlSource.contains("ExchangePanelAerial.qml",
                                     Qt::CaseInsensitive)) {
      event->accept();
      return true;
    }
    if (!hasBlockedModifiers(keyEvent->modifiers()) &&
        (RM::InputHotkeyPolicy::isGlobalPanelHotkey(keyEvent->key()) ||
         RM::InputHotkeyPolicy::isDamagePanelHotkey(keyEvent))) {
      event->accept();
      return true;
    }
    if (keyEvent->modifiers() == Qt::ControlModifier &&
        keyEvent->key() == Qt::Key_T) {
      event->accept();
      return true;
    }
    if (m_tacticalMode && isTacticalLayoutSwitchShortcut(keyEvent)) {
      event->accept();
      return true;
    }
    if (keyEvent->key() == Qt::Key_L &&
        !(keyEvent->modifiers() & Qt::ControlModifier)) {
      if (tryHandleEngineerExitShortcut()) {
        event->accept();
        return true;
      }
    }
    if (canUseKeyboardMouseControl() && !isKeyboardMouseControlBlocked() &&
        !(keyEvent->modifiers() & (Qt::AltModifier | Qt::MetaModifier)) &&
        keyboardMouseBitForKey(keyEvent->key()) >= 0) {
      event->accept();
      return true;
    }
  } else if (event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    if (tryHandleSettingsPanelShortcut(keyEvent)) {
      event->accept();
      return true;
    }
    if (m_tacticalMode &&
        RM::InputHotkeyPolicy::isTacticalLargeMapToggleHotkey(keyEvent) &&
        !isInputEventFromClientWindow(obj)) {
      return QMainWindow::eventFilter(obj, event);
    }
    if (tryHandleEngineerConfirmShortcut(keyEvent)) {
      event->accept();
      return true;
    }
    if (tryHandleRespawnShortcut(keyEvent)) {
      event->accept();
      return true;
    }
    if (!keyEvent->isAutoRepeat() &&
        keyEvent->modifiers() == Qt::NoModifier &&
        keyEvent->key() == Qt::Key_N &&
        tryHandleAerialAirSupportInterruptShortcut()) {
      event->accept();
      return true;
    }
    if (!keyEvent->isAutoRepeat() &&
        (keyEvent->modifiers() & Qt::ControlModifier)) {
      if (keyEvent->key() == Qt::Key_1 &&
          tryHandleAerialAirSupportShortcut(1)) {
        event->accept();
        return true;
      }
      if (keyEvent->key() == Qt::Key_2 &&
          tryHandleAerialAirSupportShortcut(2)) {
        event->accept();
        return true;
      }
      if (keyEvent->key() == Qt::Key_T) {
        keyPressEvent(keyEvent);
        event->accept();
        return true;
      }
    }
    if (m_tacticalMode && isTacticalLayoutSwitchShortcut(keyEvent)) {
      keyPressEvent(keyEvent);
      event->accept();
      return true;
    }
    if (!hasBlockedModifiers(keyEvent->modifiers()) &&
        RM::InputHotkeyPolicy::isDamagePanelHotkey(keyEvent)) {
      keyPressEvent(keyEvent);
      event->accept();
      return true;
    }
    // 事件提示弹窗临时调试键Ctrl+Shift+F9 / 战术定时事件弹窗临时调试键Ctrl+Shift+F8
    if (!keyEvent->isAutoRepeat() &&
        isTacticalTimedEventPopupTestHotkey(keyEvent)) {
      keyPressEvent(keyEvent);
      event->accept();
      return true;
    }
    if (!keyEvent->isAutoRepeat() &&
        isOfficialEventPopupTestHotkey(keyEvent)) {
      keyPressEvent(keyEvent);
      event->accept();
      return true;
    }
    if (!keyEvent->isAutoRepeat() && !hasBlockedModifiers(keyEvent->modifiers()) &&
        RM::InputHotkeyPolicy::isGlobalPanelHotkey(keyEvent->key())) {
      keyPressEvent(keyEvent);
      event->accept();
      return true;
    }
    if (keyEvent->key() == Qt::Key_F12) {
      if (!keyEvent->isAutoRepeat() && m_helpOverlay) {
        m_helpOverlayHotkeyActive = true;
        m_helpOverlay->setGeometry(rect());
        m_helpOverlay->show();
        m_helpOverlay->raise();
        ensureOverlayRaisedIfActive();
      }
      return true;
    }
    if (keyEvent->key() == Qt::Key_Tab) {
      if (!keyEvent->isAutoRepeat() && !m_handlingTabKey) {
        // Tab键按下 - 显示QML面板
        if (m_qmlTabPanel) {
          qDebug() << "Tab key pressed - showing Tab stats panel";
          m_handlingTabKey = true;
          showPanel(m_qmlTabPanel);
          m_handlingTabKey = false;
        }
      }
      return true; // 拦截事件
    }

    if (!keyEvent->isAutoRepeat() && m_miniMapLarge &&
        m_miniMapLarge->isVisible() &&
        !hasBlockedModifiers(keyEvent->modifiers()) &&
        isMiniMapCommandKey(keyEvent->key())) {
      keyPressEvent(keyEvent);
      event->accept();
      return true;
    }

    if (!keyEvent->isAutoRepeat() &&
        handleKeyboardMouseControlKey(keyEvent, true)) {
      // F 继续上报 KeyboardMouseControl，同时让客户端进入本地 Y/N 确认状态。
      tryHandleSiloOpenShortcut(keyEvent);
      event->accept();
      return true;
    }

    //处理L按键
    if (keyEvent->key() == Qt::Key_L &&
        !(keyEvent->modifiers() & Qt::ControlModifier)) {
      if (tryHandleEngineerExitShortcut()) {
        event->accept();
        return true;
      }
    }

  } else if (event->type() == QEvent::KeyRelease) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    if (tryHandleRespawnShortcut(keyEvent)) {
      event->accept();
      return true;
    }
    if (!keyEvent->isAutoRepeat() &&
        RM::InputHotkeyPolicy::isDamagePanelHotkey(keyEvent)) {
      keyReleaseEvent(keyEvent);
      event->accept();
      return true;
    }
    if (keyEvent->key() == Qt::Key_F12) {
      if (!keyEvent->isAutoRepeat() && m_helpOverlay) {
        m_helpOverlayHotkeyActive = false;
        m_helpOverlay->hide();
      }
      return true;
    }
    if (keyEvent->key() == Qt::Key_Tab) {
      if (!keyEvent->isAutoRepeat() && !m_handlingTabKey) {
        // Tab键松开 - 隐藏QML面板
        if (m_qmlTabPanel) {
          m_handlingTabKey = true;
          m_qmlTabPanel->hide();
          m_handlingTabKey = false;
          qDebug() << "Tab stats panel hidden";
        }
      }
      return true; // 拦截事件
    }
    if (m_tacticalMode && isTacticalLayoutSwitchShortcut(keyEvent)) {
      event->accept();
      return true;
    }
    if (!keyEvent->isAutoRepeat() &&
        handleKeyboardMouseControlKey(keyEvent, false)) {
      event->accept();
      return true;
    }
  } else if (event->type() == QEvent::MouseButtonPress ||
             event->type() == QEvent::MouseButtonRelease) {
    if (obj != this && isInputEventFromClientWindow(obj)) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      handleKeyboardMouseButton(
          mouseEvent->button(), event->type() == QEvent::MouseButtonPress);
    }
  } else if (event->type() == QEvent::MouseMove) {
    if (obj != this && isInputEventFromClientWindow(obj)) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      handleKeyboardMouseMove(mouseEvent->globalPosition().toPoint());
    }
  } else if (event->type() == QEvent::Wheel) {
    if (obj != this && isInputEventFromClientWindow(obj)) {
      auto *wheelEvent = static_cast<QWheelEvent *>(event);
      handleKeyboardMouseWheel(wheelEvent->angleDelta().y());
    }
  }

  return QMainWindow::eventFilter(obj, event);
}

// --- 事件拦截 - 保留以防万一，但主要逻辑已移至 eventFilter ---
bool MainWindow::event(QEvent *e) {
  if (e->type() == QEvent::ScreenChangeInternal) {
    QTimer::singleShot(0, this, [this]() { updatePopupResolutionScaling(); });
  }
  if (e->type() == QEvent::ShortcutOverride) {
    QKeyEvent *ke = static_cast<QKeyEvent *>(e);
    if (ke->key() == Qt::Key_Tab) {
      e->accept();
      return true;
    }
  }
  return QMainWindow::event(e);
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
  if (tryHandleSettingsPanelShortcut(event)) {
    event->accept();
    return;
  }

  if (tryHandleRespawnShortcut(event)) {
    event->accept();
    return;
  }

  // Ctrl+Shift+D: 飞镖命中测试 — 设置 BATTLE + 进入战术模式 + 触发敌方飞镖命中
  if (event->key() == Qt::Key_D &&
      event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
    if (m_gameData) {
      // 0. 判断敌方队伍（对方命中我方才会切图传）
      const RobotData *r = m_gameData->getCurrentRobot();
      const bool myIsRed = r ? (r->team == TeamColor::RED)
                             : (m_gameData->getCurrentRobotId() < 100);
      const int enemyTeam = myIsRed ? 2 : 1;
      // 1. 确保 BATTLE 阶段 (stage=4, 300s remaining)
      m_gameData->simulateGameTimeElapse(0); // 空操作，用于走通 GameStatus 路径
      robomaster::GameStatus status;
      status.set_current_stage(4);            // BATTLE
      status.set_stage_countdown_sec(300);
      status.set_stage_elapsed_sec(120);
      status.set_red_score(0);
      status.set_blue_score(0);
      status.set_current_round(1);
      status.set_total_rounds(1);
      status.set_is_paused(false);
      m_gameData->updateGameStatus(status);
      // 2. 进入战术模式
      applyTacticalMode(true);
      // 3. 触发敌方飞镖命中(前哨站, 5s 遮挡) — 敌方队伍命中我方目标
      m_gameData->simulateDartHit(enemyTeam, 1);
      qInfo() << "[DartTest] Ctrl+Shift+D: BATTLE set + tactical mode + enemy dart hit"
              << "enemyTeam=" << enemyTeam << "myIsRed=" << myIsRed;
    }
    event->accept();
    return;
  }

  // Ctrl+Shift+T: 时间前进 10s — 验证飞镖遮挡计时
  if (event->key() == Qt::Key_T &&
      event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
    if (m_gameData) {
      m_gameData->simulateGameTimeElapse(10);
      qInfo() << "[DartTest] Ctrl+Shift+T: time elapse 10s";
    }
    event->accept();
    return;
  }

  // Ctrl+T: 战术指挥屏切换
  if (event->key() == Qt::Key_T && event->modifiers() == Qt::ControlModifier) {
    applyTacticalMode(!m_tacticalMode);
    event->accept();
    return;
  }

  // macOS 使用 Command+Y（Qt ControlModifier），其它平台使用 Ctrl+Y：
  // 战术屏内部切换地图优先/图传优先，不重启图传链路。
  if (m_tacticalMode && isTacticalLayoutSwitchShortcut(event)) {
    toggleTacticalLayoutMode();
    event->accept();
    return;
  }

  // M 键原本用于切换操作界面小地图；战术模式下只切换本页浮层，不影响原路径。
  if (m_tacticalMode &&
      RM::InputHotkeyPolicy::isTacticalLargeMapToggleHotkey(event)) {
    setTacticalLargeMapMode(!m_tacticalLargeMapMode);
    event->accept();
    return;
  }

  // 事件提示测试键在战术模式下也要保留，否则无法验证官方/战术事件弹窗。
  if (isTacticalTimedEventPopupTestHotkey(event)) {
    showTacticalTimedEventPopupForTest();
    event->accept();
    return;
  }
  if (isOfficialEventPopupTestHotkey(event)) {
    showOfficialEventPopup(
        QStringLiteral("EVENT TEST"),
        QStringLiteral("临时测试弹窗：用于确认官方事件面板的样式、尺寸和居中位置。"),
        QStringLiteral("#36d6ff"), m_officialEventPopupDurationMs);
    event->accept();
    return;
  }

  // 战术模式下仅保留必须的浮层快捷键，其他快捷键忽略。
  if (m_tacticalMode &&
      !RM::InputHotkeyPolicy::isTacticalOverlayHotkey(event)) {
    QMainWindow::keyPressEvent(event);
    return;
  }

  // Ctrl+G: 切换自定义 UI 叠加层
  if (event->key() == Qt::Key_G && event->modifiers() == Qt::ControlModifier) {
    if (m_gameData) {
      bool current = m_gameData->customUIEnabled();
      m_gameData->setCustomUIEnabled(!current);
      qInfo() << "MainWindow: CustomUI overlay" << (current ? "DISABLED" : "ENABLED");
    }
    event->accept();
    return;
  }

  if (RM::InputHotkeyPolicy::isDamagePanelHotkey(event)) {
    if (!m_qmlDamagePanel) {
      m_qmlDamagePanel = new QQuickWidget(this);
      m_qmlDamagePanel->setResizeMode(QQuickWidget::SizeViewToRootObject);
      m_qmlDamagePanel->setFocusPolicy(Qt::NoFocus);
      m_qmlDamagePanel->setAttribute(Qt::WA_TransparentForMouseEvents);
      m_qmlDamagePanel->setAttribute(Qt::WA_TranslucentBackground);
      m_qmlDamagePanel->setClearColor(Qt::transparent);
      m_qmlDamagePanel->rootContext()->setContextProperty("gameData",
                                                          m_gameData);
      m_qmlDamagePanel->setSource(QUrl("qrc:/qml/DamagePanel.qml"));
    }

    applyPanelResolutionScaling();
    m_qmlDamagePanel->move(qMax(8, qRound(10.0 * getPanelScaleFactor())),
                           (height() - m_qmlDamagePanel->height()) / 2);
    safeQuickShow(m_qmlDamagePanel);
    m_qmlDamagePanel->raise();
    ensureOverlayRaisedIfActive();
    ensureInteractivePanelsRaisedIfVisible();
    event->accept();
    return;
  }

  if (event->isAutoRepeat()) {
    QMainWindow::keyPressEvent(event);
    return;
  }

  auto invokeSiloMethod = [this](const char *methodName) -> bool {
    if (!m_qmlSiloPanel || !m_qmlSiloPanel->isVisible() ||
        m_qmlSiloPanel->status() != QQuickWidget::Ready) {
      return false;
    }
    QObject *rootObj = m_qmlSiloPanel->rootObject();
    if (!rootObj) {
      return false;
    }
    return QMetaObject::invokeMethod(rootObj, methodName);
  };

  auto siloIsConfirming = [this]() -> bool {
    if (!m_qmlSiloPanel || !m_qmlSiloPanel->isVisible() ||
        m_qmlSiloPanel->status() != QQuickWidget::Ready) {
      return false;
    }
    QObject *rootObj = m_qmlSiloPanel->rootObject();
    if (!rootObj) {
      return false;
    }
    return rootObj->property("panelState").toString() == "confirming";
  };

  // 处理控制键 (Ctrl)
  if (event->key() == Qt::Key_Control) {
    if (m_controlBar)
      m_controlBar->setValue(100);
  }

  // 设置面板有焦点时自行处理按键；这里仅处理全局快捷键。

  const Qt::KeyboardModifiers blockedModifiers =
      Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
  if (m_miniMapLarge && m_miniMapLarge->isVisible() &&
      !(event->modifiers() & blockedModifiers) &&
      isMiniMapCommandKey(event->key())) {
    setMiniMapCommandMode(static_cast<Qt::Key>(event->key()));
    event->accept();
    return;
  }

  if (handleKeyboardMouseControlKey(event, true)) {
    // 保留原始键盘上报，同时独立驱动本地飞镖确认界面；焦点位于子控件或 QML
    // 表面时，上方事件过滤路径执行相同组合。
    tryHandleSiloOpenShortcut(event);
    event->accept();
    return;
  }

  switch (event->key()) {
  case Qt::Key_F:
    if (event->modifiers() & Qt::ControlModifier) {
      // Ctrl+F：循环切换弹药显示模式
      // 0：正常
      // 1：堡垒加成（+50）
      // 2：无限弹药（>1000）
      if (m_gameData) {
        static int ammoMode = 0;
        ammoMode = (ammoMode + 1) % 3;

        quint16 bonus = 0;
        QString modeName;

        switch (ammoMode) {
        case 0: // 正常
          bonus = 0;
          modeName = "Normal";
          break;
        case 1: // 堡垒加成
          bonus = 50;
          modeName = "Fortress Bonus (+50)";
          break;
        case 2:
          modeName = "Infinite Ammo";
          break;
        }

        // 为红方相关机器人统一设置，保证当前观察对象切换后界面仍能更新。

        // 先重置所有字段，避免模式切换残留旧状态。
        for (int i = 1; i <= 7; i++) {
          // 重置加成弹量
          m_gameData->setFortressBonusAmmo(i, 0);
          // 允许发弹量归零：步兵、空中和哨兵使用 17 mm，英雄使用 42 mm。
          if (i == 1) {
            m_gameData->setAllowedAmmo42mm(i, 0);
          } else {
            m_gameData->setAllowedAmmo17mm(i, 0);
          }
        }

        if (ammoMode == 2) {
          // 无限弹药模式用较大的 allowedAmmo 表示。
          for (int i = 1; i <= 7; i++) {
            if (i == 1) {
              m_gameData->setAllowedAmmo42mm(i, 2000);
            } else {
              m_gameData->setAllowedAmmo17mm(i, 2000);
            }
          }
        } else if (ammoMode == 1) {
          // 堡垒加成
          for (int i = 1; i <= 7; i++) {
            m_gameData->setFortressBonusAmmo(i, bonus);
          }
        }

        qDebug() << "Test: Cycle Ammo Mode:" << ammoMode << modeName;
      }
    } else {
      if (tryHandleRuneHoldShortcut()) {
        break;
      }

      if (tryHandleSiloOpenShortcut(event)) {
        break;
      }
    }
    break;
  case Qt::Key_P:
  case Qt::Key_Plus:
  case Qt::Key_Minus:
  case Qt::Key_Asterisk:
  case Qt::Key_Slash:
    // P / +-*/ 键显示/隐藏 QML 设置面板。
    // QQuickWidget::hide() 在 Linux/X11 上销毁 OpenGL 上下文会触发 SIGSEGV，
    // 改用 safeQuickHide/safeQuickShow 通过 QML visible 属性控制显隐。
    qDebug() << "Settings panel hotkey pressed - toggling Settings panel";
    toggleSettingsPanel();
    break;

    // K键模拟击杀事件 (测试用)
  case Qt::Key_J:
    if (event->modifiers() & Qt::ControlModifier) {
      // Ctrl+J：循环切换热量测试模式
      // 0：重置（热量为 0，未锁定）
      // 1：正常热量（先设为约 50%，再逐步冷却）
      // 2：过热（超过上限并锁定，再冷却至解锁）
      if (m_gameData) {
        static int heatMode = 0;
        heatMode = (heatMode + 1) % 3; // 0 -> 1 -> 2 -> 0

        // 确保冷却模拟定时器已创建
        QTimer *coolingTimer = this->findChild<QTimer *>("debugCoolingTimer");
        if (!coolingTimer) {
          coolingTimer = new QTimer(this);
          coolingTimer->setObjectName("debugCoolingTimer");
          coolingTimer->setInterval(20); // 50 Hz 更新
          connect(coolingTimer, &QTimer::timeout, this, [this, coolingTimer]() {
            if (!m_gameData) {
              coolingTimer->stop();
              return;
            }

            bool anyActive = false;
            // 遍历所有机器人，模拟全局冷却和裁判锁定逻辑。
            const auto &robots = m_gameData->getAllRobots();
            QList<int> ids;
            for (const auto &r : robots)
              ids << r.robotId;

            for (int i : ids) {
              RobotData *robot =
                  const_cast<RobotData *>(m_gameData->getRobotById(i));
              if (!robot)
                continue;

              // 只处理仍有热量或处于锁定状态的机器人。
              if (robot->currentHeat == 0 && !robot->shooterLocked)
                continue;

              anyActive = true;

              // 1. 冷却
              if (robot->currentHeat > 0) {
                // 优先使用机器人的 coolingValue，否则采用默认冷却率。
                int rate =
                    (robot->coolingValue > 0) ? robot->coolingValue : 120;
                int drop = std::max(1, rate / 50); // 50 Hz（20 ms）
                int newHeat = std::max(0, (int)robot->currentHeat - drop);

                m_gameData->setRobotHeat(i, static_cast<quint16>(newHeat));
              }

              // 2. 模拟裁判系统自动解锁，仅在热量降为 0 后解除。
              if (robot->shooterLocked) {
                if (robot->currentHeat == 0) {
                  robot->shooterLocked = false;
                  // 锁定状态变化后主动刷新界面
                  emit m_gameData->robotDataUpdated(i);
                  if (i == m_gameData->getCurrentRobotId()) {
                    emit m_gameData->myRobotUpdated();
                  }
                }
              }
            }

            if (!anyActive) {
              coolingTimer->stop();
            }
          });
        }

        // 按测试模式设置初始状态
        int currentRobotId = m_gameData->getCurrentRobotId();
        QList<int> targetRobots;
        if (currentRobotId > 0) {
          targetRobots << currentRobotId;
        } else {
          for (int i = 1; i <= 7; i++)
            targetRobots << i;
        }

        quint16 initHeat = 0;
        bool initLocked = false;
        bool startTimer = false;

        switch (heatMode) {
        case 0: // 重置
          initHeat = 0;
          initLocked = false;
          startTimer = false;
          coolingTimer->stop();
          break;
        case 1:           // 正常热量
          initHeat = 100; // 没有上限时使用默认值
          initLocked = false;
          startTimer = true;
          break;
        case 2:           // 过热
          initHeat = 300; // 没有上限时使用默认值
          initLocked = true;
          startTimer = true;
          break;
        }

        for (int i : targetRobots) {
          RobotData *robot =
              const_cast<RobotData *>(m_gameData->getRobotById(i));
          if (robot) {
            quint16 limit = (robot->heatLimit > 0) ? robot->heatLimit : 240;
            quint16 targetH = initHeat;

            if (heatMode == 1)
              targetH = limit / 2;
            if (heatMode == 2)
              targetH = limit + 100;
            if (heatMode == 0)
              targetH = 0;

            // 设置发射机构锁定状态
            robot->shooterLocked = initLocked;

            m_gameData->setRobotHeat(i, targetH);

            // setRobotHeat 只处理热量；手动修改锁定状态后需额外通知界面。
            emit m_gameData->robotDataUpdated(i);
            if (i == m_gameData->getCurrentRobotId()) {
              emit m_gameData->myRobotUpdated();
            }
          }
        }

        if (startTimer) {
          coolingTimer->start();
        }
      }
    } else {
      if (invokeSiloMethod("switchTarget")) {
        break;
      }
    }
    break;
  case Qt::Key_K: {
    if ((event->modifiers() & Qt::ControlModifier) &&
        (event->modifiers() & Qt::ShiftModifier)) {
      // Ctrl+Shift+K: 保留原有连杀调试逻辑
      static int testStreak = 0;
      testStreak = (testStreak % 5) + 1;
      qDebug() << "Test: Simulating Kill Streak (Updated):" << testStreak;
      bool isFirstBlood = (testStreak == 1);
      if (m_gameData) {
        KillRecord record(1, 103, testStreak, isFirstBlood);
        emit m_gameData->killEventOccurred(record);
      }
    } else if (event->modifiers() & Qt::ControlModifier) {
      // Ctrl+K: 原 K 键模拟逻辑 (裁判警告、基地/前哨站等)
      static int testMode = 0;
      testMode = (testMode + 1) % 10;

      if (testMode == 0) {
        qDebug() << "Simulating Kill: Red(1) -> Blue(101)";
        m_gameData->recordKill(1, 101);
      } else if (testMode == 1) {
        qDebug() << "Simulating Referee Warning: Yellow Card";
        robomaster::RefereeWarningData data;
        data.set_level(2); // 黄牌
        data.set_offending_robot_id(1);
        data.set_count(1);
        data.set_source("TEST");
        data.set_timestamp(QDateTime::currentMSecsSinceEpoch());
        emit m_gameData->updateRefereeWarning(data);
      } else if (testMode == 2) {
        qDebug() << "Simulating Kill: Blue(101) -> Red(1)";
        m_gameData->recordKill(101, 1);
      } else if (testMode == 3) {
        qDebug() << "Simulating Referee Warning: Red Card";
        robomaster::RefereeWarningData data;
        data.set_level(3);                // 红牌
        data.set_offending_robot_id(102); // 蓝方2号 (100+2)
        data.set_count(1);
        data.set_source("TEST");
        data.set_timestamp(QDateTime::currentMSecsSinceEpoch());
        emit m_gameData->updateRefereeWarning(data);
      } else if (testMode == 4) {
        qDebug() << "Simulating Outpost Destroyed: Red(1) -> Blue Outpost";
        if (m_battleMessage)
          m_battleMessage->showOutpostDestroyed(false);
      } else if (testMode == 5) {
        qDebug() << "Simulating Outpost Destroyed: Blue(101) -> Red Outpost";
        if (m_battleMessage)
          m_battleMessage->showOutpostDestroyed(true);
      } else if (testMode == 6) {
        qDebug() << "Simulating Small Rune Activatable";
        // 使用新的 RuneData 结构更新
        RuneData rune;
        rune.status = 1;           // 未激活
        rune.isActivatable = true; // 可激活
        rune.type = 0;             // 小符
        emit m_gameData->runeStatusChanged(rune);
      } else if (testMode == 7) {
        qDebug() << "Simulating Large Rune Activated";
        // 使用新的 RuneData 结构更新
        RuneData rune;
        rune.status = 3; // 已激活
        rune.isActivatable = true;
        rune.type = 1;          // 大符
        rune.activatedArms = 5; // 全部点亮
        emit m_gameData->runeStatusChanged(rune);
      } else if (testMode == 8) {
        qDebug() << "Simulating Red Base Status Change: Armor Closed";
        if (m_battleMessage)
          m_battleMessage->showBaseStatusChange(true, 1);
      } else if (testMode == 9) {
        qDebug() << "Simulating Blue Base Status Change: Armor Open";
        if (m_battleMessage)
          m_battleMessage->showBaseStatusChange(false, 2);
      }
    } else {
      //  K 交由 QML 内部署逻辑处理（长按进入部署）
      QMainWindow::keyPressEvent(event);
      return;
    }
  } break;

  case Qt::Key_L: {
    if (!(event->modifiers() & Qt::ControlModifier)) {
      // 1) 工程机器人：L键工程推出矿石
      if (tryHandleEngineerExitShortcut()) {
        break;
      }

      // 2) 空中机器人：L 键触发飞镖发射
      if (invokeSiloMethod("requestFire")) {
        break;
      }

      // 3) 其他情况
      QMainWindow::keyPressEvent(event);
      return;
    }

    if (!(event->modifiers() & Qt::ShiftModifier)) {
      refreshHeroVideoStream();
      event->accept();
      return;
    }

    // 循环切换射速限制测试模式
    // 0：正常
    // 1：一级超速（15 秒）
    // 2：二级超速（20 秒）
    // 3：严重超速（永久）

    static int speedTestMode = 0;
    speedTestMode = (speedTestMode + 1) % 4;

    qDebug() << "Test: Speed Limit Mode:" << speedTestMode;

    // 获取当前机器人
    int robotId = m_gameData->getCurrentRobotId();
    // ID 为 0 时回退到 1 号机器人
    if (robotId == 0)
      robotId = 1;

    RobotData *robot =
        const_cast<RobotData *>(m_gameData->getRobotById(robotId));
    if (robot) {
      float limit = robot->shootSpeedLimit;
      if (limit <= 0)
        limit = 30.0f; // 安全兜底值

      float speed = 0;
      bool locked = false;
      bool triggerPenalty = false;

      // 按机器人类型设置对应处罚区间：英雄 42 mm 使用倍率阈值，步兵
      // 17 mm 使用绝对超速值。

      bool isHero = (robot->type == ::RobotType::HERO);

      switch (speedTestMode) {
      case 0: // 正常
        speed = limit - 1.0f;
        if (speed < 0)
          speed = 0;
        locked = false;
        triggerPenalty = false;
        break;
      case 1: // 一级超速
        if (isHero) {
          speed = limit * 1.05f; // 低于 1.1 倍
        } else {
          speed = limit + 2.0f; // 增量小于 5
        }
        locked = true;
        triggerPenalty = true;
        break;
      case 2: // 二级超速
        if (isHero) {
          speed = limit * 1.15f; // 低于 1.2 倍
        } else {
          speed = limit + 7.0f; // 增量小于 10
        }
        // 英雄部署模式可能直接进入永久处罚；这里仍保留二级区间用于测试。
        locked = true;
        triggerPenalty = true;
        break;
      case 3: // 严重超速
        if (isHero) {
          speed = limit * 1.3f; // > 1.2x
        } else {
          speed = limit + 12.0f; // > +10
        }
        locked = true;
        triggerPenalty = true;
        break;
      }

      robot->firerate = speed;
      robot->shooterLocked = locked; // 测试时同步发射机构锁定状态

      if (!triggerPenalty) {
        // 手动重置处罚状态
        robot->speedLockState = SpeedLockState::Normal;
        robot->speedLockSeconds = 0;
      }

      // 通知数据更新
      emit m_gameData->robotDataUpdated(robotId);
      if (robotId == m_gameData->getCurrentRobotId()) {
        emit m_gameData->myRobotUpdated();
      }

      // 生成测试提示
      QString msg = QString("Speed Test: Mode %1 (Limit=%2, v=%3)")
                        .arg(speedTestMode)
                        .arg(limit)
                        .arg(speed);
    }
  } break;

  case Qt::Key_H: {
    if (m_tacticalMode) {
      if (m_qmlExchangePanel) {
        safeQuickHide(m_qmlExchangePanel);
      }
      updateExchangeHintText(m_selectedRobotTypeFromSettings);
      break;
    }

    if (!m_exchangeHintLoginActive) {
      if (m_qmlExchangePanel) {
        safeQuickHide(m_qmlExchangePanel);
      }
      updateExchangeHintText(m_selectedRobotTypeFromSettings);
      break;
    }

    bool isEngineer = m_exchangeQmlSource.contains("ExchangePanelEngineer.qml");
    bool isAerial = m_exchangeQmlSource.contains("ExchangePanelAerial.qml");
    if (!isEngineer && !isAerial && !canOpenRemoteExchangePanel()) {
      if (m_qmlExchangePanel) {
        safeQuickHide(m_qmlExchangePanel);
        m_qmlExchangePanel->setEnabled(false);
      }
      updateExchangeHintText(m_exchangeQmlSource);
      break;
    }

    if (m_qmlExchangePanel) {
      m_qmlExchangePanel->setEnabled(true);
    }

    // H键显示QML兑换面板
    if (!m_qmlExchangePanel) {
      m_qmlExchangePanel = new QQuickWidget(this);
      m_qmlExchangePanel->setResizeMode(QQuickWidget::SizeRootObjectToView);
      m_qmlExchangePanel->setAttribute(Qt::WA_TranslucentBackground);
      m_qmlExchangePanel->setClearColor(Qt::transparent);
      m_qmlExchangePanel->rootContext()->setContextProperty("gameData",
                                                            m_gameData);

      // 将 network 对象传递给 QML
      if (m_networkManager) {
        m_qmlExchangePanel->rootContext()->setContextProperty("network",
                                                              m_networkManager);
      }
      m_qmlExchangePanel->setSource(QUrl(m_exchangeQmlSource));
      if (QObject *rootObj = m_qmlExchangePanel->rootObject()) {
        rootObj->setProperty("selectedRobotType", m_selectedRobotTypeFromSettings);
        connectIfQmlSignalExists(rootObj, "commonCommandRequested(int,int)",
                                 this,
                                 "onCommonCommandRequested(int,int)");
        connectIfQmlSignalExists(rootObj, "exchangeRequested(int)", this,
                                 "onExchangeRequested(int)");
        connectIfQmlSignalExists(rootObj, "exchangeValue(int,int)", this,
                                 "onEngineerExchangeValue(int,int)");
        connectIfQmlSignalExists(rootObj, "ammoExchangeSucceeded(int)", this,
                                 "onEngineerAmmoExchangeSucceeded(int)");
      }

    }
    updateExchangePanelGeometry();

    if (isQuickPanelEffectivelyVisible(m_qmlExchangePanel)) {
      if (isEngineer) {
        if (QObject *rootObj = m_qmlExchangePanel->rootObject()) {
          QMetaObject::invokeMethod(rootObj, "closeAllPopups");
        }
      }
      safeQuickHide(m_qmlExchangePanel);
      if (m_hKeyHint)
        m_hKeyHint->show();
      if (m_exchangeHintBox)
        m_exchangeHintBox->show();
    } else {
      safeQuickShow(m_qmlExchangePanel);
      m_qmlExchangePanel->raise();
      ensureOverlayRaisedIfActive();
      m_qmlExchangePanel->setFocusPolicy(Qt::StrongFocus);
      m_qmlExchangePanel->setFocus(Qt::ShortcutFocusReason);
      if (m_exchangeHint)
        m_exchangeHint->hide();
      if (m_hKeyHint)
        m_hKeyHint->hide();
      if (m_exchangeHintBox)
        m_exchangeHintBox->hide();
    }
    updateExchangeHintText("");
  } break;

  case Qt::Key_R:
    // Ctrl+R 测试显示比赛结果
    if (event->modifiers() & Qt::ControlModifier) {
      if (m_gameResultWidget) {
        if (m_gameResultWidget->isVisible()) {
          m_gameResultWidget->hide();
        } else {
          // 红方胜利与蓝方胜利交替显示
          static bool testBlueWin = false;
          testBlueWin = !testBlueWin; // 保留测试状态切换
          // 每次重新打开时切换胜方，关闭动作不改变下一次显示结果。
          static bool nextIsBlue = false;

          GameResultWidget::GameResult result =
              nextIsBlue ? GameResultWidget::BlueWin : GameResultWidget::RedWin;

          // 模拟数据
          m_gameResultWidget->setGameData(2, 1, 3, "04:59");
          m_gameResultWidget->setTeamData("复旦大学", "上海交通大学", "Fudan",
                                          "SJTU");

          // 模拟机器人数据 (含伤害统计)
          QMap<int, GameResultWidget::RobotStats> redRobots;
          QMap<int, GameResultWidget::RobotStats> blueRobots;

          // 红方机器人：1 英雄、2 工程、3/4/5 步兵、6 空中、7 哨兵。
          auto addRobot = [](QMap<int, GameResultWidget::RobotStats> &map,
                             int id, int hp, int maxHp, int damage) {
            GameResultWidget::RobotStats s;
            s.currentHP = hp;
            s.maxHP = maxHp;
            s.damageDealt = damage;
            s.isAlive = (hp > 0);
            map.insert(id, s);
          };

          addRobot(redRobots, 1, 2580, 4000, 2580); // 英雄
          addRobot(redRobots, 3, 647, 600, 647);    // 步兵
          addRobot(redRobots, 4, 697, 600, 697);    // 步兵
          addRobot(redRobots, 5, 600, 600, 450);    // 步兵
          addRobot(redRobots, 6, 0, 300, 120);      // 空中机器人，已阵亡
          addRobot(redRobots, 7, 864, 600, 864);    // 哨兵

          addRobot(blueRobots, 101, 2453, 4000, 232); // 英雄
          addRobot(blueRobots, 103, 0, 600, 783);     // 步兵，已阵亡
          addRobot(blueRobots, 104, 344, 600, 1382);  // 步兵
          addRobot(blueRobots, 105, 0, 600, 400);     // 步兵，已阵亡
          addRobot(blueRobots, 106, 0, 300, 120);     // 空中机器人，已阵亡
          addRobot(blueRobots, 107, 1617, 600,
                   836); // 哨兵，测试数据使用增强血量

          m_gameResultWidget->setRobotStats(redRobots, blueRobots);

          // 确保居中 (先定位，再启动动画)
          updateGameResultWidgetGeometry();

          m_gameResultWidget->showResult(result,
                                         GameResultWidget::BaseDestroyed);
          m_gameResultWidget->show();
          updateGameResultWidgetGeometry();

          // 为下一次打开切换胜方
          nextIsBlue = !nextIsBlue;
        }
      }
    }
    break;
  // 按下M按键模拟非脱战状态
  case Qt::Key_Y:
    if (siloIsConfirming() && invokeSiloMethod("confirmOpen")) {
      break;
    }
    break;

  case Qt::Key_N:
    if (siloIsConfirming() && invokeSiloMethod("cancelConfirm")) {
      break;
    }
    if (tryHandleAerialAirSupportInterruptShortcut()) {
      break;
    }
    // 切换脱战状态 isOutOfCombat
    if (m_gameData) {
      m_gameData->toggleCurrentRobotOutOfCombat();
    }
    break;

  // 大地图将小地图复制且放大在项目中间
  case Qt::Key_M: {
    toggleLargeMiniMap();
  } break;
  // 按下O/I键购买弹丸
  case Qt::Key_O:
    ensureAmmoPanel(m_qmlAmmoSupply17Panel, this, m_gameData,
                    "17mm弹丸补给面板", "17mm",
                    RM::ExchangeCommandPolicy::kExchange17mmCommand,
                    100, 150, 1000, this,
                    "onCommonCommandRequested(int,int)");
    m_qmlAmmoSupply17Panel->isVisible() ? m_qmlAmmoSupply17Panel->hide()
                                        : showPanel(m_qmlAmmoSupply17Panel);
    break;

  case Qt::Key_I:
    if (m_miniMapLarge && m_miniMapLarge->isVisible()) {
      setMiniMapCommandMode(Qt::Key_I);
      break;
    }
    ensureAmmoPanel(m_qmlAmmoSupply42Panel, this, m_gameData,
                    "42mm弹丸补给面板", "42mm",
                    RM::ExchangeCommandPolicy::kExchange42mmCommand,
                    10, 150, 100, this,
                    "onCommonCommandRequested(int,int)");
    m_qmlAmmoSupply42Panel->isVisible() ? m_qmlAmmoSupply42Panel->hide()
                                        : showPanel(m_qmlAmmoSupply42Panel);
    break;
  // 按下Q按键模拟兑换金币复活
  case Qt::Key_Q:
    if (m_gameData) {
      // 构造两个 QVariantMap 模拟复活进度的突然中断
      QVariantMap statusBefore;
      statusBefore["is_pending_respawn"] = true;
      statusBefore["current_respawn_progress"] = 50;
      statusBefore["total_respawn_progress"] = 100;
      m_gameData->processRobotRespawnStatusMap(statusBefore);

      // 模拟下一帧：立即完成（is_pending 变为 false，进度未满 100）
      QVariantMap statusAfter;
      statusAfter["is_pending_respawn"] = false;
      statusAfter["current_respawn_progress"] = 50;
      statusAfter["total_respawn_progress"] = 100;
      m_gameData->processRobotRespawnStatusMap(statusAfter);
    }
    break;
  case Qt::Key_Escape:
    // ESC键隐藏所有面板
    hideAllPanels();
    break;
  default:
    QMainWindow::keyPressEvent(event);
    return;
  }
  event->accept();
}

void MainWindow::keyReleaseEvent(QKeyEvent *event) {
  if (tryHandleRespawnShortcut(event)) {
    event->accept();
    return;
  }

  if (event->key() == Qt::Key_Control) {
    if (m_controlBar)
      m_controlBar->setValue(0);
  }
  // Tab键松开时隐藏QML面板
  if (event->key() == Qt::Key_Tab) {
    if (m_qmlTabPanel)
      m_qmlTabPanel->hide();
  }
  // ~键松开时隐藏QML伤害面板
  if (RM::InputHotkeyPolicy::isDamagePanelHotkey(event)) {
    if (m_qmlDamagePanel)
      safeQuickHide(m_qmlDamagePanel);
  }

  if (!event->isAutoRepeat() && event->key() == Qt::Key_F) {
    endRuneHoldShortcut();
  }
  if (handleKeyboardMouseControlKey(event, false)) {
    event->accept();
    return;
  }

  updateSiloPanelVisibility();
  QMainWindow::keyReleaseEvent(event);
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
  handleKeyboardMouseButton(event->button(), true);
  if (event->button() == Qt::LeftButton) {
    if (m_triggerBar)
      m_triggerBar->setValue(100);
  }
  QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
  handleKeyboardMouseButton(event->button(), false);
  if (event->button() == Qt::LeftButton) {
    if (m_triggerBar)
      m_triggerBar->setValue(0);
  }
  QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
  handleKeyboardMouseMove(event->globalPosition().toPoint());
  QMainWindow::mouseMoveEvent(event);
}

void MainWindow::wheelEvent(QWheelEvent *event) {
  handleKeyboardMouseWheel(event->angleDelta().y());
  QMainWindow::wheelEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  updatePopupResolutionScaling();
  //面板大小自适应
  applyPanelResolutionScaling();
  if (m_qmlSiloPanel && m_centralWidget) {
    m_qmlSiloPanel->setGeometry(m_centralWidget->rect());
    if (!m_tacticalMode && m_qmlSiloPanel->isVisible()) {
      m_qmlSiloPanel->raise();
    }
  }
  if (m_qmlRunePanel && m_centralWidget) {
    m_qmlRunePanel->setGeometry(m_centralWidget->rect());
    if (m_qmlRunePanel->status() == QQuickWidget::Ready) {
      if (QObject *rootObj = m_qmlRunePanel->rootObject()) {
        const int topAreaHeight = m_topArea ? m_topArea->height()
                                            : MainLayout::getTopAreaHeight(m_screenHeight);
        rootObj->setProperty("panelLeftMargin", MainLayout::LAYOUT_MARGIN);
        rootObj->setProperty("panelTopMargin", topAreaHeight + 6);
      }
    }
    if (m_qmlRunePanel->isVisible()) {
      m_qmlRunePanel->raise();
    }
  }
  if (m_qmlGunnerOverlay && m_centralWidget) {
    m_qmlGunnerOverlay->setGeometry(m_centralWidget->rect());
  }
  if (m_qmlTacticalPage && m_centralWidget) {
    m_qmlTacticalPage->setGeometry(m_centralWidget->rect());
  }
  updateTacticalLoginButtonGeometry();
#ifndef Q_OS_LINUX
  if (m_tacticalMode && m_tacticalLoginButton &&
      m_tacticalLoginButton->isVisible()) {
    m_tacticalLoginButton->raise();
  }
#endif
  for (auto *popup : m_tacticalTimedEventPopups) {
    if (popup && popup->isVisible()) {
      updateTacticalTimedEventPopupGeometry();
      break;
    }
  }
  if (m_qmlOfficialEventPopupPanel && m_qmlOfficialEventPopupPanel->isVisible()) {
    updateOfficialEventPopupGeometry();
  }
  scheduleOverlayLayerRestack();
  updateHeroVideoWidgetGeometry();
  if (m_videoBackground) {
    m_videoBackground->setGeometry(m_centralWidget->rect());
  }
  if (m_helpOverlay) {
    m_helpOverlay->setGeometry(rect());
    m_helpOverlay->raise();
  }
  if (m_gameResultWidget && m_gameResultWidget->isVisible()) {
    updateGameResultWidgetGeometry();
  }

  // 准星容器绝对居中 (优先于其他元素)
  if (m_aimingContainer && m_centerArea && m_centralWidget) {
    const QPoint screenCenter(m_centralWidget->width() / 2,
                              m_centralWidget->height() / 2);
    const QPoint targetCenter =
        m_centerArea->mapFrom(m_centralWidget, screenCenter);
    const int cx = targetCenter.x() - m_aimingContainer->width() / 2;
    const int cy = targetCenter.y() - m_aimingContainer->height() / 2;

    m_aimingContainer->move(cx, cy);
    m_aimingContainer->raise(); // 确保在其他元素之上
  }

  if (m_qmlEventMessagePanel && m_centerArea && m_centralWidget) {
    const QPoint screenCenter(m_centralWidget->width() / 2,
                              m_centralWidget->height() / 2);
    const QPoint targetCenter =
        m_centerArea->mapFrom(m_centralWidget, screenCenter);
    int panelX = targetCenter.x() -
                 m_qmlEventMessagePanel->width() / 2;
    int panelY = targetCenter.y() + 72;

    panelX = qMax(0, qMin(panelX,
                          m_centerArea->width() -
                              m_qmlEventMessagePanel->width()));
    panelY = qMax(0, qMin(panelY,
                          m_centerArea->height() -
                              m_qmlEventMessagePanel->height()));
    m_qmlEventMessagePanel->move(panelX, panelY);
    m_qmlEventMessagePanel->raise();
  }

  // 准星居中 (备用)
  if (m_crosshair) {
    m_crosshair->move((width() - m_crosshair->width()) / 2,
                      (height() - m_crosshair->height()) / 2);
  }

  if (m_battleMessage && m_centerArea) {
    // 设置战场消息的大小和位置
    // 宽度设为与中心区域一致，确保能居中显示长文本
    // 高度设为 200，足够显示警告框或击杀条
    m_battleMessage->resize(m_centerArea->width(), 200);

    // 移动到中心区域顶部 (y=20 留出一点边距)
    // 这样就满足了 "至于中间面板顶部" 的需求
    m_battleMessage->move(0, 20);
    m_battleMessage->raise();
  }

  updateExchangeHintOverlayGeometry();

  if (m_leftBottomPanel) {
    if (m_leftSideArea) {
      // LeftBottomPanel 跟随 LeftSideArea 的底部
      // 但由于 layout 管理，这里可能不需要手动 move，取决于 LeftBottomPanel
      // 是否在 layout 中
    }
  }

  // 统一 overlay 大小/位置维护（PopupOverlay 承担所有弹窗显示）
  if (m_qmlPopupOverlay && m_centralWidget) {
    m_qmlPopupOverlay->resize(m_centralWidget->size());
    m_qmlPopupOverlay->move(0, 0);
    const bool hasActivePopup =
        m_gameData && !m_gameData->activePopups().isEmpty();
    const bool hasRespawnPending =
        m_gameData &&
        m_gameData->robotRespawnStatus()
            .value("is_pending_respawn", false)
            .toBool();
    const bool shouldShow =
        RM::PopupOverlayPolicy::shouldActivateOverlay(hasActivePopup,
                                                      hasRespawnPending);
    m_qmlPopupOverlay->setAttribute(Qt::WA_TransparentForMouseEvents,
                                    !shouldShow);
    if (shouldShow) {
      m_qmlPopupOverlay->show();
      if (hasRespawnPending) {
        m_qmlPopupOverlay->setFocus(Qt::PopupFocusReason);
      }
    } else {
      m_qmlPopupOverlay->hide();
    }
  }
  scheduleOverlayLayerRestack();

  // 调整大地图居中
  if (m_miniMapLarge) {
    m_miniMapLarge->move((width() - m_miniMapLarge->width()) / 2,
                         (height() - m_miniMapLarge->height()) / 2);
  }
  updateMiniMapLegendPanel();

  // 调整 SettingsPanel 居中位置
  if (m_qmlSettingsPanel) {
    m_qmlSettingsPanel->move((width() - m_qmlSettingsPanel->width()) / 2,
                             (height() - m_qmlSettingsPanel->height()) / 2);
  }
  ensureInteractivePanelsRaisedIfVisible();
  updateDeployModePanelGeometry();
#ifdef RM_HAS_AR_OVERLAY
  // 调整 AR 叠加层大小以匹配视频区域
  if (m_arWidget && m_centerArea) {
    m_arWidget->resize(m_centerArea->size());
    m_arWidget->move(0, 0);
  }
#endif
}

// --- AR 系统槽函数 ---
void MainWindow::onARInitializationCompleted(bool success,
                                             const QString &message) {
#ifdef RM_HAS_AR_OVERLAY
  if (success) {
    qDebug() << "AR System Initialized Successfully";
    if (ConfigManager::instance().getAROverlayEnabled()) {
      m_arManager->setEnabled(true);
      if (m_arWidget)
        m_arWidget->show();
    }
  } else {
    qWarning() << "AR System Initialization Failed:" << message;
  }
#else
  Q_UNUSED(success);
  Q_UNUSED(message);
#endif
}

void MainWindow::onARSettingsChanged() {
#ifdef RM_HAS_AR_OVERLAY
  if (!m_arManager)
    return;

  ConfigManager &config = ConfigManager::instance();
  bool enabled = config.getAROverlayEnabled();

  // 如果未初始化成功，不开启
  if (enabled && !m_arManager->isInitialized()) {
    // AR 通常在启动时完成初始化；运行期启用但尚未初始化时，本路径暂不补做初始化。
    // 状态下开关
  }

  if (m_arManager->isInitialized()) {
    m_arManager->setEnabled(enabled);
    if (m_arWidget)
      m_arWidget->setVisible(enabled);
  }

  // 更新参数
  m_arManager->setConfidenceThreshold(config.getARConfidenceThreshold());
  m_arManager->setNMSThreshold(config.getARNMSThreshold());
  m_arManager->setSmoothingFactor(config.getARSmoothingFactor());
  m_arManager->setMaxMissedFrames(config.getARMaxMissedFrames());
  m_arManager->setDetectionInterval(config.getARDetectionInterval());
#endif
}

/**
 * @brief 处理模拟器生成的协议数据
 * @param type 数据包类型
 * @param data 原始数据
 */
void MainWindow::onSettingsLoginRequested(const QString &robotType) {
  qDebug() << "Login requested for:" << robotType;
  resetKeyboardMouseControlState(false);
  m_exchangeHintLoginActive = true;
  applySettingsRobotSelection(robotType,
                              resolveRobotIdFromSettingsSelection(robotType));
#ifdef RM_HAS_MQTT
  restartMqttForRobotType(robotType);
#endif
  updateExchangeHintText(robotType);
}

void MainWindow::onCommonCommandRequested(int commandType, int param) {
  if (!m_networkManager) {
    qWarning() << "MainWindow: exchange command dropped; NetworkManager unavailable"
               << "commandType=" << commandType << "param=" << param;
    return;
  }
  if (!RM::ExchangeCommandPolicy::isValidRequest(commandType, param)) {
    qWarning() << "MainWindow: invalid exchange command dropped"
               << "commandType=" << commandType << "param=" << param;
    return;
  }

  qInfo() << "MainWindow: dispatching exchange CommonCommand"
          << "commandType=" << commandType << "param=" << param;
  m_networkManager->sendCommonCommand(commandType, param);
}

void MainWindow::onSettingsLogoutRequested() {
  resetKeyboardMouseControlState(true);
  m_exchangeHintLoginActive = false;
  cancelScheduledMqttReconnect();
  qDebug() << "Settings requested logout current robot";
  if (m_qmlExchangePanel) {
    m_qmlExchangePanel->hide();
  }
  syncSettingsPanelState();
  updateExchangeHintText(QString());
}


void MainWindow::onSimulatorDataReceived(PacketType type,
                                         const QByteArray &data) {
  if (!m_gameData)
    return;

  switch (type) {
  case PacketType::BASE_HEALTH: {
    // 基地血量兼容数据，统一写入当前接口。
    if (data.size() >= 8) {
      quint8 teamId = data[0];
      quint16 currentHP;

      memcpy(&currentHP, data.data() + 1, 2);

      bool isInvincible = (data[5] != 0);
      TeamColor team = (teamId == 1) ? TeamColor::RED : TeamColor::BLUE;

      m_gameData->updateBaseHP(team, currentHP);

      // 按 GameData 的统一语义转换旧字段：0 表示无敌或有护盾，1 表示可受伤。
      if (isInvincible) {
        m_gameData->updateBaseStatus(team, 0); // 0：无敌或有护盾
      } else {
        m_gameData->updateBaseStatus(team, 1); // 1：可受伤（护甲未展开）
      }
    }
    break;
  }
  case PacketType::GAME_STATUS: {
    // 比赛状态更新处理
    robomaster::GameInfo info;
    if (info.ParseFromArray(data.data(), data.size())) {
      m_gameData->updateGameState(info);
    }
    break;
  }
  case PacketType::ROBOT_STATUS: {
    // 处理模拟器机器人状态。数据格式：
    // [id, level, hp(2), maxhp(2), power(2), heat(2), buffer(2)]
    if (data.size() >= 12) {
      quint8 id = data[0];
      RobotData robot;
      robot.robotId = id;
      robot.level = data[1];
      memcpy(&robot.currentHP, data.data() + 2, 2);
      memcpy(&robot.maxHP, data.data() + 4, 2);
      memcpy(&robot.power, data.data() + 6, 2);
      memcpy(&robot.currentHeat, data.data() + 8, 2);
      memcpy(&robot.bufferEnergy, data.data() + 10, 2);

      // 通过现有细粒度接口更新 GameData。
      m_gameData->updateRobotHealth(id, robot.currentHP);
      m_gameData->setRobotHeat(id, robot.currentHeat);
      m_gameData->updateRobotPower(id, robot.power);
      // bufferEnergy 暂无对应的公开更新接口，本路径暂不写入。
    }
    break;
  }
  case PacketType::REFEREE_WARNING: {
    if (data.size() >= sizeof(referee_warning_t)) {
      referee_warning_t warning_raw;
      memcpy(&warning_raw, data.data(), sizeof(referee_warning_t));

      robomaster::RefereeWarningData warning;
      warning.set_level(warning_raw.level);
      warning.set_offending_robot_id(warning_raw.offending_robot_id);
      warning.set_count(warning_raw.count);
      warning.set_source("SIMULATOR");
      warning.set_timestamp(QDateTime::currentMSecsSinceEpoch());

      m_gameData->updateRefereeWarning(warning);
    }
    break;
  }
  default:
    // 其他数据包类型暂不处理
    break;
  }
}

void MainWindow::onHeroVisibilityChanged(bool visible) {
  qDebug() << "[herovideo] MainWindow: HeroVideo visibility changed:" << visible;
  // 可以在此处根据 visible 做额外操作，例如切换其它 UI 元素的可见性
}

} // namespace RM
