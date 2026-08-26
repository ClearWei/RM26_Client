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

import check_runtime_resources


class RuntimeResourceCheckTest(unittest.TestCase):
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

    def _policy(
        self,
        *,
        templates: list[dict[str, object]] | None = None,
        dynamic_resources: list[dict[str, object]] | None = None,
        optional_paths: list[str] | None = None,
        optional_evidence: str = "",
    ) -> dict[str, object]:
        return {
            "schema_version": 2,
            "cpp_roots": ["src"],
            "sounds": {
                "root": "resources/sounds",
                "extensions": [".m4a", ".mov", ".mp3", ".wav"],
                "qrc_manifest": "resources.qrc",
                "dynamic_templates": templates or [],
                "optional": {
                    "paths": optional_paths or [],
                    "evidence": optional_evidence,
                },
            },
            "dynamic_resources": dynamic_resources or [],
        }

    def _write_policy(self, root: Path, policy: dict[str, object]) -> None:
        self._write(
            root,
            check_runtime_resources.POLICY_PATH,
            json.dumps(policy, ensure_ascii=False, indent=2) + "\n",
        )

    def _write_empty_qrc(self, root: Path) -> None:
        self._write(root, "resources.qrc", "<RCC/>\n")

    def _result_contract(self) -> dict[str, object]:
        return {
            "id": "game-result-animations",
            "source": "src/widgets/GameResultWidget.cpp",
            "source_root": "resources/images/resultpanel",
            "variants": [
                "blue_win_zh",
                "red_win_zh",
                "abnormal_termination_zh",
            ],
            "extensions": [".png"],
            "required_delivery": ["qrc", "install"],
            "qrc_manifest": "resources.qrc",
            "qrc_prefix": "/images/resultpanel",
            "install_manifest": "CMakeLists.txt",
        }

    def _write_result_sources(self, root: Path) -> None:
        self._write(
            root,
            "src/widgets/GameResultWidget.cpp",
            'auto root = ":/images/resultpanel/";\n'
            'auto fallback = "/resources/images/resultpanel/";\n'
            'auto blue = "blue_win_zh";\n'
            'auto red = "red_win_zh";\n'
            'auto terminated = "abnormal_termination_zh";\n',
        )
        for variant in (
            "blue_win_zh",
            "red_win_zh",
            "abnormal_termination_zh",
        ):
            self._write(
                root,
                f"resources/images/resultpanel/{variant}/frame.png",
                "fixture\n",
            )

    def _write_result_qrc(self, root: Path, *, correct_aliases: bool) -> None:
        entries = []
        for variant in (
            "blue_win_zh",
            "red_win_zh",
            "abnormal_termination_zh",
        ):
            source = f"resources/images/resultpanel/{variant}/frame.png"
            alias = f'{variant}/frame.png' if correct_aliases else source
            entries.append(f'    <file alias="{alias}">{source}</file>')
        self._write(
            root,
            "resources.qrc",
            '<RCC>\n  <qresource prefix="/images/resultpanel">\n'
            + "\n".join(entries)
            + "\n  </qresource>\n</RCC>\n",
        )

    def test_reports_the_twelve_missing_sound_contracts(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(
            root,
            self._policy(
                templates=[
                    {
                        "source": "src/widgets/BattleMessageWidget.cpp",
                        "literal": "qrc:/sounds/%1kill.wav",
                        "values": ["2", "3", "4", "5"],
                    }
                ]
            ),
        )
        self._write_empty_qrc(root)
        self._write(
            root,
            "src/widgets/BattleMessageWidget.cpp",
            'load("qrc:/sounds/dead.wav");\n'
            'load("qrc:/sounds/firstblood.wav");\n'
            'load(QString("qrc:/sounds/%1kill.wav"));\n',
        )
        self._write(
            root,
            "src/ui/MainWindow.cpp",
            'setStyleSheet(R"(QLabel { image: url(\"decorative.png\"); })");\n'
            '// play("resources/sounds/commented-out.mp3");\n'
            'play("resources/sounds/20game_finish.mp3");\n'
            'play("resources/sounds/2自检.mp3");\n'
            'play("resources/sounds/3-1比赛开始.mp3");\n'
            'play("resources/sounds/gameBg.mp3");\n'
            'play("resources/sounds/min3bgm.mp3");\n'
            'if (current.contains("min3.mp3")) {}\n',
        )
        self._write(
            root,
            "src/widgets/GameResultWidget.cpp",
            'load("qrc:/sounds/game_finish.mp3");\n',
        )
        self._track_all(root)

        report = check_runtime_resources.audit_repository(root)

        self.assertEqual(12, len(report.missing_sound_paths))
        self.assertNotIn("resources/sounds/min3.mp3", report.missing_sound_paths)
        self.assertIn("resources/sounds/5kill.wav", report.missing_sound_paths)
        self.assertIn("sound:missing-files", {item.code for item in report.findings})

    def test_reads_sources_and_assets_from_index(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(root, self._policy())
        self._write_empty_qrc(root)
        self._write(
            root,
            "src/audio.cpp",
            'playSecondarySound("resources/sounds/ready.mp3");\n',
        )
        self._write(root, "resources/sounds/ready.mp3", "audio\n")
        self._track_all(root)

        self._write(
            root,
            "src/audio.cpp",
            'playSecondarySound("resources/sounds/local-only.mp3");\n',
        )
        (root / "resources/sounds/ready.mp3").unlink()

        report = check_runtime_resources.audit_repository(root)

        self.assertTrue(report.passed)
        self.assertEqual(1, report.referenced_sound_count)

    def test_untracked_cpp_and_sound_do_not_change_the_snapshot(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(root, self._policy())
        self._write_empty_qrc(root)
        self._track_all(root)
        self._write(
            root,
            "src/local_only.cpp",
            'playSecondarySound("resources/sounds/local-only.mp3");\n',
        )
        self._write(root, "resources/sounds/local-only.mp3", "local\n")

        report = check_runtime_resources.audit_repository(root)

        self.assertTrue(report.passed)
        self.assertEqual(0, report.referenced_sound_count)

    def test_qrc_sound_requires_the_exact_alias_and_source(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(root, self._policy())
        self._write(
            root,
            "src/audio.cpp",
            'tryLoadSound(sound, "qrc:/sounds/tone.wav");\n',
        )
        self._write(root, "resources/sounds/tone.wav", "audio\n")
        self._write(
            root,
            "resources.qrc",
            '<RCC><qresource prefix="/sounds">'
            '<file alias="tone.wav">resources/sounds/tone.wav</file>'
            "</qresource></RCC>\n",
        )
        self._track_all(root)

        report = check_runtime_resources.audit_repository(root)

        self.assertTrue(report.passed)

        self._write(
            root,
            "resources.qrc",
            '<RCC><qresource prefix="/sounds">'
            "<file>resources/sounds/tone.wav</file>"
            "</qresource></RCC>\n",
        )
        self._track_all(root)
        report = check_runtime_resources.audit_repository(root)

        self.assertIn("sound:qrc-aliases", {item.code for item in report.findings})

    def test_optional_missing_sound_is_reported_without_blocking(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(
            root,
            self._policy(
                optional_paths=["resources/sounds/ready.mp3"],
                optional_evidence="docs/optional-audio.md",
            ),
        )
        self._write_empty_qrc(root)
        self._write(
            root,
            "src/audio.cpp",
            'playSecondarySound("resources/sounds/ready.mp3");\n',
        )
        self._write(root, "docs/optional-audio.md", "# 可选音效\n")
        self._track_all(root)

        report = check_runtime_resources.audit_repository(root)
        rendered = check_runtime_resources.render_report(report)

        self.assertTrue(report.passed)
        self.assertEqual((), report.missing_sound_paths)
        self.assertEqual(
            ("resources/sounds/ready.mp3",),
            report.optional_missing_sound_paths,
        )
        self.assertIn("未随源码提供的可选音效：1", rendered)

    def test_optional_sound_contract_requires_tracked_evidence(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(
            root,
            self._policy(
                optional_paths=["resources/sounds/ready.mp3"],
                optional_evidence="docs/missing.md",
            ),
        )
        self._write_empty_qrc(root)
        self._write(
            root,
            "src/audio.cpp",
            'playSecondarySound("resources/sounds/ready.mp3");\n',
        )
        self._track_all(root)

        with self.assertRaises(check_runtime_resources.RuntimeResourceError):
            check_runtime_resources.audit_repository(root)

    def test_stale_optional_sound_contract_is_a_blocker(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(
            root,
            self._policy(
                optional_paths=["resources/sounds/ready.mp3"],
                optional_evidence="docs/optional-audio.md",
            ),
        )
        self._write_empty_qrc(root)
        self._write(root, "docs/optional-audio.md", "# 可选音效\n")
        self._track_all(root)

        report = check_runtime_resources.audit_repository(root)

        self.assertIn(
            "sound:stale-optional-contract",
            {item.code for item in report.findings},
        )

    def test_present_optional_qrc_sound_still_requires_alias(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(
            root,
            self._policy(
                optional_paths=["resources/sounds/tone.wav"],
                optional_evidence="docs/optional-audio.md",
            ),
        )
        self._write_empty_qrc(root)
        self._write(
            root,
            "src/audio.cpp",
            'tryLoadSound(sound, "qrc:/sounds/tone.wav");\n',
        )
        self._write(root, "resources/sounds/tone.wav", "audio\n")
        self._write(root, "docs/optional-audio.md", "# 可选音效\n")
        self._track_all(root)

        report = check_runtime_resources.audit_repository(root)

        self.assertIn("sound:qrc-aliases", {item.code for item in report.findings})

    def test_complete_dynamic_qrc_and_install_contract_passes(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(
            root, self._policy(dynamic_resources=[self._result_contract()])
        )
        self._write_result_sources(root)
        self._write_result_qrc(root, correct_aliases=True)
        self._write(
            root,
            "CMakeLists.txt",
            "install(DIRECTORY resources/images/resultpanel/ "
            "DESTINATION share/client/resources/images)\n",
        )
        self._track_all(root)

        report = check_runtime_resources.audit_repository(root)

        self.assertTrue(report.passed)

    def test_dynamic_source_fallback_does_not_count_as_delivery(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(
            root, self._policy(dynamic_resources=[self._result_contract()])
        )
        self._write_result_sources(root)
        self._write_result_qrc(root, correct_aliases=False)
        self._write(
            root,
            "CMakeLists.txt",
            '# 开发时从 resources/images/resultpanel 直接读取\n'
            "add_executable(Client main.cpp)\n",
        )
        self._track_all(root)

        report = check_runtime_resources.audit_repository(root)
        codes = {item.code for item in report.findings}
        rendered = check_runtime_resources.render_report(report)

        self.assertIn("dynamic:game-result-animations:qrc-delivery", codes)
        self.assertIn("dynamic:game-result-animations:install-delivery", codes)
        self.assertIn("源码目录和当前工作目录兜底不能证明", rendered)

    def test_dynamic_source_contract_ignores_variant_names_in_comments(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(
            root, self._policy(dynamic_resources=[self._result_contract()])
        )
        self._write_result_sources(root)
        self._write(
            root,
            "src/widgets/GameResultWidget.cpp",
            "// /resources/images/resultpanel/ :/images/resultpanel/\n"
            "// blue_win_zh red_win_zh abnormal_termination_zh\n"
            "int unrelated = 0;\n",
        )
        self._write_result_qrc(root, correct_aliases=True)
        self._write(
            root,
            "CMakeLists.txt",
            "install(DIRECTORY resources/images/resultpanel/ DESTINATION share/client)\n",
        )
        self._track_all(root)

        report = check_runtime_resources.audit_repository(root)

        self.assertIn(
            "dynamic:game-result-animations:source-contract",
            {item.code for item in report.findings},
        )

    def test_dynamic_source_contract_requires_real_root_literals(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(
            root, self._policy(dynamic_resources=[self._result_contract()])
        )
        self._write_result_sources(root)
        self._write(
            root,
            "src/widgets/GameResultWidget.cpp",
            'auto blue = "blue_win_zh";\n'
            'auto red = "red_win_zh";\n'
            'auto stopped = "abnormal_termination_zh";\n'
            "// /resources/images/resultpanel/ :/images/resultpanel/\n",
        )
        self._write_result_qrc(root, correct_aliases=True)
        self._write(
            root,
            "CMakeLists.txt",
            "install(DIRECTORY resources/images/resultpanel/ DESTINATION share/client)\n",
        )
        self._track_all(root)

        report = check_runtime_resources.audit_repository(root)

        self.assertIn(
            "dynamic:game-result-animations:source-contract",
            {item.code for item in report.findings},
        )

    def test_untracked_delivery_rules_do_not_satisfy_contract(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(
            root, self._policy(dynamic_resources=[self._result_contract()])
        )
        self._write_result_sources(root)
        self._write_result_qrc(root, correct_aliases=False)
        self._write(root, "CMakeLists.txt", "add_executable(Client main.cpp)\n")
        self._track_all(root)

        self._write_result_qrc(root, correct_aliases=True)
        self._write(
            root,
            "CMakeLists.txt",
            "install(DIRECTORY resources/images/resultpanel/ DESTINATION share/client)\n",
        )

        report = check_runtime_resources.audit_repository(root)
        codes = {item.code for item in report.findings}

        self.assertIn("dynamic:game-result-animations:qrc-delivery", codes)
        self.assertIn("dynamic:game-result-animations:install-delivery", codes)

    def test_invalid_private_policy_path_is_not_echoed(self) -> None:
        for private_path in (
            "/Users/example/private/src",
            "~/private/src",
            "C:/private/src",
            "D:\\private\\src",
        ):
            with self.subTest(private_path=private_path):
                temporary, root = self._new_repo()
                self.addCleanup(temporary.cleanup)
                policy = self._policy()
                policy["cpp_roots"] = [private_path]
                self._write_policy(root, policy)
                self._write_empty_qrc(root)
                self._track_all(root)

                with self.assertRaises(
                    check_runtime_resources.RuntimeResourceError
                ) as context:
                    check_runtime_resources.audit_repository(root)

                self.assertNotIn(private_path, str(context.exception))


if __name__ == "__main__":
    unittest.main()
