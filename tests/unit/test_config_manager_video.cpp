#include "config/ConfigManager.h"

#include <QtTest>

class TestConfigManagerVideo : public QObject {
  Q_OBJECT

private slots:
  void optionalArModelIsNotAssumed() {
    QCOMPARE(ConfigManager::instance().getARModelPath(), QString());
    QVERIFY(!ConfigManager::instance().getAROverlayEnabled());
  }

  void industrialCameraGridEnvironmentOverride() {
    const QByteArray previousValue = qgetenv("RM_HERO_CAMERA_GRID");

    qputenv("RM_HERO_CAMERA_GRID", "true");
    QVERIFY(ConfigManager::instance().getIndustrialCameraGridEnabled());

    qputenv("RM_HERO_CAMERA_GRID", "0");
    QVERIFY(!ConfigManager::instance().getIndustrialCameraGridEnabled());

    if (previousValue.isNull()) {
      qunsetenv("RM_HERO_CAMERA_GRID");
    } else {
      qputenv("RM_HERO_CAMERA_GRID", previousValue);
    }
  }

  void clientRobotIdEnvironmentRange() {
    const QByteArray previousValue = qgetenv("RM_CLIENT_ROBOT_ID");

    qputenv("RM_CLIENT_ROBOT_ID", "105");
    QCOMPARE(ConfigManager::instance().getClientRobotId(), 105);

    qputenv("RM_CLIENT_ROBOT_ID", "8");
    QVERIFY(ConfigManager::instance().getClientRobotId() != 8);

    if (previousValue.isNull()) {
      qunsetenv("RM_CLIENT_ROBOT_ID");
    } else {
      qputenv("RM_CLIENT_ROBOT_ID", previousValue);
    }
  }
};

QTEST_MAIN(TestConfigManagerVideo)
#include "test_config_manager_video.moc"
