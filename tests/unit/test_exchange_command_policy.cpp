#include "ui/ExchangeCommandPolicy.h"

#include <QtTest>

class TestExchangeCommandPolicy : public QObject {
  Q_OBJECT

private slots:
  void testProtocolFlagsAloneControlPanelAvailability() {
    QVERIFY(RM::ExchangeCommandPolicy::canOpenRemoteExchange(true, false));
    QVERIFY(RM::ExchangeCommandPolicy::canOpenRemoteExchange(false, true));
    QVERIFY(RM::ExchangeCommandPolicy::canOpenRemoteExchange(true, true));
    QVERIFY(!RM::ExchangeCommandPolicy::canOpenRemoteExchange(false, false));
  }

  void testRemoteAmmoPermissionDoesNotRequireOutOfCombatState() {
    // RobotDynamicStatus.can_remote_ammo 是协议给出的权威值。即使独立的
    // is_out_of_combat 字段为 false，操作页入口仍应保持可用。
    const bool canRemoteHeal = false;
    const bool canRemoteAmmo = true;
    QVERIFY(RM::ExchangeCommandPolicy::canOpenRemoteExchange(canRemoteHeal,
                                                              canRemoteAmmo));
  }

  void testAmmoRequestsRequireProtocolBatchSizes() {
    QVERIFY(RM::ExchangeCommandPolicy::isValidRequest(1, 100));
    QVERIFY(RM::ExchangeCommandPolicy::isValidRequest(1, 200));
    QVERIFY(RM::ExchangeCommandPolicy::isValidRequest(2, 10));
    QVERIFY(RM::ExchangeCommandPolicy::isValidRequest(2, 20));
    QVERIFY(!RM::ExchangeCommandPolicy::isValidRequest(1, 0));
    QVERIFY(!RM::ExchangeCommandPolicy::isValidRequest(1, 10));
    QVERIFY(!RM::ExchangeCommandPolicy::isValidRequest(1, 110));
    QVERIFY(!RM::ExchangeCommandPolicy::isValidRequest(2, 5));
  }

  void testRejectsUnknownOrInvalidExchangeRequests() {
    QVERIFY(RM::ExchangeCommandPolicy::isValidRequest(6, 60));
    QVERIFY(!RM::ExchangeCommandPolicy::isValidRequest(6, 101));
    QVERIFY(!RM::ExchangeCommandPolicy::isValidRequest(99, 10));
  }
};

QTEST_MAIN(TestExchangeCommandPolicy)
#include "test_exchange_command_policy.moc"
