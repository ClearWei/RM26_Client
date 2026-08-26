from __future__ import annotations

import sys
import unittest
from pathlib import Path

from google.protobuf import descriptor_pb2


RELEASE_DIR = Path(__file__).resolve().parents[1]
PROJECT_ROOT = RELEASE_DIR.parents[1]
if str(RELEASE_DIR) not in sys.path:
    sys.path.insert(0, str(RELEASE_DIR))

import check_sim_protobuf_runtime


class SimulatorProtobufRuntimeTest(unittest.TestCase):
    def test_repository_runtime_contract_passes(self) -> None:
        result = check_sim_protobuf_runtime.audit_repository(PROJECT_ROOT)

        self.assertEqual((), result.issues)
        self.assertEqual(
            check_sim_protobuf_runtime.ProtobufVersion(6, 33, 2),
            result.generated,
        )
        self.assertIsNotNone(result.runtime)
        self.assertFalse((PROJECT_ROOT / "sim/robomaster.proto").exists())

    def test_generated_module_is_really_imported(self) -> None:
        descriptor_name = check_sim_protobuf_runtime.import_generated_module(
            PROJECT_ROOT / "sim/robomaster_pb2.py"
        )

        self.assertEqual("robomaster.proto", descriptor_name)

    def test_generated_descriptor_matches_canonical_schema(self) -> None:
        generated = check_sim_protobuf_runtime.extract_generated_descriptor(
            PROJECT_ROOT / "sim/robomaster_pb2.py"
        )
        canonical = check_sim_protobuf_runtime.generate_canonical_descriptor(
            PROJECT_ROOT
        )

        self.assertTrue(
            check_sim_protobuf_runtime.descriptors_equal(generated, canonical)
        )

    def test_descriptor_drift_is_detected(self) -> None:
        generated = check_sim_protobuf_runtime.extract_generated_descriptor(
            PROJECT_ROOT / "sim/robomaster_pb2.py"
        )
        canonical = descriptor_pb2.FileDescriptorProto()
        canonical.CopyFrom(generated)
        canonical.package = "unexpected"

        self.assertFalse(
            check_sim_protobuf_runtime.descriptors_equal(generated, canonical)
        )

    def test_dependency_range_follows_generated_floor(self) -> None:
        generated = check_sim_protobuf_runtime.ProtobufVersion(6, 33, 2)

        self.assertEqual(
            "protobuf>=6.33.2,<7",
            check_sim_protobuf_runtime.expected_requirement(generated),
        )

    def test_unpinned_dependency_is_reported(self) -> None:
        issues = check_sim_protobuf_runtime.validate_dependency_specs(
            ["fastapi", "protobuf"],
            expected="protobuf>=6.33.2,<7",
            path="fixture.toml",
        )

        self.assertEqual(["dependency:range"], [issue.code for issue in issues])

    def test_older_runtime_is_reported(self) -> None:
        issues = check_sim_protobuf_runtime.validate_runtime_version(
            check_sim_protobuf_runtime.ProtobufVersion(6, 33, 2),
            check_sim_protobuf_runtime.ProtobufVersion(6, 33, 1),
        )

        self.assertEqual(["runtime:too-old"], [issue.code for issue in issues])


if __name__ == "__main__":
    unittest.main()
