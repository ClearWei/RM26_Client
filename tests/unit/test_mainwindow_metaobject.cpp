#include "ui/MainWindow.h"

#include <QMetaMethod>
#include <QtTest>

using RM::MainWindow;

class TestMainWindowMetaObject : public QObject {
  Q_OBJECT

private slots:
  void testToggleSettingsPanelInvokableExposed() {
    const QMetaObject &metaObject = MainWindow::staticMetaObject;
    const int methodIndex =
        metaObject.indexOfMethod("toggleSettingsPanel()");

    QVERIFY2(methodIndex >= 0,
             "MainWindow should expose toggleSettingsPanel() to QML.");

    const QMetaMethod method = metaObject.method(methodIndex);
    QCOMPARE(method.methodType(), QMetaMethod::Method);
    QCOMPARE(method.access(), QMetaMethod::Public);
  }

  void testOfficialEventSoundFileNameForEnemyDartGateOpen() {
    QCOMPARE(MainWindow::officialEventSoundFileName(10),
             QStringLiteral("enemy_dart_open.mov"));
    QVERIFY(MainWindow::officialEventSoundFileName(12).isEmpty());
  }

  void testAllyBaseArmorOpenedSoundFileNameUsesBundledAsset() {
    QCOMPARE(MainWindow::allyBaseArmorOpenedSoundFileName(),
             QStringLiteral("我方基地护甲展开.mp3"));
    QCOMPARE(MainWindow::allyBaseArmorOpenedSoundLoopCount(), 3);
  }

  void testPaidRespawnRobotSoundFileNames() {
    QCOMPARE(MainWindow::paidRespawnSoundFileName(101),
             QStringLiteral("enemy_buyback_hero_revived.mp3"));
    QCOMPARE(MainWindow::paidRespawnSoundFileName(102),
             QStringLiteral("enemy_buyback_engineer_revived.mp3"));
    QCOMPARE(MainWindow::paidRespawnSoundFileName(103),
             QStringLiteral("enemy_buyback_infantry_3_revived.mp3"));
    QCOMPARE(MainWindow::paidRespawnSoundFileName(104),
             QStringLiteral("enemy_buyback_infantry_4_revived.mp3"));
    QCOMPARE(MainWindow::paidRespawnSoundFileName(106),
             QStringLiteral("enemy_buyback_aerial_revived.mp3"));
    QCOMPARE(MainWindow::paidRespawnSoundFileName(107),
             QStringLiteral("enemy_buyback_sentry_revived.mp3"));
    QVERIFY(MainWindow::paidRespawnSoundFileName(105).isEmpty());
  }
};

QTEST_MAIN(TestMainWindowMetaObject)
#include "test_mainwindow_metaobject.moc"
