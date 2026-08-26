from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


RELEASE_DIR = Path(__file__).resolve().parents[1]
if str(RELEASE_DIR) not in sys.path:
    sys.path.insert(0, str(RELEASE_DIR))

import check_qml_lint


class QmlLintBaselineCheckTest(unittest.TestCase):
    def _rules(self, max_count: int = 3) -> dict[str, check_qml_lint.DiagnosticRule]:
        return {
            "unqualified": check_qml_lint.DiagnosticRule("warning", max_count)
        }

    def _payload(self, diagnostics: list[dict[str, object]]) -> dict[str, object]:
        return {
            "revision": 3,
            "files": [
                {
                    "filename": "src/qml/Panel.qml",
                    "success": not diagnostics,
                    "warnings": diagnostics,
                }
            ],
        }

    def _diagnostic(
        self, diagnostic_id: str, diagnostic_type: str = "warning"
    ) -> dict[str, object]:
        return {
            "id": diagnostic_id,
            "type": diagnostic_type,
            "line": 1,
            "column": 1,
            "message": "test diagnostic",
        }

    def _qt_call_later_sources(
        self, line: str = "Qt.callLater(doWork)"
    ) -> dict[str, tuple[str, ...]]:
        return {"src/qml/Panel.qml": (line,)}

    def test_current_or_lower_allowed_count_passes(self) -> None:
        payload = self._payload([self._diagnostic("unqualified")] * 3)

        report = check_qml_lint.audit_payload(payload, self._rules())

        self.assertTrue(report.passed)
        self.assertEqual(3, report.counts[("unqualified", "warning")])

    def test_allowed_count_cannot_regress(self) -> None:
        payload = self._payload([self._diagnostic("unqualified")] * 4)

        report = check_qml_lint.audit_payload(payload, self._rules())

        self.assertFalse(report.passed)
        self.assertIn(
            "diagnostic:count-regression",
            {finding.code for finding in report.findings},
        )

    def test_new_diagnostic_category_is_rejected(self) -> None:
        payload = self._payload([self._diagnostic("missing-property")])

        report = check_qml_lint.audit_payload(payload, self._rules())

        self.assertFalse(report.passed)
        self.assertIn("missing-property", check_qml_lint.render_report(report))

    def test_legacy_diagnostic_keeps_a_short_message_sample(self) -> None:
        diagnostic = self._diagnostic("")
        diagnostic.pop("id")
        diagnostic["message"] = "  Older Qt diagnostic\nwith   extra spaces  "

        report = check_qml_lint.audit_payload(self._payload([diagnostic]), self._rules())

        rendered = check_qml_lint.render_report(report)
        self.assertFalse(report.passed)
        self.assertIn("unknown", rendered)
        self.assertIn("Older Qt diagnostic with extra spaces", rendered)

    def test_qt64_call_later_false_positive_is_ignored_explicitly(self) -> None:
        diagnostic = self._diagnostic("")
        diagnostic.pop("id")
        diagnostic["message"] = check_qml_lint.QT64_CALL_LATER_FALSE_POSITIVE

        report = check_qml_lint.audit_payload(
            self._payload([diagnostic]),
            self._rules(),
            source_lines=self._qt_call_later_sources(),
        )

        self.assertTrue(report.passed)
        self.assertEqual(1, report.compatibility_ignores)
        self.assertIn("已知误报 1 条", check_qml_lint.render_report(report))

    def test_qt64_call_later_compatibility_count_cannot_regress(self) -> None:
        diagnostic = self._diagnostic("")
        diagnostic.pop("id")
        diagnostic["message"] = check_qml_lint.QT64_CALL_LATER_FALSE_POSITIVE
        diagnostics = [
            diagnostic.copy()
            for _ in range(check_qml_lint.QT64_CALL_LATER_MAX_IGNORES + 1)
        ]

        report = check_qml_lint.audit_payload(
            self._payload(diagnostics),
            self._rules(),
            source_lines=self._qt_call_later_sources(),
        )

        self.assertFalse(report.passed)
        self.assertIn(
            "diagnostic:compatibility-regression",
            {finding.code for finding in report.findings},
        )

    def test_qt64_call_later_current_compatibility_baseline_passes(self) -> None:
        diagnostic = self._diagnostic("")
        diagnostic.pop("id")
        diagnostic["message"] = check_qml_lint.QT64_CALL_LATER_FALSE_POSITIVE
        diagnostics = [
            diagnostic.copy()
            for _ in range(check_qml_lint.QT64_CALL_LATER_MAX_IGNORES)
        ]

        report = check_qml_lint.audit_payload(
            self._payload(diagnostics),
            self._rules(),
            source_lines=self._qt_call_later_sources(),
        )

        self.assertTrue(report.passed)
        self.assertEqual(3, report.compatibility_ignores)

    def test_same_legacy_message_on_non_qt_receiver_is_rejected(self) -> None:
        diagnostic = self._diagnostic("")
        diagnostic.pop("id")
        diagnostic["message"] = check_qml_lint.QT64_CALL_LATER_FALSE_POSITIVE

        for source_lines in (None, self._qt_call_later_sources("obj.callLater()")):
            with self.subTest(source_lines=source_lines):
                report = check_qml_lint.audit_payload(
                    self._payload([diagnostic]),
                    self._rules(),
                    source_lines=source_lines,
                )
                self.assertFalse(report.passed)
                self.assertEqual(0, report.compatibility_ignores)

    def test_modern_call_later_diagnostic_is_still_enforced(self) -> None:
        diagnostic = self._diagnostic("use-proper-function")
        diagnostic["message"] = check_qml_lint.QT64_CALL_LATER_FALSE_POSITIVE

        report = check_qml_lint.audit_payload(self._payload([diagnostic]), self._rules())

        self.assertFalse(report.passed)
        self.assertEqual(0, report.compatibility_ignores)
        self.assertIn("use-proper-function", check_qml_lint.render_report(report))

    def test_diagnostic_type_drift_is_rejected(self) -> None:
        payload = self._payload([self._diagnostic("unqualified", "info")])

        report = check_qml_lint.audit_payload(payload, self._rules())

        self.assertFalse(report.passed)
        self.assertIn(
            "diagnostic:type-drift", {finding.code for finding in report.findings}
        )

    def test_empty_diagnostic_set_passes(self) -> None:
        report = check_qml_lint.audit_payload(self._payload([]), self._rules())

        self.assertTrue(report.passed)
        self.assertEqual({}, report.counts)

    def test_policy_rejects_absolute_source_path(self) -> None:
        with self.assertRaises(check_qml_lint.QmlLintCheckError):
            check_qml_lint.parse_policy(
                {
                    "schema_version": 1,
                    "qml_root": "/private/qml",
                    "allowed_diagnostics": {},
                }
            )

    def test_policy_rejects_boolean_count(self) -> None:
        with self.assertRaises(check_qml_lint.QmlLintCheckError):
            check_qml_lint.parse_policy(
                {
                    "schema_version": 1,
                    "qml_root": "src/qml",
                    "allowed_diagnostics": {
                        "unqualified": {"type": "warning", "max_count": True}
                    },
                }
            )

    def test_repository_reports_missing_qmllint(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "src/qml").mkdir(parents=True)
            (root / "src/qml/Test.qml").write_text(
                "import QtQuick\nItem {}\n", encoding="utf-8"
            )
            policy = {
                "schema_version": 1,
                "qml_root": "src/qml",
                "allowed_diagnostics": {},
            }
            policy_path = root / "policy.json"
            policy_path.write_text(json.dumps(policy), encoding="utf-8")

            with self.assertRaisesRegex(
                check_qml_lint.QmlLintCheckError, "无法启动 qmllint"
            ):
                check_qml_lint.audit_repository(
                    root, policy_path, qmllint=str(root / "missing-qmllint")
                )

    def test_qtpaths_fallback_finds_qmllint(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            qt_bin = Path(temporary)
            qmllint = qt_bin / "qmllint"
            qmllint.touch()
            completed = mock.Mock(returncode=0, stdout=f"{qt_bin}\n", stderr="")

            with mock.patch.object(
                check_qml_lint.shutil,
                "which",
                side_effect=lambda name: "/usr/bin/qtpaths6"
                if name == "qtpaths6"
                else None,
            ), mock.patch.object(
                check_qml_lint.subprocess, "run", return_value=completed
            ):
                self.assertEqual(str(qmllint), check_qml_lint.find_qmllint())

    def test_qmllint_command_is_compatible_with_qt_6_4(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            qml_file = root / "src/qml/Test.qml"
            qml_file.parent.mkdir(parents=True)
            qml_file.write_text("import QtQuick\nItem {}\n", encoding="utf-8")
            captured: list[str] = []

            def fake_run(command: list[str], **_kwargs: object) -> mock.Mock:
                captured.extend(command)
                if command[1] == "--help":
                    return mock.Mock(
                        returncode=0,
                        stdout="--deferred-property-id <level>",
                        stderr="",
                    )
                report_path = Path(command[2])
                report_path.write_text(
                    json.dumps(
                        {
                            "revision": 3,
                            "files": [
                                {
                                    "filename": "src/qml/Test.qml",
                                    "success": True,
                                    "warnings": [],
                                }
                            ],
                        }
                    ),
                    encoding="utf-8",
                )
                return mock.Mock(returncode=0, stdout="", stderr="")

            with mock.patch.object(
                check_qml_lint.subprocess, "run", side_effect=fake_run
            ):
                payload = check_qml_lint.run_qmllint(
                    root, [qml_file], qmllint="/usr/lib/qt6/bin/qmllint"
                )

            self.assertEqual(1, len(payload["files"]))
            self.assertNotIn("-W", captured)
            self.assertNotIn("--max-warnings", captured)
            option_index = captured.index("--deferred-property-id")
            self.assertEqual("disable", captured[option_index + 1])

    def test_newer_qmllint_does_not_receive_removed_compatibility_option(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            qml_file = root / "src/qml/Test.qml"
            qml_file.parent.mkdir(parents=True)
            qml_file.write_text("import QtQuick\nItem {}\n", encoding="utf-8")
            commands: list[list[str]] = []

            def fake_run(command: list[str], **_kwargs: object) -> mock.Mock:
                commands.append(command)
                if command[1] == "--help":
                    return mock.Mock(returncode=0, stdout="qmllint help", stderr="")
                Path(command[2]).write_text(
                    json.dumps(
                        {
                            "revision": 3,
                            "files": [
                                {
                                    "filename": "src/qml/Test.qml",
                                    "success": True,
                                    "warnings": [],
                                }
                            ],
                        }
                    ),
                    encoding="utf-8",
                )
                return mock.Mock(returncode=0, stdout="", stderr="")

            with mock.patch.object(
                check_qml_lint.subprocess, "run", side_effect=fake_run
            ):
                check_qml_lint.run_qmllint(
                    root, [qml_file], qmllint="/usr/local/bin/qmllint"
                )

            self.assertEqual(2, len(commands))
            self.assertNotIn("--deferred-property-id", commands[1])

    def test_malformed_qmllint_payload_is_rejected(self) -> None:
        with self.assertRaises(check_qml_lint.QmlLintCheckError):
            check_qml_lint.audit_payload({"files": [{}]}, self._rules())

    def test_incomplete_scan_is_rejected(self) -> None:
        with self.assertRaisesRegex(
            check_qml_lint.QmlLintCheckError, "扫描结果不完整"
        ):
            check_qml_lint.audit_payload(
                self._payload([]), self._rules(), expected_file_count=2
            )


if __name__ == "__main__":
    unittest.main()
