#include "core/GameData.h"
#include <QObject>
#include <QSignalSpy>
#include <QTest>

class TestGameData : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void testSingleton();
  void testUpdateRobotHP();
  void testCleanup();
};

void TestGameData::initTestCase() {
  // 若有遗留状态，尽量在这里清理
}

void TestGameData::testSingleton() {
  // 当前实现中的 GameData 不是单例
  GameData data1;
  GameData data2;
  QVERIFY(&data1 != &data2);
}

void TestGameData::testUpdateRobotHP() {
  GameData data;

  // 监听更新信号
  QSignalSpy spy(&data, &GameData::robotDataUpdated);

  // 构造测试数据
  robomaster::RobotStatus statusData;
  statusData.set_id(1);
  statusData.set_hp(150);
  statusData.set_max_hp(200);
  statusData.set_level(1);
  statusData.set_heat(0);
  statusData.set_heat_limit(100);
  statusData.set_ammo(0);
  statusData.set_ammo_limit(100);

  data.updateRobotData(statusData);

  QCOMPARE(spy.count(), 1);
  QList<QVariant> arguments = spy.takeFirst();
  QCOMPARE(arguments.at(0).toInt(), 1); // robotId

  // 校验更新后的状态
  const RobotData *robot = data.getRobotById(1);
  QVERIFY(robot != nullptr);
  QCOMPARE(robot->currentHP, 150);
  QCOMPARE(robot->maxHP, 200);
}

void TestGameData::testCleanup() {
  // 按需清理测试状态
}

QTEST_MAIN(TestGameData)
#include "test_game_data.moc"
