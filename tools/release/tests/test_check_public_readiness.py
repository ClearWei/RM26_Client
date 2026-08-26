from __future__ import annotations

import contextlib
import io
import json
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path


RELEASE_DIR = Path(__file__).resolve().parents[1]
PROJECT_ROOT = RELEASE_DIR.parents[1]
if str(RELEASE_DIR) not in sys.path:
    sys.path.insert(0, str(RELEASE_DIR))

import check_public_readiness


def _png_chunk(chunk_type: bytes, data: bytes) -> bytes:
    checksum = zlib.crc32(chunk_type + data) & 0xFFFFFFFF
    return (
        struct.pack(">I", len(data))
        + chunk_type
        + data
        + struct.pack(">I", checksum)
    )


VALID_PNG = (
    b"\x89PNG\r\n\x1a\n"
    + _png_chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0))
    + _png_chunk(b"IDAT", zlib.compress(b"\x00\x00\x00\x00\x00"))
    + _png_chunk(b"IEND", b"")
)

VALID_PNG_ALT = (
    b"\x89PNG\r\n\x1a\n"
    + _png_chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0))
    + _png_chunk(b"IDAT", zlib.compress(b"\x00\xff\x00\x00\xff"))
    + _png_chunk(b"IEND", b"")
)


class PublicReadinessTest(unittest.TestCase):
    def _new_repo(self) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary = tempfile.TemporaryDirectory()
        root = Path(temporary.name)
        subprocess.run(["git", "init", "--quiet"], cwd=root, check=True)
        return temporary, root

    def _write(self, root: Path, relative_path: str, content: str) -> None:
        path = root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def _write_bytes(self, root: Path, relative_path: str, content: bytes) -> None:
        path = root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)

    def _track_all(self, root: Path) -> None:
        subprocess.run(["git", "add", "--all"], cwd=root, check=True)

    def _write_repository_example(self, root: Path) -> None:
        self._write(
            root,
            "config.example.json",
            (PROJECT_ROOT / "config.example.json").read_text(encoding="utf-8"),
        )

    def _write_path_policy(
        self,
        root: Path,
        *,
        directories: tuple[str, ...] = ("docs", "resources", "tools"),
        files: tuple[str, ...] = ("README.md",),
    ) -> None:
        self._write(
            root,
            "tools/release/public_paths.json",
            json.dumps(
                {
                    "schema_version": 1,
                    "directories": list(directories),
                    "files": list(files),
                },
                ensure_ascii=False,
                indent=2,
            )
            + "\n",
        )

    def _write_asset_policy(
        self,
        root: Path,
        *,
        status: str,
        evidence: str = "",
        path: str = "resources/images",
    ) -> None:
        group_root = root / path
        if group_root.is_file():
            files = [group_root]
        elif group_root.is_dir():
            files = [
                candidate
                for candidate in group_root.rglob("*")
                if candidate.is_file()
            ]
        else:
            files = []
        entries: list[check_public_readiness.GitIndexEntry] = []
        for candidate in files:
            relative_path = candidate.relative_to(root).as_posix()
            completed = subprocess.run(
                ["git", "hash-object", "--", relative_path],
                cwd=root,
                check=True,
                stdout=subprocess.PIPE,
                text=True,
            )
            entries.append(
                check_public_readiness.GitIndexEntry(
                    path=relative_path,
                    blob_oid=completed.stdout.strip(),
                    stage=0,
                )
            )
        file_count, digest = check_public_readiness._asset_snapshot(entries)
        self._write(
            root,
            "tools/release/public_assets.json",
            json.dumps(
                {
                    "schema_version": 2,
                    "snapshot_algorithm": (
                        check_public_readiness.ASSET_SNAPSHOT_ALGORITHM
                    ),
                    "groups": [
                        {
                            "path": path,
                            "status": status,
                            "evidence": evidence,
                            "snapshot": {
                                "file_count": file_count,
                                "sha256": digest,
                            },
                        }
                    ],
                },
                ensure_ascii=False,
                indent=2,
            )
            + "\n",
        )

    def test_public_docs_cannot_use_fixed_competition_addresses(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write(root, "README.md", "fixture\n")
        self._write(
            root,
            "docs/architecture/protocol-network.md",
            "gateway=192.168.12.1 client=192.168.12.2\n",
        )
        self._write_path_policy(root)
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)

        self.assertIn(
            "content:field-address", {item.code for item in report.findings}
        )

    def test_executable_default_cannot_use_competition_address(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write(root, "README.md", "fixture\n")
        self._write(
            root,
            "run.sh",
            'SERVER_IP="${SERVER_IP:-192.168.12.1}"\n',
        )
        self._write_path_policy(root, files=("README.md", "run.sh"))
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)

        self.assertIn("content:field-address", {item.code for item in report.findings})

    def test_control_api_cannot_bind_all_interfaces_by_default(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write(root, "README.md", "fixture\n")
        all_interfaces = "0.0.0." + "0"
        self._write(
            root,
            "service.py",
            f'app.run(host="{all_interfaces}", port=8000)\n',
        )
        self._write_path_policy(root, files=("README.md", "service.py"))
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)

        self.assertIn(
            "content:public-api-bind", {item.code for item in report.findings}
        )

    def test_tracked_runtime_config_requires_public_example(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write(root, "README.md", "fixture\n")
        self._write(root, "config.json", "{}\n")
        self._write_path_policy(
            root,
            files=("README.md", "config.json", "config.example.json"),
        )
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)

        self.assertIn("config:example-missing", {item.code for item in report.findings})

    def test_public_example_is_read_from_git_index(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write(root, "README.md", "fixture\n")
        self._write(root, "config.json", "{}\n")
        self._write_repository_example(root)
        self._write_path_policy(
            root,
            files=("README.md", "config.json", "config.example.json"),
        )
        self._track_all(root)
        self._write(root, "config.example.json", '{"network": {}}\n')

        report = check_public_readiness.audit_repository(root)

        self.assertNotIn(
            "config:example-invalid", {item.code for item in report.findings}
        )

    def test_runtime_config_with_field_address_is_blocked(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write(root, "README.md", "fixture\n")
        self._write_repository_example(root)
        payload = json.loads(
            (PROJECT_ROOT / "config.example.json").read_text(encoding="utf-8")
        )
        payload["network"]["server_ip"] = "192.168.12.1"
        payload["network"]["mqtt_broker"] = "192.168.12.1"
        self._write(
            root,
            "config.json",
            json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
        )
        self._write_path_policy(
            root,
            files=("README.md", "config.json", "config.example.json"),
        )
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)

        self.assertIn("config:runtime-not-public", {item.code for item in report.findings})

    def test_missing_license_forbidden_paths_and_private_metadata_fail(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        private_address = "192.168." + "1.56"
        personal_home = "/" + "Users" + "/" + "maintainer" + "/Code/client"
        self._write(root, ".marscode/deviceInfo.json", "{}\n")
        self._write(root, "docs/source/manual.pdf", "fixture\n")
        self._write(root, "docs/needs/internal.md", "temporary notes\n")
        self._write(root, "docs/assets/robomaster-player-manual/page-001.jpg", "fixture\n")
        self._write(root, "docs/robomaster-competition-rules.md", "OCR fixture\n")
        self._write(
            root,
            "tools/field_notes.md",
            f"workspace={personal_home}\nssh Administrator@{private_address}\n",
        )
        self._write_path_policy(root)
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)
        codes = {finding.code for finding in report.findings}

        self.assertFalse(report.passed)
        self.assertTrue(
            {
                "license:missing",
                "path:marscode",
                "path:official-source",
                "path:internal-needs",
                "path:official-assets",
                "path:official-ocr",
                "content:personal-path",
                "content:field-address",
                "content:ssh-metadata",
            }.issubset(codes)
        )

    def test_agent_records_logs_and_internal_plans_are_not_publishable(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write(root, ".codex/config.toml", "model = 'fixture'\n")
        self._write(root, ".github/copilot-instructions.md", "fixture\n")
        self._write(root, "docs/plans/internal-plan.md", "fixture\n")
        self._write(root, "output/runtime.log", "fixture\n")
        self._write_path_policy(
            root,
            directories=(".codex", ".github", "docs", "output", "tools"),
        )
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)
        codes = {finding.code for finding in report.findings}

        self.assertTrue(
            {
                "path:agent-metadata",
                "path:ai-instructions",
                "path:internal-process-doc",
                "path:runtime-evidence",
            }.issubset(codes)
        )

    def test_untracked_files_are_outside_the_release_snapshot(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write_path_policy(root)
        self._track_all(root)

        private_address = "10." + "23.4.5"
        self._write(root, "docs/source/untracked.pdf", "fixture\n")
        self._write(root, "notes.txt", f"ssh root@{private_address}\n")

        report = check_public_readiness.audit_repository(root)

        self.assertTrue(report.passed)
        self.assertEqual(2, report.tracked_file_count)

    def test_audit_reads_the_index_instead_of_unstaged_content(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write(root, "README.md", "safe\n")
        self._write_path_policy(root)
        self._track_all(root)

        private_address = "10." + "23.4.5"
        self._write(root, "README.md", f"ssh root@{private_address}\n")

        report = check_public_readiness.audit_repository(root)

        self.assertTrue(report.passed)

    def test_staged_private_metadata_is_not_hidden_by_safe_worktree_content(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        private_address = "10." + "23.4.5"
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write(root, "README.md", f"ssh root@{private_address}\n")
        self._write_path_policy(root)
        self._track_all(root)
        self._write(root, "README.md", "safe\n")

        report = check_public_readiness.audit_repository(root)

        self.assertFalse(report.passed)
        self.assertIn("content:field-address", {item.code for item in report.findings})

    def test_pending_asset_group_blocks_release(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write_bytes(root, "resources/images/map.png", VALID_PNG)
        self._write_asset_policy(root, status="pending")
        self._write_path_policy(root)
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)

        self.assertFalse(report.passed)
        self.assertIn("asset:not-approved", {item.code for item in report.findings})

    def test_asset_snapshot_algorithm_has_stable_known_vector(self) -> None:
        entries = (
            check_public_readiness.GitIndexEntry("b.png", "b" * 40, 0),
            check_public_readiness.GitIndexEntry("a.png", "a" * 40, 0),
        )

        file_count, digest = check_public_readiness._asset_snapshot(entries)

        self.assertEqual(2, file_count)
        self.assertEqual(
            "e0fb26cc652d23b1be7c6bf37bafc72b5f65f62deabbfdb4ca0384f8de6d95f2",
            digest,
        )

    def test_approved_asset_group_requires_and_accepts_evidence(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write_bytes(root, "resources/images/map.png", VALID_PNG)
        self._write_asset_policy(root, status="approved", evidence="docs/license-record.md")
        self._write_path_policy(root)
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)

        self.assertTrue(report.passed)

    def test_asset_snapshot_detects_staged_addition(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write_bytes(root, "resources/images/map.png", VALID_PNG)
        self._write_asset_policy(root, status="approved", evidence="docs/license.md")
        self._write_path_policy(root)
        self._track_all(root)
        self._write_bytes(root, "resources/images/extra.png", VALID_PNG_ALT)
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)

        self.assertIn("asset:snapshot-mismatch", {item.code for item in report.findings})

    def test_asset_snapshot_detects_staged_deletion(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write_bytes(root, "resources/images/map.png", VALID_PNG)
        self._write_bytes(root, "resources/images/extra.png", VALID_PNG_ALT)
        self._write_asset_policy(root, status="approved", evidence="docs/license.md")
        self._write_path_policy(root)
        self._track_all(root)
        subprocess.run(
            [
                "git",
                "rm",
                "--cached",
                "--force",
                "--quiet",
                "resources/images/extra.png",
            ],
            cwd=root,
            check=True,
        )

        report = check_public_readiness.audit_repository(root)

        self.assertIn("asset:snapshot-mismatch", {item.code for item in report.findings})

    def test_staged_asset_replacement_cannot_be_hidden_by_worktree(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write_bytes(root, "resources/images/map.png", VALID_PNG)
        self._write_asset_policy(root, status="approved", evidence="docs/license.md")
        self._write_path_policy(root)
        self._track_all(root)
        self._write_bytes(root, "resources/images/map.png", VALID_PNG_ALT)
        self._track_all(root)
        self._write_bytes(root, "resources/images/map.png", VALID_PNG)

        report = check_public_readiness.audit_repository(root)

        self.assertIn("asset:snapshot-mismatch", {item.code for item in report.findings})

    def test_asset_snapshot_ignores_unstaged_worktree_replacement(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write_bytes(root, "resources/images/map.png", VALID_PNG)
        self._write_asset_policy(root, status="approved", evidence="docs/license.md")
        self._write_path_policy(root)
        self._track_all(root)
        self._write_bytes(root, "resources/images/map.png", VALID_PNG_ALT)

        report = check_public_readiness.audit_repository(root)

        self.assertTrue(report.passed)

    def test_empty_asset_group_is_blocked(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write_asset_policy(
            root,
            status="approved",
            evidence="docs/license.md",
            path="resources/removed",
        )
        self._write_path_policy(root)
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)

        self.assertIn("asset:empty-group", {item.code for item in report.findings})

    def test_asset_policy_rejects_invalid_snapshot_format(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write_bytes(root, "resources/images/map.png", VALID_PNG)
        self._write_asset_policy(root, status="approved", evidence="docs/license.md")
        self._write_path_policy(root)
        policy_path = root / "tools/release/public_assets.json"
        payload = json.loads(policy_path.read_text(encoding="utf-8"))
        payload["groups"][0]["snapshot"]["file_count"] = True
        policy_path.write_text(
            json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        self._track_all(root)

        with self.assertRaisesRegex(
            check_public_readiness.ReadinessError,
            "snapshot.file_count",
        ):
            check_public_readiness.audit_repository(root)

    def test_unmerged_index_is_rejected_before_snapshot_audit(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        blob_oids: list[str] = []
        for content in (b"base", b"ours", b"theirs"):
            completed = subprocess.run(
                ["git", "hash-object", "-w", "--stdin"],
                cwd=root,
                input=content,
                stdout=subprocess.PIPE,
                check=True,
            )
            blob_oids.append(completed.stdout.decode("ascii").strip())
        index_info = "".join(
            f"100644 {oid} {stage}\tresources/images/map.png\n"
            for stage, oid in enumerate(blob_oids, start=1)
        )
        subprocess.run(
            ["git", "update-index", "--index-info"],
            cwd=root,
            input=index_info,
            text=True,
            check=True,
        )

        with self.assertRaisesRegex(
            check_public_readiness.ReadinessError,
            "未解决冲突",
        ):
            check_public_readiness.audit_repository(root)

    def test_asset_outside_resources_must_be_listed(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write(root, "README.md", "fixture\n")
        self._write_bytes(root, "sim/server/static/map.png", VALID_PNG)
        self._write_asset_policy(root, status="pending")
        self._write_path_policy(
            root,
            directories=("docs", "resources", "sim", "tools"),
        )
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)

        self.assertIn("asset:unlisted", {item.code for item in report.findings})

    def test_approved_asset_outside_resources_accepts_evidence(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write(root, "README.md", "fixture\n")
        self._write_bytes(root, "sim/server/static/map.png", VALID_PNG)
        self._write_asset_policy(
            root,
            status="approved",
            evidence="docs/license-record.md",
            path="sim/server/static/map.png",
        )
        self._write_path_policy(
            root,
            directories=("docs", "resources", "sim", "tools"),
        )
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)

        self.assertTrue(report.passed)

    def test_bare_lfs_pointer_is_not_a_publishable_asset(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write(
            root,
            "resources/images/map.png",
            "version https://git-lfs.github.com/spec/v1\n"
            "oid sha256:0123456789abcdef\nsize 123\n",
        )
        self._write_asset_policy(
            root,
            status="approved",
            evidence="docs/license-record.md",
        )
        self._write_path_policy(root)
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)

        self.assertIn("asset:lfs-pointer", {item.code for item in report.findings})

    def test_invalid_font_placeholder_is_blocked(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write(root, "resources/fonts/roboto.ttf", "download unavailable\n")
        self._write_asset_policy(
            root,
            status="approved",
            evidence="docs/license-record.md",
            path="resources/fonts",
        )
        self._write_path_policy(root)
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)

        self.assertIn("asset:invalid-file", {item.code for item in report.findings})

    def test_png_crc_mismatch_is_blocked(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        damaged_png = bytearray(VALID_PNG)
        damaged_png[-1] ^= 0x01
        self._write_bytes(root, "resources/images/map.png", bytes(damaged_png))
        self._write_asset_policy(
            root,
            status="approved",
            evidence="docs/license-record.md",
        )
        self._write_path_policy(root)
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)

        self.assertIn("asset:invalid-file", {item.code for item in report.findings})

    def test_temporary_download_file_is_blocked(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write(root, "resources/images/map.png.drivedownload", "fixture\n")
        self._write_path_policy(root)
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)

        self.assertIn("asset:temporary-file", {item.code for item in report.findings})

    def test_unknown_top_level_path_is_blocked(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "LICENSE", "Example license for test fixture\n")
        self._write(root, "internal/notes.md", "fixture\n")
        self._write_path_policy(root)
        self._track_all(root)

        report = check_public_readiness.audit_repository(root)

        self.assertFalse(report.passed)
        self.assertIn(
            "path:outside-public-roots",
            {item.code for item in report.findings},
        )

    def test_cli_returns_nonzero_and_prints_actionable_chinese_report(self) -> None:
        temporary, root = self._new_repo()
        self.addCleanup(temporary.cleanup)
        self._write(root, "README.md", "project\n")
        self._write_path_policy(root)
        self._track_all(root)
        output = io.StringIO()

        with contextlib.redirect_stdout(output):
            exit_code = check_public_readiness.main(["--repo", str(root)])

        rendered = output.getvalue()
        self.assertEqual(1, exit_code)
        self.assertIn("公开发布预检：未通过", rendered)
        self.assertIn("仓库根目录缺少许可证", rendered)
        self.assertIn("处理：", rendered)


if __name__ == "__main__":
    unittest.main()
