#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import unittest

from server.match_simulation_engine import MatchSimulationEngine
from server.state_manager import StateManager


class FakeClock:
    def __init__(self):
        self.value = 0.0

    def __call__(self):
        return self.value

    def set(self, value):
        self.value = float(value)


class FakeRadarPublisher:
    def __init__(self):
        self.radar_calls = []

    def publish_radar_info_to_client(self, **payload):
        self.radar_calls.append(dict(payload))
        return True


class MatchSimulationEngineTest(unittest.TestCase):
    def setUp(self):
        self.clock = FakeClock()
        self.state_manager = StateManager()
        self.publisher = FakeRadarPublisher()
        self.engine = MatchSimulationEngine(
            self.state_manager,
            mqtt_publisher=self.publisher,
            clock=self.clock,
            seed=2026,
        )

    def start(self, **options):
        return self.engine.control(
            "start",
            mode=options.get("mode", "quick"),
            speed=options.get("speed", 1.0),
            features=options.get(
                "features",
                {"positions": True, "hp": True, "events": True},
            ),
        )

    def tick_at(self, elapsed):
        self.clock.set(elapsed)
        return self.engine.tick()

    def test_quick_stage_boundaries_and_completion(self):
        status = self.start()
        self.assertEqual(status["status"], "running")
        self.assertEqual(status["current_stage"], 3)
        self.assertEqual(status["stage_countdown_sec"], 5)

        frame = self.tick_at(4.999)
        self.assertEqual(frame["current_stage"], 3)
        frame = self.tick_at(5.001)
        self.assertEqual(frame["current_stage"], 4)
        self.assertGreater(frame["remaining_sec"], 0)

        frame = self.tick_at(89.999)
        self.assertEqual(frame["status"], "running")
        self.assertEqual(frame["current_stage"], 4)
        frame = self.tick_at(90.0)
        self.assertEqual(frame["status"], "completed")
        self.assertEqual(frame["current_stage"], 5)
        self.assertEqual(frame["remaining_sec"], 0.0)

    def test_full_mode_uses_420_second_duration(self):
        status = self.start(mode="full")
        self.assertEqual(status["duration_sec"], 420.0)
        self.tick_at(419.9)
        self.assertEqual(self.engine.get_status()["status"], "running")
        self.tick_at(420.0)
        self.assertEqual(self.engine.get_status()["status"], "completed")

    def test_pause_freezes_time_and_resume_uses_new_clock_baseline(self):
        self.start()
        self.tick_at(5.0)
        radar_calls_before_pause = len(self.publisher.radar_calls)
        paused = self.engine.control("pause")
        frozen_elapsed = paused["elapsed_sec"]

        self.clock.set(55.0)
        self.assertIsNone(self.engine.tick())
        self.assertEqual(self.engine.get_status()["elapsed_sec"], frozen_elapsed)
        self.assertEqual(len(self.publisher.radar_calls), radar_calls_before_pause)

        self.engine.control("resume")
        self.tick_at(56.0)
        self.assertAlmostEqual(
            self.engine.get_status()["elapsed_sec"],
            frozen_elapsed + 1.0,
            places=3,
        )

    def test_timeline_events_are_emitted_exactly_once_even_when_tick_skips(self):
        self.start()
        self.tick_at(45.0)
        first_batch = self.state_manager.get_events()
        self.assertGreater(len(first_batch), 0)

        # 重复时间戳不能再次触发已经跨过的事件。
        self.tick_at(45.0)
        self.assertEqual(self.state_manager.get_events(), [])

        # 直接跳到结算阶段时，剩余跨过的事件仍应各发送一次。
        self.tick_at(90.0)
        second_batch = self.state_manager.get_events()
        all_keys = [event.get("key") for event in first_batch + second_batch]
        self.assertTrue(
            all(isinstance(event.get("id"), int) for event in first_batch + second_batch)
        )
        expected_keys = [item[0] for item in MatchSimulationEngine.EVENT_TIMELINE]
        self.assertCountEqual(all_keys, expected_keys)
        self.assertEqual(len(all_keys), len(set(all_keys)))
        by_key = {event["key"]: event for event in first_batch + second_batch}
        self.assertEqual(by_key["rune_metrics"]["param"], "4,7.2")
        self.assertEqual(by_key["dart_hit"]["param"], "1,1")
        self.assertEqual(by_key["outpost_destroyed"]["param"], "111")

    def test_hp_never_negative_and_death_is_followed_by_respawn(self):
        self.start()
        duration = self.engine.QUICK_DURATION_SEC
        countdown = self.engine.COUNTDOWN_DURATION_SEC
        battle_duration = duration - countdown

        self.tick_at(countdown + battle_duration * 0.35)
        death_state = self.state_manager.get_state()["globalUnitStatus"][
            "robot_health"
        ]
        self.assertEqual(death_state[5], 0)
        self.assertTrue(all(0 <= hp <= 600 for hp in death_state))

        self.tick_at(countdown + battle_duration * 0.40)
        respawn_state = self.state_manager.get_state()["globalUnitStatus"][
            "robot_health"
        ]
        self.assertGreater(respawn_state[5], 0)
        self.assertTrue(all(0 <= hp <= 600 for hp in respawn_state))

    def test_each_tick_publishes_at_most_one_batch_radar_frame(self):
        self.start()
        for timestamp in (0.1, 0.2, 0.3, 0.4):
            before = len(self.publisher.radar_calls)
            self.tick_at(timestamp)
            self.assertEqual(len(self.publisher.radar_calls) - before, 1)
            self.assertTrue(self.publisher.radar_calls[-1]["periodic"])

        self.engine.control("configure", features={"positions": False})
        before = len(self.publisher.radar_calls)
        self.tick_at(0.5)
        self.assertEqual(len(self.publisher.radar_calls), before)

    def test_disabled_hp_feature_keeps_existing_health_in_frame_and_state(self):
        self.start(features={"positions": True, "hp": False, "events": True})
        self.state_manager.set_robot_hp(0, 321)
        frame = self.tick_at(40.0)
        self.assertEqual(frame["robots"][0]["hp"], 321)
        self.assertEqual(
            self.state_manager.get_state()["globalUnitStatus"]["robot_health"][0],
            321,
        )

    def test_manual_position_override_pauses_live_engine(self):
        self.start()
        self.tick_at(4.0)
        was_running, status = self.engine.pause_for_manual_override()
        self.assertTrue(was_running)
        self.assertEqual(status["status"], "paused")
        self.assertEqual(status["pause_reason"], "manual_position")
        self.assertTrue(self.state_manager.get_state()["gameStatus"]["is_paused"])

        was_running, repeated_status = self.engine.pause_for_manual_override()
        self.assertFalse(was_running)
        self.assertEqual(repeated_status["status"], "paused")

    def test_configure_hot_switch_preserves_normalized_progress(self):
        self.start(mode="quick")
        self.tick_at(45.0)
        configured = self.engine.control("configure", mode="full", speed=2.0)
        self.assertEqual(configured["duration_sec"], 420.0)
        self.assertAlmostEqual(configured["elapsed_sec"], 210.0, places=2)
        self.assertEqual(configured["speed"], 2.0)

    def test_state_snapshot_contains_simulation_status(self):
        self.start()
        status = self.state_manager.get_state()["simulationStatus"]
        self.assertEqual(status["status"], "running")
        self.assertEqual(status["mode"], "quick")
        self.assertEqual(status["operator_team"], "red")
        self.assertEqual(status["features"]["positions"], True)

    def test_blue_operator_uses_protocol_relative_ally_enemy_semantics(self):
        state_manager = StateManager()
        state_manager.current_robot_id = 101
        publisher = FakeRadarPublisher()
        engine = MatchSimulationEngine(
            state_manager,
            mqtt_publisher=publisher,
            clock=self.clock,
            seed=2026,
        )
        status = engine.control("start", mode="quick", speed=1.0)
        self.assertEqual(status["operator_team"], "blue")

        battle_duration = engine.QUICK_DURATION_SEC - engine.COUNTDOWN_DURATION_SEC
        self.clock.set(engine.COUNTDOWN_DURATION_SEC + battle_duration * 0.23)
        frame = engine.tick()
        queued_events = state_manager.get_events()
        by_key = {event["key"]: event for event in queued_events}

        self.assertEqual(by_key["rune_metrics"]["team"], "blue")
        self.assertEqual(by_key["rune_metrics"]["target_team"], "blue")
        self.assertEqual(by_key["air_support"]["team"], "red")
        self.assertEqual(
            state_manager.referee_info_by_team["blue"]["activated_arms"],
            4,
        )
        self.assertEqual(state_manager.air_support["red_status"], 1)
        self.assertEqual(state_manager.air_support["blue_status"], 0)
        labels = {event["key"]: event["label"] for event in frame["recent_events"]}
        self.assertIn("蓝方", labels["rune_metrics"])
        self.assertIn("红方", labels["air_support"])

        self.clock.set(
            engine.COUNTDOWN_DURATION_SEC + battle_duration * 0.35
        )
        engine.tick()
        first_death = state_manager.get_state()["globalUnitStatus"]["robot_health"]
        self.assertEqual(first_death[1], 0)  # 红方步兵被蓝方英雄击杀
        self.assertNotEqual(first_death[5], 0)

        self.clock.set(
            engine.COUNTDOWN_DURATION_SEC + battle_duration * 0.63
        )
        engine.tick()
        second_death = state_manager.get_state()["globalUnitStatus"]["robot_health"]
        self.assertEqual(second_death[6], 0)  # 蓝方空中机器人被红方步兵击杀
        self.assertNotEqual(second_death[2], 0)

    def test_outpost_stops_while_alive_then_is_destroyed_with_event_2(self):
        self.start()
        battle_duration = self.engine.QUICK_DURATION_SEC - self.engine.COUNTDOWN_DURATION_SEC

        self.tick_at(
            self.engine.COUNTDOWN_DURATION_SEC + battle_duration * 0.70
        )
        stopped_state = self.state_manager.get_state()["globalUnitStatus"]
        self.assertGreater(stopped_state["blue_outpost_health"], 0)
        self.assertEqual(stopped_state["blue_outpost_status"], 2)
        self.assertFalse(stopped_state["blue_outpost_destroyed"])
        self.state_manager.get_events()

        self.tick_at(
            self.engine.COUNTDOWN_DURATION_SEC + battle_duration * 0.77
        )
        destroyed_state = self.state_manager.get_state()["globalUnitStatus"]
        self.assertEqual(destroyed_state["blue_outpost_health"], 0)
        self.assertEqual(destroyed_state["blue_outpost_status"], 3)
        self.assertTrue(destroyed_state["blue_outpost_destroyed"])
        destroyed_events = self.state_manager.get_events()
        self.assertTrue(
            any(
                event.get("id") == 2 and event.get("param") == "111"
                for event in destroyed_events
            )
        )

    def test_base_under_attack_damage_tracks_operator_team(self):
        self.start()
        battle_duration = self.engine.QUICK_DURATION_SEC - self.engine.COUNTDOWN_DURATION_SEC
        self.tick_at(
            self.engine.COUNTDOWN_DURATION_SEC + battle_duration * 0.85
        )
        red_state = self.state_manager.get_state()["globalUnitStatus"]
        self.assertLess(red_state["red_base_health"], 5000)
        self.assertEqual(red_state["blue_base_health"], 5000)

        blue_state_manager = StateManager()
        blue_state_manager.current_robot_id = 101
        blue_engine = MatchSimulationEngine(
            blue_state_manager,
            mqtt_publisher=FakeRadarPublisher(),
            clock=self.clock,
            seed=2026,
        )
        self.clock.set(0.0)
        blue_engine.control("start", mode="quick", speed=1.0)
        self.clock.set(
            blue_engine.COUNTDOWN_DURATION_SEC + battle_duration * 0.85
        )
        blue_engine.tick()
        blue_state = blue_state_manager.get_state()["globalUnitStatus"]
        self.assertLess(blue_state["blue_base_health"], 5000)
        self.assertEqual(blue_state["red_base_health"], 5000)

    def test_events_cannot_bypass_disabled_hp_for_outpost_destruction(self):
        self.start(
            features={"positions": True, "hp": False, "events": True}
        )
        battle_duration = self.engine.QUICK_DURATION_SEC - self.engine.COUNTDOWN_DURATION_SEC
        self.tick_at(
            self.engine.COUNTDOWN_DURATION_SEC + battle_duration * 0.77
        )
        state = self.state_manager.get_state()["globalUnitStatus"]
        events = self.state_manager.get_events()

        self.assertEqual(state["blue_outpost_health"], 1500)
        self.assertEqual(state["blue_outpost_status"], 2)
        self.assertFalse(state["blue_outpost_destroyed"])
        self.assertFalse(any(event.get("id") == 2 for event in events))

    def test_repeated_control_calls_are_idempotent(self):
        first_start = self.start()
        second_start = self.start()
        self.assertEqual(first_start["elapsed_sec"], second_start["elapsed_sec"])

        first_pause = self.engine.control("pause")
        second_pause = self.engine.control("pause")
        self.assertEqual(first_pause, second_pause)

        first_stop = self.engine.control("stop")
        second_stop = self.engine.control("stop")
        self.assertEqual(first_stop, second_stop)
        self.assertEqual(second_stop["status"], "stopped")
        stopped_state = self.state_manager.get_state()
        self.assertFalse(stopped_state["gameStatus"]["is_paused"])
        self.assertEqual(stopped_state["gameStatus"]["current_stage"], 0)
        self.assertEqual(second_stop["elapsed_sec"], 0.0)

    def test_fixed_seed_replays_identical_positions(self):
        self.start()
        first_frame = self.tick_at(25.0)
        first_positions = [
            (robot["x"], robot["y"], robot["angle"])
            for robot in first_frame["robots"]
        ]

        self.engine.control("stop")
        self.clock.set(100.0)
        self.engine.control("start", mode="quick", speed=1.0)
        second_frame = self.tick_at(125.0)
        second_positions = [
            (robot["x"], robot["y"], robot["angle"])
            for robot in second_frame["robots"]
        ]
        self.assertEqual(first_positions, second_positions)


if __name__ == "__main__":
    unittest.main()
