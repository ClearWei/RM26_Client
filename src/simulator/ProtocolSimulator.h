#ifndef PROTOCOLSIMULATOR_H
#define PROTOCOLSIMULATOR_H

#include "../network/Protocol.h"
#include <QByteArray>
#include <QObject>
#include <QTimer>

namespace RM {

class ProtocolSimulator : public QObject {
  Q_OBJECT
public:
  explicit ProtocolSimulator(QObject *parent = nullptr);
  void startSimulation();
  void stopSimulation();

  Q_INVOKABLE void simulateGameStage(int stage, int time = 0);
  Q_INVOKABLE void simulateRobotPunishment(int type);

signals:
  void dataReceived(PacketType type, const QByteArray &data);

private slots:
  void onTimer();

private:
  QTimer *m_timer;
  int m_counter;
  quint8 m_robotId;

  QByteArray generateBaseHealthPacket();
  QByteArray generateRobotStatusPacket(quint8 id);
};

} // namespace RM

#endif // PROTOCOLSIMULATOR_H
