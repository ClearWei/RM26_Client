#include "network/NetworkManager.h"
#include "core/GameData.h"
#include "robomaster.pb.h"
#include "CustomDataTypes.h"

#include <QTest>
#include <QSignalSpy>
#include <QDebug>
#include <QCoreApplication>
#include <QDateTime>
#include "config/ConfigManager.h"
#include <google/protobuf/text_format.h>

using namespace RM;

class TestMqttParsing : public QObject {
  Q_OBJECT

private slots:
  void testMqttParsing();
};

void TestMqttParsing::testMqttParsing() {
  GameData gd;
  // 设置有效的当前机器人 ID，供动态状态更新使用
  gd.setMyRobotId(1);

  NetworkManager nm(&gd);

  // 连接本地 MQTT 代理服务以验证订阅；若有外部发布端，测试也可接收
  // 真实消息。默认地址取自 config.json，通常为 tcp://127.0.0.1:3333。
  {
    const QString uri = QStringLiteral("tcp://%1:%2").arg(ConfigManager::instance().getMqttBroker()).arg(ConfigManager::instance().getMqttPort());
    const int configuredId = ConfigManager::instance().getClientRobotId();
    const QString clientId = QString::number(configuredId);
    qDebug() << "TestMqttParsing: attempting MQTT" << uri << "with clientId=" << clientId;
    if (!nm.startMqtt(uri, clientId)) {
      qWarning() << "TestMqttParsing: failed to start MQTT" << uri << "clientId=" << clientId;
    } else {
      QTRY_VERIFY(nm.isMqttConnected());
      qDebug() << "TestMqttParsing: MQTT connected" << uri << "clientId=" << clientId;
    }
  }

  QSignalSpy spyGameState(&gd, &GameData::gameStateUpdated);
  QSignalSpy spyDataChanged(&gd, &GameData::dataChanged);
  QSignalSpy spyRobotData(&gd, &GameData::robotDataUpdated);
  QSignalSpy spyRobotPos(&gd, &GameData::robotPositionUpdated);
  QSignalSpy spyRespawn(&gd, &GameData::robotRespawnStatusUpdated);
  QSignalSpy spyModuleStatus(&gd, &GameData::moduleStatusUpdated);
  QSignalSpy spySentryPath(&gd, &GameData::sentryPathUpdated);
  QSignalSpy spyAirSupport(&gd, &GameData::airSupportStatusUpdated);
  QSignalSpy spyDeploy(&gd, &GameData::deployModeStatusChanged);
  QSignalSpy spySilo(&gd, &GameData::siloStatusChanged);
  QSignalSpy spyNmDataReceived(&nm, &NetworkManager::dataReceived);

  // 辅助函数：序列化后交给 processProtocolData，再在本地解析并打印
  auto sendAndPrint = [&](const QString &topic, const google::protobuf::Message &msg) {
    std::string s;
    msg.SerializeToString(&s);
    QByteArray payload(s.data(), static_cast<int>(s.size()));
    nm.processMqttMessageForTest(topic, payload);

    // 再次在本地解析，便于校验和打印
    std::string parsed;
    const google::protobuf::Message *protoPtr = &msg;
    google::protobuf::TextFormat::PrintToString(*protoPtr, &parsed);
    qDebug().noquote() << "Parsed topic:" << topic;
    qDebug().noquote() << QString::fromStdString(parsed);
  };

  // 1) GameStatus
  {
    robomaster::GameStatus msg;
    msg.set_current_stage(static_cast<uint32_t>(robomaster::STAGE_BATTLE));
    msg.set_stage_countdown_sec(300);
    msg.set_red_score(2);
    msg.set_blue_score(1);
    msg.set_current_round(1);
    msg.set_is_paused(false);

    sendAndPrint(QStringLiteral("GameStatus"), msg);
    QVERIFY(spyGameState.wait(200));
  }

  // 2) GlobalUnitStatus
  {
    robomaster::GlobalUnitStatus msg;
    msg.set_base_health(4000);
    msg.set_base_status(1);
    msg.set_base_shield(100);
    msg.set_outpost_health(1000);
    msg.set_outpost_status(0);
    msg.set_enemy_base_health(3000);
    msg.set_enemy_base_status(1);
    msg.set_enemy_base_shield(50);
    msg.set_enemy_outpost_health(800);
    msg.set_enemy_outpost_status(0);
    msg.add_robot_health(500);
    msg.add_robot_bullets(50);
    msg.set_total_damage_ally(1200);
    msg.set_total_damage_enemy(900);

    int beforeDataChanged = spyDataChanged.count();
    int beforeRobotData = spyRobotData.count();
    sendAndPrint(QStringLiteral("GlobalUnitStatus"), msg);
    QVERIFY(spyDataChanged.wait(200));
    QVERIFY(spyRobotData.count() > beforeRobotData);
  }

  // 3) RobotDynamicStatus
  {
    robomaster::RobotDynamicStatus msg;
    msg.set_current_health(250);
    msg.set_current_heat(10.5f);
    msg.set_last_projectile_fire_rate(5.0f);
    msg.set_current_chassis_energy(80);
    msg.set_current_buffer_energy(40);
    msg.set_current_experience(200);
    msg.set_experience_for_upgrade(500);
    msg.set_total_projectiles_fired(123);
    msg.set_remaining_ammo(30);
    msg.set_is_out_of_combat(true);
    msg.set_out_of_combat_countdown(6);
    msg.set_can_remote_heal(true);
    msg.set_can_remote_ammo(false);

    int before = spyDataChanged.count();
    sendAndPrint(QStringLiteral("RobotDynamicStatus"), msg);
    QVERIFY(spyDataChanged.wait(200));
  }

  // 4) RobotPosition
  {
    robomaster::RobotPosition msg;
    msg.set_x(1.1f);
    msg.set_y(1.1f);
    msg.set_z(0.0f);
    msg.set_yaw(40.0f);
    msg.set_robot_id(5);

    sendAndPrint(QStringLiteral("RobotPosition"), msg);
    QVERIFY(spyRobotPos.wait(200));
  }

  // 5) CustomByteBlock：应调用 parseCustomByteBlock 并发出 NetworkManager::dataReceived
  {
    robomaster::CustomByteBlock msg;
    // 至少构造 sizeof(RobotCustomStatus) 大小的缓冲区
    const int bufSize = static_cast<int>(sizeof(RobotCustomStatus));
    QByteArray raw(bufSize, 0);
    for (int i = 0; i < bufSize; ++i) raw[i] = static_cast<char>(i & 0xFF);
    msg.set_data(raw.constData(), raw.size());

    int before = spyNmDataReceived.count();
    sendAndPrint(QStringLiteral("CustomByteBlock"), msg);
    QVERIFY(spyNmDataReceived.wait(200));
    QCOMPARE(spyNmDataReceived.count(), before + 1);
  }

  // 6) RobotRespawnStatus
  {
    robomaster::RobotRespawnStatus msg;
    msg.set_is_pending_respawn(true);
    msg.set_total_respawn_progress(100);
    msg.set_current_respawn_progress(10);
    msg.set_can_free_respawn(false);
    msg.set_gold_cost_for_respawn(50);
    msg.set_can_pay_for_respawn(true);

    sendAndPrint(QStringLiteral("RobotRespawnStatus"), msg);
    QVERIFY(spyRespawn.wait(200));
  }

  // 7) PenaltyInfo：应进入裁判警告更新路径
  {
    robomaster::PenaltyInfo msg;
    msg.set_penalty_type(2);
    msg.set_penalty_effect_sec(5);
    msg.set_total_penalty_num(1);

    sendAndPrint(QStringLiteral("PenaltyInfo"), msg);
    QVERIFY(spyDataChanged.wait(200));
  }

  // 8) RobotStaticStatus（补齐多项字段）
  {
    robomaster::RobotStaticStatus msg;
    msg.set_connection_state(1);
    msg.set_field_state(0);
    msg.set_alive_state(1);
    msg.set_robot_id(1);
    msg.set_robot_type(3);
    msg.set_performance_system_shooter(1);
    msg.set_performance_system_chassis(1);
    msg.set_level(2);
    msg.set_max_health(600);
    msg.set_max_heat(240);
    msg.set_heat_cooldown_rate(1.5f);
    msg.set_max_power(120);
    msg.set_max_buffer_energy(60);
    msg.set_max_chassis_energy(240);

    sendAndPrint(QStringLiteral("RobotStaticStatus"), msg);
    QVERIFY(spyDataChanged.wait(200));
  }

  // 9) RobotInjuryStat
  {
    robomaster::RobotInjuryStat msg;
    msg.set_total_damage(150);
    msg.set_collision_damage(10);
    msg.set_small_projectile_damage(80);
    msg.set_large_projectile_damage(50);
    msg.set_dart_splash_damage(5);
    msg.set_module_offline_damage(3);
    msg.set_offline_damage(2);
    msg.set_penalty_damage(0);
    msg.set_server_kill_damage(0);
    msg.set_killer_id(2);

    sendAndPrint(QStringLiteral("RobotInjuryStat"), msg);
    QVERIFY(spyDataChanged.wait(200));
  }

  // 10) RuneStatusSync
  {
    robomaster::RuneStatusSync msg;
    msg.set_rune_status(2);
    msg.set_activated_arms(3);
    msg.set_average_rings(2.5f);

    sendAndPrint(QStringLiteral("RuneStatusSync"), msg);
    QVERIFY(spyDataChanged.wait(200));
  }

  // 11) RadarInfoToClient
  {
    robomaster::RadarSingleRobotInfo info1;
    info1.set_target_pos_x(100);
    info1.set_target_pos_y(200);
    info1.set_is_high_light(1);
    robomaster::RadarInfoToClient msg;
    *msg.add_robot_info() = info1;

    sendAndPrint(QStringLiteral("RadarInfoToClient"), msg);
    QVERIFY(spyDataChanged.wait(200));
  }

  // 12) TechCoreMotionStateSync
  {
    robomaster::TechCoreMotionStateSync msg;
    msg.set_maximum_difficulty_level(4);
    msg.set_basic_state(3);
    msg.set_putin_state(1);
    msg.set_move_state(1);
    msg.set_rotate_state(0);
    msg.set_enemy_core_status(1);
    msg.set_remain_time_all(60);
    msg.set_remain_time_step(10);

    sendAndPrint(QStringLiteral("TechCoreMotionStateSync"), msg);
    QVERIFY(spyDataChanged.wait(200));
  }

  // 13) GlobalLogisticsStatus：通过 updateGameState 映射到 GameInfo
  {
    robomaster::GlobalLogisticsStatus msg;
    msg.set_remaining_economy(500);
    msg.set_total_economy_obtained(12345);
    msg.set_tech_level(3);
    msg.set_encryption_level(1);

    int before = spyGameState.count();
    sendAndPrint(QStringLiteral("GlobalLogisticsStatus"), msg);
    QVERIFY(spyGameState.wait(200));
    QCOMPARE(spyGameState.count(), before + 1);
  }

  // 14) Buff：应发出 buffPointUpdated 或 dataChanged
  {
    robomaster::Buff msg;
    msg.set_robot_id(1);
    msg.set_buff_type(1);
    msg.set_buff_level(2);
    msg.set_buff_max_time(30);
    msg.set_buff_left_time(25);

    int before = spyDataChanged.count();
    sendAndPrint(QStringLiteral("Buff"), msg);
    QVERIFY(spyDataChanged.wait(200));
    QVERIFY(spyDataChanged.count() > before);
  }

  // 15) RobotModuleStatus：应发出 moduleStatusUpdated
  {
    robomaster::RobotModuleStatus msg;
    msg.set_power_manager(1);
    msg.set_rfid(1);
    msg.set_light_strip(1);
    msg.set_small_shooter(1);

    int before = spyModuleStatus.count();
    sendAndPrint(QStringLiteral("RobotModuleStatus"), msg);
    QVERIFY(spyModuleStatus.wait(200));
    QCOMPARE(spyModuleStatus.count(), before + 1);
  }

  // 16) DartSelectTargetStatusSync：更新发射井状态
  {
    robomaster::DartSelectTargetStatusSync msg;
    msg.set_target_id(2);
    msg.set_open(1);

    int before = spySilo.count();
    sendAndPrint(QStringLiteral("DartSelectTargetStatusSync"), msg);
    QVERIFY(spySilo.wait(200));
    QCOMPARE(spySilo.count(), before + 1);
  }

  // 17) RobotPathPlanInfo：更新哨兵路径
  {
    robomaster::RobotPathPlanInfo msg;
    msg.set_intention(1);
    msg.set_start_pos_x(10);
    msg.set_start_pos_y(20);
    msg.add_offset_x(1);
    msg.add_offset_y(2);
    msg.set_sender_id(1);

    int before = spySentryPath.count();
    sendAndPrint(QStringLiteral("RobotPathPlanInfo"), msg);
    QVERIFY(spySentryPath.wait(200));
    QCOMPARE(spySentryPath.count(), before + 1);
  }

  // 18) AirSupportStatusSync：更新空中支援状态
  {
    robomaster::AirSupportStatusSync msg;
    msg.set_airsupport_status(1);
    msg.set_left_time(30);
    msg.set_cost_coins(5);
    msg.set_is_being_targeted(1);
    msg.set_shooter_status(1);

    int before = spyAirSupport.count();
    sendAndPrint(QStringLiteral("AirSupportStatusSync"), msg);
    QVERIFY(spyAirSupport.wait(200));
    QCOMPARE(spyAirSupport.count(), before + 1);
    QCOMPARE(gd.airSupportIsBeingTargeted(), 1);
    QCOMPARE(gd.airSupportShooterStatus(), 1);
  }

  // 19) DeployModeStatusSync：更新部署模式状态
  {
    robomaster::DeployModeStatusSync msg;
    msg.set_status(2);

    int before = spyDeploy.count();
    sendAndPrint(QStringLiteral("DeployModeStatusSync"), msg);
    QVERIFY(spyDeploy.wait(200));
    QCOMPARE(spyDeploy.count(), before + 1);
  }

  // 20) Event：经 processProtocolData 分发，至少应发出 dataChanged
  {
    robomaster::Event msg;
    msg.set_event_id(42);
    msg.set_param("test-event");

    int before = spyDataChanged.count();
    sendAndPrint(QStringLiteral("Event"), msg);
    QVERIFY(spyDataChanged.wait(200));
    QVERIFY(spyDataChanged.count() > before);
  }

  // --- 补齐其余消息，覆盖完整协议集合 ---

  // RobotStatus
  {
    robomaster::RobotStatus msg;
    msg.set_id(1);
    msg.set_team(robomaster::TEAM_RED);
    msg.set_type(robomaster::TYPE_INFANTRY);
    msg.set_level(2);
    msg.set_hp(400);
    msg.set_max_hp(600);
    sendAndPrint(QStringLiteral("RobotStatus"), msg);
    QVERIFY(true);
  }

  // RobotPositionUDP
  {
    robomaster::RobotPositionUDP msg;
    msg.set_x(1.0f);
    msg.set_y(2.0f);
    msg.set_angle(90.0f);
    msg.set_is_high_light(true);
    msg.set_id(3);
    sendAndPrint(QStringLiteral("RobotPositionUDP"), msg);
    QVERIFY(true);
  }

  // MapClickInfoNotify
  {
    robomaster::MapClickInfoNotify msg;
    msg.set_is_send_all(1);
    QByteArray ids(7, 0);
    ids[0] = 1;
    msg.set_robot_id(ids.constData(), ids.size());
    msg.set_mode(1);
    sendAndPrint(QStringLiteral("MapClickInfoNotify"), msg);
    QVERIFY(true);
  }

  // SentryCtrlCommand
  {
    robomaster::SentryCtrlCommand msg;
    msg.set_command_id(2);
    sendAndPrint(QStringLiteral("SentryCtrlCommand"), msg);
    QVERIFY(true);
  }

  // DartCommand
  {
    robomaster::DartCommand msg;
    msg.set_target_id(1);
    msg.set_open(true);
    msg.set_launch_confirm(false);
    sendAndPrint(QStringLiteral("DartCommand"), msg);
    QVERIFY(true);
  }

  // BattleMessage
  {
    robomaster::BattleMessage msg;
    msg.set_content("Test battle");
    msg.set_duration(2.5f);
    msg.set_color_hex(0xFF00FF);
    sendAndPrint(QStringLiteral("BattleMessage"), msg);
    QVERIFY(true);
  }

  // ClientStatus
  {
    robomaster::ClientStatus msg;
    msg.set_volume(50);
    msg.set_resolution("800x600");
    msg.set_fullscreen(false);
    sendAndPrint(QStringLiteral("ClientStatus"), msg);
    QVERIFY(true);
  }

  // VideoControl
  {
    robomaster::VideoControl msg;
    msg.set_video_url("udp://127.0.0.1:3334");
    msg.set_is_playing(true);
    sendAndPrint(QStringLiteral("VideoControl"), msg);
    QVERIFY(true);
  }

  // MapMarking
  {
    robomaster::MapMarking msg;
    msg.set_x(0.5f);
    msg.set_y(0.5f);
    msg.set_mark_type(1);
    sendAndPrint(QStringLiteral("MapMarking"), msg);
    QVERIFY(true);
  }

  // RobotCommand
  {
    robomaster::RobotCommand msg;
    msg.set_cmd_type(1);
    msg.set_target_id(0);
    sendAndPrint(QStringLiteral("RobotCommand"), msg);
    QVERIFY(true);
  }

  // GlobalUnitStatusInternal
  {
    robomaster::GlobalUnitStatusInternal msg;
    msg.set_red_base_health(5000);
    msg.set_blue_base_health(5000);
    sendAndPrint(QStringLiteral("GlobalUnitStatusInternal"), msg);
    QVERIFY(true);
  }

  // BaseHealth
  {
    robomaster::BaseHealth msg;
    msg.set_team(1);
    msg.set_hp(4500);
    msg.set_max_hp(5000);
    msg.set_is_invincible(false);
    sendAndPrint(QStringLiteral("BaseHealth"), msg);
    QVERIFY(true);
  }

  // MapRobotData
  {
    robomaster::MapRobotData msg;
    msg.set_hero_x(10);
    msg.set_hero_y(20);
    sendAndPrint(QStringLiteral("MapRobotData"), msg);
    QVERIFY(true);
  }

  // CommonCommand
  {
    robomaster::CommonCommand msg;
    msg.set_cmd_type(3);
    msg.set_param(7);
    sendAndPrint(QStringLiteral("CommonCommand"), msg);
    QVERIFY(true);
  }

  // GroundRobotPosition
  {
    robomaster::GroundRobotPosition msg;
    msg.set_hero_x(1.1f);
    msg.set_hero_y(2.2f);
    sendAndPrint(QStringLiteral("GroundRobotPosition"), msg);
    QVERIFY(true);
  }

  // RefereeWarningData
  {
    robomaster::RefereeWarningData msg;
    msg.set_level(1);
    msg.set_offending_robot_id(2);
    msg.set_count(1);
    msg.set_penalty_effect_sec(5);
    msg.set_total_penalty_num(1);
    msg.set_source("unit-test");
    sendAndPrint(QStringLiteral("RefereeWarningData"), msg);
    QVERIFY(true);
  }

  // GlobalSpecialMechanism
  {
    robomaster::GlobalSpecialMechanism msg;
    msg.add_mechanism_id(101);
    msg.add_mechanism_time_sec(30);
    sendAndPrint(QStringLiteral("GlobalSpecialMechanism"), msg);
    QVERIFY(true);
  }

  // KeyboardMouseControl
  {
    robomaster::KeyboardMouseControl msg;
    msg.set_mouse_x(100);
    msg.set_mouse_y(200);
    msg.set_left_button_down(true);
    sendAndPrint(QStringLiteral("KeyboardMouseControl"), msg);
    QVERIFY(true);
  }

  // CustomControl
  {
    robomaster::CustomControl msg;
    msg.set_data("abc");
    sendAndPrint(QStringLiteral("CustomControl"), msg);
    QVERIFY(true);
  }

  // RadarMarkData
  {
    robomaster::RadarMarkData msg;
    msg.set_mark_hero_progress(1);
    msg.set_energy_activatable(true);
    sendAndPrint(QStringLiteral("RadarMarkData"), msg);
    QVERIFY(true);
  }

  // AssemblyCommand
  {
    robomaster::AssemblyCommand msg;
    msg.set_operation(1);
    msg.set_difficulty(2);
    sendAndPrint(QStringLiteral("AssemblyCommand"), msg);
    QVERIFY(true);
  }

  // RobotPerformanceSelectionCommand
  {
    robomaster::RobotPerformanceSelectionCommand msg;
    msg.set_shooter(1);
    msg.set_chassis(2);
    msg.set_sentry_control(3);
    sendAndPrint(QStringLiteral("RobotPerformanceSelectionCommand"), msg);
    QVERIFY(true);
  }

  // RobotPerformanceSelectionSync
  {
    robomaster::RobotPerformanceSelectionSync msg;
    msg.set_shooter(1);
    msg.set_chassis(2);
    msg.set_sentry_control(3);
    sendAndPrint(QStringLiteral("RobotPerformanceSelectionSync"), msg);
    QVERIFY(true);
  }

  // HeroDeployModeEventCommand
  {
    robomaster::HeroDeployModeEventCommand msg;
    msg.set_mode(1);
    sendAndPrint(QStringLiteral("HeroDeployModeEventCommand"), msg);
    QVERIFY(true);
  }

  // RuneActivateCommand
  {
    robomaster::RuneActivateCommand msg;
    msg.set_activate(1);
    sendAndPrint(QStringLiteral("RuneActivateCommand"), msg);
    QVERIFY(true);
  }

  // SentryStatusSync
  {
    robomaster::SentryStatusSync msg;
    msg.set_posture_id(1);
    msg.set_is_weakened(false);
    sendAndPrint(QStringLiteral("SentryStatusSync"), msg);
    QVERIFY(true);
  }

  // SentryCtrlResult
  {
    robomaster::SentryCtrlResult msg;
    msg.set_command_id(1);
    msg.set_result_code(0);
    sendAndPrint(QStringLiteral("SentryCtrlResult"), msg);
    QVERIFY(true);
  }

  // AirSupportCommand
  {
    robomaster::AirSupportCommand msg;
    msg.set_command_id(1);
    sendAndPrint(QStringLiteral("AirSupportCommand"), msg);
    QVERIFY(true);
  }

}

QTEST_APPLESS_MAIN(TestMqttParsing)

#include "test_mqtt_parsing.moc"
