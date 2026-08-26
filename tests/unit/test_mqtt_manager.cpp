#include "network/MqttManager.h"

#include <QSignalSpy>
#include <QTimer>
#include <QtTest>

class TestMqttManager : public QObject {
  Q_OBJECT

private slots:
  void testIdentifierRejectedErrorMentionsEngineRegistration() {
#ifndef RM_HAS_MQTT
    QSKIP("Paho MQTT support is not enabled in this build");
#else
    MqttManager manager;
    QSignalSpy completedSpy(&manager, &MqttManager::connectCompleted);
    QSignalSpy errorSpy(&manager, &MqttManager::errorOccurred);

    QVERIFY(QMetaObject::invokeMethod(&manager, "onConnectCompleted",
                                      Qt::DirectConnection, Q_ARG(int, 2)));
    QCOMPARE(completedSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 1);

    const QString completedError = completedSpy.at(0).at(1).toString();
    QVERIFY(completedError.contains(QStringLiteral("CONNACK(2)")));
    QVERIFY(completedError.contains(QStringLiteral("custom-client registration")));
#endif
  }

  void testInvalidClientIdDoesNotRetry() {
#ifndef RM_HAS_MQTT
    QSKIP("Paho MQTT support is not enabled in this build");
#else
    MqttManager manager;
    QSignalSpy completedSpy(&manager, &MqttManager::connectCompleted);

    QVERIFY(QMetaObject::invokeMethod(&manager, "onConnectCompleted",
                                      Qt::DirectConnection, Q_ARG(int, 133)));
    QCOMPARE(completedSpy.count(), 1);

    QTimer *reconnectTimer = manager.findChild<QTimer *>();
    QVERIFY(reconnectTimer != nullptr);
    QVERIFY(!reconnectTimer->isActive());
#endif
  }

  void testDisconnectInFailureCallbackCancelsRetryTimer() {
#ifndef RM_HAS_MQTT
    QSKIP("Paho MQTT support is not enabled in this build");
#else
    MqttManager manager;
    QSignalSpy completedSpy(&manager, &MqttManager::connectCompleted);

    connect(&manager, &MqttManager::connectCompleted, &manager,
            [&manager](bool success, const QString &) {
              if (!success) {
                manager.disconnect();
              }
            });

    QVERIFY(QMetaObject::invokeMethod(&manager, "onConnectCompleted",
                                      Qt::DirectConnection, Q_ARG(int, -1)));
    QCOMPARE(completedSpy.count(), 1);

    QTimer *reconnectTimer = manager.findChild<QTimer *>();
    QVERIFY(reconnectTimer != nullptr);
    QVERIFY(!reconnectTimer->isActive());
#endif
  }
};

QTEST_MAIN(TestMqttManager)
#include "test_mqtt_manager.moc"
