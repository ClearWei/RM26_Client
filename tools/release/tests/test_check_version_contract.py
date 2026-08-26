from __future__ import annotations

import json
import plistlib
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


RELEASE_DIR = Path(__file__).resolve().parents[1]
if str(RELEASE_DIR) not in sys.path:
    sys.path.insert(0, str(RELEASE_DIR))

import check_version_contract


class VersionContractCheckTest(unittest.TestCase):
    def _new_repo(self) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary = tempfile.TemporaryDirectory()
        root = Path(temporary.name)
        subprocess.run(["git", "init", "--quiet"], cwd=root, check=True)
        return temporary, root

    def _write_text(self, root: Path, relative_path: str, content: str) -> None:
        path = root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def _write_bytes(self, root: Path, relative_path: str, content: bytes) -> None:
        path = root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)

    def _track_all(self, root: Path) -> None:
        subprocess.run(["git", "add", "--all"], cwd=root, check=True)

    def _policy(
        self,
        *,
        application_strategy: str = "single-source",
        application_version: str = "1.2.3",
        simulator_strategy: str = "independent",
        simulator_version: str = "0.4.0",
        executable_strategy: str = "legacy-compatible",
        compatibility_through: str = "1.9.0",
    ) -> dict[str, object]:
        return {
            "schema_version": 1,
            "application": {
                "strategy": application_strategy,
                "declared_version": (
                    application_version
                    if application_strategy == "single-source"
                    else ""
                ),
                "sources": {
                    "cmake_project": "CMakeLists.txt",
                    "example_config": "config.example.json",
                    "runtime_metadata": "src/main.cpp",
                    "config_fallback": "src/config/ConfigManager.cpp",
                    "docker_image": "docker/client.Dockerfile",
                    "macos_bundle": "Info.plist",
                },
            },
            "simulator": {
                "strategy": simulator_strategy,
                "declared_version": (
                    simulator_version
                    if simulator_strategy == "independent"
                    else ""
                ),
                "sources": {"pyproject": "sim/pyproject.toml"},
            },
            "protocol": {
                "strategy": "independent",
                "sources": {
                    "manifest": "src/network/proto/protocol_manifest.json"
                },
            },
            "executable": {
                "strategy": executable_strategy,
                "canonical_name": "RM26CustomClient",
                "legacy_names": ["RoboMasterClient2025"],
                "legacy_compatibility_through": (
                    compatibility_through
                    if executable_strategy == "legacy-compatible"
                    else ""
                ),
                "sources": {
                    "cmake_targets": "CMakeLists.txt",
                    "docker_entrypoint": "docker/client-entrypoint.sh",
                    "macos_bundle": "Info.plist",
                },
            },
        }

    def _write_policy(self, root: Path, policy: dict[str, object]) -> None:
        self._write_text(
            root,
            check_version_contract.POLICY_PATH,
            json.dumps(policy, ensure_ascii=False, indent=2) + "\n",
        )

    def _write_sources(
        self,
        root: Path,
        *,
        application_version: str = "1.2.3",
        simulator_version: str = "0.4.0",
        protocol_version: str = "2.0.0",
        executable_name: str = "RoboMasterClient2025",
        runtime_version: str | None = None,
    ) -> None:
        runtime_version = runtime_version or application_version
        self._write_text(
            root,
            "CMakeLists.txt",
            "cmake_minimum_required(VERSION 3.16)\n"
            f"project(RM26CustomClient VERSION {application_version} LANGUAGES CXX)\n"
            f"add_executable({executable_name} src/main.cpp)\n",
        )
        self._write_text(
            root,
            "config.example.json",
            json.dumps(
                {"app_settings": {"version": application_version}},
                ensure_ascii=False,
            )
            + "\n",
        )
        self._write_text(
            root,
            "src/main.cpp",
            "int main() {\n"
            f'  app.setApplicationVersion("{runtime_version}");\n'
            "}\n",
        )
        self._write_text(
            root,
            "src/config/ConfigManager.cpp",
            "QString ConfigManager::getAppVersion() const {\n"
            "  return m_config[\"app_settings\"].toObject()[\"version\"]"
            f'.toString("{application_version}");\n'
            "}\n",
        )
        self._write_text(
            root,
            "docker/client.Dockerfile",
            "FROM example.invalid/base\n"
            "LABEL org.opencontainers.image.title=\"RM26 Test\" "
            f'org.opencontainers.image.version="{application_version}"\n',
        )
        self._write_text(
            root,
            "docker/client-entrypoint.sh",
            "#!/usr/bin/env sh\n"
            f"exec /opt/rmclient/build/{executable_name} \"$@\"\n",
        )
        plist = {
            "CFBundleExecutable": executable_name,
            "CFBundleVersion": application_version,
            "CFBundleShortVersionString": application_version,
        }
        self._write_bytes(root, "Info.plist", plistlib.dumps(plist))
        self._write_text(
            root,
            "sim/pyproject.toml",
            "[build-system]\nrequires = [\"setuptools\"]\n\n"
            "[project]\n"
            "name = \"rm26-sim\"\n"
            f'version = "{simulator_version}"\n',
        )
        self._write_text(
            root,
            "src/network/proto/protocol_manifest.json",
            json.dumps(
                {"protocol": {"name": "RoboMaster", "version": protocol_version}},
                ensure_ascii=False,
            )
            + "\n",
        )

    def _prepare_valid_repo(
        self,
        root: Path,
        *,
        application_version: str = "1.2.3",
        simulator_version: str = "0.4.0",
        simulator_strategy: str = "independent",
        executable_name: str = "RoboMasterClient2025",
        executable_strategy: str = "legacy-compatible",
        compatibility_through: str = "1.9.0",
    ) -> None:
        self._write_sources(
            root,
            application_version=application_version,
            simulator_version=simulator_version,
            executable_name=executable_name,
        )
        self._write_policy(
            root,
            self._policy(
                application_version=application_version,
                simulator_strategy=simulator_strategy,
                simulator_version=simulator_version,
                executable_strategy=executable_strategy,
                compatibility_through=compatibility_through,
            ),
        )
        self._track_all(root)

    def test_reads_all_sources_and_policy_from_index(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._prepare_valid_repo(root)

        # 工作区改成冲突状态，但公开快照仍应以已经暂存的内容为准。
        self._write_sources(
            root,
            application_version="9.9.9",
            simulator_version="8.8.8",
            executable_name="LocalOnlyTarget",
        )
        self._write_policy(
            root,
            self._policy(
                application_strategy="undecided",
                simulator_strategy="undecided",
                executable_strategy="undecided",
            ),
        )

        report = check_version_contract.audit_repository(root)

        self.assertTrue(report.passed)
        self.assertEqual(
            {"1.2.3", "0.4.0", "2.0.0"},
            {source.version for source in report.version_sources},
        )
        self.assertEqual(
            {"RoboMasterClient2025"},
            {name for source in report.name_sources for name in source.names},
        )

    def test_undecided_policy_is_a_real_blocker_and_reports_source_groups(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_sources(root, runtime_version="12.0.0.111")
        self._write_policy(
            root,
            self._policy(
                application_strategy="undecided",
                simulator_strategy="undecided",
                executable_strategy="undecided",
            ),
        )
        self._track_all(root)

        report = check_version_contract.audit_repository(root)
        rendered = check_version_contract.render_report(report)
        codes = {finding.code for finding in report.findings}

        self.assertFalse(report.passed)
        self.assertTrue(
            {
                "application:version-drift",
                "application:strategy-undecided",
                "simulator:strategy-undecided",
                "executable:strategy-undecided",
            }.issubset(codes)
        )
        self.assertIn("CMake 工程=1.2.3", rendered)
        self.assertIn("Qt 运行时元数据=12.0.0.111", rendered)
        self.assertIn("协议（独立维度，不参与应用版本比较）", rendered)

    def test_consistent_single_source_contract_passes(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._prepare_valid_repo(root)

        report = check_version_contract.audit_repository(root)

        self.assertTrue(report.passed)

    def test_application_drift_names_only_the_conflicting_sources(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_sources(root, runtime_version="1.2.4")
        self._write_policy(root, self._policy())
        self._track_all(root)

        report = check_version_contract.audit_repository(root)
        rendered = check_version_contract.render_report(report)

        self.assertIn(
            "application:version-drift", {finding.code for finding in report.findings}
        )
        self.assertIn("Qt 运行时元数据", rendered)
        self.assertNotIn("src/main.cpp", rendered)

    def test_simulator_can_use_independent_or_shared_versioning(self) -> None:
        for strategy, simulator_version, expected_code in (
            ("independent", "0.4.0", None),
            ("shared", "1.2.3", None),
            ("shared", "0.4.0", "simulator:shared-version-drift"),
        ):
            with self.subTest(strategy=strategy, simulator_version=simulator_version):
                temporary, root = self._new_repo()
                self.addCleanup(temporary.cleanup)
                self._prepare_valid_repo(
                    root,
                    simulator_version=simulator_version,
                    simulator_strategy=strategy,
                )

                report = check_version_contract.audit_repository(root)
                codes = {finding.code for finding in report.findings}

                if expected_code is None:
                    self.assertTrue(report.passed)
                else:
                    self.assertIn(expected_code, codes)

    def test_protocol_version_is_never_compared_with_application_version(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._prepare_valid_repo(root)
        self._write_sources(root, protocol_version="9.8.7")
        self._track_all(root)

        report = check_version_contract.audit_repository(root)

        self.assertTrue(report.passed)
        self.assertEqual(
            ["9.8.7"],
            [
                source.version
                for source in report.version_sources
                if source.category == "protocol"
            ],
        )

    def test_legacy_name_is_allowed_only_inside_compatibility_period(self) -> None:
        for app_version, expected_code in (
            ("1.2.3", None),
            ("2.0.0", "executable:legacy-expired"),
        ):
            with self.subTest(application_version=app_version):
                temporary, root = self._new_repo()
                self.addCleanup(temporary.cleanup)
                self._prepare_valid_repo(
                    root,
                    application_version=app_version,
                    compatibility_through="1.9.0",
                )

                report = check_version_contract.audit_repository(root)
                codes = {finding.code for finding in report.findings}

                if expected_code is None:
                    self.assertTrue(report.passed)
                else:
                    self.assertIn(expected_code, codes)

    def test_canonical_only_rejects_legacy_name_and_accepts_new_name(self) -> None:
        for executable_name, should_pass in (
            ("RoboMasterClient2025", False),
            ("RM26CustomClient", True),
        ):
            with self.subTest(executable_name=executable_name):
                temporary, root = self._new_repo()
                self.addCleanup(temporary.cleanup)
                self._prepare_valid_repo(
                    root,
                    executable_name=executable_name,
                    executable_strategy="canonical-only",
                )

                report = check_version_contract.audit_repository(root)

                self.assertEqual(should_pass, report.passed)
                if not should_pass:
                    self.assertIn(
                        "executable:legacy-forbidden",
                        {finding.code for finding in report.findings},
                    )

    def test_policy_rejects_protocol_application_coupling(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_sources(root)
        policy = self._policy()
        protocol = policy["protocol"]
        self.assertIsInstance(protocol, dict)
        protocol["strategy"] = "shared"
        self._write_policy(root, policy)
        self._track_all(root)

        with self.assertRaisesRegex(
            check_version_contract.VersionContractError,
            "协议版本不得复用应用版本策略",
        ):
            check_version_contract.audit_repository(root)

    def test_invalid_policy_path_is_not_echoed(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_sources(root)
        policy = self._policy()
        application = policy["application"]
        self.assertIsInstance(application, dict)
        sources = application["sources"]
        self.assertIsInstance(sources, dict)
        private_path = "/Users/example/private/CMakeLists.txt"
        sources["cmake_project"] = private_path
        self._write_policy(root, policy)
        self._track_all(root)

        with self.assertRaises(check_version_contract.VersionContractError) as context:
            check_version_contract.audit_repository(root)

        self.assertNotIn(private_path, str(context.exception))
        self.assertIn("application.sources.cmake_project", str(context.exception))


if __name__ == "__main__":
    unittest.main()
