#include "core/GameData.h"
#include "ui/MainWindow.h"
#include "widgets/HeroVideoWidget.h"

#include <QApplication>
#include <QImage>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWidget>
#include <QSignalSpy>
#include <QWidget>
#include <QtTest>

#include "robomaster.pb.h"

namespace {

int childStackIndex(QWidget *parent, QObject *child) {
  if (!parent || !child) {
    return -1;
  }
  return parent->children().indexOf(child);
}

} // namespace

class TestMainWindowTacticalHotkeyIntegration : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    qputenv("RM_DISABLE_AUDIO", "1");
    qunsetenv("RM_DEVTOOLS");
    qunsetenv("RM_S1_ENGINE_HOST");
    Q_INIT_RESOURCE(qml);
    Q_INIT_RESOURCE(resources);
  }

  void cleanupTestCase() {
    Q_CLEANUP_RESOURCE(resources);
    Q_CLEANUP_RESOURCE(qml);
  }

  void testTacticalMapStartsLargeAndMIsScoped() {
    // MainWindow 使用零延时 single-shot 延后启动服务。保持本测试同步执行，
    // 这样只覆盖 UI 路由，不启动网络、模拟器和视频服务。
    RM::MainWindow window;
    QWidget *clientTarget = window.centralWidget();
    QVERIFY(clientTarget);
    auto *tacticalPage =
        window.findChild<QQuickWidget *>(QStringLiteral("tacticalPageWidget"));
    QVERIFY(tacticalPage);
    QVERIFY(tacticalPage->rootObject());
    auto *gameData = window.findChild<GameData *>();
    auto *heroVideo = window.findChild<RM::HeroVideoWidget *>();
    QVERIFY(gameData);
    QVERIFY(heroVideo);
    QVERIFY(tacticalPage->rootObject()
                ->property("cameraRefreshEnabled")
                .toBool());
    QVERIFY(window.tacticalLargeMapMode());
    QVERIFY(window.tacticalLargeMapRendered());

    QSignalSpy modeChangedSpy(&window,
                              &RM::MainWindow::tacticalLargeMapModeChanged);
    QVERIFY(modeChangedSpy.isValid());

    QKeyEvent shortcutOverride(QEvent::ShortcutOverride, Qt::Key_M,
                               Qt::NoModifier);
    QApplication::sendEvent(clientTarget, &shortcutOverride);
    QVERIFY(shortcutOverride.isAccepted());
    QVERIFY(window.tacticalLargeMapMode());
    QCOMPARE(modeChangedSpy.count(), 0);

    QWidget unrelatedTopLevel;
    QKeyEvent unrelatedWindowM(QEvent::KeyPress, Qt::Key_M, Qt::NoModifier);
    QApplication::sendEvent(&unrelatedTopLevel, &unrelatedWindowM);
    QVERIFY(window.tacticalLargeMapMode());
    QCOMPARE(modeChangedSpy.count(), 0);

    // 现有飞镖模拟会依次经过 GameData 的飞镖信号和真实敌方命中共用的
    // 处理路径；触发后应退出启动时的大地图，切到操作/视频页。
    QKeyEvent dartHit(QEvent::KeyPress, Qt::Key_D,
                      Qt::ControlModifier | Qt::ShiftModifier);
    QApplication::sendEvent(&window, &dartHit);
    QVERIFY(dartHit.isAccepted());
    QVERIFY(!window.tacticalLargeMapMode());
    QVERIFY(!tacticalPage->rootObject()
                 ->property("cameraRefreshEnabled")
                 .toBool());
    QVERIFY(!window.tacticalLargeMapRendered());
    QCOMPARE(modeChangedSpy.count(), 1);

    // 操作页沿用原有 M 键行为，但不能改动战术页新增状态。
    QKeyEvent operationScreenM(QEvent::KeyPress, Qt::Key_M, Qt::NoModifier);
    QApplication::sendEvent(clientTarget, &operationScreenM);
    QVERIFY(operationScreenM.isAccepted());
    QVERIFY(!window.tacticalLargeMapMode());
    QCOMPARE(modeChangedSpy.count(), 1);

    QKeyEvent enterTacticalMode(QEvent::KeyPress, Qt::Key_T,
                                Qt::ControlModifier);
    QApplication::sendEvent(clientTarget, &enterTacticalMode);
    QVERIFY(enterTacticalMode.isAccepted());
    QVERIFY(tacticalPage->rootObject()
                ->property("cameraRefreshEnabled")
                .toBool());
    QVERIFY(!window.tacticalLargeMapMode());
    QCOMPARE(modeChangedSpy.count(), 1);

    QKeyEvent tacticalMAgain(QEvent::KeyPress, Qt::Key_M, Qt::NoModifier);
    QApplication::sendEvent(clientTarget, &tacticalMAgain);
    QVERIFY(tacticalMAgain.isAccepted());
    QVERIFY(window.tacticalLargeMapMode());
    QCOMPARE(modeChangedSpy.count(), 2);

    // QML 战术页显示期间，从工业相机机器人切换到 R3 后，不能重新显示
    // 先前非战术页面遗留的原生视频帧。
    heroVideo->setCurrentRobotId(6);
    heroVideo->setFrame(QImage(16, 16, QImage::Format_RGB32));
    heroVideo->hide();
    gameData->setCurrentRobotId(3);
    gameData->robotDataUpdated(3);
    QVERIFY(heroVideo->isHidden());

    QKeyEvent leaveTacticalMode(QEvent::KeyPress, Qt::Key_T,
                                Qt::ControlModifier);
    QApplication::sendEvent(clientTarget, &leaveTacticalMode);
    QVERIFY(leaveTacticalMode.isAccepted());
    QVERIFY(!tacticalPage->rootObject()
                 ->property("cameraRefreshEnabled")
                 .toBool());
    QVERIFY(heroVideo->isHidden());
    QVERIFY(!window.tacticalLargeMapMode());
    QCOMPARE(modeChangedSpy.count(), 3);
  }

  void testDartOcclusionReturnsToMapOnlyTacticalScreen() {
    RM::MainWindow window;
    QWidget *clientTarget = window.centralWidget();
    QVERIFY(clientTarget);

    auto *tacticalPage =
        window.findChild<QQuickWidget *>(QStringLiteral("tacticalPageWidget"));
    auto *gameData = window.findChild<GameData *>();
    QVERIFY(tacticalPage);
    QVERIFY(gameData);

    QKeyEvent dartHit(QEvent::KeyPress, Qt::Key_D,
                      Qt::ControlModifier | Qt::ShiftModifier);
    QApplication::sendEvent(&window, &dartHit);
    QVERIFY(dartHit.isAccepted());
    QVERIFY(!window.tacticalLargeMapMode());
    QVERIFY(tacticalPage->isHidden());

    // 生产策略把命中当秒计入遮挡，因此固定 10 秒的遮挡需经过
    // 11 次协议倒计时更新才归零。
    gameData->simulateGameTimeElapse(11);

    QVERIFY(!tacticalPage->isHidden());
    QVERIFY(window.tacticalLargeMapMode());
    QVERIFY(window.tacticalLargeMapRendered());

    // 自动恢复后，M 键仍可打开原有战术指令页。
    QKeyEvent showCommandScreen(QEvent::KeyPress, Qt::Key_M, Qt::NoModifier);
    QApplication::sendEvent(clientTarget, &showCommandScreen);
    QVERIFY(showCommandScreen.isAccepted());
    QVERIFY(!window.tacticalLargeMapMode());
  }

  void testTacticalLargeMapSitsBetweenPhasePopupAndTimedEventPopup() {
    RM::MainWindow window;
    window.resize(1920, 1080);
    window.show();
    QTest::qWait(50);

    QWidget *clientTarget = window.centralWidget();
    QVERIFY(clientTarget);

    auto *gameData = window.findChild<GameData *>();
    QVERIFY(gameData);

    auto *tacticalPage =
        window.findChild<QQuickWidget *>(QStringLiteral("tacticalPageWidget"));
    auto *officialPopup = window.findChild<QQuickWidget *>(
        QStringLiteral("officialEventPopupWidget"));
    auto *timedPopup = window.findChild<QQuickWidget *>(
        QStringLiteral("tacticalTimedEventPopup0"));
    auto *popupOverlay =
        window.findChild<QQuickWidget *>(QStringLiteral("popupOverlayWidget"));
    QVERIFY(tacticalPage);
    QVERIFY(officialPopup);
    QVERIFY(timedPopup);
    QVERIFY(popupOverlay);

    auto sendKeyPress = [&](int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
      QKeyEvent event(QEvent::KeyPress, key, modifiers);
      QApplication::sendEvent(clientTarget, &event);
      QVERIFY(event.isAccepted());
    };

    sendKeyPress(Qt::Key_M);
    QTRY_VERIFY(!tacticalPage->isHidden());

    const int officialDisplayToken =
        officialPopup->property("displayToken").toInt();
    sendKeyPress(Qt::Key_F9, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_VERIFY(officialPopup->property("displayToken").toInt() >
                officialDisplayToken);
    // 官方事件面板当前按产品策略保持隐藏，调试键只刷新事件内容。
    QVERIFY(officialPopup->isHidden());

    sendKeyPress(Qt::Key_F8, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_VERIFY(!timedPopup->isHidden());

    QTRY_VERIFY(childStackIndex(clientTarget, timedPopup) >
                childStackIndex(clientTarget, tacticalPage));

    sendKeyPress(Qt::Key_M);
    QTRY_VERIFY(window.tacticalLargeMapMode());
    QTRY_VERIFY(window.tacticalLargeMapRendered());
    QTRY_VERIFY(childStackIndex(clientTarget, tacticalPage) >
                childStackIndex(clientTarget, timedPopup));

    robomaster::GameStatus status;
    status.set_current_stage(1);
    status.set_stage_countdown_sec(30);
    status.set_stage_elapsed_sec(0);
    status.set_red_score(0);
    status.set_blue_score(0);
    status.set_current_round(1);
    status.set_total_rounds(1);
    status.set_is_paused(false);
    gameData->updateGameStatus(status);

    QTRY_VERIFY(!popupOverlay->isHidden());
    QTRY_VERIFY(childStackIndex(clientTarget, popupOverlay) >
                childStackIndex(clientTarget, tacticalPage));
    QTRY_VERIFY(childStackIndex(clientTarget, tacticalPage) >
                childStackIndex(clientTarget, timedPopup));
  }
};

QTEST_MAIN(TestMainWindowTacticalHotkeyIntegration)
#include "test_mainwindow_tactical_hotkey_integration.moc"
