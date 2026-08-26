#include "ui/MainWindowStatePolicy.h"

#include <QtTest>

class TestMainWindowStatePolicy : public QObject {
  Q_OBJECT

private slots:
  void testAutomaticReconnectPreservesActiveClientId() {
    QCOMPARE(RM::MainWindowStatePolicy::resolvedActiveMqttClientId(
                 QStringLiteral("6"), QString()),
             QStringLiteral("6"));
  }

  void testExplicitSwitchUsesRequestedClientId() {
    QCOMPARE(RM::MainWindowStatePolicy::resolvedActiveMqttClientId(
                 QStringLiteral("3"), QStringLiteral(" 6 ")),
             QStringLiteral("6"));
  }

  void testTransientMqttFailuresScheduleRetry() {
    QVERIFY(RM::MainWindowStatePolicy::shouldScheduleMqttRetry(
        QStringLiteral("MQTT transport error (rc=-1): connection refused/unreachable")));
    QVERIFY(RM::MainWindowStatePolicy::shouldScheduleMqttRetry(
        QStringLiteral("MQTT CONNACK(3): server unavailable")));
    QVERIFY(RM::MainWindowStatePolicy::shouldScheduleMqttRetry(
        QStringLiteral("MQTT transport error (rc=-3): timed out")));
  }

  void testPermanentMqttFailuresDoNotScheduleRetry() {
    QVERIFY(!RM::MainWindowStatePolicy::shouldScheduleMqttRetry(
        QStringLiteral("MQTT CONNACK(2): identifier rejected for clientId=6")));
    QVERIFY(!RM::MainWindowStatePolicy::shouldScheduleMqttRetry(
        QStringLiteral("MQTT CONNACK(5): not authorized")));
    QVERIFY(!RM::MainWindowStatePolicy::shouldScheduleMqttRetry(QString()));
  }

  void testSiloPanelVisibleOnlyForAerialInMainView() {
    using RM::MainWindowStatePolicy::shouldShowSiloPanel;

    QVERIFY(shouldShowSiloPanel(6, GameStage::BATTLE, false));
    QVERIFY(shouldShowSiloPanel(106, GameStage::PREPARATION, false));
    QVERIFY(!shouldShowSiloPanel(3, GameStage::BATTLE, false));
    QVERIFY(!shouldShowSiloPanel(6, GameStage::SETTLEMENT, false));
    QVERIFY(!shouldShowSiloPanel(6, GameStage::BATTLE, true));
  }

  void testSiloOpenShortcutRequiresEligibleUnmodifiedF() {
    using RM::MainWindowStatePolicy::shouldRequestSiloOpen;

    QVERIFY(shouldRequestSiloOpen(6, GameStage::BATTLE, false, true, false,
                                  true));
    QVERIFY(shouldRequestSiloOpen(106, GameStage::BATTLE, false, true, false,
                                  true));
    QVERIFY(!shouldRequestSiloOpen(3, GameStage::BATTLE, false, true, false,
                                   true));
    QVERIFY(!shouldRequestSiloOpen(6, GameStage::PREPARATION, false, true,
                                   false, true));
    QVERIFY(!shouldRequestSiloOpen(6, GameStage::BATTLE, true, true, false,
                                   true));
    QVERIFY(!shouldRequestSiloOpen(6, GameStage::BATTLE, false, false, false,
                                   true));
    QVERIFY(!shouldRequestSiloOpen(6, GameStage::BATTLE, false, true, true,
                                   true));
    QVERIFY(!shouldRequestSiloOpen(6, GameStage::BATTLE, false, true, false,
                                   false));
  }

  void testQuickPanelToggleOpensWhenBothLayersAreHidden() {
    QVERIFY(RM::MainWindowStatePolicy::shouldOpenQuickPanelOnToggle(false,
                                                                    false));
  }

  void testQuickPanelToggleClosesWhenBothLayersAreVisible() {
    QVERIFY(!RM::MainWindowStatePolicy::shouldOpenQuickPanelOnToggle(true,
                                                                     true));
  }

  void testQuickPanelToggleRepairsHiddenQmlRoot() {
    // 回归场景：只调用 QWidget::show() 会让 QML 根节点继续隐藏，
    // 因此按 H 看起来没有反应。重新打开时需同时恢复两层可见性。
    QVERIFY(RM::MainWindowStatePolicy::shouldOpenQuickPanelOnToggle(true,
                                                                    false));
  }

  void testQuickPanelToggleRepairsHiddenWidgetWrapper() {
    QVERIFY(RM::MainWindowStatePolicy::shouldOpenQuickPanelOnToggle(false,
                                                                    true));
  }

  void testDartOcclusionRemainingSecondsNeverGoesNegative() {
    using RM::MainWindowStatePolicy::remainingDartOcclusionSeconds;

    QCOMPARE(remainingDartOcclusionSeconds(-1, 5, 300), 0);
    QCOMPARE(remainingDartOcclusionSeconds(300, 5, 294), 0);
    QCOMPARE(remainingDartOcclusionSeconds(300, 5, 295), 1);
    QCOMPARE(remainingDartOcclusionSeconds(300, 5, 296), 2);
    QCOMPARE(remainingDartOcclusionSeconds(300, 5, 300), 6);
  }

  void testDartOcclusionAccumulationAddsNewHitToActiveRemainingWindow() {
    using RM::MainWindowStatePolicy::accumulatedDartOcclusionSeconds;

    QCOMPARE(accumulatedDartOcclusionSeconds(-1, 0, 300, 5), 5);
    QCOMPARE(accumulatedDartOcclusionSeconds(300, 5, 298, 3), 7);
    QCOMPARE(accumulatedDartOcclusionSeconds(300, 5, 295, 3), 4);
    QCOMPARE(accumulatedDartOcclusionSeconds(300, 5, 294, 3), 3);
  }

  void testDartOcclusionAccumulationMatchesTenPlusFiveRule() {
    using RM::MainWindowStatePolicy::accumulatedDartOcclusionSeconds;

    QCOMPARE(accumulatedDartOcclusionSeconds(300, 10, 298, 5), 14);
    QCOMPARE(accumulatedDartOcclusionSeconds(300, 10, 295, 5), 11);
    QCOMPARE(accumulatedDartOcclusionSeconds(300, 10, 290, 5), 6);
  }
};

QTEST_MAIN(TestMainWindowStatePolicy)
#include "test_mainwindow_state_policy.moc"
