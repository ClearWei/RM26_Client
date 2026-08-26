from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


RELEASE_DIR = Path(__file__).resolve().parents[1]
if str(RELEASE_DIR) not in sys.path:
    sys.path.insert(0, str(RELEASE_DIR))

import check_packaging_readiness


class PackagingReadinessCheckTest(unittest.TestCase):
    def _new_repo(self) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary = tempfile.TemporaryDirectory()
        root = Path(temporary.name)
        subprocess.run(["git", "init", "--quiet"], cwd=root, check=True)
        return temporary, root

    def _write(self, root: Path, relative_path: str, content: str) -> None:
        path = root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def _track_all(self, root: Path) -> None:
        subprocess.run(["git", "add", "--all"], cwd=root, check=True)

    def _supply_chain(self) -> dict[str, str]:
        return {
            "sbom_generator": "tools/release/generate_sbom.py",
            "checksum_generator": "tools/release/generate_checksums.py",
            "signing_status": "not-required",
            "signing_evidence": "docs/signing-policy.md",
        }

    def _policy(self, mode: str) -> dict[str, object]:
        return {
            "schema_version": 1,
            "release_mode": mode,
            "source": {
                "artifacts": ["source-archive"],
                "build_evidence": "docs/source-build.md",
            },
            "binary": {
                "platforms": [],
                "artifacts": {},
                "platform_metadata": {},
                "runtime_dependency_evidence": "",
                "install_smoke_test": "",
            },
            "supply_chain": self._supply_chain(),
        }

    def _write_policy(
        self, root: Path, payload: dict[str, object]
    ) -> None:
        self._write(
            root,
            check_packaging_readiness.POLICY_PATH,
            json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
        )

    def _write_common_evidence(self, root: Path) -> None:
        self._write(root, "docs/source-build.md", "源码构建记录。\n")
        self._write(root, "docs/signing-policy.md", "源码归档暂不签名。\n")
        self._write(root, "tools/release/generate_sbom.py", "# fixture\n")
        self._write(root, "tools/release/generate_checksums.py", "# fixture\n")

    def test_undecided_mode_is_an_actionable_blocker(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(root, self._policy("undecided"))
        self._track_all(root)

        report = check_packaging_readiness.audit_repository(root)
        rendered = check_packaging_readiness.render_report(report)

        self.assertFalse(report.passed)
        self.assertEqual("undecided", report.release_mode)
        self.assertIn("mode:undecided", {item.code for item in report.findings})
        self.assertIn("由维护者选择 source 或 binary", rendered)

    def test_source_mode_accepts_only_source_archive_contract(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(root, self._policy("source"))
        self._write_common_evidence(root)
        self._track_all(root)

        report = check_packaging_readiness.audit_repository(root)

        self.assertTrue(report.passed)

    def test_source_mode_rejects_binary_package_claim(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        policy = self._policy("source")
        source = policy["source"]
        self.assertIsInstance(source, dict)
        source["artifacts"] = ["source-archive", "dmg"]
        self._write_policy(root, policy)
        self._write_common_evidence(root)
        self._track_all(root)

        report = check_packaging_readiness.audit_repository(root)

        self.assertIn(
            "source:binary-artifact-claim",
            {item.code for item in report.findings},
        )

    def test_reads_policy_and_artifacts_from_index(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(root, self._policy("source"))
        self._write_common_evidence(root)
        self._track_all(root)

        self._write_policy(root, self._policy("undecided"))
        self._write(root, "sim/sim.egg-info/PKG-INFO", "local only\n")
        self._write(root, "output/client.whl", "local only\n")

        report = check_packaging_readiness.audit_repository(root)

        self.assertTrue(report.passed)
        self.assertEqual("source", report.release_mode)

    def test_tracked_egg_info_and_build_outputs_are_blocked(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(root, self._policy("source"))
        self._write_common_evidence(root)
        self._write(root, "sim/sim.egg-info/PKG-INFO", "generated\n")
        self._write(root, "build/CMakeCache.txt", "generated\n")
        self._write(root, "dist/sim.whl", "generated\n")
        self._track_all(root)

        report = check_packaging_readiness.audit_repository(root)

        self.assertIn(
            "artifact:tracked-generated", {item.code for item in report.findings}
        )

    def test_binary_mode_requires_install_and_platform_contract(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        policy = self._policy("binary")
        policy["binary"] = {
            "platforms": ["macos"],
            "artifacts": {},
            "platform_metadata": {},
            "runtime_dependency_evidence": "",
            "install_smoke_test": "",
        }
        self._write_policy(root, policy)
        self._write_common_evidence(root)
        self._write(root, "CMakeLists.txt", "project(Fixture VERSION 1.0.0)\n")
        self._track_all(root)

        report = check_packaging_readiness.audit_repository(root)
        codes = {item.code for item in report.findings}

        self.assertTrue(
            {
                "binary:install-rules",
                "binary:artifact-format",
                "binary:platform-metadata",
                "binary:runtime-dependencies",
                "binary:install-smoke",
            }.issubset(codes)
        )

    def test_complete_binary_static_contract_passes(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        policy = self._policy("binary")
        policy["binary"] = {
            "platforms": ["macos"],
            "artifacts": {"macos": ["dmg"]},
            "platform_metadata": {"macos": "packaging/Info.plist.in"},
            "runtime_dependency_evidence": "tools/release/deploy_runtime.py",
            "install_smoke_test": "tools/release/test_installed_app.py",
        }
        self._write_policy(root, policy)
        self._write_common_evidence(root)
        self._write(
            root,
            "CMakeLists.txt",
            "project(Fixture VERSION 1.0.0)\ninstall(TARGETS Fixture DESTINATION bin)\n",
        )
        self._write(root, "packaging/Info.plist.in", "CFBundleIdentifier\n")
        self._write(root, "tools/release/deploy_runtime.py", "# fixture\n")
        self._write(root, "tools/release/test_installed_app.py", "# fixture\n")
        self._track_all(root)

        report = check_packaging_readiness.audit_repository(root)

        self.assertTrue(report.passed)

    def test_invalid_absolute_policy_path_is_not_echoed(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        policy = self._policy("source")
        source = policy["source"]
        self.assertIsInstance(source, dict)
        private_path = "/Users/example/private/build.md"
        source["build_evidence"] = private_path
        self._write_policy(root, policy)
        self._write_common_evidence(root)
        self._track_all(root)

        with self.assertRaises(check_packaging_readiness.PackagingReadinessError) as context:
            check_packaging_readiness.audit_repository(root)

        self.assertNotIn(private_path, str(context.exception))


if __name__ == "__main__":
    unittest.main()
