#include "core/GameData.h"
#include "core/TacticalAnalyzer.h"

#include <QtTest>
#include <QSignalSpy>

#include <initializer_list>

class TestTacticalAnalyzer : public QObject {
    Q_OBJECT

private slots:
    void testRespawnEconomyCalculation_data() {
        QTest::addColumn<int>("remainingTime");
        QTest::addColumn<int>("robotLevel");
        QTest::addColumn<int>("economy");
        QTest::addColumn<int>("expectedCost");
        QTest::addColumn<int>("expectedCount");

        QTest::newRow("match-start-insufficient") << 420 << 1 << 19 << 20 << 0;
        QTest::newRow("first-second-exact") << 419 << 1 << 100 << 100 << 1;
        QTest::newRow("minute-boundary") << 360 << 2 << 239 << 120 << 1;
        QTest::newRow("after-minute-boundary") << 359 << 3 << 659 << 220 << 2;
        QTest::newRow("match-end-high-level") << 0 << 10 << 1520 << 760 << 2;
    }

    void testRespawnEconomyCalculation() {
        QFETCH(int, remainingTime);
        QFETCH(int, robotLevel);
        QFETCH(int, economy);
        QFETCH(int, expectedCost);
        QFETCH(int, expectedCount);

        GameData gameData;
        gameData.setCurrentRobotId(1);
        gameData.getRobotDataRef(1).level = robotLevel;
        gameData.setRedEconomy(economy);
        gameData.updateGameStateFromS1(
            static_cast<int>(GameStage::BATTLE), remainingTime, 0, 0, 1, false);

        RM::TacticalAnalyzer analyzer(&gameData);
        analyzer.setUseMockData(false);
        analyzer.analyze();

        const QVariantMap status = analyzer.topStatusData();
        QCOMPARE(status.value(QStringLiteral("respawnGoldCost")).toInt(), expectedCost);
        QCOMPARE(status.value(QStringLiteral("affordableRespawnCount")).toInt(), expectedCount);
        QVERIFY(status.value(QStringLiteral("respawnEconomyVisible")).toBool());
    }

    void testRespawnEconomyUsesBluePerspective() {
        GameData gameData;
        gameData.setCurrentRobotId(101);
        gameData.getRobotDataRef(101).level = 4;
        gameData.setRedEconomy(9999);
        gameData.setBlueEconomy(399);
        gameData.updateGameStateFromS1(
            static_cast<int>(GameStage::BATTLE), 300, 0, 0, 1, false);

        RM::TacticalAnalyzer analyzer(&gameData);
        analyzer.setUseMockData(false);
        analyzer.analyze();

        const QVariantMap status = analyzer.topStatusData();
        QCOMPARE(status.value(QStringLiteral("respawnGoldCost")).toInt(), 240);
        QCOMPARE(status.value(QStringLiteral("affordableRespawnCount")).toInt(), 1);
    }

    void testRespawnEconomyCardBeforeBattleAndHiddenWhenPaused() {
        GameData gameData;
        gameData.setCurrentRobotId(1);
        gameData.updateGameStateFromS1(
            static_cast<int>(GameStage::PREPARATION), 89, 0, 0, 1, false);

        RM::TacticalAnalyzer analyzer(&gameData);
        analyzer.setUseMockData(false);
        analyzer.analyze();

        const QVariantMap status = analyzer.topStatusData();
        QVERIFY(status.value(QStringLiteral("respawnEconomyVisible")).toBool());
        QCOMPARE(status.value(QStringLiteral("respawnGoldCost")).toInt(), 0);
        QCOMPARE(status.value(QStringLiteral("affordableRespawnCount")).toInt(), 0);

        gameData.updateGameStateFromS1(
            static_cast<int>(GameStage::BATTLE), 300, 0, 0, 1, true);
        analyzer.analyze();
        QVERIFY(!analyzer.topStatusData()
                     .value(QStringLiteral("respawnEconomyVisible"))
                     .toBool());
    }

    void testDefaultLayoutModeIsMapPrimary() {
        GameData gameData;
        RM::TacticalAnalyzer analyzer(&gameData);

        QCOMPARE(analyzer.layoutMode(), QStringLiteral("map_primary"));
    }

    void testMockAnalysisMetricsExposeFiveConfiguredCards() {
        GameData gameData;
        RM::TacticalAnalyzer analyzer(&gameData);

        analyzer.analyze();

        const QVariantList metrics = analyzer.analysisMetrics();
        QCOMPARE(metrics.size(), 5);

        const QStringList expectedKeys{
            QStringLiteral("ally_economy"),
            QStringLiteral("enemy_economy"),
            QStringLiteral("ally_damage"),
            QStringLiteral("enemy_damage"),
            QStringLiteral("hp_diff")
        };
        const QStringList expectedTitles{
            QStringLiteral("我方总经济"),
            QStringLiteral("敌方总经济"),
            QStringLiteral("我方总伤害"),
            QStringLiteral("敌方总伤害"),
            QStringLiteral("总血量差")
        };

        for (int i = 0; i < metrics.size(); ++i) {
            const QVariantMap metric = metrics.at(i).toMap();
            QCOMPARE(metric.value(QStringLiteral("key")).toString(), expectedKeys.at(i));
            QCOMPARE(metric.value(QStringLiteral("title")).toString(), expectedTitles.at(i));
            QVERIFY(metric.contains(QStringLiteral("compareText")));
        }
    }

    void testRobotListsSkipSlotFive() {
        GameData gameData;
        RM::TacticalAnalyzer analyzer(&gameData);

        analyzer.setUseMockData(false);

        const QVariantList allyRobots = analyzer.allyRobotList();
        const QVariantList enemyRobots = analyzer.enemyRobotList();

        QCOMPARE(allyRobots.size(), 6);
        QCOMPARE(enemyRobots.size(), 6);

        auto containsSlotFive = [](const QVariantList &robots) {
            for (const QVariant &entry : robots) {
                if (entry.toMap().value("slot").toInt() == 5) {
                    return true;
                }
            }
            return false;
        };

        QVERIFY(!containsSlotFive(allyRobots));
        QVERIFY(!containsSlotFive(enemyRobots));
    }

    void testRobotListsDefaultMaxHpToOneHundredFifty() {
        GameData gameData;
        RM::TacticalAnalyzer analyzer(&gameData);

        analyzer.setUseMockData(false);

        const QVariantList allyRobots = analyzer.allyRobotList();
        const QVariantList enemyRobots = analyzer.enemyRobotList();

        QCOMPARE(allyRobots.size(), 6);
        QCOMPARE(enemyRobots.size(), 6);

        for (const QVariant &entry : allyRobots) {
            const QVariantMap robot = entry.toMap();
            QCOMPARE(robot.value(QStringLiteral("maxHp")).toInt(), 150);
        }

        for (const QVariant &entry : enemyRobots) {
            const QVariantMap robot = entry.toMap();
            QCOMPARE(robot.value(QStringLiteral("maxHp")).toInt(), 150);
        }
    }

    void testRealAnalysisMetricsUseAllyEnemyTotals() {
        GameData gameData;
        RM::TacticalAnalyzer analyzer(&gameData);

        gameData.setRedEconomy(1200);
        gameData.setBlueEconomy(900);

        DamageEventData allyDamage{};
        allyDamage.attackerId = 1;
        allyDamage.victimId = 101;
        allyDamage.damage = 500;
        allyDamage.armorId = 1;
        allyDamage.hurtType = 0;
        gameData.recordDamageEvent(allyDamage);

        DamageEventData enemyDamage{};
        enemyDamage.attackerId = 101;
        enemyDamage.victimId = 1;
        enemyDamage.damage = 300;
        enemyDamage.armorId = 1;
        enemyDamage.hurtType = 0;
        gameData.recordDamageEvent(enemyDamage);

        analyzer.setUseMockData(false);
        analyzer.analyze();

        const QVariantList metrics = analyzer.analysisMetrics();
        QCOMPARE(metrics.size(), 5);

        const QVariantMap allyEconomy = metrics.at(0).toMap();
        QCOMPARE(allyEconomy.value(QStringLiteral("key")).toString(), QStringLiteral("ally_economy"));
        QCOMPARE(allyEconomy.value(QStringLiteral("value")).toInt(), 1200);
        QCOMPARE(allyEconomy.value(QStringLiteral("compareText")).toString(), QStringLiteral("敌方 900"));

        const QVariantMap enemyEconomy = metrics.at(1).toMap();
        QCOMPARE(enemyEconomy.value(QStringLiteral("key")).toString(), QStringLiteral("enemy_economy"));
        QCOMPARE(enemyEconomy.value(QStringLiteral("value")).toInt(), 900);
        QCOMPARE(enemyEconomy.value(QStringLiteral("compareText")).toString(), QStringLiteral("我方 1200"));

        const QVariantMap allyDamageMetric = metrics.at(2).toMap();
        QCOMPARE(allyDamageMetric.value(QStringLiteral("key")).toString(), QStringLiteral("ally_damage"));
        QCOMPARE(allyDamageMetric.value(QStringLiteral("value")).toInt(), 500);
        QCOMPARE(allyDamageMetric.value(QStringLiteral("compareText")).toString(), QStringLiteral("敌方 300"));

        const QVariantMap enemyDamageMetric = metrics.at(3).toMap();
        QCOMPARE(enemyDamageMetric.value(QStringLiteral("key")).toString(), QStringLiteral("enemy_damage"));
        QCOMPARE(enemyDamageMetric.value(QStringLiteral("value")).toInt(), 300);
        QCOMPARE(enemyDamageMetric.value(QStringLiteral("compareText")).toString(), QStringLiteral("我方 500"));

        const QVariantMap hpDiffMetric = metrics.at(4).toMap();
        QCOMPARE(hpDiffMetric.value(QStringLiteral("key")).toString(), QStringLiteral("hp_diff"));
        QCOMPARE(hpDiffMetric.value(QStringLiteral("value")).toString(), QStringLiteral("+0"));
    }

    void testEnemyPaidRespawnSignalMatchesFlashAndDoesNotRepeat() {
        GameData gameData;
        gameData.setCurrentRobotId(1);
        RM::TacticalAnalyzer analyzer(&gameData);
        analyzer.setUseMockData(false);

        RobotData &enemyHero = gameData.getRobotDataRef(101);
        enemyHero.hasTabGlobalSnapshot = true;
        enemyHero.tabGlobalCurrentHP = 300;
        QSignalSpy paidRespawnSpy(
            &analyzer, &RM::TacticalAnalyzer::enemyPaidRespawnDetected);
        QVERIFY(paidRespawnSpy.isValid());

        analyzer.analyze();
        QCOMPARE(paidRespawnSpy.count(), 0);

        enemyHero.tabGlobalCurrentHP = 0;
        analyzer.analyze();
        QCOMPARE(paidRespawnSpy.count(), 0);

        enemyHero.tabGlobalCurrentHP = 300;
        analyzer.analyze();
        QCOMPARE(paidRespawnSpy.count(), 1);
        QCOMPARE(paidRespawnSpy.takeFirst().at(0).toInt(), 101);

        analyzer.analyze();
        QCOMPARE(paidRespawnSpy.count(), 0);
    }

    void testEnemyPaidRespawnUsesGlobalZeroWhileStaticConnectionRemainsOnline() {
        GameData gameData;
        gameData.setCurrentRobotId(106);
        RM::TacticalAnalyzer analyzer(&gameData);
        analyzer.setUseMockData(false);

        RobotData &enemyHero = gameData.getRobotDataRef(1);
        enemyHero.tabStaticConnected = true;
        enemyHero.currentHP = 600;
        enemyHero.hasTabGlobalSnapshot = true;
        enemyHero.tabGlobalCurrentHP = 600;

        QSignalSpy paidRespawnSpy(
            &analyzer, &RM::TacticalAnalyzer::enemyPaidRespawnDetected);
        QVERIFY(paidRespawnSpy.isValid());

        analyzer.analyze();
        QCOMPARE(paidRespawnSpy.count(), 0);

        enemyHero.tabGlobalCurrentHP = 0;
        analyzer.analyze();
        QCOMPARE(paidRespawnSpy.count(), 0);

        enemyHero.tabGlobalCurrentHP = 600;
        analyzer.analyze();
        QCOMPARE(paidRespawnSpy.count(), 1);
        QCOMPARE(paidRespawnSpy.takeFirst().at(0).toInt(), 1);

        analyzer.analyze();
        QCOMPARE(paidRespawnSpy.count(), 0);
    }
};

QTEST_MAIN(TestTacticalAnalyzer)
#include "test_tactical_analyzer.moc"
