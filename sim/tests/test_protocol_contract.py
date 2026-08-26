#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import unittest

from google.protobuf import descriptor_pb2

import robomaster_pb2 as pb
from server.mqtt_publisher import MQTTPublisher
from server.state_manager import StateManager


class ProtocolContractTest(unittest.TestCase):
    @staticmethod
    def _message_descriptor(name):
        file_descriptor = descriptor_pb2.FileDescriptorProto.FromString(
            pb.DESCRIPTOR.serialized_pb
        )
        return next(
            message
            for message in file_descriptor.message_type
            if message.name == name
        )

    @staticmethod
    def _publisher():
        publisher = object.__new__(MQTTPublisher)
        publisher.state_manager = type(
            "FakeStateManager",
            (),
            {
                "last_game_result": {
                    "winner": 0,
                    "end_reason": 255,
                }
            },
        )()
        published = []
        publisher._publish = (
            lambda topic, payload, qos=1: published.append((topic, payload, qos))
        )
        return publisher, published

    def test_official_descriptor_fields_and_presence(self):
        expected_fields = {
            "GameStatus": [
                ("current_round", 1),
                ("total_rounds", 2),
                ("red_score", 3),
                ("blue_score", 4),
                ("current_stage", 5),
                ("stage_countdown_sec", 6),
                ("stage_elapsed_sec", 7),
                ("is_paused", 8),
                ("game_result", 9),
                ("end_reason", 10),
            ],
            "Buff": [
                ("robot_id", 1),
                ("buff_type", 2),
                ("buff_level", 3),
                ("buff_max_time", 4),
                ("buff_left_time", 5),
            ],
            "AssemblyCommand": [("operation", 1), ("difficulty", 2)],
            "SentryStatusSync": [
                ("posture_id", 1),
                ("is_weakened", 2),
                ("is_powered", 3),
            ],
        }

        for message_name, expected in expected_fields.items():
            with self.subTest(message=message_name):
                descriptor = self._message_descriptor(message_name)
                self.assertEqual(
                    expected,
                    [(field.name, field.number) for field in descriptor.field],
                )
                self.assertTrue(
                    all(field.proto3_optional for field in descriptor.field)
                )

        buff = self._message_descriptor("Buff")
        self.assertEqual(["msg_params"], list(buff.reserved_name))
        self.assertEqual(
            [(6, 7)],
            [(item.start, item.end) for item in buff.reserved_range],
        )

    def test_cross_language_wire_golden_payloads(self):
        game_status = pb.GameStatus(
            current_round=0,
            total_rounds=0,
            red_score=0,
            blue_score=0,
            current_stage=0,
            stage_countdown_sec=0,
            stage_elapsed_sec=0,
            is_paused=False,
            game_result=0,
            end_reason=0,
        )
        self.assertEqual(
            bytes.fromhex("0800100018002000280030003800400048005000"),
            game_status.SerializeToString(),
        )

        buff = pb.Buff(
            robot_id=0,
            buff_type=0,
            buff_level=0,
            buff_max_time=0,
            buff_left_time=0,
        )
        self.assertEqual(
            bytes.fromhex("08001000180020002800"),
            buff.SerializeToString(),
        )

        assembly = pb.AssemblyCommand(operation=0, difficulty=1)
        self.assertEqual(bytes.fromhex("08001001"), assembly.SerializeToString())

        sentry = pb.SentryStatusSync(
            posture_id=0,
            is_weakened=False,
            is_powered=False,
        )
        self.assertEqual(
            bytes.fromhex("080010001800"),
            sentry.SerializeToString(),
        )

    def test_game_status_uses_unsettled_sentinel_before_settlement(self):
        publisher, published = self._publisher()
        publisher._publish_game_status(
            {
                "gameStatus": {
                    "current_round": 1,
                    "total_rounds": 3,
                    "red_score": 9,
                    "blue_score": 1,
                    "current_stage": 4,
                    "stage_countdown_sec": 120,
                    "stage_elapsed_sec": 300,
                    "is_paused": False,
                },
                "lastGameResult": {"winner": 1, "end_reason": 1},
            }
        )

        self.assertEqual("GameStatus", published[-1][0])
        message = pb.GameStatus.FromString(published[-1][1])
        self.assertEqual(255, message.game_result)
        self.assertEqual(255, message.end_reason)
        self.assertTrue(message.HasField("game_result"))
        self.assertTrue(message.HasField("end_reason"))

    def test_game_status_uses_state_manager_result_during_settlement(self):
        publisher, published = self._publisher()
        publisher._publish_game_status(
            {
                "gameStatus": {
                    "current_round": 1,
                    "total_rounds": 3,
                    "red_score": 0,
                    "blue_score": 0,
                    "current_stage": 5,
                    "stage_countdown_sec": 0,
                    "stage_elapsed_sec": 0,
                    "is_paused": False,
                },
                "lastGameResult": {"winner": 2, "end_reason": 1},
            }
        )

        message = pb.GameStatus.FromString(published[-1][1])
        self.assertEqual(2, message.game_result)
        self.assertEqual(1, message.end_reason)

    def test_sentry_publisher_includes_powered_state(self):
        publisher, published = self._publisher()
        self.assertTrue(
            publisher.publish_sentry_status_sync(
                {"posture_id": 2, "is_weakened": True, "is_powered": True}
            )
        )

        message = pb.SentryStatusSync.FromString(published[-1][1])
        self.assertEqual(2, message.posture_id)
        self.assertTrue(message.is_weakened)
        self.assertTrue(message.is_powered)

    def test_state_manager_exposes_authoritative_settlement_result(self):
        state_manager = StateManager()
        state_manager.end_match()

        result = state_manager.get_state()["lastGameResult"]
        self.assertIn(result["winner"], (0, 1, 2))
        self.assertEqual(1, result["end_reason"])


if __name__ == "__main__":
    unittest.main()
