#include "network/Protocol.h"
#include "robomaster.pb.h"
#include <QObject>
#include <QTest>

class TestProtobuf : public QObject {
  Q_OBJECT

private slots:
  void testSerialization();
  void testDeserialization();
};

void TestProtobuf::testSerialization() {
  robomaster::RoboMasterMessage message;
  auto *robotStatus = message.mutable_robot_status();
  robotStatus->set_id(1);
  robotStatus->set_hp(100);
  robotStatus->set_max_hp(200);

  QByteArray data = Protocol::serializePacket(message);
  QVERIFY(!data.isEmpty());
}

void TestProtobuf::testDeserialization() {
  robomaster::RoboMasterMessage originalMessage;
  auto *robotStatus = originalMessage.mutable_robot_status();
  robotStatus->set_id(2);
  robotStatus->set_hp(150);
  robotStatus->set_max_hp(300);

  QByteArray data = Protocol::serializePacket(originalMessage);

  robomaster::RoboMasterMessage parsedMessage;
  bool success = Protocol::parsePacket(data, parsedMessage);

  QVERIFY(success);
  QVERIFY(parsedMessage.has_robot_status());
  QCOMPARE(parsedMessage.robot_status().id(), 2);
  QCOMPARE(parsedMessage.robot_status().hp(), 150);
  QCOMPARE(parsedMessage.robot_status().max_hp(), 300);
}

QTEST_MAIN(TestProtobuf)
#include "test_protobuf.moc"
