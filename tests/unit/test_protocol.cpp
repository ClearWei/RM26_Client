#include "network/Protocol.h"
#include "robomaster.pb.h"
#include <QtTest>

/**
 * @brief Protocol 单元测试类
 *
 * 测试 Protocol 类的序列化和反序列化功能。
 */
class TestProtocol : public QObject {
  Q_OBJECT

private slots:
  /**
   * @brief 测试序列化和反序列化
   */
  void testSerializeAndParse() {
    // 1. 创建一个 RoboMasterMessage 对象
    robomaster::RoboMasterMessage originalMsg;
    robomaster::RobotStatus *status = originalMsg.mutable_robot_status();
    status->set_id(1);
    status->set_hp(100);
    status->set_max_hp(200);

    // 2. 序列化
    QByteArray data = Protocol::serializePacket(originalMsg);
    QVERIFY(!data.isEmpty());

    // 3. 反序列化
    robomaster::RoboMasterMessage parsedMsg;
    bool success = Protocol::parsePacket(data, parsedMsg);

    // 4. 验证
    QVERIFY(success);
    QVERIFY(parsedMsg.has_robot_status());
    QCOMPARE(parsedMsg.robot_status().id(), 1);
    QCOMPARE(parsedMsg.robot_status().hp(), 100);
    QCOMPARE(parsedMsg.robot_status().max_hp(), 200);
  }

  /**
   * @brief 测试无效数据解析
   */
  void testParseInvalidData() {
    QByteArray invalidData = "Invalid Data";
    robomaster::RoboMasterMessage parsedMsg;
    bool success = Protocol::parsePacket(invalidData, parsedMsg);
    QVERIFY(!success);
  }

  /**
   * @brief 测试空数据解析
   */
  void testParseEmptyData() {
    QByteArray emptyData;
    robomaster::RoboMasterMessage parsedMsg;
    bool success = Protocol::parsePacket(emptyData, parsedMsg);
    QVERIFY(!success); // 应该失败，因为 Protobuf 需要有效数据
  }
};

QTEST_MAIN(TestProtocol)
#include "test_protocol.moc"
