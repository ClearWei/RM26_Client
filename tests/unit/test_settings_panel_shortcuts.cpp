#include <QQmlComponent>
#include <QQmlEngine>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QtTest>

class TestSettingsPanelShortcuts : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    Q_INIT_RESOURCE(qml);
    Q_INIT_RESOURCE(resources);
  }

  void cleanupTestCase() {
    Q_CLEANUP_RESOURCE(resources);
    Q_CLEANUP_RESOURCE(qml);
  }

  void testCanonicalRobotSelectionsRemainUnchanged() {
    QQmlEngine engine;
    QQmlComponent component(
        &engine, QUrl(QStringLiteral("qrc:/qml/SettingsPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> panel(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));

    const QStringList robotTypes{
        QStringLiteral("R1 - Hero"),     QStringLiteral("R2 - Engineer"),
        QStringLiteral("R3 - Standard"), QStringLiteral("R4 - Standard"),
        QStringLiteral("R6 - Aerial"),   QStringLiteral("B1 - Hero"),
        QStringLiteral("B2 - Engineer"), QStringLiteral("B3 - Standard"),
        QStringLiteral("B4 - Standard"), QStringLiteral("B6 - Aerial"),
    };

    for (const QString &robotType : robotTypes) {
      QVariant handled;
      const bool invoked = QMetaObject::invokeMethod(
          panel.data(), "selectRobotByShortcut",
          Q_RETURN_ARG(QVariant, handled),
          Q_ARG(QVariant, QVariant(robotType)));
      QVERIFY2(invoked, qPrintable(robotType));
      QVERIFY2(handled.toBool(), qPrintable(robotType));
      QCOMPARE(panel->property("currentRobotSelection").toString(), robotType);
      QVERIFY(panel->property("robotShortcutPending").toBool());

      QVERIFY(QMetaObject::invokeMethod(panel.data(),
                                        "clearRobotShortcutSelection"));
      QVERIFY(!panel->property("robotShortcutPending").toBool());
    }

    const QString selectionBeforeInvalid =
        panel->property("currentRobotSelection").toString();
    QVariant invalidHandled;
    QVERIFY(QMetaObject::invokeMethod(
        panel.data(), "selectRobotByShortcut",
        Q_RETURN_ARG(QVariant, invalidHandled),
        Q_ARG(QVariant, QVariant(QStringLiteral("B7 - Sentry")))));
    QVERIFY(!invalidHandled.toBool());
    QCOMPARE(panel->property("currentRobotSelection").toString(),
             selectionBeforeInvalid);
    QVERIFY(!panel->property("robotShortcutPending").toBool());
  }

  void testConfirmRequiresDigitSelectionAndUsesLoginSignal() {
    QQmlEngine engine;
    QQmlComponent component(
        &engine, QUrl(QStringLiteral("qrc:/qml/SettingsPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> panel(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QSignalSpy loginSpy(panel.data(), SIGNAL(loginRequested(QString)));
    QSignalSpy logoutSpy(panel.data(), SIGNAL(logoutRequested()));
    QVERIFY(loginSpy.isValid());
    QVERIFY(logoutSpy.isValid());

    QVariant handledWithoutSelection;
    QVERIFY(QMetaObject::invokeMethod(
        panel.data(), "confirmRobotShortcutLogin",
        Q_RETURN_ARG(QVariant, handledWithoutSelection)));
    QVERIFY(!handledWithoutSelection.toBool());
    QCOMPARE(loginSpy.count(), 0);
    QCOMPARE(logoutSpy.count(), 0);

    QVariant selectionHandled;
    QVERIFY(QMetaObject::invokeMethod(
        panel.data(), "selectRobotByShortcut",
        Q_RETURN_ARG(QVariant, selectionHandled),
        Q_ARG(QVariant, QVariant(QStringLiteral("B6 - Aerial")))));
    QVERIFY(selectionHandled.toBool());

    // 即使已有活动机器人，数字键 + Enter 仍走现有登录槽完成身份切换；
    // 鼠标按钮原有的登录/退出二态行为不在这里改写。
    panel->setProperty("activeRobotLabel", QStringLiteral("R3 - Standard"));
    QVariant loginHandled;
    QVERIFY(QMetaObject::invokeMethod(
        panel.data(), "confirmRobotShortcutLogin",
        Q_RETURN_ARG(QVariant, loginHandled)));
    QVERIFY(loginHandled.toBool());
    QCOMPARE(loginSpy.count(), 1);
    QCOMPARE(loginSpy.takeFirst().at(0).toString(),
             QStringLiteral("B6 - Aerial"));
    QCOMPARE(logoutSpy.count(), 0);
    QVERIFY(!panel->property("robotShortcutPending").toBool());

    QVariant repeatedConfirmHandled;
    QVERIFY(QMetaObject::invokeMethod(
        panel.data(), "confirmRobotShortcutLogin",
        Q_RETURN_ARG(QVariant, repeatedConfirmHandled)));
    QVERIFY(!repeatedConfirmHandled.toBool());
    QCOMPARE(loginSpy.count(), 0);
  }

  void testPendingSelectionClearsWhenPanelCloses() {
    QQmlEngine engine;
    QQmlComponent component(
        &engine, QUrl(QStringLiteral("qrc:/qml/SettingsPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> panel(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));

    QVariant handled;
    QVERIFY(QMetaObject::invokeMethod(
        panel.data(), "selectRobotByShortcut", Q_RETURN_ARG(QVariant, handled),
        Q_ARG(QVariant, QVariant(QStringLiteral("R1 - Hero")))));
    QVERIFY(handled.toBool());
    QVERIFY(panel->property("robotShortcutPending").toBool());

    panel->setProperty("visible", false);
    QVERIFY(!panel->property("robotShortcutPending").toBool());
  }
};

QTEST_MAIN(TestSettingsPanelShortcuts)
#include "test_settings_panel_shortcuts.moc"
