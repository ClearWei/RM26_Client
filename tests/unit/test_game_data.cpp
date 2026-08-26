#include "core/GameData.h"
#include "core/GameConstants.h"
#include "network/Protocol.h"
#include "robomaster.pb.h"
#include <QtTest>

namespace {

QStringList *g_runeStatusLogMessages = nullptr;

void captureRuneStatusLog(QtMsgType type, const QMessageLogContext &,
                          const QString &message) {
  if (type == QtInfoMsg && g_runeStatusLogMessages &&
      message.startsWith(
          QStringLiteral("[RuneVoice] RuneStatusSync status changed"))) {
    g_runeStatusLogMessages->append(message);
  }
}

} // namespace

/**
 * @brief GameData 单元测试类
 *
 * 测试 GameData 类的核心功能，包括：
 * 1. 机器人数据的更新与检索
 * 2. 基地数据的更新
 * 3. 比赛状态的更新
 * 4. 信号的正确发射
 */
class TestGameData : public QObject {
  Q_OBJECT

private slots:
  /**
   * @brief 初始化测试
   * 验证 GameData 初始化后的默认状态是否正确。
   */
  void initTestCase() {
    GameData gameData;

    // 验证默认机器人数量
    QCOMPARE(gameData.getAllRobots().size(),
             12); // 每方初始化 6 台可见机器人（1、2、3、4、6、7）；
                  // 当前客户端界面不显示 5/105。
    QVERIFY(gameData.getRobotById(5) == nullptr);
    QVERIFY(gameData.getRobotById(105) == nullptr);

    // 验证默认比赛阶段
    QCOMPARE(gameData.getCurrentStage(), GameStage::NOT_STARTED);

    // 验证默认比赛时间
    QCOMPARE(gameData.getGameTime(), 420);
  }

  /**
   * @brief 测试机器人数据更新
   * 模拟接收到机器人状态包，验证数据是否正确更新。
   */
  void testUpdateRobotData() {
    GameData gameData;

    // 构造模拟数据
    robomaster::RobotStatus statusMsg;
    statusMsg.set_id(1); // 红方英雄
    statusMsg.set_hp(150);
    statusMsg.set_max_hp(200);
    statusMsg.set_heat(50);
    statusMsg.set_heat_limit(100);

    // 监听信号
    QSignalSpy spy(&gameData, &GameData::robotDataUpdated);

    // 执行更新
    gameData.updateRobotData(statusMsg);

    // 验证信号是否发射
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), 1);

    // 验证数据是否更新
    const RobotData *robot = gameData.getRobotById(1);
    QVERIFY(robot != nullptr);
    QCOMPARE(robot->currentHP, 150);
    QCOMPARE(robot->maxHP, 200);
    QCOMPARE(robot->currentHeat, 50);
  }

  void testRobotRosterRequiresStaticStatus() {
    GameData gameData;

    robomaster::GlobalUnitStatusInternal unitStatus;
    unitStatus.add_red_robot_health(4000);
    unitStatus.add_red_robot_health(500);
    unitStatus.add_red_robot_health(400);
    unitStatus.add_red_robot_health(400);
    unitStatus.add_red_robot_health(300);
    unitStatus.add_red_robot_health(600);
    gameData.updateGlobalUnitStatusInternal(unitStatus);

    QVariantMap robot3 = gameData.getRobotInfo(3);
    QCOMPARE(robot3.value("hp").toInt(), 400);
    QVERIFY(robot3.value("isOffline").toBool());
    QVERIFY(!robot3.value("isClientConnected").toBool());

    robomaster::RobotStaticStatus staticStatus;
    staticStatus.set_robot_id(3);
    staticStatus.set_connection_state(2);
    staticStatus.set_field_state(0);
    staticStatus.set_alive_state(1);
    staticStatus.set_level(2);
    staticStatus.set_max_health(400);
    gameData.updateRobotStaticStatus(staticStatus);

    robot3 = gameData.getRobotInfo(3);
    QVERIFY(!robot3.value("isOffline").toBool());
    QVERIFY(robot3.value("isClientConnected").toBool());

    staticStatus.set_robot_id(6);
    staticStatus.set_connection_state(0);
    staticStatus.set_field_state(0);
    staticStatus.set_alive_state(1);
    staticStatus.set_max_health(300);
    gameData.updateRobotStaticStatus(staticStatus);

    QVariantMap robot6 = gameData.getRobotInfo(6);
    QVERIFY(!robot6.value("isOffline").toBool());
    QVERIFY(!robot6.value("isClientConnected").toBool());

    staticStatus.set_field_state(1);
    gameData.updateRobotStaticStatus(staticStatus);

    robot6 = gameData.getRobotInfo(6);
    QVERIFY(robot6.value("isOffline").toBool());
  }

  void testFreeRespawnCompletionOverridesStaleProgress() {
    GameData gameData;

    QVariantMap reading;
    reading.insert(QStringLiteral("is_pending_respawn"), true);
    reading.insert(QStringLiteral("total_respawn_progress"), 47u);
    reading.insert(QStringLiteral("current_respawn_progress"), 46u);
    reading.insert(QStringLiteral("can_free_respawn"), false);
    gameData.processRobotRespawnStatusMap(reading);

    QVariantMap completed;
    completed.insert(QStringLiteral("is_pending_respawn"), true);
    completed.insert(QStringLiteral("total_respawn_progress"), 0u);
    completed.insert(QStringLiteral("current_respawn_progress"), 0u);
    completed.insert(QStringLiteral("can_free_respawn"), true);
    gameData.processRobotRespawnStatusMap(completed);

    const QVariantMap status = gameData.robotRespawnStatus();
    QVERIFY(status.value(QStringLiteral("can_free_respawn")).toBool());
    QCOMPARE(status.value(QStringLiteral("total_respawn_progress")).toUInt(),
             47u);
    QCOMPARE(status.value(QStringLiteral("current_respawn_progress")).toUInt(),
             47u);
  }

  /**
   * @brief 测试基地数据更新
   * 验证基地血量更新逻辑。
   */
  void testUpdateBaseData() {
    GameData gameData;

    QSignalSpy spy(&gameData, &GameData::baseHealthUpdated);

    // 使用新的辅助接口
    gameData.updateBaseHP(TeamColor::RED, 4500);
    gameData.updateBaseStatus(TeamColor::RED, 0); // 0 表示无敌

    QCOMPARE(spy.count(), 1);

    const BaseData &redBase = gameData.getRedBase();
    QCOMPARE(redBase.currentHP, 4500);
    QVERIFY(redBase.isInvincible);
  }

  void testAllyBaseHealthDropAlertTriggersAfterOneSecondWindow() {
    GameData gameData;
    gameData.setCurrentRobotId(1);

    QSignalSpy spy(&gameData, &GameData::allyBaseHealthDropAlertTriggered);

    gameData.updateBaseHP(TeamColor::RED, 5000);
    QTest::qWait(1100);
    gameData.updateBaseHP(TeamColor::RED, 4980);

    QCOMPARE(spy.count(), 1);
  }

  /**
   * @brief 测试比赛状态更新
   * 验证比赛阶段和时间的更新。
   */
  void testUpdateGameState() {
    GameData gameData;

    robomaster::GameInfo gameInfo;
    gameInfo.set_stage(
        robomaster::GameStage::STAGE_BATTLE); // 在 GameData 中映射为 BATTLE（4）
    gameInfo.set_time_remaining(300);

    QSignalSpy spy(&gameData, &GameData::gameStateUpdated);

    gameData.updateGameState(gameInfo);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(gameData.getCurrentStage(), GameStage::BATTLE);
    QCOMPARE(gameData.getGameTime(), 300);
  }

  void testDetermineWinnerUsesBattleToSettlementScoreDelta() {
    GameData gameData;

    robomaster::GameStatus status;
    status.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    status.set_stage_countdown_sec(300);
    status.set_current_round(1);
    status.set_red_score(2);
    status.set_blue_score(1);
    gameData.updateGameStatus(status);

    status.set_current_stage(robomaster::GameStage::STAGE_SETTLEMENT);
    status.set_stage_countdown_sec(0);
    status.set_red_score(3);
    status.set_blue_score(1);
    gameData.updateGameStatus(status);

    QCOMPARE(gameData.redRoundScoreDelta(), 1);
    QCOMPARE(gameData.blueRoundScoreDelta(), 0);
    QCOMPARE(gameData.determineWinner(), quint8(1));
  }

  void testDetermineWinnerIgnoresAbsoluteScoreLead() {
    GameData gameData;

    robomaster::GameStatus status;
    status.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    status.set_stage_countdown_sec(300);
    status.set_current_round(1);
    status.set_red_score(4);
    status.set_blue_score(1);
    gameData.updateGameStatus(status);

    status.set_current_stage(robomaster::GameStage::STAGE_SETTLEMENT);
    status.set_stage_countdown_sec(0);
    status.set_red_score(4);
    status.set_blue_score(2);
    gameData.updateGameStatus(status);

    QCOMPARE(gameData.redRoundScoreDelta(), 0);
    QCOMPARE(gameData.blueRoundScoreDelta(), 1);
    QCOMPARE(gameData.determineWinner(), quint8(2));
  }

  void testDetermineWinnerDrawsWhenScoreDeltaEqual() {
    GameData gameData;

    robomaster::GameStatus status;
    status.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    status.set_stage_countdown_sec(300);
    status.set_current_round(1);
    status.set_red_score(2);
    status.set_blue_score(2);
    gameData.updateGameStatus(status);

    status.set_current_stage(robomaster::GameStage::STAGE_SETTLEMENT);
    status.set_stage_countdown_sec(0);
    status.set_red_score(3);
    status.set_blue_score(3);
    gameData.updateGameStatus(status);

    QCOMPARE(gameData.determineWinner(), quint8(0));
  }

  void testDetermineWinnerFallbackToAbsoluteScoreWhenBaselineMissing() {
    GameData gameData;
    // 从未进入 BATTLE 阶段，基线未设置
    robomaster::GameStatus status;
    status.set_current_stage(robomaster::GameStage::STAGE_SETTLEMENT);
    status.set_stage_countdown_sec(0);
    status.set_current_round(1);
    status.set_red_score(5);
    status.set_blue_score(3);
    gameData.updateGameStatus(status);

    // 基线丢失时应回退到绝对分数判定：红方 5 > 蓝方 3
    QCOMPARE(gameData.determineWinner(), quint8(1));
  }

  void testDetermineWinnerFallbackDrawWhenBaselineMissingAndScoreEqual() {
    GameData gameData;
    robomaster::GameStatus status;
    status.set_current_stage(robomaster::GameStage::STAGE_SETTLEMENT);
    status.set_stage_countdown_sec(0);
    status.set_current_round(1);
    status.set_red_score(3);
    status.set_blue_score(3);
    gameData.updateGameStatus(status);

    QCOMPARE(gameData.determineWinner(), quint8(0));
  }

  void testBaselinePreservedInSettlementOnRoundChange() {
    GameData gameData;

    robomaster::GameStatus status;
    status.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    status.set_stage_countdown_sec(300);
    status.set_current_round(1);
    status.set_red_score(2);
    status.set_blue_score(1);
    gameData.updateGameStatus(status);

    // 进入结算阶段，同时回合数增加（模拟引擎结算时发送 round+1）
    status.set_current_stage(robomaster::GameStage::STAGE_SETTLEMENT);
    status.set_stage_countdown_sec(0);
    status.set_current_round(2);
    status.set_red_score(3);
    status.set_blue_score(1);
    gameData.updateGameStatus(status);

    // 基线应保留，增量为 1:0，红方胜
    QCOMPARE(gameData.determineWinner(), quint8(1));
  }

  void testProcessGameResultEmitsScoreDeltaWinner() {
    GameData gameData;

    robomaster::GameStatus status;
    status.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    status.set_stage_countdown_sec(300);
    status.set_current_round(1);
    status.set_red_score(1);
    status.set_blue_score(1);
    gameData.updateGameStatus(status);

    status.set_current_stage(robomaster::GameStage::STAGE_SETTLEMENT);
    status.set_stage_countdown_sec(0);
    status.set_red_score(1);
    status.set_blue_score(2);
    gameData.updateGameStatus(status);

    QSignalSpy resultSpy(&gameData, &GameData::gameResultReceived);
    gameData.processGameResult(0);

    QCOMPARE(resultSpy.count(), 1);
    QCOMPARE(resultSpy.takeFirst().at(0).toUInt(), uint(2));
  }

  void testGameResultEventDoesNotPreemptSettlementScoreSnapshot() {
    GameData gameData;

    robomaster::GameStatus status;
    status.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    status.set_stage_countdown_sec(300);
    status.set_current_round(1);
    status.set_red_score(0);
    status.set_blue_score(0);
    gameData.updateGameStatus(status);

    QSignalSpy resultSpy(&gameData, &GameData::gameResultReceived);
    gameData.processGameResult(0);

    QCOMPARE(resultSpy.count(), 0);
    QCOMPARE(gameData.getCurrentStage(), GameStage::BATTLE);

    status.set_current_stage(robomaster::GameStage::STAGE_SETTLEMENT);
    status.set_stage_countdown_sec(0);
    status.set_red_score(1);
    status.set_blue_score(0);
    gameData.updateGameStatus(status);

    QCOMPARE(gameData.redRoundScoreDelta(), 1);
    QCOMPARE(gameData.blueRoundScoreDelta(), 0);
    QCOMPARE(gameData.determineWinner(), quint8(1));
  }

  void testGlobalSpecialMechanismTriggersAlertOnceAtOneSecond() {
    GameData gameData;
    QSignalSpy alertSpy(&gameData, &GameData::allyFortressOccupationAlertTriggered);

    robomaster::GlobalSpecialMechanism twoSecondMsg;
    twoSecondMsg.add_mechanism_id(1);
    twoSecondMsg.add_mechanism_time_sec(2);
    gameData.updateGlobalSpecialMechanism(twoSecondMsg);

    QCOMPARE(gameData.allyFortressOccupationSec(), 2);
    QCOMPARE(gameData.enemyFortressOccupationSec(), 0);
    QCOMPARE(alertSpy.count(), 0);

    robomaster::GlobalSpecialMechanism oneSecondMsg;
    oneSecondMsg.add_mechanism_id(1);
    oneSecondMsg.add_mechanism_time_sec(1);
    gameData.updateGlobalSpecialMechanism(oneSecondMsg);

    QCOMPARE(gameData.allyFortressOccupationSec(), 1);
    QCOMPARE(alertSpy.count(), 1);

    gameData.updateGlobalSpecialMechanism(oneSecondMsg);
    QCOMPARE(alertSpy.count(), 1);

    robomaster::GlobalSpecialMechanism resetMsg;
    gameData.updateGlobalSpecialMechanism(resetMsg);
    QCOMPARE(gameData.allyFortressOccupationSec(), 0);
    QCOMPARE(alertSpy.count(), 1);
  }

  void testAllyOutpostHealthDropAlertOnlyTracksOwnSide() {
    GameData gameData;
    QSignalSpy alertSpy(&gameData, &GameData::allyOutpostHealthDropAlertTriggered);

    gameData.updateOutpostHP(TeamColor::RED, 1500);
    QTest::qWait(5100);

    gameData.updateOutpostHP(TeamColor::BLUE, 1000);
    QCOMPARE(alertSpy.count(), 0);

    // A：5 秒内正好掉 200 血，作为首次有效提示发出。
    gameData.updateOutpostHP(TeamColor::RED, 1300);
    QCOMPARE(alertSpy.count(), 1);

    // B：又过 5 秒再掉 300 血，但 B-A < 15s，丢弃 B 且不更新 A。
    QTest::qWait(5100);
    gameData.updateOutpostHP(TeamColor::RED, 1000);
    QCOMPARE(alertSpy.count(), 1);

    // 维持一个新的 5 秒窗口基线。
    QTest::qWait(5100);
    gameData.updateOutpostHP(TeamColor::RED, 1000);
    QCOMPARE(alertSpy.count(), 1);

    // C：距 A 已超过 15s，5 秒内正好掉 200 血，应再次发出。
    QTest::qWait(5100);
    gameData.updateOutpostHP(TeamColor::RED, 800);
    QCOMPARE(alertSpy.count(), 2);
  }

  void testEnemyOutpostHealthTracksCurrentRobotPerspective() {
    GameData gameData;
    gameData.updateOutpostHP(TeamColor::RED, 900);
    gameData.updateOutpostHP(TeamColor::BLUE, 300);

    gameData.setCurrentRobotId(1);
    QCOMPARE(gameData.enemyOutpostHealth(), 300);

    gameData.setCurrentRobotId(101);
    QCOMPARE(gameData.enemyOutpostHealth(), 900);
  }

  void testAllyBaseHealthDropAlertOnlyTracksOwnSide() {
    GameData gameData;
    QSignalSpy alertSpy(&gameData, &GameData::allyBaseHealthDropAlertTriggered);

    gameData.updateBaseHP(TeamColor::RED, 5000);
    QTest::qWait(1010);

    gameData.updateBaseHP(TeamColor::BLUE, 4980);
    QCOMPARE(alertSpy.count(), 0);

    gameData.updateBaseHP(TeamColor::RED, 4980);
    QCOMPARE(alertSpy.count(), 1);

    gameData.updateBaseHP(TeamColor::RED, 4970);
    QCOMPARE(alertSpy.count(), 1);
  }

  /**
   * @brief 测试无效机器人ID
   */
  void testInvalidRobotId() {
    GameData gameData;
    const RobotData *robot = gameData.getRobotById(99); // 不存在的ID
    QVERIFY(robot == nullptr);
  }

  /**
   * @brief 测试边界条件
   */
  void testBoundaryConditions() {
    GameData gameData;
    robomaster::RobotStatus statusMsg;
    statusMsg.set_id(1);

    // 测试 0 血量
    statusMsg.set_hp(0);
    gameData.updateRobotData(statusMsg);
    const RobotData *robot = gameData.getRobotById(1);
    QCOMPARE(robot->currentHP, 0);

    // 测试超大血量 (虽然逻辑上不应该发生，但验证是否正确存储)
    statusMsg.set_hp(65535);
    gameData.updateRobotData(statusMsg);
    QCOMPARE(robot->currentHP, 65535);
  }

  void testRadarInfoUsesEnemyThenAllyOrder() {
    GameData gameData;
    gameData.setCurrentRobotId(1);

    robomaster::RadarInfoToClient packetFromRedPerspective;
    for (int i = 0; i < 12; ++i) {
      auto *info = packetFromRedPerspective.add_robot_info();
      info->set_target_pos_x(static_cast<uint32_t>((i + 1) * 100));
      info->set_target_pos_y(static_cast<uint32_t>((i + 1) * 200));
      info->set_is_high_light(i % 2);
    }

    gameData.updateRadarInfo(packetFromRedPerspective);

    const RobotData *redHero = gameData.getRobotById(1);
    const RobotData *redSentry = gameData.getRobotById(7);
    const RobotData *blueHero = gameData.getRobotById(101);
    const RobotData *blueSentry = gameData.getRobotById(107);
    QVERIFY(redHero != nullptr);
    QVERIFY(redSentry != nullptr);
    QVERIFY(blueHero != nullptr);
    QVERIFY(blueSentry != nullptr);

    // V2.0.0 固定按对方 1/2/3/4/6/7、己方 1/2/3/4/6/7 排列。
    QCOMPARE(redHero->posX, 7.0f);
    QCOMPARE(redHero->posY, 14.0f);
    QCOMPARE(redHero->isHighLight, quint8(0));
    QCOMPARE(redSentry->posX, 12.0f);
    QCOMPARE(redSentry->posY, 24.0f);
    QCOMPARE(redSentry->isHighLight, quint8(1));
    QCOMPARE(blueHero->posX, 1.0f);
    QCOMPARE(blueHero->posY, 2.0f);
    QCOMPARE(blueHero->isHighLight, quint8(0));
    QCOMPARE(blueSentry->posX, 6.0f);
    QCOMPARE(blueSentry->posY, 12.0f);
    QCOMPARE(blueSentry->isHighLight, quint8(1));

    gameData.setCurrentRobotId(101);
    robomaster::RadarInfoToClient packetFromBluePerspective;
    for (int i = 0; i < 12; ++i) {
      auto *info = packetFromBluePerspective.add_robot_info();
      info->set_target_pos_x(static_cast<uint32_t>((i + 1) * 150));
      info->set_target_pos_y(static_cast<uint32_t>((i + 1) * 250));
      info->set_is_high_light((i + 1) % 2);
    }

    gameData.updateRadarInfo(packetFromBluePerspective);

    QCOMPARE(redHero->posX, 1.5f);
    QCOMPARE(redHero->posY, 2.5f);
    QCOMPARE(redHero->isHighLight, quint8(1));
    QCOMPARE(redSentry->posX, 9.0f);
    QCOMPARE(redSentry->posY, 15.0f);
    QCOMPARE(redSentry->isHighLight, quint8(0));
    QCOMPARE(blueHero->posX, 10.5f);
    QCOMPARE(blueHero->posY, 17.5f);
    QCOMPARE(blueHero->isHighLight, quint8(1));
    QCOMPARE(blueSentry->posX, 18.0f);
    QCOMPARE(blueSentry->posY, 30.0f);
    QCOMPARE(blueSentry->isHighLight, quint8(0));
  }

  /**
   * @brief 测试伤害记录
   */
  void testDamageRecord() {
    GameData gameData;
    robomaster::RobotStatus victimStatus;
    victimStatus.set_id(101);
    victimStatus.set_hp(200);
    victimStatus.set_max_hp(200);
    gameData.updateRobotData(victimStatus);

    DamageEventData damageData;
    damageData.attackerId = 1;
    damageData.victimId = 101;
    damageData.damage = 50;
    damageData.armorId = 1;
    damageData.hurtType = 0;

    gameData.recordDamageEvent(damageData);

    // 验证伤害历史
    QCOMPARE(gameData.getDamageHistory().size(), 1);
    QCOMPARE(gameData.getDamageHistory().first().damage, 50);

    // 受击事件只记录伤害历史和刷新信号，权威 HP 仍由 RobotStatus /
    // GlobalUnitStatus 等状态包驱动，不能在这里本地扣血。
    const RobotData *victim = gameData.getRobotById(101);
    QVERIFY(victim != nullptr);
    QCOMPARE(victim->currentHP, 200);
    QCOMPARE(victim->maxHP, 200);
  }

  void testOverPowerPenaltyUpdatesChassisStatus() {
    GameData gameData;
    gameData.setMyRobotId(1);

    QSignalSpy myRobotSpy(&gameData, &GameData::myRobotUpdated);

    robomaster::PenaltyInfo penalty;
    penalty.set_penalty_type(4);
    penalty.set_penalty_effect_sec(10);
    penalty.set_total_penalty_num(1);
    gameData.updatePenalty(penalty);

    QCOMPARE(gameData.chassisStatus(), QStringLiteral("超限断电 10s"));
    QVERIFY(myRobotSpy.count() >= 1);

    const int signalCountAfterFirstUpdate = myRobotSpy.count();
    penalty.set_penalty_effect_sec(7);
    gameData.updatePenalty(penalty);

    QCOMPARE(gameData.chassisStatus(), QStringLiteral("超限断电 7s"));
    QVERIFY(myRobotSpy.count() > signalCountAfterFirstUpdate);

    penalty.set_penalty_effect_sec(0);
    gameData.updatePenalty(penalty);

    QCOMPARE(gameData.chassisStatus(), QString());
  }

  void testHeroFrameSourceUpdatesForQmlImageProvider() {
    GameData gameData;
    QSignalSpy spy(&gameData, &GameData::heroFrameUpdated);

    QVERIFY(!gameData.hasHeroFrame());
    QCOMPARE(gameData.heroFrameRevision(), quint64(0));
    QVERIFY(gameData.heroFrameSource().isEmpty());

    QImage frame(8, 6, QImage::Format_RGB32);
    frame.fill(Qt::red);
    gameData.onVideoFrameReceived(frame);

    QVERIFY(gameData.hasHeroFrame());
    QCOMPARE(gameData.heroFrame().size(), QSize(8, 6));
    QCOMPARE(gameData.heroFrameRevision(), quint64(1));
    QVERIFY(gameData.heroFrameSource().startsWith(
        QStringLiteral("image://herovideo/frame?rev=1")));
    QCOMPARE(spy.count(), 1);

    gameData.clearHeroFrame();

    QVERIFY(!gameData.hasHeroFrame());
    QCOMPARE(gameData.heroFrameRevision(), quint64(2));
    QVERIFY(gameData.heroFrameSource().isEmpty());
    QCOMPARE(spy.count(), 2);
  }

  void testDartHitOcclusionDurationFollowsFixedTargetHitCount() {
    GameData gameData;

    gameData.simulateDartHit(1, 1);
    QCOMPARE(gameData.dartMessageData().value("occlusionDurationSec").toInt(),
             RM::Dart::kOcclusionFixedHit1);

    gameData.simulateDartHit(1, 2);
    QCOMPARE(gameData.dartMessageData().value("occlusionDurationSec").toInt(),
             RM::Dart::kOcclusionFixedHit2);

    gameData.simulateDartHit(1, 3);
    QCOMPARE(gameData.dartMessageData().value("occlusionDurationSec").toInt(),
             RM::Dart::kOcclusionFixedHit3);

    gameData.simulateDartHit(1, 1);
    QCOMPARE(gameData.dartMessageData().value("occlusionDurationSec").toInt(),
             RM::Dart::kOcclusionFixedHit4);
  }

  void testOfficialTechCoreStatusAndRemoteExchangePermission() {
    GameData gameData;

    robomaster::TechCoreMotionStateSync techCore;
    techCore.set_maximum_difficulty_level(4);
    techCore.set_basic_state(3);
    techCore.set_putin_state(1);
    techCore.set_move_state(1);
    techCore.set_rotate_state(0);
    techCore.set_enemy_core_status(2);
    techCore.set_remain_time_all(45);
    techCore.set_remain_time_step(5);

    QSignalSpy techCoreSpy(&gameData,
                           &GameData::techCoreMotionStateSyncUpdated);
    gameData.updateTechCoreMotionStateSync(techCore);

    QCOMPARE(techCoreSpy.count(), 1);
    QCOMPARE(gameData.maximumDifficultyLevel(), 4u);
    QCOMPARE(gameData.techCoreBasicState(), 3u);
    QCOMPARE(gameData.techCorePutinState(), 1u);
    QCOMPARE(gameData.techCoreMoveState(), 1u);
    QCOMPARE(gameData.techCoreRotateState(), 0u);
    QCOMPARE(gameData.enemyStatus(), 2u);
    QCOMPARE(gameData.remainTimeAll(), 45u);
    QCOMPARE(gameData.remainTimeStep(), 5u);

    robomaster::RoboMasterMessage eventWrapper;
    eventWrapper.mutable_event()->set_event_id(15);
    eventWrapper.mutable_event()->set_param("0");
    gameData.processProtocolData(eventWrapper);

    // Event 15 是飞镖闸门开启，不得修改 V2.0.0 工程装配同步状态。
    QCOMPARE(gameData.techCoreBasicState(), 3u);
    QCOMPARE(gameData.techCorePutinState(), 1u);
    QCOMPARE(gameData.techCoreMoveState(), 1u);
    QCOMPARE(gameData.techCoreRotateState(), 0u);
    QCOMPARE(gameData.enemyStatus(), 2u);

    eventWrapper.mutable_event()->set_event_id(14);
    eventWrapper.mutable_event()->set_param("1");
    gameData.processProtocolData(eventWrapper);

    // Event 14 是飞镖命中，也不得修改 V2.0.0 工程装配同步状态。
    QCOMPARE(gameData.techCoreBasicState(), 3u);
    QCOMPARE(gameData.techCorePutinState(), 1u);
    QCOMPARE(gameData.techCoreMoveState(), 1u);
    QCOMPARE(gameData.techCoreRotateState(), 0u);
    QCOMPARE(gameData.enemyStatus(), 2u);

    gameData.setMyRobotId(3);
    robomaster::RobotDynamicStatus dynamicStatus;
    dynamicStatus.set_is_out_of_combat(false);
    dynamicStatus.set_can_remote_heal(false);
    dynamicStatus.set_can_remote_ammo(true);
    gameData.updateRobotDynamicStatus(dynamicStatus);

    QVERIFY(gameData.canRemoteAmmo());
    QVERIFY(gameData.canRemoteExchange());
  }

  void testDartHitOcclusionDurationForMovingTargetsIsAlwaysTenSeconds() {
    GameData gameData;

    gameData.simulateDartHit(1, 1);
    gameData.simulateDartHit(1, 2);
    gameData.simulateDartHit(1, 3);

    gameData.simulateDartHit(1, 4);
    QCOMPARE(gameData.dartMessageData().value("occlusionDurationSec").toInt(),
             RM::Dart::kOcclusionMovingTarget);

    gameData.simulateDartHit(1, 5);
    QCOMPARE(gameData.dartMessageData().value("occlusionDurationSec").toInt(),
             RM::Dart::kOcclusionMovingTarget);
  }

  void testDartHitCountersResetWhenBattleReenters() {
    GameData gameData;

    gameData.simulateDartHit(1, 1);
    gameData.simulateDartHit(1, 2);
    QCOMPARE(gameData.redDartHits(), 2);

    robomaster::GameStatus status;
    status.set_current_round(1);
    status.set_current_stage(robomaster::GameStage::STAGE_PREPARATION);
    status.set_stage_countdown_sec(20);
    gameData.updateGameStatus(status);

    status.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    status.set_stage_countdown_sec(420);
    gameData.updateGameStatus(status);

    QCOMPARE(gameData.redDartHits(), 0);
    QCOMPARE(gameData.blueDartHits(), 0);

    gameData.simulateDartHit(1, 1);
    QCOMPARE(gameData.dartMessageData().value("occlusionDurationSec").toInt(),
             RM::Dart::kOcclusionFixedHit1);
  }

  void testDartHitCountersResetWhenEnteringSettlement() {
    GameData gameData;

    gameData.simulateDartHit(1, 1);
    gameData.simulateDartHit(1, 2);
    gameData.simulateDartHit(1, 3);
    QCOMPARE(gameData.redDartHits(), 3);
    QCOMPARE(gameData.dartMessageData().value("occlusionDurationSec").toInt(),
             RM::Dart::kOcclusionFixedHit3);

    robomaster::GameStatus status;
    status.set_current_round(1);
    status.set_current_stage(robomaster::GameStage::STAGE_SETTLEMENT);
    status.set_stage_countdown_sec(0);
    gameData.updateGameStatus(status);

    QCOMPARE(gameData.redDartHits(), 0);
    QCOMPARE(gameData.blueDartHits(), 0);

    gameData.simulateDartHit(1, 1);
    QCOMPARE(gameData.redDartHits(), 1);
    QCOMPARE(gameData.dartMessageData().value("occlusionDurationSec").toInt(),
             RM::Dart::kOcclusionFixedHit1);
  }

  void testEvent8AirSupportCounteredEmitsSoundSelector() {
    GameData gameData;
    QSignalSpy soundSpy(&gameData, &GameData::airSupportCountered);
    QSignalSpy systemSpy(&gameData, &GameData::systemMessageReceived);

    robomaster::RoboMasterMessage message;
    auto *event = message.mutable_event();
    event->set_event_id(8);
    event->set_param("2");

    gameData.processProtocolData(message);

    QCOMPARE(soundSpy.count(), 1);
    QCOMPARE(soundSpy.takeFirst().at(0).toString(),
             QStringLiteral("2"));
    QCOMPARE(systemSpy.count(), 1);
    QCOMPARE(systemSpy.takeFirst().at(0).toString(),
             QStringLiteral("7:00 对方空中支援被反制，己方剩余可反制次数 2"));
  }

  void testAirSupportLaserTargetingUsesStateEdges() {
    GameData gameData;
    QSignalSpy targetingSpy(
        &gameData, &GameData::airSupportTargetingStateChanged);
    QSignalSpy statusSpy(&gameData, &GameData::airSupportStatusUpdated);

    robomaster::AirSupportStatusSync status;
    status.set_airsupport_status(1);
    status.set_left_time(30);
    status.set_cost_coins(5);
    status.set_is_being_targeted(1);
    status.set_shooter_status(1);

    gameData.updateAirSupportStatusSync(status);
    QCOMPARE(gameData.airSupportIsBeingTargeted(), 1);
    QCOMPARE(gameData.airSupportShooterStatus(), 1);
    QCOMPARE(targetingSpy.count(), 1);
    QCOMPARE(targetingSpy.at(0).at(0).toBool(), true);
    QCOMPARE(statusSpy.count(), 1);

    gameData.updateAirSupportStatusSync(status);
    QCOMPARE(targetingSpy.count(), 1);
    QCOMPARE(statusSpy.count(), 1);

    status.set_left_time(29);
    gameData.updateAirSupportStatusSync(status);
    QCOMPARE(targetingSpy.count(), 1);
    QCOMPARE(statusSpy.count(), 2);

    status.set_shooter_status(0);
    gameData.updateAirSupportStatusSync(status);
    QCOMPARE(gameData.airSupportShooterStatus(), 0);
    QCOMPARE(targetingSpy.count(), 1);
    QCOMPARE(statusSpy.count(), 3);

    status.set_is_being_targeted(0);
    gameData.updateAirSupportStatusSync(status);
    QCOMPARE(targetingSpy.count(), 2);
    QCOMPARE(targetingSpy.at(1).at(0).toBool(), false);

    status.set_is_being_targeted(2);
    gameData.updateAirSupportStatusSync(status);
    QCOMPARE(gameData.airSupportIsBeingTargeted(), 2);
    QCOMPARE(targetingSpy.count(), 2);

    status.set_is_being_targeted(1);
    gameData.updateAirSupportStatusSync(status);
    QCOMPARE(targetingSpy.count(), 3);
    QCOMPARE(targetingSpy.at(2).at(0).toBool(), true);
  }

  void testAllyBaseArmorOpenedUsesStateEdgesForRedPerspective() {
    GameData gameData;
    robomaster::GameStatus gameStatus;
    gameStatus.set_current_round(1);
    gameStatus.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    gameStatus.set_stage_countdown_sec(420);
    gameData.updateGameStatus(gameStatus);

    QSignalSpy armorOpenedSpy(
        &gameData, &GameData::allyBaseArmorOpenedTriggered);
    QSignalSpy baseStatusSpy(&gameData, &GameData::baseStatusChanged);

    robomaster::GlobalUnitStatus status;
    status.set_base_health(5000);
    status.set_base_shield(100);
    status.set_base_status(2);

    // 首包只建立基线，即使已经展开也不补播。
    gameData.updateGlobalUnitStatus(status);
    QCOMPARE(armorOpenedSpy.count(), 0);
    QCOMPARE(gameData.getRedBase().status, static_cast<quint8>(2));
    QCOMPARE(gameData.getRedBase().virtualShield, static_cast<quint16>(100));

    // 周期性重复快照不应重复触发。
    gameData.updateGlobalUnitStatus(status);
    QCOMPARE(armorOpenedSpy.count(), 0);

    status.set_base_status(1);
    gameData.updateGlobalUnitStatus(status);
    QCOMPARE(armorOpenedSpy.count(), 0);

    status.set_base_status(2);
    gameData.updateGlobalUnitStatus(status);
    QCOMPARE(armorOpenedSpy.count(), 1);

    gameData.updateGlobalUnitStatus(status);
    QCOMPARE(armorOpenedSpy.count(), 1);

    status.set_base_status(0);
    gameData.updateGlobalUnitStatus(status);
    status.set_base_status(2);
    gameData.updateGlobalUnitStatus(status);
    QCOMPARE(armorOpenedSpy.count(), 2);

    // 原有基地状态通知仍按状态变化发送。
    QCOMPARE(baseStatusSpy.count(), 5);
  }

  void testEnemyBaseArmorOpenedDoesNotTriggerAllyVoice() {
    GameData gameData;
    robomaster::GameStatus gameStatus;
    gameStatus.set_current_round(1);
    gameStatus.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    gameStatus.set_stage_countdown_sec(420);
    gameData.updateGameStatus(gameStatus);

    QSignalSpy armorOpenedSpy(
        &gameData, &GameData::allyBaseArmorOpenedTriggered);

    robomaster::GlobalUnitStatus status;
    status.set_base_health(5000);
    status.set_base_status(1);
    status.set_enemy_base_health(5000);
    status.set_enemy_base_status(1);
    gameData.updateGlobalUnitStatus(status);

    status.set_enemy_base_status(2);
    gameData.updateGlobalUnitStatus(status);
    QCOMPARE(armorOpenedSpy.count(), 0);
    QCOMPARE(gameData.getBlueBase().status, static_cast<quint8>(2));

    status.set_base_status(2);
    gameData.updateGlobalUnitStatus(status);
    QCOMPARE(armorOpenedSpy.count(), 1);
  }

  void testAllyBaseArmorOpenedUsesBluePerspective() {
    GameData gameData;
    gameData.setCurrentRobotId(101);
    robomaster::GameStatus gameStatus;
    gameStatus.set_current_round(1);
    gameStatus.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    gameStatus.set_stage_countdown_sec(420);
    gameData.updateGameStatus(gameStatus);

    QSignalSpy armorOpenedSpy(
        &gameData, &GameData::allyBaseArmorOpenedTriggered);

    robomaster::GlobalUnitStatus status;
    status.set_base_health(5000);
    status.set_base_shield(75);
    status.set_base_status(1);
    gameData.updateGlobalUnitStatus(status);
    QCOMPARE(armorOpenedSpy.count(), 0);

    status.set_base_status(2);
    gameData.updateGlobalUnitStatus(status);
    QCOMPARE(armorOpenedSpy.count(), 1);
    QCOMPARE(gameData.getBlueBase().status, static_cast<quint8>(2));
    QCOMPARE(gameData.getBlueBase().virtualShield, static_cast<quint16>(75));
  }

  void testAllyBaseArmorOpenedIsSuppressedBeforeBattle() {
    GameData gameData;
    QSignalSpy armorOpenedSpy(
        &gameData, &GameData::allyBaseArmorOpenedTriggered);

    robomaster::GameStatus gameStatus;
    gameStatus.set_current_round(1);
    gameStatus.set_current_stage(robomaster::GameStage::STAGE_PREPARATION);
    gameStatus.set_stage_countdown_sec(0);
    gameData.updateGameStatus(gameStatus);

    robomaster::GlobalUnitStatus status;
    status.set_base_health(5000);
    status.set_base_status(1);
    gameData.updateGlobalUnitStatus(status);
    status.set_base_status(2);
    gameData.updateGlobalUnitStatus(status);
    QCOMPARE(armorOpenedSpy.count(), 0);

    // 进入比赛后仍为展开状态时不补播准备阶段事件。
    gameStatus.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    gameStatus.set_stage_countdown_sec(420);
    gameData.updateGameStatus(gameStatus);
    gameData.updateGlobalUnitStatus(status);
    QCOMPARE(armorOpenedSpy.count(), 0);

    // 比赛中离开状态 2 后再次展开应正常播报。
    status.set_base_status(1);
    gameData.updateGlobalUnitStatus(status);
    status.set_base_status(2);
    gameData.updateGlobalUnitStatus(status);
    QCOMPARE(armorOpenedSpy.count(), 1);
  }

  void testRuneVoicePromptFollowsGameClockAndRuneStatus() {
    GameData gameData;
    QSignalSpy voiceSpy(&gameData, &GameData::runeVoicePromptRequested);

    robomaster::GameStatus gameStatus;
    gameStatus.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    gameStatus.set_stage_countdown_sec(420);
    gameStatus.set_current_round(1);
    gameData.updateGameStatus(gameStatus);

    robomaster::RuneStatusSync runeStatus;
    runeStatus.set_rune_status(1);
    gameData.updateRuneStatusSync(runeStatus);

    gameStatus.set_stage_countdown_sec(390);
    gameData.updateGameStatus(gameStatus);
    QCOMPARE(voiceSpy.count(), 1);
    QCOMPARE(voiceSpy.at(0).at(0).toInt(),
             static_cast<int>(RM::RuneVoiceType::Small));
    QCOMPARE(voiceSpy.at(0).at(1).toInt(), 1);

    gameData.updateGameStatus(gameStatus);
    QCOMPARE(voiceSpy.count(), 1);

    runeStatus.set_rune_status(2);
    gameData.updateRuneStatusSync(runeStatus);
    gameStatus.set_stage_countdown_sec(360);
    gameData.updateGameStatus(gameStatus);
    QCOMPARE(voiceSpy.count(), 1);

    gameStatus.set_stage_countdown_sec(330);
    gameData.updateGameStatus(gameStatus);
    runeStatus.set_rune_status(3);
    gameData.updateRuneStatusSync(runeStatus);
    gameStatus.set_stage_countdown_sec(300);
    gameData.updateGameStatus(gameStatus);

    QCOMPARE(voiceSpy.count(), 2);
    QCOMPARE(voiceSpy.at(1).at(0).toInt(),
             static_cast<int>(RM::RuneVoiceType::Small));
    QCOMPARE(voiceSpy.at(1).at(1).toInt(), 1);
  }

  void testRuneVoicePromptHandlesServerOpeningZeroSequence() {
    GameData gameData;
    QSignalSpy voiceSpy(&gameData, &GameData::runeVoicePromptRequested);

    robomaster::GameStatus gameStatus;
    gameStatus.set_current_round(1);
    gameStatus.set_current_stage(robomaster::GameStage::STAGE_COUNTDOWN);
    gameStatus.set_stage_countdown_sec(0);
    gameData.updateGameStatus(gameStatus);

    gameStatus.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    gameData.updateGameStatus(gameStatus);
    gameData.updateGameStatus(gameStatus);

    robomaster::RuneStatusSync runeStatus;
    runeStatus.set_rune_status(1);
    gameData.updateRuneStatusSync(runeStatus);

    gameStatus.set_stage_countdown_sec(420);
    gameData.updateGameStatus(gameStatus);
    gameStatus.set_stage_countdown_sec(390);
    gameData.updateGameStatus(gameStatus);

    QCOMPARE(voiceSpy.count(), 1);
    QCOMPARE(voiceSpy.at(0).at(0).toInt(),
             static_cast<int>(RM::RuneVoiceType::Small));
    QCOMPARE(voiceSpy.at(0).at(1).toInt(), 1);

    gameStatus.set_current_stage(robomaster::GameStage::STAGE_SETTLEMENT);
    gameStatus.set_stage_countdown_sec(0);
    gameData.updateGameStatus(gameStatus);
    gameStatus.set_current_round(2);
    gameStatus.set_current_stage(robomaster::GameStage::STAGE_COUNTDOWN);
    gameData.updateGameStatus(gameStatus);
    gameStatus.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    gameData.updateGameStatus(gameStatus);
    gameData.updateRuneStatusSync(runeStatus);
    gameStatus.set_stage_countdown_sec(420);
    gameData.updateGameStatus(gameStatus);
    gameStatus.set_stage_countdown_sec(390);
    gameData.updateGameStatus(gameStatus);

    QCOMPARE(voiceSpy.count(), 2);
    QCOMPARE(voiceSpy.at(1).at(0).toInt(),
             static_cast<int>(RM::RuneVoiceType::Small));
    QCOMPARE(voiceSpy.at(1).at(1).toInt(), 1);
  }

  void testRuneVoicePromptHandlesServerOpeningOneSequence() {
    GameData gameData;
    QSignalSpy voiceSpy(&gameData, &GameData::runeVoicePromptRequested);

    robomaster::GameStatus gameStatus;
    gameStatus.set_current_round(1);
    gameStatus.set_current_stage(robomaster::GameStage::STAGE_COUNTDOWN);
    gameStatus.set_stage_countdown_sec(1);
    gameData.updateGameStatus(gameStatus);

    gameStatus.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    gameData.updateGameStatus(gameStatus);
    gameData.updateGameStatus(gameStatus);

    robomaster::RuneStatusSync runeStatus;
    runeStatus.set_rune_status(1);
    gameData.updateRuneStatusSync(runeStatus);

    gameStatus.set_stage_countdown_sec(420);
    gameData.updateGameStatus(gameStatus);
    gameStatus.set_stage_countdown_sec(390);
    gameData.updateGameStatus(gameStatus);

    QCOMPARE(voiceSpy.count(), 1);
    QCOMPARE(voiceSpy.at(0).at(0).toInt(),
             static_cast<int>(RM::RuneVoiceType::Small));
    QCOMPARE(voiceSpy.at(0).at(1).toInt(), 1);
  }

  void testRuneVoiceSettlementResetsSameRoundSimulation() {
    GameData gameData;
    QSignalSpy voiceSpy(&gameData, &GameData::runeVoicePromptRequested);

    robomaster::GameStatus gameStatus;
    gameStatus.set_current_round(0);
    gameStatus.set_current_stage(robomaster::GameStage::STAGE_COUNTDOWN);
    gameStatus.set_stage_countdown_sec(1);
    gameData.updateGameStatus(gameStatus);

    gameStatus.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    gameData.updateGameStatus(gameStatus);

    robomaster::RuneStatusSync runeStatus;
    runeStatus.set_rune_status(1);
    gameData.updateRuneStatusSync(runeStatus);

    gameStatus.set_stage_countdown_sec(420);
    gameData.updateGameStatus(gameStatus);
    gameStatus.set_stage_countdown_sec(390);
    gameData.updateGameStatus(gameStatus);
    QCOMPARE(voiceSpy.count(), 1);
    QCOMPARE(voiceSpy.at(0).at(1).toInt(), 1);

    gameStatus.set_current_stage(robomaster::GameStage::STAGE_SETTLEMENT);
    gameStatus.set_stage_countdown_sec(0);
    gameData.updateGameStatus(gameStatus);

    gameStatus.set_current_stage(robomaster::GameStage::STAGE_COUNTDOWN);
    gameStatus.set_stage_countdown_sec(1);
    gameData.updateGameStatus(gameStatus);
    gameStatus.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    gameData.updateGameStatus(gameStatus);
    gameData.updateRuneStatusSync(runeStatus);
    gameStatus.set_stage_countdown_sec(420);
    gameData.updateGameStatus(gameStatus);
    gameStatus.set_stage_countdown_sec(390);
    gameData.updateGameStatus(gameStatus);

    QCOMPARE(voiceSpy.count(), 2);
    QCOMPARE(voiceSpy.at(1).at(0).toInt(),
             static_cast<int>(RM::RuneVoiceType::Small));
    QCOMPARE(voiceSpy.at(1).at(1).toInt(), 1);
  }

  void testRuneVoiceDirectActivatedStatusReducesPromptCount() {
    GameData gameData;
    QSignalSpy voiceSpy(&gameData, &GameData::runeVoicePromptRequested);

    robomaster::GameStatus gameStatus;
    gameStatus.set_current_stage(robomaster::GameStage::STAGE_BATTLE);
    gameStatus.set_stage_countdown_sec(420);
    gameStatus.set_current_round(1);
    gameData.updateGameStatus(gameStatus);

    robomaster::RuneStatusSync runeStatus;
    runeStatus.set_rune_status(1);
    gameData.updateRuneStatusSync(runeStatus);

    gameStatus.set_stage_countdown_sec(330);
    gameData.updateGameStatus(gameStatus);
    QCOMPARE(voiceSpy.count(), 1);
    QCOMPARE(voiceSpy.at(0).at(1).toInt(), 2);

    runeStatus.set_rune_status(3);
    gameData.updateRuneStatusSync(runeStatus);
    gameStatus.set_stage_countdown_sec(300);
    gameData.updateGameStatus(gameStatus);

    QCOMPARE(voiceSpy.count(), 2);
    QCOMPARE(voiceSpy.at(1).at(0).toInt(),
             static_cast<int>(RM::RuneVoiceType::Small));
    QCOMPARE(voiceSpy.at(1).at(1).toInt(), 1);

    runeStatus.set_rune_status(1);
    gameData.updateRuneStatusSync(runeStatus);
    gameStatus.set_stage_countdown_sec(240);
    gameData.updateGameStatus(gameStatus);
    QCOMPARE(voiceSpy.last().at(0).toInt(),
             static_cast<int>(RM::RuneVoiceType::Large));
    QCOMPARE(voiceSpy.last().at(1).toInt(), 1);

    runeStatus.set_rune_status(3);
    gameData.updateRuneStatusSync(runeStatus);
    gameStatus.set_stage_countdown_sec(210);
    gameData.updateGameStatus(gameStatus);
    QCOMPARE(voiceSpy.last().at(0).toInt(),
             static_cast<int>(RM::RuneVoiceType::Large));
    QCOMPARE(voiceSpy.last().at(1).toInt(), 1);
    QCOMPARE(voiceSpy.count(), 3);
  }

  void testRuneStatusChangeLoggingIgnoresRepeatedAndInvalidPackets() {
    GameData gameData;
    robomaster::RuneStatusSync runeStatus;
    QStringList capturedMessages;

    g_runeStatusLogMessages = &capturedMessages;
    const QtMessageHandler previousHandler =
        qInstallMessageHandler(captureRuneStatusLog);

    runeStatus.set_rune_status(1);
    gameData.updateRuneStatusSync(runeStatus);
    gameData.updateRuneStatusSync(runeStatus);

    runeStatus.set_rune_status(4);
    gameData.updateRuneStatusSync(runeStatus);

    runeStatus.set_rune_status(2);
    gameData.updateRuneStatusSync(runeStatus);
    gameData.updateRuneStatusSync(runeStatus);

    qInstallMessageHandler(previousHandler);
    g_runeStatusLogMessages = nullptr;

    QCOMPARE(capturedMessages.size(), 2);
    QVERIFY(capturedMessages.at(0).contains(QStringLiteral("old=unknown new=1")));
    QVERIFY(capturedMessages.at(0).contains(QStringLiteral("stage=0")));
    QVERIFY(capturedMessages.at(0).contains(QStringLiteral("gameTime=420")));
    QVERIFY(capturedMessages.at(1).contains(QStringLiteral("old=1 new=2")));
  }
};

QTEST_MAIN(TestGameData)
#include "test_game_data.moc"
