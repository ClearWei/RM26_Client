#!/usr/bin/env python3
"""检查 Git 索引中的发布快照是否满足公开发布前的基本要求。"""

from __future__ import annotations

import argparse
import hashlib
import ipaddress
import json
import re
import subprocess
import sys
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterator, Sequence

from check_example_config import render_issue, validate_example_config


LICENSE_NAMES = {
    "license",
    "license.md",
    "license.txt",
    "copying",
    "copying.md",
    "copying.txt",
}

PATH_POLICY_PATH = "tools/release/public_paths.json"
ASSET_POLICY_PATH = "tools/release/public_assets.json"
ASSET_POLICY_SCHEMA_VERSION = 2
ASSET_SNAPSHOT_ALGORITHM = "sha256:path-nul-blob-oid-lf:v1"
RUNTIME_CONFIG_PATH = "config.json"
EXAMPLE_CONFIG_PATH = "config.example.json"
ASSET_SUFFIXES = {
    ".aac",
    ".avi",
    ".bin",
    ".flac",
    ".gif",
    ".icns",
    ".ico",
    ".jpeg",
    ".jpg",
    ".m4a",
    ".mkv",
    ".mov",
    ".mp3",
    ".mp4",
    ".ogg",
    ".onnx",
    ".otf",
    ".png",
    ".pt",
    ".pth",
    ".svg",
    ".ttc",
    ".ttf",
    ".wav",
    ".webm",
    ".webp",
    ".woff",
    ".woff2",
}

TEMPORARY_ASSET_SUFFIXES = {
    ".crdownload",
    ".drivedownload",
    ".part",
}

LFS_POINTER_HEADER = b"version https://git-lfs.github.com/spec/v1\n"

ASSET_MAGIC_PREFIXES = {
    ".gif": (b"GIF87a", b"GIF89a"),
    ".icns": (b"icns",),
    ".ico": (b"\x00\x00\x01\x00",),
    ".jpeg": (b"\xff\xd8\xff",),
    ".jpg": (b"\xff\xd8\xff",),
    ".otf": (b"OTTO",),
    ".png": (b"\x89PNG\r\n\x1a\n",),
    ".ttc": (b"ttcf",),
    ".ttf": (b"\x00\x01\x00\x00", b"true", b"typ1"),
    ".woff": (b"wOFF",),
    ".woff2": (b"wOF2",),
}

TEXT_SUFFIXES = {
    ".bat",
    ".cfg",
    ".cmake",
    ".conf",
    ".cpp",
    ".css",
    ".h",
    ".hpp",
    ".html",
    ".ini",
    ".js",
    ".json",
    ".md",
    ".ps1",
    ".py",
    ".qml",
    ".sh",
    ".toml",
    ".txt",
    ".xml",
    ".yaml",
    ".yml",
}

TEXT_FILE_NAMES = {
    ".dockerignore",
    ".editorconfig",
    ".gitattributes",
    ".gitignore",
    "dockerfile",
    "makefile",
}

PLACEHOLDER_USERS = {
    "demo",
    "developer",
    "example",
    "name",
    "runner",
    "shared",
    "user",
    "username",
    "yourname",
}

ALLOWED_IPV4_NETWORKS = tuple(
    ipaddress.ip_network(network)
    for network in (
        "127.0.0.0/8",
        "192.0.2.0/24",
        "198.51.100.0/24",
        "203.0.113.0/24",
    )
)

ALLOWED_PRIVATE_EXAMPLES = {
    ipaddress.ip_address("192.168.12.1"),
    ipaddress.ip_address("192.168.12.2"),
}

# 固定地址只用于检查器自身和单测样例。公开文档和可执行配置仍需使用
# 回环地址或 RFC 5737 示例地址，避免普通启动命令误连现场网络。
PRIVATE_EXAMPLE_PATHS = {
    "tools/release/check_public_readiness.py",
    "tools/release/tests/test_check_public_readiness.py",
}

IPV4_RE = re.compile(r"(?<![\d.])(?:\d{1,3}\.){3}\d{1,3}(?![\d.])")
POSIX_HOME_RE = re.compile(
    r"(?<![\w])/(?P<root>Users|home)/(?P<user>[A-Za-z0-9._-]+)"
    r"(?P<tail>/[^\s`\"'<>]*)?"
)
WINDOWS_HOME_RE = re.compile(
    r"(?i)(?<![A-Za-z0-9])(?P<drive>[A-Z]):[\\/]+Users[\\/]+"
    r"(?P<user>[A-Za-z0-9._-]+)(?P<tail>[\\/][^\s`\"'<>]*)?"
)
SSH_IDENTITY_RE = re.compile(
    r"(?i)(?:\bIdentityFile\s+\S+|\b(?:ssh|scp|sftp|rsync)\b[^\n]*\s-i\s+\S+)"
)
SSH_COMMAND_RE = re.compile(r"(?i)\b(?:ssh|scp|sftp|rsync)\b")
SSH_HOST_RE = re.compile(
    r"(?i)(?:[A-Za-z0-9._-]+@|\bHostName\s+)((?:\d{1,3}\.){3}\d{1,3})"
)
PUBLIC_API_BIND_RE = re.compile(
    r"(?i)(?:"
    r"[\"']api_host[\"']\s*[:=]\s*[\"']0\.0\.0\.0[\"']"
    r"|(?:uvicorn\.run|app\.run)\([^\n#]*\bhost\s*=\s*[\"']0\.0\.0\.0[\"']"
    r")"
)


@dataclass(frozen=True)
class Finding:
    code: str
    title: str
    detail: str
    action: str
    path: str | None = None
    line: int | None = None


@dataclass(frozen=True)
class AuditReport:
    tracked_file_count: int
    findings: tuple[Finding, ...]

    @property
    def passed(self) -> bool:
        return not self.findings


class ReadinessError(RuntimeError):
    """预检无法可靠执行时抛出。"""


@dataclass(frozen=True)
class GitIndexEntry:
    path: str
    blob_oid: str
    stage: int


@dataclass(frozen=True)
class ForbiddenPathRule:
    code: str
    title: str
    action: str
    predicate: Callable[[str], bool]


def _is_official_asset(path: str) -> bool:
    lowered = path.lower()
    if lowered.startswith("docs/assets/robomaster-"):
        return True
    name = Path(lowered).name
    return lowered.startswith("docs/assets/") and (
        name.startswith("command_screen_")
        or (name.startswith("rm_command_screen_") and "official" in name)
    )


def _is_official_derived_markdown(path: str) -> bool:
    lowered = path.lower()
    if not lowered.startswith("docs/robomaster-") or not lowered.endswith(".md"):
        return False
    return any(
        marker in lowered
        for marker in (
            "communication-protocol",
            "competition-rules",
            "player-ui-manual",
            "referee-ui-manual",
        )
    )


FORBIDDEN_PATH_RULES = (
    ForbiddenPathRule(
        "path:agent-metadata",
        "包含智能开发工具或代理元数据",
        "从公开树移除代理配置、技能和会话目录；这些内容不属于客户端或模拟器交付物。",
        lambda path: any(
            path == root or path.startswith(f"{root}/")
            for root in (
                ".agent",
                ".agents",
                ".claude",
                ".codex",
                ".rm26_dev",
                ".trellis",
            )
        ),
    ),
    ForbiddenPathRule(
        "path:ai-instructions",
        "包含面向智能开发工具的指令文件",
        "从公开树移除 AI 助手专用指令，只保留面向开发者的 CONTRIBUTING 和工程文档。",
        lambda path: path
        in {
            ".github/copilot-instructions.md",
            ".traeignore",
            "AGENTS.md",
            "CLAUDE.md",
        },
    ),
    ForbiddenPathRule(
        "path:runtime-evidence",
        "包含本地运行记录或自动化证据",
        "从公开树移除日志、抓包、浏览器快照和本地运行产物。",
        lambda path: any(
            path == root or path.startswith(f"{root}/")
            for root in (
                ".playwright-cli",
                ".playwright-mcp",
                ".tmp",
                "harness/runs",
                "logs",
                "output",
                "recordings",
                "tmp",
            )
        )
        or Path(path).suffix.lower()
        in {".har", ".log", ".pcap", ".pcapng", ".profraw", ".trace"},
    ),
    ForbiddenPathRule(
        "path:internal-process-doc",
        "包含内部计划、审计过程或任务记录",
        "从公开树移除过程稿；仍有长期价值的结论应整理到架构、使用或贡献文档。",
        lambda path: path.startswith("docs/plans/")
        or path
        in {
            "docs/communications/2026-rm26-custom-client-interview-brief.md",
            "docs/maintainers/history-audit-2026-08-22.md",
            "docs/maintainers/open-source-hardening-baseline.md",
            "docs/maintainers/open-source-readiness.md",
            "harness/task_video_decoder_fix.md",
            "RM26-lite.code-workspace",
        },
    ),
    ForbiddenPathRule(
        "path:marscode",
        "包含本机编辑器元数据",
        "从公开树移除 .marscode；开发者本地配置应由 .gitignore 管理。",
        lambda path: path == ".marscode" or path.startswith(".marscode/"),
    ),
    ForbiddenPathRule(
        "path:official-source",
        "包含官方原始资料",
        "将 docs/source 移出公开树，或先取得并记录可再分发授权。",
        lambda path: path == "docs/source" or path.startswith("docs/source/"),
    ),
    ForbiddenPathRule(
        "path:internal-needs",
        "包含内部需求和过程文档",
        "从公开树移除 docs/needs，并把仍有价值的内容整理为面向贡献者的文档。",
        lambda path: path == "docs/needs" or path.startswith("docs/needs/"),
    ),
    ForbiddenPathRule(
        "path:official-assets",
        "包含疑似官方手册截图或界面素材",
        "逐项确认素材权利；未确认前从公开树移除，必要时换成项目自制示意图。",
        _is_official_asset,
    ),
    ForbiddenPathRule(
        "path:official-ocr",
        "包含疑似官方资料的 OCR 或转写稿",
        "逐项确认官方资料的再分发条件；未确认前仅保留链接和项目原创摘要。",
        _is_official_derived_markdown,
    ),
)


def git_index_entries(repo_root: Path) -> tuple[GitIndexEntry, ...]:
    completed = subprocess.run(
        ["git", "ls-files", "--stage", "-z"],
        cwd=repo_root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        message = completed.stderr.decode("utf-8", errors="replace").strip()
        raise ReadinessError(f"git ls-files 执行失败：{message or '未知错误'}")

    entries: list[GitIndexEntry] = []
    for record in completed.stdout.split(b"\0"):
        if not record:
            continue
        metadata, separator, raw_path = record.partition(b"\t")
        parts = metadata.split()
        if not separator or len(parts) != 3:
            raise ReadinessError("git ls-files 返回了无法识别的索引记录")
        _, raw_oid, raw_stage = parts
        try:
            path = raw_path.decode("utf-8")
            blob_oid = raw_oid.decode("ascii")
            stage = int(raw_stage)
        except (UnicodeDecodeError, ValueError) as error:
            raise ReadinessError("Git 索引包含无法解析的路径或对象编号") from error
        if not re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", blob_oid):
            raise ReadinessError(f"Git 索引中的对象编号格式无效：{path}")
        entries.append(GitIndexEntry(path=path, blob_oid=blob_oid, stage=stage))

    conflict_paths = sorted({entry.path for entry in entries if entry.stage != 0})
    if conflict_paths:
        raise ReadinessError(
            "Git 索引仍有未解决冲突："
            f"{_summarize_paths(conflict_paths)}；请先解决冲突并写入 stage 0。"
        )
    return tuple(sorted(entries, key=lambda entry: entry.path.encode("utf-8")))


def git_tracked_files(repo_root: Path) -> tuple[str, ...]:
    return tuple(entry.path for entry in git_index_entries(repo_root))


def _has_root_license(paths: Sequence[str]) -> bool:
    return any("/" not in path and path.lower() in LICENSE_NAMES for path in paths)


def _summarize_paths(paths: Sequence[str], limit: int = 6) -> str:
    shown = ", ".join(paths[:limit])
    remaining = len(paths) - limit
    return f"{shown}（另有 {remaining} 项）" if remaining > 0 else shown


def _path_findings(paths: Sequence[str]) -> list[Finding]:
    findings: list[Finding] = []
    for rule in FORBIDDEN_PATH_RULES:
        matches = sorted(path for path in paths if rule.predicate(path))
        if not matches:
            continue
        findings.append(
            Finding(
                code=rule.code,
                title=rule.title,
                detail=f"Git 已跟踪 {_summarize_paths(matches)}",
                action=rule.action,
            )
        )
    return findings


def _root_findings(repo_root: Path, paths: Sequence[str]) -> list[Finding]:
    if PATH_POLICY_PATH not in paths:
        return [
            Finding(
                code="path:policy-missing",
                title="缺少公开树白名单",
                detail=f"Git 索引中没有 {PATH_POLICY_PATH}。",
                action="由维护者审查顶层目录和文件后补充机器可读白名单。",
            )
        ]

    payload = _load_index_json(repo_root, PATH_POLICY_PATH)
    raw_directories = payload.get("directories", [])
    raw_files = payload.get("files", [])
    if not isinstance(raw_directories, list) or not all(
        isinstance(item, str) and item and "/" not in item
        for item in raw_directories
    ):
        raise ReadinessError(f"{PATH_POLICY_PATH} 的 directories 必须是顶层目录名数组")
    if not isinstance(raw_files, list) or not all(
        isinstance(item, str) and item and "/" not in item for item in raw_files
    ):
        raise ReadinessError(f"{PATH_POLICY_PATH} 的 files 必须是根目录文件名数组")

    allowed_directories = set(raw_directories)
    allowed_files = set(raw_files)
    unknown: list[str] = []
    for path in paths:
        root, separator, _ = path.partition("/")
        if separator and root in allowed_directories:
            continue
        if not separator and (
            root in allowed_files or root.lower() in LICENSE_NAMES
        ):
            continue
        if any(rule.predicate(path) for rule in FORBIDDEN_PATH_RULES):
            continue
        unknown.append(path)

    if not unknown:
        return []
    return [
        Finding(
            code="path:outside-public-roots",
            title="包含未列入公开树的顶层路径",
            detail=f"Git 已跟踪 {_summarize_paths(sorted(unknown))}",
            action="确认用途后移动到已批准目录，或由维护者审查并更新公开树白名单。",
        )
    ]


def _path_is_under(path: str, prefix: str) -> bool:
    return path == prefix or path.startswith(f"{prefix}/")


def _is_asset(path: str) -> bool:
    return Path(path).suffix.lower() in ASSET_SUFFIXES


def _has_valid_png_structure(data: bytes) -> bool:
    if not data.startswith(ASSET_MAGIC_PREFIXES[".png"]):
        return False

    offset = 8
    saw_header = False
    compressed_parts: list[bytes] = []
    while offset < len(data):
        if offset + 12 > len(data):
            return False
        length = int.from_bytes(data[offset : offset + 4], "big")
        chunk_end = offset + 12 + length
        if chunk_end > len(data):
            return False

        chunk_type = data[offset + 4 : offset + 8]
        chunk_data = data[offset + 8 : offset + 8 + length]
        stored_crc = int.from_bytes(data[offset + 8 + length : chunk_end], "big")
        actual_crc = zlib.crc32(chunk_type + chunk_data) & 0xFFFFFFFF
        if stored_crc != actual_crc:
            return False

        if chunk_type == b"IHDR":
            if saw_header or offset != 8 or length != 13:
                return False
            saw_header = True
        elif chunk_type == b"IDAT":
            compressed_parts.append(chunk_data)
        elif chunk_type == b"IEND":
            if length != 0 or chunk_end != len(data):
                return False
            if not saw_header or not compressed_parts:
                return False
            try:
                zlib.decompress(b"".join(compressed_parts))
            except zlib.error:
                return False
            return True

        offset = chunk_end
    return False


def _has_valid_asset_magic(path: str, data: bytes) -> bool:
    suffix = Path(path).suffix.lower()
    if suffix == ".png":
        return _has_valid_png_structure(data)
    prefixes = ASSET_MAGIC_PREFIXES.get(suffix)
    return prefixes is None or data.startswith(prefixes)


def _asset_snapshot(entries: Sequence[GitIndexEntry]) -> tuple[int, str]:
    """按公开策略约定，对排序后的索引路径和 blob OID 生成稳定摘要。"""

    digest = hashlib.sha256()
    ordered = sorted(entries, key=lambda entry: entry.path.encode("utf-8"))
    for entry in ordered:
        digest.update(entry.path.encode("utf-8"))
        digest.update(b"\0")
        digest.update(entry.blob_oid.encode("ascii"))
        digest.update(b"\n")
    return len(ordered), digest.hexdigest()


def _asset_findings(
    repo_root: Path,
    paths: Sequence[str],
    index_entries: Sequence[GitIndexEntry],
) -> list[Finding]:
    asset_paths = sorted(path for path in paths if _is_asset(path))
    temporary_paths = sorted(
        path
        for path in paths
        if Path(path).suffix.lower() in TEMPORARY_ASSET_SUFFIXES
    )
    if not asset_paths and not temporary_paths and ASSET_POLICY_PATH not in paths:
        return []

    findings: list[Finding] = []
    if temporary_paths:
        findings.append(
            Finding(
                code="asset:temporary-file",
                title="包含临时下载文件",
                detail=f"Git 已跟踪 {_summarize_paths(temporary_paths)}",
                action="删除临时下载产物；需要保留的素材应恢复为可识别的正式文件并登记来源。",
            )
        )

    pointer_paths: list[str] = []
    invalid_paths: list[str] = []
    for path, data in _read_index_blobs(repo_root, asset_paths):
        if data.startswith(LFS_POINTER_HEADER):
            pointer_paths.append(path)
            continue
        if not _has_valid_asset_magic(path, data):
            invalid_paths.append(path)

    if pointer_paths:
        findings.append(
            Finding(
                code="asset:lfs-pointer",
                title="素材仍是裸 Git LFS 指针",
                detail=f"发现 {_summarize_paths(pointer_paths)}",
                action="提交真实素材对象，或从公开快照移除这些失效指针；不要把指针文本当作资源文件发布。",
            )
        )
    if invalid_paths:
        findings.append(
            Finding(
                code="asset:invalid-file",
                title="素材文件头与扩展名不匹配",
                detail=f"发现 {_summarize_paths(invalid_paths)}",
                action="恢复有效原文件或移除占位内容，并重新运行公开发布预检。",
            )
        )

    if ASSET_POLICY_PATH not in paths:
        if asset_paths:
            findings.append(
                Finding(
                    code="asset:policy-missing",
                    title="缺少机器可读素材授权清单",
                    detail=f"发现 {_summarize_paths(asset_paths)}",
                    action=f"补充 {ASSET_POLICY_PATH}，逐组记录状态、授权证据和索引快照。",
                )
            )
        return findings

    payload = _load_index_json(repo_root, ASSET_POLICY_PATH)
    if payload.get("schema_version") != ASSET_POLICY_SCHEMA_VERSION:
        raise ReadinessError(
            f"{ASSET_POLICY_PATH} 的 schema_version 必须为 "
            f"{ASSET_POLICY_SCHEMA_VERSION}"
        )
    if payload.get("snapshot_algorithm") != ASSET_SNAPSHOT_ALGORITHM:
        raise ReadinessError(
            f"{ASSET_POLICY_PATH} 的 snapshot_algorithm 必须为 "
            f"{ASSET_SNAPSHOT_ALGORITHM}"
        )

    raw_groups = payload.get("groups", [])
    if not isinstance(raw_groups, list):
        raise ReadinessError(f"{ASSET_POLICY_PATH} 的 groups 必须是数组")

    groups: dict[str, dict[str, object]] = {}
    for item in raw_groups:
        if not isinstance(item, dict):
            raise ReadinessError(f"{ASSET_POLICY_PATH} 包含无效的素材组")
        raw_prefix = item.get("path")
        if not isinstance(raw_prefix, str):
            raise ReadinessError(f"{ASSET_POLICY_PATH} 包含无效的素材组路径")
        prefix = raw_prefix
        if (
            not prefix
            or prefix.startswith("/")
            or prefix.endswith("/")
            or "\\" in prefix
            or any(part in {"", ".", ".."} for part in prefix.split("/"))
        ):
            raise ReadinessError(
                f"{ASSET_POLICY_PATH} 的素材组路径不是规范相对路径：{prefix!r}"
            )
        if prefix in groups:
            raise ReadinessError(f"{ASSET_POLICY_PATH} 重复定义素材组：{prefix}")

        status = item.get("status")
        if status not in {"pending", "approved"}:
            raise ReadinessError(
                f"{ASSET_POLICY_PATH} 的 {prefix} 状态必须为 pending 或 approved"
            )
        if not isinstance(item.get("evidence"), str):
            raise ReadinessError(
                f"{ASSET_POLICY_PATH} 的 {prefix} evidence 必须是字符串"
            )

        snapshot = item.get("snapshot")
        if not isinstance(snapshot, dict):
            raise ReadinessError(
                f"{ASSET_POLICY_PATH} 的 {prefix} 缺少 snapshot 对象"
            )
        file_count = snapshot.get("file_count")
        expected_digest = snapshot.get("sha256")
        if type(file_count) is not int or file_count < 0:
            raise ReadinessError(
                f"{ASSET_POLICY_PATH} 的 {prefix} snapshot.file_count 必须是非负整数"
            )
        if not isinstance(expected_digest, str) or not re.fullmatch(
            r"[0-9a-f]{64}", expected_digest
        ):
            raise ReadinessError(
                f"{ASSET_POLICY_PATH} 的 {prefix} snapshot.sha256 必须是 64 位小写十六进制"
            )
        groups[prefix] = item

    prefixes = tuple(groups)
    for index, prefix in enumerate(prefixes):
        for other in prefixes[index + 1 :]:
            if _path_is_under(prefix, other) or _path_is_under(other, prefix):
                raise ReadinessError(
                    f"{ASSET_POLICY_PATH} 的素材组范围重叠：{prefix} 与 {other}"
                )

    covered: set[str] = set()
    for prefix, item in groups.items():
        group_entries = [
            entry for entry in index_entries if _path_is_under(entry.path, prefix)
        ]
        asset_matches = [path for path in asset_paths if _path_is_under(path, prefix)]
        if not group_entries:
            findings.append(
                Finding(
                    code="asset:empty-group",
                    title="素材策略包含空组",
                    detail=f"{prefix} 在 Git 索引中没有任何 stage 0 文件。",
                    action="删除失效素材组，或先把需要审查的文件写入 Git 索引后重新生成快照。",
                )
            )
            continue
        covered.update(asset_matches)

        actual_count, actual_digest = _asset_snapshot(group_entries)
        snapshot = item["snapshot"]
        assert isinstance(snapshot, dict)
        expected_count = snapshot["file_count"]
        expected_digest = snapshot["sha256"]
        if expected_count != actual_count or expected_digest != actual_digest:
            findings.append(
                Finding(
                    code="asset:snapshot-mismatch",
                    title="素材组与批准时的索引快照不一致",
                    detail=(
                        f"{prefix} 登记为 {expected_count} 项/{expected_digest}，"
                        f"当前为 {actual_count} 项/{actual_digest}。"
                    ),
                    action="逐项复核新增、删除或替换内容；确认权利后再用当前 Git 索引重新生成快照。",
                )
            )

        if not asset_matches:
            continue
        status = str(item.get("status", ""))
        evidence = str(item.get("evidence", "")).strip()
        if status == "approved" and evidence:
            continue
        findings.append(
            Finding(
                code="asset:not-approved",
                title="素材组尚未批准公开",
                detail=f"{prefix} 共 {len(asset_matches)} 项，状态为 {status}。",
                action="确认来源和再分发权后，将状态改为 approved 并填写可复核的 evidence。",
            )
        )

    unlisted = sorted(set(asset_paths) - covered)
    if unlisted:
        findings.append(
            Finding(
                code="asset:unlisted",
                title="存在未纳入素材授权清单的文件",
                detail=f"发现 {_summarize_paths(unlisted)}",
                action=f"在 {ASSET_POLICY_PATH} 中新增对应素材组，未确认前保持 pending。",
            )
        )
    return findings


def _is_text_candidate(path: Path) -> bool:
    return path.suffix.lower() in TEXT_SUFFIXES or path.name.lower() in TEXT_FILE_NAMES


def _read_index_blob(repo_root: Path, relative_path: str) -> bytes:
    completed = subprocess.run(
        ["git", "cat-file", "blob", f":{relative_path}"],
        cwd=repo_root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        message = completed.stderr.decode("utf-8", errors="replace").strip()
        raise ReadinessError(
            f"无法读取索引中的 {relative_path}：{message or '未知错误'}"
        )
    return completed.stdout


def _read_index_blobs(
    repo_root: Path, relative_paths: Sequence[str]
) -> Iterator[tuple[str, bytes]]:
    process = subprocess.Popen(
        ["git", "cat-file", "--batch"],
        cwd=repo_root,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if process.stdin is None or process.stdout is None or process.stderr is None:
        process.kill()
        raise ReadinessError("无法启动 git cat-file 批量读取进程")

    try:
        for relative_path in relative_paths:
            if "\n" in relative_path or "\r" in relative_path:
                raise ReadinessError(f"素材路径包含不支持的换行符：{relative_path!r}")
            query = f":{relative_path}\n".encode(
                "utf-8", errors="surrogateescape"
            )
            process.stdin.write(query)
            process.stdin.flush()

            header = process.stdout.readline()
            parts = header.rstrip(b"\n").rsplit(b" ", 2)
            if len(parts) != 3 or parts[1] != b"blob":
                detail = header.decode("utf-8", errors="replace").strip()
                raise ReadinessError(
                    f"无法读取索引中的 {relative_path}：{detail or '未知错误'}"
                )
            try:
                size = int(parts[2])
            except ValueError as error:
                raise ReadinessError(
                    f"无法解析 {relative_path} 的 Git 对象大小"
                ) from error

            data = process.stdout.read(size)
            terminator = process.stdout.read(1)
            if len(data) != size or terminator != b"\n":
                raise ReadinessError(f"索引中的 {relative_path} 读取不完整")
            yield relative_path, data
    finally:
        process.stdin.close()
        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            process.terminate()
            process.wait()
        process.stdout.close()
        process.stderr.close()


def _read_index_text(repo_root: Path, relative_path: str) -> str | None:
    if not _is_text_candidate(Path(relative_path)):
        return None
    data = _read_index_blob(repo_root, relative_path)
    if b"\0" in data:
        return None
    return data.decode("utf-8", errors="replace")


def _load_index_json(repo_root: Path, relative_path: str) -> dict[str, object]:
    text = _read_index_text(repo_root, relative_path)
    if text is None:
        raise ReadinessError(f"{relative_path} 不是可读取的 UTF-8 文本")
    try:
        payload = json.loads(text)
    except json.JSONDecodeError as error:
        raise ReadinessError(f"{relative_path} 格式错误：{error}") from error
    if not isinstance(payload, dict):
        raise ReadinessError(f"{relative_path} 顶层必须是 JSON 对象")
    return payload


def _config_findings(repo_root: Path, paths: Sequence[str]) -> list[Finding]:
    path_set = set(paths)
    findings: list[Finding] = []
    if RUNTIME_CONFIG_PATH in path_set and EXAMPLE_CONFIG_PATH not in path_set:
        findings.append(
            Finding(
                code="config:example-missing",
                title="缺少公开示例配置",
                detail=f"仓库跟踪 {RUNTIME_CONFIG_PATH}，但没有 {EXAMPLE_CONFIG_PATH}。",
                action="补充只使用回环地址的完整示例，并通过示例配置检查。",
            )
        )

    for relative_path, code, title in (
        (
            RUNTIME_CONFIG_PATH,
            "config:runtime-not-public",
            "运行配置不适合作为公开默认值",
        ),
        (
            EXAMPLE_CONFIG_PATH,
            "config:example-invalid",
            "公开示例配置不安全或不完整",
        ),
    ):
        if relative_path not in path_set:
            continue
        payload = _load_index_json(repo_root, relative_path)
        issues = validate_example_config(payload)
        if not issues:
            continue
        detail = "；".join(render_issue(issue) for issue in issues[:3])
        if len(issues) > 3:
            detail += f"；另有 {len(issues) - 3} 项"
        findings.append(
            Finding(
                code=code,
                title=title,
                detail=detail,
                action=(
                    "公开默认值只保留回环地址、中性身份和安全相对路径；"
                    "现场值改由未跟踪的本地配置或环境变量注入。"
                ),
                path=relative_path,
            )
        )
    return findings


def _personal_paths(line: str) -> list[str]:
    matches: list[str] = []
    for pattern in (POSIX_HOME_RE, WINDOWS_HOME_RE):
        for match in pattern.finditer(line):
            if match.group("user").lower() in PLACEHOLDER_USERS:
                continue
            matches.append(match.group(0).rstrip(".,;:)]}"))
    return matches


def _parse_ipv4(value: str) -> ipaddress.IPv4Address | None:
    try:
        address = ipaddress.ip_address(value)
    except ValueError:
        return None
    return address if isinstance(address, ipaddress.IPv4Address) else None


def _allows_private_examples(relative_path: str) -> bool:
    return relative_path in PRIVATE_EXAMPLE_PATHS


def _is_allowed_ipv4(
    address: ipaddress.IPv4Address, *, allow_private_examples: bool = False
) -> bool:
    if address.is_unspecified:
        return True
    if allow_private_examples and address in ALLOWED_PRIVATE_EXAMPLES:
        return True
    return any(address in network for network in ALLOWED_IPV4_NETWORKS)


def _field_addresses(line: str, *, allow_private_examples: bool = False) -> list[str]:
    matches: list[str] = []
    for value in IPV4_RE.findall(line):
        address = _parse_ipv4(value)
        if address is None or _is_allowed_ipv4(
            address, allow_private_examples=allow_private_examples
        ):
            continue
        if address.is_private or address.is_link_local:
            matches.append(value)
    return matches


def _ssh_metadata(line: str) -> list[str]:
    matches: list[str] = []
    identity = SSH_IDENTITY_RE.search(line)
    if identity:
        matches.append(identity.group(0))

    if SSH_COMMAND_RE.search(line) or re.search(r"(?i)^\s*HostName\b", line):
        for match in SSH_HOST_RE.finditer(line):
            address = _parse_ipv4(match.group(1))
            if address is not None and not _is_allowed_ipv4(address) and (
                address.is_private or address.is_link_local
            ):
                matches.append(match.group(0))
    return matches


def _public_api_bindings(line: str) -> list[str]:
    return [match.group(0) for match in PUBLIC_API_BIND_RE.finditer(line)]


def _short_evidence(values: Sequence[str], limit: int = 3) -> str:
    unique = list(dict.fromkeys(" ".join(value.split()) for value in values))
    evidence = ", ".join(unique[:limit])
    return evidence[:180] + ("…" if len(evidence) > 180 else "")


def _content_findings(repo_root: Path, paths: Sequence[str]) -> list[Finding]:
    findings: list[Finding] = []
    for relative_path in paths:
        text = _read_index_text(repo_root, relative_path)
        if text is None:
            continue

        allow_private_examples = _allows_private_examples(relative_path)
        personal: list[tuple[int, str]] = []
        addresses: list[tuple[int, str]] = []
        ssh_metadata: list[tuple[int, str]] = []
        public_bindings: list[tuple[int, str]] = []
        for line_number, line in enumerate(text.splitlines(), start=1):
            personal.extend((line_number, value) for value in _personal_paths(line))
            addresses.extend(
                (line_number, value)
                for value in _field_addresses(
                    line, allow_private_examples=allow_private_examples
                )
            )
            ssh_metadata.extend((line_number, value) for value in _ssh_metadata(line))
            public_bindings.extend(
                (line_number, value) for value in _public_api_bindings(line)
            )

        if personal:
            findings.append(
                Finding(
                    code="content:personal-path",
                    title="包含个人绝对路径",
                    detail=f"发现 {_short_evidence([value for _, value in personal])}",
                    action="改用仓库相对路径、环境变量或不含个人账号的占位符。",
                    path=relative_path,
                    line=personal[0][0],
                )
            )
        if addresses:
            findings.append(
                Finding(
                    code="content:field-address",
                    title="包含疑似真实现场私网地址",
                    detail=f"发现 {_short_evidence([value for _, value in addresses])}",
                    action="把现场地址放入不跟踪的本地配置，公开文件仅保留回环、RFC 5737 或约定示例地址。",
                    path=relative_path,
                    line=addresses[0][0],
                )
            )
        if ssh_metadata:
            findings.append(
                Finding(
                    code="content:ssh-metadata",
                    title="包含 SSH 主机或密钥元信息",
                    detail=f"发现 {_short_evidence([value for _, value in ssh_metadata])}",
                    action="移除真实账号、主机和密钥路径，改用本地配置及文档占位符。",
                    path=relative_path,
                    line=ssh_metadata[0][0],
                )
            )
        if public_bindings:
            findings.append(
                Finding(
                    code="content:public-api-bind",
                    title="控制接口默认监听所有网卡",
                    detail=f"发现 {_short_evidence([value for _, value in public_bindings])}",
                    action="默认绑定 127.0.0.1；需要远程访问时通过受控本地配置显式开放并补充访问保护。",
                    path=relative_path,
                    line=public_bindings[0][0],
                )
            )
    return findings


def audit_repository(repo_root: Path) -> AuditReport:
    root = repo_root.resolve()
    index_entries = git_index_entries(root)
    paths = tuple(entry.path for entry in index_entries)
    findings: list[Finding] = []

    if not _has_root_license(paths):
        findings.append(
            Finding(
                code="license:missing",
                title="仓库根目录缺少许可证",
                detail="Git 索引中未找到 LICENSE、LICENSE.md、LICENSE.txt 或 COPYING。",
                action="在权利归属确认后补充根目录许可证，并同步第三方依赖与素材声明。",
            )
        )

    findings.extend(_root_findings(root, paths))
    findings.extend(_path_findings(paths))
    findings.extend(_config_findings(root, paths))
    findings.extend(_asset_findings(root, paths, index_entries))
    findings.extend(_content_findings(root, paths))
    return AuditReport(tracked_file_count=len(paths), findings=tuple(findings))


def _location(finding: Finding) -> str:
    if finding.path is None:
        return "仓库级"
    return f"{finding.path}:{finding.line}" if finding.line is not None else finding.path


def render_report(report: AuditReport) -> str:
    if report.passed:
        return (
            "公开发布预检：通过\n"
            f"已审计 {report.tracked_file_count} 个 Git 跟踪文件，未发现当前规则定义的阻断项。\n"
            "提示：本工具不审计 Git 历史，正式发布前仍需单独检查历史提交和素材授权。"
        )

    lines = [
        "公开发布预检：未通过",
        f"已审计 {report.tracked_file_count} 个 Git 跟踪文件，发现 {len(report.findings)} 个阻断项：",
    ]
    for index, finding in enumerate(report.findings, start=1):
        lines.extend(
            (
                "",
                f"{index}. [{finding.code}] {finding.title}",
                f"   位置：{_location(finding)}",
                f"   说明：{finding.detail}",
                f"   处理：{finding.action}",
            )
        )
    lines.extend(
        (
            "",
            "说明：仅审计 Git 索引中的发布快照；Git 历史、未暂存改动和未跟踪文件不在本次范围内。",
        )
    )
    return "\n".join(lines)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="检查 Git 索引中的发布快照能否公开")
    parser.add_argument(
        "--repo",
        type=Path,
        default=Path.cwd(),
        help="待检查的 Git 仓库，默认使用当前目录",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        report = audit_repository(args.repo)
    except ReadinessError as error:
        print(f"公开发布预检无法执行：{error}", file=sys.stderr)
        return 2
    print(render_report(report))
    return 0 if report.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
