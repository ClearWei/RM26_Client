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

import check_media_metadata


class MediaMetadataCheckTest(unittest.TestCase):
    def _new_repo(self) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary = tempfile.TemporaryDirectory()
        root = Path(temporary.name)
        subprocess.run(["git", "init", "--quiet"], cwd=root, check=True)
        return temporary, root

    def _write_bytes(self, root: Path, relative_path: str, content: bytes) -> None:
        path = root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)

    def _write_text(self, root: Path, relative_path: str, content: str) -> None:
        path = root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def _write_policy(self, root: Path, allowlist: list[dict[str, str]]) -> None:
        path = root / check_media_metadata.POLICY_PATH
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            json.dumps(
                {"schema_version": 1, "allowlist": allowlist},
                ensure_ascii=False,
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )

    def _track_all(self, root: Path) -> None:
        subprocess.run(["git", "add", "--all"], cwd=root, check=True)

    def _fake_ffprobe(self, root: Path) -> str:
        executable = root / "fake_ffprobe.py"
        executable.write_text(
            f"#!{sys.executable}\n"
            "import json\n"
            "import pathlib\n"
            "import sys\n"
            "\n"
            "content = pathlib.Path(sys.argv[-1]).read_bytes()\n"
            "if content == b'probe-error':\n"
            "    print('fixture probe failed', file=sys.stderr)\n"
            "    raise SystemExit(3)\n"
            "payload = {}\n"
            "if content == b'technical':\n"
            "    payload = {\n"
            "        'format': {'tags': {\n"
            "            'creation_time': '2026-08-22T00:00:00Z',\n"
            "            'encoder': 'Lavf62',\n"
            "            'com.android.version': '15',\n"
            "        }},\n"
            "        'streams': [{'tags': {'handler_name': 'Core Media Audio'}}],\n"
            "    }\n"
            "elif content == b'title':\n"
            "    payload = {'format': {'tags': {'title': '赛场提示音'}}}\n"
            "elif content == b'other-title':\n"
            "    payload = {'format': {'tags': {'title': '家庭录音'}}}\n"
            "elif content == b'private':\n"
            "    payload = {\n"
            "        'format': {'tags': {\n"
            "            'TITLE': '家庭录音',\n"
            "            'comment': '由个人设备导出',\n"
            "        }},\n"
            "        'streams': [{'tags': {\n"
            "            'com.apple.quicktime.location.ISO6709': '+31.0+121.0/',\n"
            "            'deviceModel': 'Personal Phone',\n"
            "        }}],\n"
            "        'chapters': [{'tags': {'artist': '个人姓名'}}],\n"
            "    }\n"
            "print(json.dumps(payload, ensure_ascii=False))\n",
            encoding="utf-8",
        )
        executable.chmod(0o755)
        return str(executable)

    def test_reads_media_from_index_instead_of_worktree(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_bytes(root, "resources/sounds/notice.mp3", b"technical")
        self._write_policy(root, [])
        self._track_all(root)
        self._write_bytes(root, "resources/sounds/notice.mp3", b"private")

        result = check_media_metadata.audit_repository(
            root, ffprobe=self._fake_ffprobe(root)
        )

        self.assertTrue(result.passed)
        self.assertEqual(1, result.media_count)

    def test_untracked_media_is_not_part_of_release_snapshot(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_policy(root, [])
        self._track_all(root)
        self._write_bytes(root, "resources/sounds/local.mp3", b"private")

        result = check_media_metadata.audit_repository(
            root, ffprobe=self._fake_ffprobe(root)
        )

        self.assertTrue(result.passed)
        self.assertEqual(0, result.media_count)

    def test_sensitive_format_and_stream_tags_are_blocked(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_bytes(root, "resources/sounds/private.m4a", b"private")
        self._write_policy(root, [])
        self._track_all(root)

        result = check_media_metadata.audit_repository(
            root, ffprobe=self._fake_ffprobe(root)
        )

        self.assertFalse(result.passed)
        self.assertEqual(
            {
                ("format", "TITLE"),
                ("format", "comment"),
                ("stream:0", "com.apple.quicktime.location.ISO6709"),
                ("stream:0", "deviceModel"),
                ("chapter:0", "artist"),
            },
            {(issue.scope, issue.field) for issue in result.issues},
        )
        self.assertEqual(
            {"metadata:not-approved"}, {issue.code for issue in result.issues}
        )

    def test_technical_container_tags_are_allowed(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_bytes(root, "resources/sounds/notice.mov", b"technical")
        self._write_policy(root, [])
        self._track_all(root)

        result = check_media_metadata.audit_repository(
            root, ffprobe=self._fake_ffprobe(root)
        )

        self.assertTrue(result.passed)

    def test_exact_allowlist_entry_with_evidence_is_accepted(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        media_path = "resources/sounds/notice.mp3"
        self._write_bytes(root, media_path, b"title")
        self._write_policy(
            root,
            [
                {
                    "path": media_path,
                    "scope": "format",
                    "field": "title",
                    "value": "赛场提示音",
                    "evidence": "https://example.invalid/review/42",
                }
            ],
        )
        self._track_all(root)

        result = check_media_metadata.audit_repository(
            root, ffprobe=self._fake_ffprobe(root)
        )

        self.assertTrue(result.passed)

    def test_allowlist_does_not_approve_a_changed_value(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        media_path = "resources/sounds/notice.mp3"
        self._write_bytes(root, media_path, b"other-title")
        self._write_policy(
            root,
            [
                {
                    "path": media_path,
                    "scope": "format",
                    "field": "title",
                    "value": "赛场提示音",
                    "evidence": "https://example.invalid/review/42",
                }
            ],
        )
        self._track_all(root)

        result = check_media_metadata.audit_repository(
            root, ffprobe=self._fake_ffprobe(root)
        )

        self.assertEqual(
            {"metadata:not-approved", "policy:stale-approval"},
            {issue.code for issue in result.issues},
        )

    def test_allowlist_is_also_read_from_git_index(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        media_path = "resources/sounds/notice.mp3"
        self._write_bytes(root, media_path, b"title")
        self._write_policy(root, [])
        self._track_all(root)
        self._write_policy(
            root,
            [
                {
                    "path": media_path,
                    "scope": "format",
                    "field": "title",
                    "value": "赛场提示音",
                    "evidence": "https://example.invalid/review/42",
                }
            ],
        )

        result = check_media_metadata.audit_repository(
            root, ffprobe=self._fake_ffprobe(root)
        )

        self.assertEqual(
            ["metadata:not-approved"], [issue.code for issue in result.issues]
        )

    def test_allowlist_entry_requires_evidence(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        media_path = "resources/sounds/notice.mp3"
        self._write_bytes(root, media_path, b"title")
        self._write_policy(
            root,
            [
                {
                    "path": media_path,
                    "scope": "format",
                    "field": "title",
                    "value": "赛场提示音",
                    "evidence": "",
                }
            ],
        )
        self._track_all(root)

        with self.assertRaisesRegex(
            check_media_metadata.MediaMetadataError, "evidence"
        ):
            check_media_metadata.audit_repository(
                root, ffprobe=self._fake_ffprobe(root)
            )

    def test_allowlist_evidence_can_reference_a_tracked_file(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        media_path = "resources/sounds/notice.mp3"
        self._write_bytes(root, media_path, b"title")
        self._write_text(root, "docs/reviews/notice.md", "已确认可公开。\n")
        self._write_policy(
            root,
            [
                {
                    "path": media_path,
                    "scope": "format",
                    "field": "title",
                    "value": "赛场提示音",
                    "evidence": "docs/reviews/notice.md",
                }
            ],
        )
        self._track_all(root)

        result = check_media_metadata.audit_repository(
            root, ffprobe=self._fake_ffprobe(root)
        )

        self.assertTrue(result.passed)

    def test_allowlist_rejects_unverifiable_evidence(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        media_path = "resources/sounds/notice.mp3"
        self._write_bytes(root, media_path, b"title")
        self._write_policy(
            root,
            [
                {
                    "path": media_path,
                    "scope": "format",
                    "field": "title",
                    "value": "赛场提示音",
                    "evidence": "review-42",
                }
            ],
        )
        self._track_all(root)

        with self.assertRaisesRegex(
            check_media_metadata.MediaMetadataError, "Git 索引.*HTTPS"
        ):
            check_media_metadata.audit_repository(
                root, ffprobe=self._fake_ffprobe(root)
            )

    def test_removed_tag_makes_allowlist_entry_stale(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        media_path = "resources/sounds/notice.mp3"
        self._write_bytes(root, media_path, b"technical")
        self._write_policy(
            root,
            [
                {
                    "path": media_path,
                    "scope": "format",
                    "field": "title",
                    "value": "赛场提示音",
                    "evidence": "https://example.invalid/review/42",
                }
            ],
        )
        self._track_all(root)

        result = check_media_metadata.audit_repository(
            root, ffprobe=self._fake_ffprobe(root)
        )

        self.assertEqual(
            ["policy:stale-approval"], [issue.code for issue in result.issues]
        )

    def test_probe_failure_blocks_release(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write_bytes(root, "resources/sounds/broken.wav", b"probe-error")
        self._write_policy(root, [])
        self._track_all(root)

        result = check_media_metadata.audit_repository(
            root, ffprobe=self._fake_ffprobe(root)
        )

        self.assertEqual(["probe:failed"], [issue.code for issue in result.issues])

    def test_report_does_not_echo_sensitive_value(self) -> None:
        result = check_media_metadata.AuditResult(
            1,
            (
                check_media_metadata.MetadataIssue(
                    "metadata:not-approved",
                    "resources/sounds/private.mp3",
                    "format",
                    "title",
                    value="家庭住址",
                ),
                check_media_metadata.MetadataIssue(
                    "policy:stale-approval",
                    "resources/sounds/private.mp3",
                    "format",
                    "comment",
                    value="个人设备备注",
                ),
            ),
        )

        rendered = check_media_metadata.render_result(result)

        self.assertIn("值摘要 sha256:", rendered)
        self.assertNotIn("家庭住址", rendered)
        self.assertNotIn("个人设备备注", rendered)


if __name__ == "__main__":
    unittest.main()
