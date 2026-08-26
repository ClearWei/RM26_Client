#include "core/TimedEventRules.h"

#include <QtTest>
#include <algorithm>
#include <QSet>

class TestTimedEventRules : public QObject {
  Q_OBJECT

private slots:
  void testStructureHitSoundsOnlyTargetGunnerOperator() {
    QVERIFY(RM::isGunnerOperator(6));
    QVERIFY(RM::isGunnerOperator(106));

    QVERIFY(!RM::isGunnerOperator(1));
    QVERIFY(!RM::isGunnerOperator(3));
    QVERIFY(!RM::isGunnerOperator(7));
    QVERIFY(!RM::isGunnerOperator(101));
    QVERIFY(!RM::isGunnerOperator(103));
    QVERIFY(!RM::isGunnerOperator(107));
    QVERIFY(!RM::isGunnerOperator(0));
  }

  void testDartGateVoiceOnlyTargetsGunnerOperator() {
    QVERIFY(RM::isDartGateVoiceOperator(6));
    QVERIFY(RM::isDartGateVoiceOperator(106));

    QVERIFY(!RM::isDartGateVoiceOperator(1));
    QVERIFY(!RM::isDartGateVoiceOperator(3));
    QVERIFY(!RM::isDartGateVoiceOperator(7));
    QVERIFY(!RM::isDartGateVoiceOperator(101));
    QVERIFY(!RM::isDartGateVoiceOperator(103));
    QVERIFY(!RM::isDartGateVoiceOperator(107));
    QVERIFY(!RM::isDartGateVoiceOperator(0));
  }

  void testDartGateTimedEventsDoNotCarrySoundFile() {
    const QVector<RM::TimedEventHit> hits = RM::tryMatchTimedEvents(390, 1500);
    auto dartHitIt = std::find_if(hits.cbegin(), hits.cend(), [](const auto &hit) {
      return hit.key == QStringLiteral("dart_gate_open_630");
    });

    QVERIFY(dartHitIt != hits.cend());
    QCOMPARE(dartHitIt->label, QStringLiteral("可开启飞镖闸门"));
    QVERIFY(dartHitIt->soundFileName.isEmpty());
  }

  void testDartGateTimedEventsStaySilentAfterEnemyOutpostDestroyed() {
    const QVector<RM::TimedEventHit> hits = RM::tryMatchTimedEvents(390, 0);
    auto dartHitIt = std::find_if(hits.cbegin(), hits.cend(), [](const auto &hit) {
      return hit.key == QStringLiteral("dart_gate_open_630");
    });

    QVERIFY(dartHitIt != hits.cend());
    QCOMPARE(dartHitIt->label, QStringLiteral("可开启飞镖闸门"));
    QVERIFY(dartHitIt->soundFileName.isEmpty());
  }

  void testDartCanOpenDropSoundTriggersOnceWindowSeesZero() {
    QVERIFY(RM::shouldPlayDartCanOpenDropSound(390, 1, 0));
    QVERIFY(RM::shouldPlayDartCanOpenDropSound(300, 1500, 0));
    QVERIFY(RM::shouldPlayDartCanOpenDropSound(181, 20, 0));
    QVERIFY(RM::shouldPlayDartCanOpenDropSound(300, 0, 0));

    QVERIFY(!RM::shouldPlayDartCanOpenDropSound(391, 1500, 0));
    QVERIFY(!RM::shouldPlayDartCanOpenDropSound(180, 1500, 0));
    QVERIFY(!RM::shouldPlayDartCanOpenDropSound(300, 1500, 1));
    QVERIFY(!RM::shouldPlayDartCanOpenDropSound(300, 1500, 0, false));
  }

  void testDartCanOpenLateSoundTriggersWhenAlreadyZeroAfterThreeMinutes() {
    QVERIFY(RM::shouldPlayDartCanOpenLateSound(180, 0));
    QVERIFY(RM::shouldPlayDartCanOpenLateSound(1, 0));
    QVERIFY(RM::shouldPlayDartCanOpenLateSound(0, 0));

    QVERIFY(!RM::shouldPlayDartCanOpenLateSound(181, 0));
    QVERIFY(!RM::shouldPlayDartCanOpenLateSound(-1, 0));
    QVERIFY(!RM::shouldPlayDartCanOpenLateSound(180, 1));
    QVERIFY(!RM::shouldPlayDartCanOpenLateSound(180, 0, false));
  }

  void testDartCanOpenSoundRequiresBattleStage() {
    QVERIFY(RM::shouldPlayDartCanOpenSound(300, 1500, 0, true));
    QVERIFY(!RM::shouldPlayDartCanOpenSound(300, 1500, 0, false));

    QVERIFY(RM::shouldPlayDartCanOpenSound(180, 0, 0, true));
    QVERIFY(!RM::shouldPlayDartCanOpenSound(180, 0, 0, false));
  }

  void testTimedEventsWithoutSoundRemainSilent() {
    RM::TimedEventHit hit;
    QVERIFY(RM::tryMatchTimedEvent(419, &hit));

    QCOMPARE(hit.key, QStringLiteral("small_mechanism_659"));
    QVERIFY(hit.soundFileName.isEmpty());
  }

  void testOutpostReviveReminderOnlyRunsForRebuildableAllyOutpostInBattle() {
    QCOMPARE(RM::kOutpostReviveReminderIntervalMs, 30000);
    QVERIFY(RM::shouldEnableOutpostReviveReminder(true, 4, 420, true));
    QVERIFY(RM::shouldEnableOutpostReviveReminder(true, 4, 121, true));

    QVERIFY(!RM::shouldEnableOutpostReviveReminder(true, 4, 120, true));
    QVERIFY(!RM::shouldEnableOutpostReviveReminder(true, 4, 60, true));
    QVERIFY(!RM::shouldEnableOutpostReviveReminder(false, 4, 420, true));
    QVERIFY(!RM::shouldEnableOutpostReviveReminder(true, 3, 420, true));
    QVERIFY(!RM::shouldEnableOutpostReviveReminder(true, 5, 420, true));
    QVERIFY(!RM::shouldEnableOutpostReviveReminder(true, 4, 420, false));
  }

  void testOutpostReviveReminderBoundaryTickStopsAtTwoMinutesExactly() {
    QVERIFY(RM::shouldEnableOutpostReviveReminder(true, 4, 121, true));
    QVERIFY(!RM::shouldEnableOutpostReviveReminder(true, 4, 120, true));
    QVERIFY(!RM::shouldEnableOutpostReviveReminder(true, 4, 119, true));
  }

  void testRuneVoiceMappingsAreCompleteAndUnique() {
    const QStringList files = {
        RM::runeVoiceSoundFileName(RM::RuneVoiceType::Small, 1),
        RM::runeVoiceSoundFileName(RM::RuneVoiceType::Small, 2),
        RM::runeVoiceSoundFileName(RM::RuneVoiceType::Large, 1),
        RM::runeVoiceSoundFileName(RM::RuneVoiceType::Large, 2),
        RM::runeVoiceSoundFileName(RM::RuneVoiceType::Large, 3),
    };

    for (const QString &file : files) {
      QVERIFY(!file.isEmpty());
    }
    QCOMPARE(QSet<QString>(files.cbegin(), files.cend()).size(), files.size());
    QVERIFY(RM::runeVoiceSoundFileName(RM::RuneVoiceType::Small, 3).isEmpty());
    QVERIFY(RM::runeVoiceSoundFileName(RM::RuneVoiceType::Large, 0).isEmpty());
  }

  void testSmallRuneVoiceScheduleIncludesNewGrantBeforePrompt() {
    RM::RuneVoicePromptTracker tracker;
    tracker.startBattle(420, true);
    tracker.updateRuneStatus(1);

    QVERIFY(!tracker.updateTime(420).has_value());

    const int remainingTimes[] = {390, 360, 330, 300, 270};
    const int expectedChances[] = {1, 1, 2, 2, 2};
    for (int i = 0; i < 5; ++i) {
      const auto prompt = tracker.updateTime(remainingTimes[i]);
      QVERIFY(prompt.has_value());
      QCOMPARE(prompt->type, RM::RuneVoiceType::Small);
      QCOMPARE(prompt->remainingChances, expectedChances[i]);
    }

    QVERIFY(!tracker.updateTime(270).has_value());
  }

  void testLargeRuneVoiceScheduleAndFixedFourFifteenPrompt() {
    RM::RuneVoicePromptTracker tracker;
    tracker.startBattle(420, true);
    tracker.updateRuneStatus(1);

    const int remainingTimes[] = {240, 210, 180, 165, 150,
                                  120, 90,  60,  30};
    const int expectedChances[] = {1, 1, 1, 2, 2, 2, 3, 3, 3};
    for (int i = 0; i < 9; ++i) {
      const auto prompt = tracker.updateTime(remainingTimes[i]);
      QVERIFY(prompt.has_value());
      QCOMPARE(prompt->type, RM::RuneVoiceType::Large);
      QCOMPARE(prompt->remainingChances, expectedChances[i]);
    }
  }

  void testRuneOpportunityConsumedForActivatingOrDirectActivatedStatus() {
    RM::RuneVoicePromptTracker tracker;
    tracker.startBattle(420, true);
    tracker.updateRuneStatus(1);
    QCOMPARE(tracker.remainingSmallChances(), 1);

    tracker.updateRuneStatus(3);
    QCOMPARE(tracker.remainingSmallChances(), 0);
    tracker.updateRuneStatus(3);
    QCOMPARE(tracker.remainingSmallChances(), 0);
    tracker.updateRuneStatus(1);

    const auto promptAtNinety = tracker.updateTime(330);
    QVERIFY(promptAtNinety.has_value());
    QCOMPARE(promptAtNinety->remainingChances, 1);

    tracker.updateRuneStatus(2);
    QCOMPARE(tracker.remainingSmallChances(), 0);
    tracker.updateRuneStatus(3);
    QCOMPARE(tracker.remainingSmallChances(), 0);
  }

  void testFirstObservedActivatingStatusConsumesOpportunity() {
    RM::RuneVoicePromptTracker tracker;
    tracker.startBattle(420, true);

    tracker.updateRuneStatus(2);
    QCOMPARE(tracker.remainingSmallChances(), 0);

    QVERIFY(!tracker.updateTime(330).has_value());
    QCOMPARE(tracker.remainingSmallChances(), 1);

    tracker.updateRuneStatus(3);
    QCOMPARE(tracker.remainingSmallChances(), 1);
  }

  void testFirstObservedActivatedStatusConsumesOpportunity() {
    RM::RuneVoicePromptTracker tracker;
    tracker.startBattle(420, true);

    tracker.updateRuneStatus(3);
    QCOMPARE(tracker.remainingSmallChances(), 0);
    tracker.updateRuneStatus(3);
    QCOMPARE(tracker.remainingSmallChances(), 0);
  }

  void testDirectActivatedStatusConsumesCorrectRuneType() {
    RM::RuneVoicePromptTracker tracker;
    tracker.startBattle(420, true);
    tracker.updateRuneStatus(1);

    tracker.updateTime(330);
    QCOMPARE(tracker.remainingSmallChances(), 2);
    tracker.updateRuneStatus(3);
    QCOMPARE(tracker.remainingSmallChances(), 1);

    tracker.updateRuneStatus(1);
    tracker.updateTime(240);
    QCOMPARE(tracker.remainingLargeChances(), 1);
    tracker.updateRuneStatus(3);
    QCOMPARE(tracker.remainingLargeChances(), 0);
    QCOMPARE(tracker.remainingSmallChances(), 1);
  }

  void testRuneActivatingStatusSuppressesPromptButActivatedStatusAllowsIt() {
    RM::RuneVoicePromptTracker tracker;
    tracker.startBattle(420, true);
    tracker.updateRuneStatus(1);
    tracker.updateTime(241);
    tracker.updateRuneStatus(2);

    QVERIFY(!tracker.updateTime(240).has_value());
    tracker.updateRuneStatus(3);

    const auto laterPrompt = tracker.updateTime(210);
    QVERIFY(laterPrompt.has_value());
    QCOMPARE(laterPrompt->type, RM::RuneVoiceType::Large);
    QCOMPARE(laterPrompt->remainingChances, 1);
  }

  void testRuneInvalidStatusDoesNotReplaceLatestValidStatus() {
    RM::RuneVoicePromptTracker tracker;
    tracker.startBattle(420, true);

    tracker.updateRuneStatus(4);
    QVERIFY(!tracker.updateTime(390).has_value());

    tracker.updateRuneStatus(1);
    const auto promptAtSixty = tracker.updateTime(360);
    QVERIFY(promptAtSixty.has_value());
    QCOMPARE(promptAtSixty->remainingChances, 1);

    tracker.updateRuneStatus(0);
    const auto promptAtNinety = tracker.updateTime(330);
    QVERIFY(promptAtNinety.has_value());
    QCOMPARE(promptAtNinety->remainingChances, 2);
  }

  void testRuneVoiceReconnectIgnoresHistoricalOpportunities() {
    RM::RuneVoicePromptTracker tracker;
    tracker.startBattle(320, false);
    tracker.updateRuneStatus(1);

    QVERIFY(!tracker.updateTime(300).has_value());
    const auto firstReliableLargeGrant = tracker.updateTime(240);
    QVERIFY(firstReliableLargeGrant.has_value());
    QCOMPARE(firstReliableLargeGrant->type, RM::RuneVoiceType::Large);
    QCOMPARE(firstReliableLargeGrant->remainingChances, 1);

    tracker.startBattle(200, false);
    tracker.updateRuneStatus(1);
    QVERIFY(!tracker.updateTime(180).has_value());
    const auto nextReliableGrant = tracker.updateTime(165);
    QVERIFY(nextReliableGrant.has_value());
    QCOMPARE(nextReliableGrant->remainingChances, 1);
  }

  void testRuneVoiceRequiresValidStatusAndResetsBetweenBattles() {
    RM::RuneVoicePromptTracker tracker;
    tracker.startBattle(420, true);
    QVERIFY(!tracker.updateTime(390).has_value());

    tracker.updateRuneStatus(1);
    QVERIFY(!tracker.updateTime(400).has_value());

    tracker.reset();
    tracker.startBattle(420, true);
    tracker.updateRuneStatus(1);
    const auto prompt = tracker.updateTime(390);
    QVERIFY(prompt.has_value());
    QCOMPARE(prompt->remainingChances, 1);
  }

  void testRuneVoiceAcceptsOpeningZeroBeforeAuthoritativeClock() {
    RM::RuneVoicePromptTracker tracker;
    tracker.startBattle(0, true);
    tracker.updateRuneStatus(1);

    QCOMPARE(tracker.remainingSmallChances(), 1);
    QVERIFY(!tracker.updateTime(0).has_value());
    QVERIFY(!tracker.updateTime(0).has_value());
    QVERIFY(!tracker.updateTime(420).has_value());

    const auto prompt = tracker.updateTime(390);
    QVERIFY(prompt.has_value());
    QCOMPARE(prompt->type, RM::RuneVoiceType::Small);
    QCOMPARE(prompt->remainingChances, 1);

    QVERIFY(!tracker.updateTime(400).has_value());
    QVERIFY(!tracker.updateTime(390).has_value());
  }

  void testRuneVoiceAcceptsOpeningOneBeforeAuthoritativeClock() {
    RM::RuneVoicePromptTracker tracker;
    tracker.startBattle(1, true);
    tracker.updateRuneStatus(1);

    QCOMPARE(tracker.remainingSmallChances(), 1);
    QVERIFY(!tracker.updateTime(1).has_value());
    QVERIFY(!tracker.updateTime(1).has_value());
    QVERIFY(!tracker.updateTime(420).has_value());

    const auto prompt = tracker.updateTime(390);
    QVERIFY(prompt.has_value());
    QCOMPARE(prompt->type, RM::RuneVoiceType::Small);
    QCOMPARE(prompt->remainingChances, 1);
  }

  void testRuneVoiceOpeningZeroConsumesActivatingStatusOnce() {
    RM::RuneVoicePromptTracker tracker;
    tracker.startBattle(0, true);

    tracker.updateRuneStatus(2);
    QCOMPARE(tracker.remainingSmallChances(), 0);
    tracker.updateRuneStatus(2);
    QCOMPARE(tracker.remainingSmallChances(), 0);

    QVERIFY(!tracker.updateTime(0).has_value());
    QVERIFY(!tracker.updateTime(420).has_value());
    tracker.updateRuneStatus(3);
    QVERIFY(!tracker.updateTime(390).has_value());

    const auto promptAtNinety = tracker.updateTime(330);
    QVERIFY(promptAtNinety.has_value());
    QCOMPARE(promptAtNinety->remainingChances, 1);
  }

  void testRuneVoiceOpeningZeroReconnectRemainsConservative() {
    RM::RuneVoicePromptTracker tracker;
    tracker.startBattle(0, false);
    tracker.updateRuneStatus(1);

    QVERIFY(!tracker.updateTime(0).has_value());
    QVERIFY(!tracker.updateTime(397).has_value());
    QCOMPARE(tracker.remainingSmallChances(), 0);
    QVERIFY(!tracker.updateTime(390).has_value());

    const auto firstReliableLargeGrant = tracker.updateTime(240);
    QVERIFY(firstReliableLargeGrant.has_value());
    QCOMPARE(firstReliableLargeGrant->type, RM::RuneVoiceType::Large);
    QCOMPARE(firstReliableLargeGrant->remainingChances, 1);
  }
};

QTEST_MAIN(TestTimedEventRules)
#include "test_timed_event_rules.moc"
