#include "core/GameData.h"
#include "network/Protocol.h"
#include "robomaster.pb.h"
#include <QtTest>

/**
 * @brief 性能基准测试类
 */
class TestBenchmark : public QObject {
  Q_OBJECT

private slots:
  /**
   * @brief 测试 Protocol 序列化性能
   */
  void benchmarkProtocolSerialization() {
    robomaster::RoboMasterMessage msg;
    robomaster::RobotStatus *status = msg.mutable_robot_status();
    status->set_id(1);
    status->set_hp(100);
    status->set_max_hp(200);
    status->set_heat(50);
    status->set_heat_limit(100);

    QBENCHMARK {
      QByteArray data = Protocol::serializePacket(msg);
      Q_UNUSED(data);
    }
  }

  /**
   * @brief 测试 Protocol 反序列化性能
   */
  void benchmarkProtocolParsing() {
    robomaster::RoboMasterMessage msg;
    robomaster::RobotStatus *status = msg.mutable_robot_status();
    status->set_id(1);
    status->set_hp(100);
    status->set_max_hp(200);
    QByteArray data = Protocol::serializePacket(msg);

    QBENCHMARK {
      robomaster::RoboMasterMessage parsedMsg;
      Protocol::parsePacket(data, parsedMsg);
    }
  }

  /**
   * @brief 测试 GameData 更新性能
   */
  void benchmarkGameDataUpdate() {
    GameData gameData;
    robomaster::RobotStatus statusMsg;
    statusMsg.set_id(1);
    statusMsg.set_hp(150);
    statusMsg.set_max_hp(200);

    QBENCHMARK { gameData.updateRobotData(statusMsg); }
  }
};

QTEST_MAIN(TestBenchmark)
#include "test_benchmark.moc"
