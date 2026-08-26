#include "network/H264StreamRecoveryPolicy.h"

#include <QtTest>

class TestH264StreamRecoveryPolicy : public QObject {
  Q_OBJECT

private slots:
  void partialFragmentsAreNotDecodeErrors() {
    RM::H264StreamRecoveryPolicy policy(2000, 2000);

    for (qint64 nowMs = 0; nowMs < 2000; nowMs += 2) {
      QVERIFY(!policy.shouldRecoverBeforeFragment(nowMs));
      policy.observeDecodeResult(nowMs, false);
    }
  }

  void sustainedStallTriggersRateLimitedRecovery() {
    RM::H264StreamRecoveryPolicy policy(2000, 2000);

    QVERIFY(!policy.shouldRecoverBeforeFragment(0));
    QVERIFY(!policy.shouldRecoverBeforeFragment(1999));
    QVERIFY(policy.shouldRecoverBeforeFragment(2000));
    QVERIFY(!policy.shouldRecoverBeforeFragment(2500));
    QVERIFY(!policy.shouldRecoverBeforeFragment(3999));
    QVERIFY(policy.shouldRecoverBeforeFragment(4000));
  }

  void decodedFrameRestartsStallWindow() {
    RM::H264StreamRecoveryPolicy policy(2000, 2000);

    QVERIFY(!policy.shouldRecoverBeforeFragment(0));
    QVERIFY(policy.shouldRecoverBeforeFragment(2000));
    QVERIFY(!policy.shouldRecoverBeforeFragment(2500));
    policy.observeDecodeResult(2500, true);
    QVERIFY(!policy.shouldRecoverBeforeFragment(4499));
    QVERIFY(policy.shouldRecoverBeforeFragment(4500));
  }

  void resetStartsANewStreamWindow() {
    RM::H264StreamRecoveryPolicy policy(2000, 2000);

    QVERIFY(!policy.shouldRecoverBeforeFragment(0));
    QVERIFY(policy.shouldRecoverBeforeFragment(2000));
    policy.reset();
    QVERIFY(!policy.shouldRecoverBeforeFragment(10000));
    QVERIFY(!policy.shouldRecoverBeforeFragment(11999));
    QVERIFY(policy.shouldRecoverBeforeFragment(12000));
  }

  void recoveryDecisionPrecedesTriggeringFragment() {
    RM::H264StreamRecoveryPolicy policy(2000, 2000);

    QVERIFY(!policy.shouldRecoverBeforeFragment(0));
    QVERIFY(policy.shouldRecoverBeforeFragment(2000));
    // 调用方此时先 reset，再把时间点 2000 的当前分片送入新 parser。
    policy.observeDecodeResult(2000, true);
    QCOMPARE(policy.lastDecodedFrameMs(), 2000);
    QVERIFY(!policy.shouldRecoverBeforeFragment(3999));
  }
};

QTEST_MAIN(TestH264StreamRecoveryPolicy)
#include "test_h264_stream_recovery_policy.moc"
