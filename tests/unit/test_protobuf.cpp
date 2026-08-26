#include "network/Protocol.h"
#include "robomaster.pb.h"
#include <QtTest>

/**
 * @brief Protocol 单元测试类
 *
 * 测试通信协议的序列化与反序列化功能。
 */
class TestProtobuf : public QObject {
  Q_OBJECT

private slots:
  /**
   * @brief 测试 RoboMasterMessage 的序列化与反序列化
   */
  void testSerialization() {
    // 1. 创建原始消息
    robomaster::RoboMasterMessage originalMsg;
    auto *robotStatus = originalMsg.mutable_robot_status();
    robotStatus->set_id(7);
    robotStatus->set_hp(500);
    robotStatus->set_max_hp(600);

    // 2. 序列化
    QByteArray data = Protocol::serializePacket(originalMsg);
    QVERIFY(!data.isEmpty());

    // 3. 反序列化
    robomaster::RoboMasterMessage parsedMsg;
    bool success = Protocol::parsePacket(data, parsedMsg);

    // 4. 验证
    QVERIFY(success);
    QVERIFY(parsedMsg.has_robot_status());
    QCOMPARE(parsedMsg.robot_status().id(), 7);
    QCOMPARE(parsedMsg.robot_status().hp(), 500);
    QCOMPARE(parsedMsg.robot_status().max_hp(), 600);
  }

  /**
   * @brief 测试无效数据解析
   */
  void testInvalidData() {
    QByteArray invalidData = "InvalidData";
    robomaster::RoboMasterMessage msg;

    bool success = Protocol::parsePacket(invalidData, msg);
    QVERIFY(!success);
  }

  void testAssemblyStartCommandKeepsExplicitZeroOperation() {
    robomaster::AssemblyCommand command;
    command.set_operation(0);
    command.set_difficulty(1);

    QVERIFY(command.has_operation());
    QVERIFY(command.has_difficulty());

    const std::string serialized = command.SerializeAsString();
    const QByteArray payload(serialized.data(),
                             static_cast<qsizetype>(serialized.size()));
    QCOMPARE(payload, QByteArray::fromHex("08001001"));

    robomaster::AssemblyCommand parsed;
    QVERIFY(parsed.ParseFromArray(payload.constData(), payload.size()));
    QVERIFY(parsed.has_operation());
    QCOMPARE(parsed.operation(), 0u);
    QCOMPARE(parsed.difficulty(), 1u);
  }

  void testOfficialOptionalFieldsKeepExplicitZeroValues() {
    robomaster::GameStatus gameStatus;
    gameStatus.set_current_round(0);
    gameStatus.set_total_rounds(0);
    gameStatus.set_red_score(0);
    gameStatus.set_blue_score(0);
    gameStatus.set_current_stage(0);
    gameStatus.set_stage_countdown_sec(0);
    gameStatus.set_stage_elapsed_sec(0);
    gameStatus.set_is_paused(false);
    gameStatus.set_game_result(0);
    gameStatus.set_end_reason(0);
    QCOMPARE(QByteArray::fromStdString(gameStatus.SerializeAsString()),
             QByteArray::fromHex("0800100018002000280030003800400048005000"));

    robomaster::Buff buff;
    buff.set_robot_id(0);
    buff.set_buff_type(0);
    buff.set_buff_level(0);
    buff.set_buff_max_time(0);
    buff.set_buff_left_time(0);
    QCOMPARE(QByteArray::fromStdString(buff.SerializeAsString()),
             QByteArray::fromHex("08001000180020002800"));

    robomaster::SentryStatusSync sentry;
    sentry.set_posture_id(0);
    sentry.set_is_weakened(false);
    sentry.set_is_powered(false);
    QCOMPARE(QByteArray::fromStdString(sentry.SerializeAsString()),
             QByteArray::fromHex("080010001800"));
  }

  void testOfficialProtocolDescriptorShape() {
    const auto *gameStatus = robomaster::GameStatus::descriptor();
    QCOMPARE(gameStatus->field_count(), 10);
    for (int index = 0; index < gameStatus->field_count(); ++index) {
      QVERIFY(gameStatus->field(index)->has_presence());
    }

    const auto *buff = robomaster::Buff::descriptor();
    QCOMPARE(buff->field_count(), 5);
    QVERIFY(buff->FindFieldByName("msg_params") == nullptr);
    for (int index = 0; index < buff->field_count(); ++index) {
      QVERIFY(buff->field(index)->has_presence());
    }

    const auto *assembly = robomaster::AssemblyCommand::descriptor();
    QCOMPARE(assembly->field_count(), 2);
    QVERIFY(assembly->field(0)->has_presence());
    QVERIFY(assembly->field(1)->has_presence());

    const auto *sentry = robomaster::SentryStatusSync::descriptor();
    QCOMPARE(sentry->field_count(), 3);
    for (int index = 0; index < sentry->field_count(); ++index) {
      QVERIFY(sentry->field(index)->has_presence());
    }
  }

  void testAirSupportStatusSyncIncludesLaserFields() {
    robomaster::AirSupportStatusSync original;
    original.set_airsupport_status(1);
    original.set_left_time(20);
    original.set_cost_coins(8);
    original.set_is_being_targeted(1);
    original.set_shooter_status(0);

    const std::string serialized = original.SerializeAsString();
    QVERIFY(!serialized.empty());

    robomaster::AirSupportStatusSync parsed;
    QVERIFY(parsed.ParseFromString(serialized));
    QCOMPARE(parsed.airsupport_status(), 1u);
    QCOMPARE(parsed.left_time(), 20u);
    QCOMPARE(parsed.cost_coins(), 8u);
    QCOMPARE(parsed.is_being_targeted(), 1u);
    QCOMPARE(parsed.shooter_status(), 0u);
  }
};

QTEST_MAIN(TestProtobuf)
#include "test_protobuf.moc"
