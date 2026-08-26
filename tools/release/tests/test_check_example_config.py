from __future__ import annotations

import copy
import json
import sys
import unittest
from pathlib import Path


RELEASE_DIR = Path(__file__).resolve().parents[1]
PROJECT_ROOT = RELEASE_DIR.parents[1]
if str(RELEASE_DIR) not in sys.path:
    sys.path.insert(0, str(RELEASE_DIR))

import check_example_config


class ExampleConfigTest(unittest.TestCase):
    def setUp(self) -> None:
        self.payload = json.loads(
            (PROJECT_ROOT / "config.example.json").read_text(encoding="utf-8")
        )

    def _codes(self, payload: object) -> set[str]:
        return {
            issue.code for issue in check_example_config.validate_example_config(payload)
        }

    def test_repository_example_passes(self) -> None:
        self.assertEqual((), check_example_config.validate_example_config(self.payload))

    def test_missing_required_root_is_reported(self) -> None:
        payload = copy.deepcopy(self.payload)
        del payload["robots"]

        self.assertIn("type:object", self._codes(payload))

    def test_non_loopback_targets_are_rejected(self) -> None:
        payload = copy.deepcopy(self.payload)
        payload["network"]["server_ip"] = "192.168." + "1.20"
        payload["network"]["mqtt_broker"] = "10." + "0.0.8"

        issues = check_example_config.validate_example_config(payload)

        self.assertEqual(
            2, sum(issue.code == "network:not-loopback" for issue in issues)
        )

    def test_sensitive_field_is_rejected_recursively(self) -> None:
        payload = copy.deepcopy(self.payload)
        payload["network"]["access_token"] = "fixture"

        self.assertIn("secret:field", self._codes(payload))

    def test_absolute_and_parent_paths_are_rejected(self) -> None:
        payload = copy.deepcopy(self.payload)
        payload["video"]["default_path"] = "/Users/example/demo.mov"
        payload["ar_overlay"]["model_path"] = "../models/demo.onnx"

        issues = check_example_config.validate_example_config(payload)

        self.assertEqual(2, sum(issue.code == "path:unsafe" for issue in issues))

    def test_invalid_ports_and_robot_id_are_rejected(self) -> None:
        payload = copy.deepcopy(self.payload)
        payload["network"]["server_port"] = 0
        payload["network"]["mqtt_port"] = True
        payload["network"]["client_robot_id"] = 0

        codes = self._codes(payload)

        self.assertIn("value:range", codes)
        self.assertIn("type:integer", codes)

    def test_video_url_must_match_video_port(self) -> None:
        payload = copy.deepcopy(self.payload)
        payload["video"]["stream_url"] = "udp://0.0.0.0:4000"

        self.assertIn("video:port-mismatch", self._codes(payload))

    def test_robot_id_must_follow_protocol_range(self) -> None:
        payload = copy.deepcopy(self.payload)
        payload["network"]["client_robot_id"] = 8

        self.assertIn("network:robot-id", self._codes(payload))


if __name__ == "__main__":
    unittest.main()
