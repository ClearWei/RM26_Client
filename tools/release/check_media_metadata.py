#!/usr/bin/env python3
"""检查 Git 索引中公开音视频是否携带未经批准的敏感元数据。"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Sequence
from urllib.parse import urlsplit


POLICY_PATH = "tools/release/public_media_metadata.json"

MEDIA_SUFFIXES = {
    ".3g2",
    ".3gp",
    ".aac",
    ".aif",
    ".aiff",
    ".amr",
    ".avi",
    ".flac",
    ".m4a",
    ".m2ts",
    ".mkv",
    ".mp2",
    ".mpeg",
    ".mpg",
    ".mts",
    ".mov",
    ".mp3",
    ".mp4",
    ".oga",
    ".ogg",
    ".opus",
    ".ts",
    ".wav",
    ".webm",
    ".wma",
    ".wmv",
}

# 编码器、容器版本等技术字段可以保留。这里关注会暴露个人、地点或
# 采集设备的字段，并兼容 QuickTime 等工具添加的命名空间前缀。
SENSITIVE_TOKENS = {
    "album",
    "artist",
    "author",
    "comment",
    "comments",
    "composer",
    "contact",
    "copyright",
    "description",
    "device",
    "email",
    "gps",
    "imei",
    "latitude",
    "location",
    "longitude",
    "make",
    "manufacturer",
    "model",
    "owner",
    "performer",
    "phone",
    "publisher",
    "serial",
    "synopsis",
    "title",
    "uuid",
}

SENSITIVE_ALIASES = {
    "art",
    "cmt",
    "nam",
    "wrt",
    "xyz",
}


@dataclass(frozen=True)
class Approval:
    path: str
    scope: str
    field: str
    value: str
    evidence: str

    @property
    def key(self) -> tuple[str, str, str, str]:
        return self.path, self.scope, self.field, self.value


@dataclass(frozen=True)
class MetadataIssue:
    code: str
    path: str
    scope: str
    field: str
    value: str = ""
    detail: str = ""


@dataclass(frozen=True)
class AuditResult:
    media_count: int
    issues: tuple[MetadataIssue, ...]

    @property
    def passed(self) -> bool:
        return not self.issues


class MediaMetadataError(RuntimeError):
    """表示检查所需的 Git 快照、策略或探测器无法可靠读取。"""


def _normalize_field(field: str) -> str:
    with_word_boundaries = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", field)
    return re.sub(r"[^a-z0-9]+", "_", with_word_boundaries.casefold()).strip("_")


def is_sensitive_field(field: str) -> bool:
    normalized = _normalize_field(field)
    tokens = set(normalized.split("_"))
    return bool(tokens & SENSITIVE_TOKENS) or normalized in SENSITIVE_ALIASES


def git_tracked_files(repo_root: Path) -> tuple[str, ...]:
    completed = subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=repo_root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        message = completed.stderr.decode("utf-8", errors="replace").strip()
        raise MediaMetadataError(
            f"git ls-files 执行失败：{message or '未知错误'}"
        )
    decoded = completed.stdout.decode("utf-8", errors="surrogateescape")
    return tuple(path for path in decoded.split("\0") if path)


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
        raise MediaMetadataError(
            f"无法读取索引中的 {relative_path}：{message or '未知错误'}"
        )
    return completed.stdout


def _valid_evidence(evidence: str, tracked_paths: Sequence[str]) -> bool:
    parsed = urlsplit(evidence)
    if parsed.scheme:
        return (
            parsed.scheme == "https"
            and bool(parsed.hostname)
            and parsed.username is None
            and parsed.password is None
        )

    relative_path = PurePosixPath(evidence)
    return (
        not relative_path.is_absolute()
        and "\\" not in evidence
        and ".." not in relative_path.parts
        and evidence in tracked_paths
    )


def _load_approvals(
    repo_root: Path, tracked_paths: Sequence[str], media_paths: Sequence[str]
) -> tuple[Approval, ...]:
    if POLICY_PATH not in tracked_paths:
        return ()

    try:
        payload = json.loads(_read_index_blob(repo_root, POLICY_PATH))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise MediaMetadataError(f"{POLICY_PATH} 格式错误：{error}") from error
    if not isinstance(payload, dict) or payload.get("schema_version") != 1:
        raise MediaMetadataError(f"{POLICY_PATH} 必须使用 schema_version 1")

    raw_allowlist = payload.get("allowlist")
    if not isinstance(raw_allowlist, list):
        raise MediaMetadataError(f"{POLICY_PATH} 的 allowlist 必须是数组")

    known_media = set(media_paths)
    approvals: list[Approval] = []
    seen: set[tuple[str, str, str, str]] = set()
    for index, item in enumerate(raw_allowlist):
        location = f"{POLICY_PATH} allowlist[{index}]"
        if not isinstance(item, dict):
            raise MediaMetadataError(f"{location} 必须是对象")
        required_fields = ("path", "scope", "field", "value", "evidence")
        values = {key: item.get(key) for key in required_fields}
        if not all(
            isinstance(value, str) and value.strip()
            for value in values.values()
        ):
            raise MediaMetadataError(
                f"{location} 必须填写非空的 path、scope、field、value "
                "和 evidence"
            )

        path = str(values["path"])
        scope = str(values["scope"])
        field = _normalize_field(str(values["field"]))
        value = str(values["value"])
        evidence = str(values["evidence"]).strip()
        if path not in known_media:
            raise MediaMetadataError(f"{location} 指向未跟踪的媒体：{path}")
        indexed_scope = re.fullmatch(r"(?:stream|chapter|program):\d+", scope)
        if scope != "format" and indexed_scope is None:
            raise MediaMetadataError(
                f"{location} 的 scope 必须是 format、stream:<序号>、"
                "chapter:<序号> 或 program:<序号>"
            )
        if not is_sensitive_field(field):
            raise MediaMetadataError(
                f"{location} 的 field 不是受控敏感字段：{field}"
            )
        if not _valid_evidence(evidence, tracked_paths):
            raise MediaMetadataError(
                f"{location} 的 evidence 必须是 Git 索引中存在的相对路径，"
                "或不含账号信息的 HTTPS URL"
            )

        approval = Approval(path, scope, field, value, evidence)
        if approval.key in seen:
            raise MediaMetadataError(f"{location} 与已有批准项重复")
        seen.add(approval.key)
        approvals.append(approval)
    return tuple(approvals)


def _parse_probe_payload(path: str, output: bytes) -> list[tuple[str, str, str]]:
    try:
        payload = json.loads(output)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise MediaMetadataError(
            f"ffprobe 返回的 {path} 元数据不是有效 JSON：{error}"
        ) from error
    if not isinstance(payload, dict):
        raise MediaMetadataError(
            f"ffprobe 返回的 {path} 元数据顶层必须是对象"
        )

    metadata: list[tuple[str, str, str]] = []

    def append_tags(scope: str, raw_tags: object) -> None:
        if raw_tags is None:
            return
        if not isinstance(raw_tags, dict):
            raise MediaMetadataError(
                f"ffprobe 返回的 {path} {scope} tags 不是对象"
            )
        for raw_field, raw_value in raw_tags.items():
            if not isinstance(raw_field, str) or not isinstance(raw_value, str):
                raise MediaMetadataError(
                    f"ffprobe 返回的 {path} {scope} tags 不是字符串映射"
                )
            if raw_value:
                metadata.append((scope, raw_field, raw_value))

    raw_format = payload.get("format")
    if raw_format is not None:
        if not isinstance(raw_format, dict):
            raise MediaMetadataError(f"ffprobe 返回的 {path} format 不是对象")
        append_tags("format", raw_format.get("tags"))

    for collection, scope_name in (
        ("streams", "stream"),
        ("chapters", "chapter"),
        ("programs", "program"),
    ):
        raw_items = payload.get(collection, [])
        if not isinstance(raw_items, list):
            raise MediaMetadataError(
                f"ffprobe 返回的 {path} {collection} 不是数组"
            )
        for index, raw_item in enumerate(raw_items):
            if not isinstance(raw_item, dict):
                raise MediaMetadataError(
                    f"ffprobe 返回的 {path} {scope_name}:{index} 不是对象"
                )
            append_tags(f"{scope_name}:{index}", raw_item.get("tags"))
    return metadata


def _probe_index_media(
    repo_root: Path, relative_path: str, ffprobe: str, temporary_root: Path
) -> tuple[list[tuple[str, str, str]], MetadataIssue | None]:
    suffix = Path(relative_path).suffix.lower()
    path_digest = hashlib.sha256(
        relative_path.encode("utf-8", errors="surrogateescape")
    ).hexdigest()[:16]
    probe_path = temporary_root / f"media-{path_digest}{suffix}"
    probe_path.write_bytes(_read_index_blob(repo_root, relative_path))
    try:
        completed = subprocess.run(
            [
                ffprobe,
                "-v",
                "error",
                "-show_entries",
                "format_tags:stream_tags:chapter_tags:program_tags",
                "-of",
                "json",
                str(probe_path),
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
            timeout=30,
        )
    except FileNotFoundError as error:
        raise MediaMetadataError(f"找不到 ffprobe：{ffprobe}") from error
    except subprocess.TimeoutExpired:
        return [], MetadataIssue(
            "probe:timeout",
            relative_path,
            "media",
            "",
            detail="ffprobe 超过 30 秒仍未完成",
        )
    finally:
        probe_path.unlink(missing_ok=True)

    if completed.returncode != 0:
        detail = completed.stderr.decode("utf-8", errors="replace").strip()
        return [], MetadataIssue(
            "probe:failed",
            relative_path,
            "media",
            "",
            detail=detail or f"ffprobe 退出码为 {completed.returncode}",
        )
    return _parse_probe_payload(relative_path, completed.stdout), None


def audit_repository(repo_root: Path, *, ffprobe: str = "ffprobe") -> AuditResult:
    root = repo_root.resolve()
    tracked_paths = git_tracked_files(root)
    media_paths = tuple(
        sorted(
            path
            for path in tracked_paths
            if Path(path).suffix.lower() in MEDIA_SUFFIXES
        )
    )
    approvals = _load_approvals(root, tracked_paths, media_paths)
    approved_keys = {approval.key for approval in approvals}
    observed_sensitive_keys: set[tuple[str, str, str, str]] = set()
    successfully_probed: set[str] = set()
    issues: list[MetadataIssue] = []

    with tempfile.TemporaryDirectory(prefix="rm26-media-metadata-") as temporary:
        temporary_root = Path(temporary)
        for relative_path in media_paths:
            metadata, probe_issue = _probe_index_media(
                root, relative_path, ffprobe, temporary_root
            )
            if probe_issue is not None:
                issues.append(probe_issue)
                continue
            successfully_probed.add(relative_path)
            for scope, raw_field, value in metadata:
                field = _normalize_field(raw_field)
                if not is_sensitive_field(field):
                    continue
                key = relative_path, scope, field, value
                observed_sensitive_keys.add(key)
                if key not in approved_keys:
                    issues.append(
                        MetadataIssue(
                            "metadata:not-approved",
                            relative_path,
                            scope,
                            raw_field,
                            value=value,
                        )
                    )

    for approval in approvals:
        if (
            approval.path in successfully_probed
            and approval.key not in observed_sensitive_keys
        ):
            issues.append(
                MetadataIssue(
                    "policy:stale-approval",
                    approval.path,
                    approval.scope,
                    approval.field,
                    value=approval.value,
                    detail="批准项与当前媒体中的敏感标签不再匹配",
                )
            )
    return AuditResult(len(media_paths), tuple(issues))


def _value_fingerprint(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest()[:12]


def render_result(result: AuditResult) -> str:
    if result.passed:
        return (
            "公开媒体元数据检查：通过\n"
            f"已检查 Git 索引中的 {result.media_count} 个音视频文件，"
            "未发现未经批准的敏感元数据。"
        )

    lines = [
        "公开媒体元数据检查：未通过",
        f"已检查 Git 索引中的 {result.media_count} 个音视频文件，"
        f"发现 {len(result.issues)} 个阻断项：",
    ]
    for issue in result.issues:
        if issue.code == "metadata:not-approved":
            lines.append(
                f"- {issue.path} [{issue.scope}.{issue.field}]："
                "敏感字段未经批准"
                f"（值摘要 sha256:{_value_fingerprint(issue.value)}）"
                f" [{issue.code}]"
            )
        elif issue.code == "policy:stale-approval":
            lines.append(
                f"- {issue.path} [{issue.scope}.{issue.field}]："
                "批准项与当前媒体不再匹配"
                f"（登记值摘要 sha256:{_value_fingerprint(issue.value)}）"
                f" [{issue.code}]"
            )
        else:
            lines.append(
                f"- {issue.path}：媒体探测失败，{issue.detail} [{issue.code}]"
            )
    lines.append(
        f"如确需保留，请在 {POLICY_PATH} 中按文件、作用域、字段和"
        "精确值登记，并填写可复核证据。"
    )
    return "\n".join(lines)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="检查 Git 索引中的公开音视频元数据"
    )
    parser.add_argument(
        "--repo",
        type=Path,
        default=Path.cwd(),
        help="待检查的 Git 仓库，默认使用当前目录",
    )
    parser.add_argument(
        "--ffprobe",
        default="ffprobe",
        help="ffprobe 可执行文件，默认从 PATH 查找",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        result = audit_repository(args.repo, ffprobe=args.ffprobe)
    except MediaMetadataError as error:
        print(f"公开媒体元数据检查无法执行：{error}", file=sys.stderr)
        return 2
    print(render_result(result))
    return 0 if result.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
