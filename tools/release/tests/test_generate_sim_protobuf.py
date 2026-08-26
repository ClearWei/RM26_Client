from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


RELEASE_DIR = Path(__file__).resolve().parents[1]
if str(RELEASE_DIR) not in sys.path:
    sys.path.insert(0, str(RELEASE_DIR))

import generate_sim_protobuf


class GenerateSimulatorProtobufTest(unittest.TestCase):
    def test_build_command_uses_canonical_schema_and_sim_output(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            (root / "src/network/proto").mkdir(parents=True)
            (root / "src/network/proto/robomaster.proto").write_text(
                'syntax = "proto3";\n', encoding="utf-8"
            )
            (root / "sim").mkdir()

            command = generate_sim_protobuf.build_command(root, "/tmp/protoc")

        self.assertEqual("/tmp/protoc", command[0])
        self.assertIn(f"--proto_path={root / 'src/network/proto'}", command)
        self.assertIn(f"--python_out={root / 'sim'}", command)
        self.assertEqual(str(root / "src/network/proto/robomaster.proto"), command[-1])

    def test_missing_canonical_schema_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            (root / "sim").mkdir()

            with self.assertRaisesRegex(
                generate_sim_protobuf.GenerationError,
                "robomaster.proto",
            ):
                generate_sim_protobuf.build_command(root, "protoc")


if __name__ == "__main__":
    unittest.main()
