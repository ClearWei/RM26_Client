#include "ProtocolSimulator.h"
#include "robomaster.pb.h"
#include <QDateTime>
#include <QDebug>
#include <string>

namespace RM {


ProtocolSimulator::ProtocolSimulator(QObject *parent)
    : QObject(parent), m_counter(0), m_robotId(1) {
  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, &ProtocolSimulator::onTimer);
}

void ProtocolSimulator::startSimulation() {
  m_timer->start(100); // 10Hz
}

void ProtocolSimulator::stopSimulation() { m_timer->stop(); }

void ProtocolSimulator::simulateGameStage(int stage, int time) {
  robomaster::GameInfo info;
  info.set_stage(static_cast<robomaster::GameStage>(stage));
  info.set_time_remaining(time);

  // 先给出默认比分，避免界面在首帧数据到达前显示异常。
  info.set_red_score(0);
  info.set_blue_score(0);
  info.set_current_round(1);

  int size = info.ByteSizeLong();
  QByteArray data(size, 0);
  info.SerializeToArray(data.data(), size);

  emit dataReceived(PacketType::GAME_STATUS, data);
}

void ProtocolSimulator::simulateRobotPunishment(int type) {
  // 批量控制时统一应用到全部机器人。
  // 红方编号为 1—7，蓝方编号为 101—107。

  QList<int> ids = {1, 2, 3, 4, 5, 6, 7, 101, 102, 103, 104, 105, 106, 107};

  for (int id : ids) {
    robomaster::RobotStatus status;
    status.set_id(id);
    // 兼容入口暂时借用血量字段表现判罚状态。
    // 类型：0 正常，1 离线或罚下，6 黄牌，8 红牌或受罚。
    status.set_max_hp(200); // 默认占位血量
    if (type == 1 || type == 8) {
      status.set_hp(0);
    } else {
      status.set_hp(200);
    }

    int size = status.ByteSizeLong();
    QByteArray data(size, 0);
    status.SerializeToArray(data.data(), size);

    emit dataReceived(PacketType::ROBOT_STATUS, data);
  }
}

void ProtocolSimulator::onTimer() {
  // 自动数据由外部模拟器下发，兼容定时器不再发送数据。
}

QByteArray ProtocolSimulator::generateBaseHealthPacket() {
  QByteArray data(8, 0);
  quint8 team = (quint8)TeamColor::RED;
  quint16 hp = 5000 - (m_counter % 1000); // 周期变化
  quint16 maxHp = 5000;

  data[0] = team;
  memcpy(data.data() + 1, &hp, 2);
  memcpy(data.data() + 3, &maxHp, 2);
  // 护盾值。
  data[5] = 1; // 无敌状态
  return data;
}

QByteArray ProtocolSimulator::generateRobotStatusPacket(quint8 id) {
  QByteArray data(12, 0);
  quint8 level = 2;
  // 按编号生成不同血量，便于在界面上区分机器人。
  quint16 maxHp = 400 + (id * 50);
  quint16 hp = (m_counter * 5 + id * 20) % maxHp;

  // 模拟动态功率。
  quint16 power = (m_counter * 2 + id) % 120;
  // 模拟动态增益。
  quint16 buffer = (m_counter * 10 + id) % 60;
  quint16 heat = (m_counter + id * 10) % 200;

  data[0] = id;
  data[1] = level;
  memcpy(data.data() + 2, &hp, 2);
  memcpy(data.data() + 4, &maxHp, 2);
  memcpy(data.data() + 6, &power, 2);
  memcpy(data.data() + 8, &heat, 2);
  memcpy(data.data() + 10, &buffer, 2);

  return data;
}

} // namespace RM
