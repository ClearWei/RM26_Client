#include "ui/PopupOverlayPolicy.h"
#include <QFile>
#include <QtTest>

class TestPopupOverlayPolicy : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { Q_INIT_RESOURCE(qml); }

  void cleanupTestCase() { Q_CLEANUP_RESOURCE(qml); }

  void testResolutionScaleUses1920By1080Reference() {
    QCOMPARE(RM::PopupOverlayPolicy::resolutionScale(QSize(1920, 1080)), 1.0);
    QCOMPARE(RM::PopupOverlayPolicy::resolutionScale(QSize(1280, 720)),
             2.0 / 3.0);
    QCOMPARE(RM::PopupOverlayPolicy::resolutionScale(QSize(2560, 1080)), 1.0);
  }

  void testScaledSizePreservesPopupAspectRatio() {
    QCOMPARE(RM::PopupOverlayPolicy::scaledSize(QSize(650, 250),
                                                QSize(1280, 720)),
             QSize(433, 167));
    QCOMPARE(RM::PopupOverlayPolicy::scaledSize(QSize(640, 180),
                                                QSize(3840, 2160)),
             QSize(1280, 360));
  }

  void testInvalidViewportFallsBackToDesignSize() {
    QCOMPARE(RM::PopupOverlayPolicy::scaledSize(QSize(540, 132), QSize()),
             QSize(540, 132));
  }

  void testOverlayInactiveWithoutPopupsOrRespawn() {
    QVERIFY(!RM::PopupOverlayPolicy::shouldActivateOverlay(false, false));
  }

  void testOverlayActiveForStateMachinePopup() {
    QVERIFY(RM::PopupOverlayPolicy::shouldActivateOverlay(true, false));
  }

  void testOverlayActiveForRespawnOnly() {
    QVERIFY(RM::PopupOverlayPolicy::shouldActivateOverlay(false, true));
  }

  void testPaidRespawnRequiresPendingPayableOnlineRobot() {
    QVERIFY(RM::PopupOverlayPolicy::canSendPaidRespawn(true, true, true));
    QVERIFY(!RM::PopupOverlayPolicy::canSendPaidRespawn(false, true, true));
    QVERIFY(!RM::PopupOverlayPolicy::canSendPaidRespawn(true, false, true));
    QVERIFY(!RM::PopupOverlayPolicy::canSendPaidRespawn(true, true, false));
  }

  void testFreeRespawnUsesAuthoritativePermissionAndOnlineState() {
    // 不把 progress 作为输入：服务端字段 can_free_respawn 才是权威许可，
    // 即使完成帧省略 progress，该字段仍然有效。
    QVERIFY(RM::PopupOverlayPolicy::canSendFreeRespawn(true, true, true));
    QVERIFY(!RM::PopupOverlayPolicy::canSendFreeRespawn(false, true, true));
    QVERIFY(!RM::PopupOverlayPolicy::canSendFreeRespawn(true, false, true));
    QVERIFY(!RM::PopupOverlayPolicy::canSendFreeRespawn(true, true, false));
  }

  void testDynamicPopupCallsUseGuardedLoaderBoundary() {
    QFile source(QStringLiteral(":/qml/PopupOverlay.qml"));
    QVERIFY2(source.open(QIODevice::ReadOnly),
             "PopupOverlay.qml should be available from the QML resource");

    const QString qml = QString::fromUtf8(source.readAll());
    QVERIFY(qml.contains(QStringLiteral("function invokeLoaderMethod(")));
    QVERIFY(qml.contains(QStringLiteral("typeof method !== \"function\"")));
    QVERIFY(qml.contains(QStringLiteral("arguments.length >= 3")));

    // 动态组件的方法只能经统一边界调用，避免 Loader.item 类型漂移后直接报错。
    QVERIFY(!qml.contains(QStringLiteral(".item.applyPayload")));
    QVERIFY(!qml.contains(QStringLiteral(".item.updateFromStatus")));
    QVERIFY(!qml.contains(QStringLiteral(".item.forceActiveFocus")));
  }
};

QTEST_MAIN(TestPopupOverlayPolicy)
#include "test_popup_overlay_policy.moc"
