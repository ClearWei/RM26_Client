#!/usr/bin/env python3
"""检查 Git 索引中的运行时资源引用与发行交付契约。"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Mapping, Sequence


POLICY_PATH = "tools/release/runtime_resources.json"
CPP_SUFFIXES = {".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
DELIVERY_KINDS = {"qrc", "install"}


@dataclass(frozen=True)
class Finding:
    code: str
    title: str
    detail: str
    action: str


@dataclass(frozen=True)
class SoundReference:
    asset_path: str
    qrc_path: str | None
    source_path: str


@dataclass(frozen=True)
class AuditReport:
    tracked_file_count: int
    referenced_sound_count: int
    missing_sound_paths: tuple[str, ...]
    optional_missing_sound_paths: tuple[str, ...]
    findings: tuple[Finding, ...]

    @property
    def passed(self) -> bool:
        return not self.findings


class RuntimeResourceError(RuntimeError):
    """索引或策略无法可靠读取时抛出。"""


def git_tracked_files(repo_root: Path) -> tuple[str, ...]:
    completed = subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=repo_root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeResourceError("无法读取 Git 索引")
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
        raise RuntimeResourceError(f"无法读取索引中的 {relative_path}")
    return completed.stdout


def _read_index_text(repo_root: Path, relative_path: str) -> str:
    try:
        return _read_index_blob(repo_root, relative_path).decode("utf-8")
    except UnicodeDecodeError as error:
        raise RuntimeResourceError(
            f"索引中的 {relative_path} 不是 UTF-8 文本"
        ) from error


def _safe_path(value: object, field: str, *, allow_empty: bool = False) -> str:
    if not isinstance(value, str) or (not value and not allow_empty):
        raise RuntimeResourceError(f"{POLICY_PATH} 的 {field} 必须是仓库相对路径")
    if not value:
        return ""
    candidate = PurePosixPath(value)
    if (
        candidate.is_absolute()
        or value.startswith("~")
        or re.match(r"^[A-Za-z]:[/\\]", value)
        or ".." in candidate.parts
        or "\\" in value
    ):
        # 不回显策略值，避免把本机路径带入公开日志。
        raise RuntimeResourceError(f"{POLICY_PATH} 的 {field} 必须是仓库相对路径")
    return value.rstrip("/")


def _string_list(value: object, field: str, *, nonempty: bool = False) -> tuple[str, ...]:
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        raise RuntimeResourceError(f"{POLICY_PATH} 的 {field} 必须是字符串数组")
    if nonempty and not value:
        raise RuntimeResourceError(f"{POLICY_PATH} 的 {field} 不能为空")
    return tuple(value)


def _mapping(value: object, field: str) -> Mapping[str, object]:
    if not isinstance(value, dict):
        raise RuntimeResourceError(f"{POLICY_PATH} 的 {field} 必须是对象")
    return value


def _load_policy(repo_root: Path) -> Mapping[str, object]:
    try:
        payload = json.loads(_read_index_text(repo_root, POLICY_PATH))
    except json.JSONDecodeError as error:
        raise RuntimeResourceError(
            f"{POLICY_PATH} JSON 格式错误（第 {error.lineno} 行）"
        ) from error
    if not isinstance(payload, dict):
        raise RuntimeResourceError(f"{POLICY_PATH} 顶层必须是 JSON 对象")
    if payload.get("schema_version") != 2:
        raise RuntimeResourceError(f"{POLICY_PATH} schema_version 必须为 2")
    return payload


def _unescape_cpp_literal(value: str) -> str:
    return re.sub(r"\\([\\\"])", r"\1", value)


def _source_literals(text: str) -> tuple[tuple[str, int], ...]:
    """按 C++ 词法边界提取字符串，跳过注释、字符常量和原始字符串内部引号。"""

    literals: list[tuple[str, int]] = []
    index = 0
    raw_start = re.compile(r'(?:u8|u|U|L)?R"([^ ()\\\t\r\n]{0,16})\(')
    while index < len(text):
        following = text[index + 1] if index + 1 < len(text) else ""
        if text[index] == "/" and following == "/":
            newline = text.find("\n", index + 2)
            index = len(text) if newline < 0 else newline + 1
            continue
        if text[index] == "/" and following == "*":
            closing = text.find("*/", index + 2)
            index = len(text) if closing < 0 else closing + 2
            continue

        raw_match = raw_start.match(text, index)
        if raw_match:
            delimiter = raw_match.group(1)
            closing_marker = ")" + delimiter + '"'
            closing = text.find(closing_marker, raw_match.end())
            if closing < 0:
                break
            literals.append((text[raw_match.end() : closing], raw_match.end()))
            index = closing + len(closing_marker)
            continue

        if text[index] == "'":
            index += 1
            while index < len(text):
                if text[index] == "\\" and index + 1 < len(text):
                    index += 2
                elif text[index] == "'":
                    index += 1
                    break
                else:
                    index += 1
            continue

        if text[index] != '"':
            index += 1
            continue
        content_start = index + 1
        index = content_start
        chunks: list[str] = []
        while index < len(text):
            if text[index] == "\\" and index + 1 < len(text):
                chunks.append(text[index : index + 2])
                index += 2
            elif text[index] == '"':
                literals.append((_unescape_cpp_literal("".join(chunks)), content_start))
                index += 1
                break
            else:
                chunks.append(text[index])
                index += 1
    return tuple(literals)


def _literal_is_filename_contract(text: str, start: int) -> bool:
    """裸文件名只在实际加载或返回路径的语境中算作资源引用。"""

    prefix = text[max(0, start - 160) : start]
    if re.search(r"\.contains\s*\([^()]*$", prefix):
        return False
    if re.search(
        r"\b(?:playBackgroundMusic|playSecondarySound|playSoundFromResourceFolder|"
        r"tryLoadSound)\s*\([^;\n]*$",
        prefix,
    ):
        return True
    return bool(
        re.search(r"\breturn\s+(?:QStringLiteral|QString::fromUtf8)\s*\([^()]*$", prefix)
    )


def _classify_sound_literal(
    literal: str,
    source_text: str,
    start: int,
    sound_root: str,
    extensions: set[str],
) -> tuple[str, str | None] | None:
    lowered = literal.lower()
    if not any(lowered.endswith(extension) for extension in extensions):
        return None

    qrc_path: str | None = None
    if literal.startswith("qrc:/sounds/"):
        qrc_path = literal[len("qrc:") :]
        name = literal[len("qrc:/sounds/") :]
    elif literal.startswith(":/sounds/"):
        qrc_path = literal[1:]
        name = literal[len(":/sounds/") :]
    elif literal.startswith(sound_root + "/"):
        name = literal[len(sound_root) + 1 :]
    elif "/" not in literal and _literal_is_filename_contract(source_text, start):
        name = literal
    else:
        return None

    if not name or "%" in name or "\\" in name:
        return None
    candidate = PurePosixPath(name)
    if candidate.is_absolute() or ".." in candidate.parts:
        return None
    return f"{sound_root}/{name}", qrc_path


def _dynamic_sound_references(
    repo_root: Path,
    tracked: set[str],
    sound_policy: Mapping[str, object],
    sound_root: str,
    extensions: set[str],
) -> tuple[SoundReference, ...]:
    raw_templates = sound_policy.get("dynamic_templates", [])
    if not isinstance(raw_templates, list):
        raise RuntimeResourceError(
            f"{POLICY_PATH} 的 sounds.dynamic_templates 必须是数组"
        )
    references: list[SoundReference] = []
    for index, raw_template in enumerate(raw_templates):
        field = f"sounds.dynamic_templates[{index}]"
        template = _mapping(raw_template, field)
        source = _safe_path(template.get("source"), f"{field}.source")
        literal = template.get("literal")
        values = _string_list(template.get("values"), f"{field}.values", nonempty=True)
        if source not in tracked:
            raise RuntimeResourceError(f"{POLICY_PATH} 的 {field}.source 未进入 Git 索引")
        if not isinstance(literal, str) or literal.count("%1") != 1:
            raise RuntimeResourceError(f"{POLICY_PATH} 的 {field}.literal 必须包含一个 %1")
        source_text = _read_index_text(repo_root, source)
        if literal not in {item[0] for item in _source_literals(source_text)}:
            raise RuntimeResourceError(f"{POLICY_PATH} 的 {field}.literal 与索引源码不一致")
        for value in values:
            if not re.fullmatch(r"[A-Za-z0-9_-]+", value):
                raise RuntimeResourceError(f"{POLICY_PATH} 的 {field}.values 包含无效值")
            expanded = literal.replace("%1", value)
            classified = _classify_sound_literal(
                expanded, f"return QStringLiteral(\"{expanded}\")", 22, sound_root, extensions
            )
            if classified is None:
                raise RuntimeResourceError(f"{POLICY_PATH} 的 {field}.literal 不是受支持的音效路径")
            asset_path, qrc_path = classified
            references.append(SoundReference(asset_path, qrc_path, source))
    return tuple(references)


def _sound_references(
    repo_root: Path,
    tracked_paths: Sequence[str],
    policy: Mapping[str, object],
) -> tuple[SoundReference, ...]:
    sound_policy = _mapping(policy.get("sounds"), "sounds")
    sound_root = _safe_path(sound_policy.get("root"), "sounds.root")
    extensions = {
        item.lower()
        for item in _string_list(
            sound_policy.get("extensions"), "sounds.extensions", nonempty=True
        )
    }
    if any(not re.fullmatch(r"\.[a-z0-9]+", item) for item in extensions):
        raise RuntimeResourceError(f"{POLICY_PATH} 的 sounds.extensions 值无效")
    roots = tuple(
        _safe_path(item, f"cpp_roots[{index}]")
        for index, item in enumerate(
            _string_list(policy.get("cpp_roots"), "cpp_roots", nonempty=True)
        )
    )

    references: list[SoundReference] = []
    tracked = set(tracked_paths)
    for source_path in tracked_paths:
        candidate = PurePosixPath(source_path)
        if candidate.suffix.lower() not in CPP_SUFFIXES:
            continue
        if not any(source_path == root or source_path.startswith(root + "/") for root in roots):
            continue
        source_text = _read_index_text(repo_root, source_path)
        for literal, literal_start in _source_literals(source_text):
            classified = _classify_sound_literal(
                literal, source_text, literal_start, sound_root, extensions
            )
            if classified is None:
                continue
            asset_path, qrc_path = classified
            references.append(SoundReference(asset_path, qrc_path, source_path))

    references.extend(
        _dynamic_sound_references(
            repo_root, tracked, sound_policy, sound_root, extensions
        )
    )
    return tuple(references)


def _join_qrc_path(prefix: str, alias: str) -> str:
    parts = [part.strip("/") for part in (prefix, alias) if part.strip("/")]
    return "/" + "/".join(parts)


def _qrc_aliases(repo_root: Path, qrc_path: str) -> Mapping[str, str]:
    try:
        root = ET.fromstring(_read_index_text(repo_root, qrc_path))
    except ET.ParseError as error:
        raise RuntimeResourceError(f"索引中的 {qrc_path} 不是有效的 Qt 资源清单") from error
    aliases: dict[str, str] = {}
    for resource in root.findall("qresource"):
        prefix = resource.get("prefix", "/")
        for file_node in resource.findall("file"):
            source = (file_node.text or "").strip()
            if not source:
                continue
            alias = file_node.get("alias", source)
            aliases[_join_qrc_path(prefix, alias)] = source
    return aliases


def _summarize_paths(paths: Sequence[str], limit: int = 12) -> str:
    shown = "、".join(paths[:limit])
    remaining = len(paths) - min(len(paths), limit)
    return f"{shown}（另有 {remaining} 项）" if remaining else shown


def _optional_sound_contract(
    repo_root: Path,
    tracked: set[str],
    sound_policy: Mapping[str, object],
) -> tuple[str, ...]:
    optional = _mapping(sound_policy.get("optional"), "sounds.optional")
    paths = tuple(
        _safe_path(item, f"sounds.optional.paths[{index}]")
        for index, item in enumerate(
            _string_list(optional.get("paths"), "sounds.optional.paths")
        )
    )
    if len(paths) != len(set(paths)):
        raise RuntimeResourceError(f"{POLICY_PATH} 的 sounds.optional.paths 不能重复")

    sound_root = _safe_path(sound_policy.get("root"), "sounds.root")
    extensions = {
        item.lower()
        for item in _string_list(
            sound_policy.get("extensions"), "sounds.extensions", nonempty=True
        )
    }
    for path in paths:
        if not path.startswith(sound_root + "/") or PurePosixPath(path).suffix.lower() not in extensions:
            raise RuntimeResourceError(
                f"{POLICY_PATH} 的 sounds.optional.paths 必须位于音效目录且扩展名受支持"
            )

    evidence = _safe_path(
        optional.get("evidence"),
        "sounds.optional.evidence",
        allow_empty=not paths,
    )
    if paths and evidence not in tracked:
        raise RuntimeResourceError(
            f"{POLICY_PATH} 的 sounds.optional.evidence 未进入 Git 索引"
        )
    return paths


def _sound_findings(
    repo_root: Path,
    tracked: set[str],
    policy: Mapping[str, object],
    references: Sequence[SoundReference],
) -> tuple[tuple[str, ...], tuple[str, ...], list[Finding]]:
    sound_policy = _mapping(policy.get("sounds"), "sounds")
    optional_paths = set(_optional_sound_contract(repo_root, tracked, sound_policy))
    referenced_paths = {ref.asset_path for ref in references}
    stale_optional = tuple(sorted(optional_paths - referenced_paths))

    all_missing = {ref.asset_path for ref in references if ref.asset_path not in tracked}
    missing = tuple(sorted(all_missing - optional_paths))
    optional_missing = tuple(sorted(all_missing & optional_paths))
    findings: list[Finding] = []
    if stale_optional:
        findings.append(
            Finding(
                code="sound:stale-optional-contract",
                title="可选音效登记与源码引用不一致",
                detail=_summarize_paths(stale_optional),
                action="删除失效登记，或恢复对应的可选播放入口和公开说明。",
            )
        )
    if missing:
        findings.append(
            Finding(
                code="sound:missing-files",
                title=f"代码引用了 {len(missing)} 个索引中不存在的音效",
                detail=_summarize_paths(missing),
                action="补齐允许再分发的音效文件，或删除已经失效的播放路径；不要依赖开发机目录中的未跟踪文件。",
            )
        )

    qrc_manifest = _safe_path(sound_policy.get("qrc_manifest"), "sounds.qrc_manifest")
    aliases = _qrc_aliases(repo_root, qrc_manifest) if qrc_manifest in tracked else {}
    qrc_missing = tuple(
        sorted(
            {
                ref.qrc_path
                for ref in references
                if ref.qrc_path is not None
                and ref.asset_path in tracked
                and (ref.qrc_path not in aliases or aliases[ref.qrc_path] != ref.asset_path)
            }
        )
    )
    if qrc_missing:
        findings.append(
            Finding(
                code="sound:qrc-aliases",
                title=f"{len(qrc_missing)} 个音效没有可用的 QRC alias",
                detail=_summarize_paths(qrc_missing),
                action=f"在 {qrc_manifest} 中把这些 URL 显式映射到对应的索引文件。",
            )
        )
    return missing, optional_missing, findings


def _cmake_install_commands(text: str) -> tuple[str, ...]:
    stripped = re.sub(r"(?m)#.*$", "", text)
    return tuple(match.group(1) for match in re.finditer(r"(?is)\binstall\s*\((.*?)\)", stripped))


def _has_install_delivery(commands: Sequence[str], source_root: str, assets: Sequence[str]) -> bool:
    normalized_root = source_root.rstrip("/")
    for command in commands:
        if re.search(r"(?i)\bDIRECTORY\b", command) and re.search(
            rf"(?<![A-Za-z0-9_./-])[\"']?{re.escape(normalized_root)}/?[\"']?(?![A-Za-z0-9_./-])",
            command,
        ):
            return True
    installed_text = "\n".join(commands)
    return bool(assets) and all(asset in installed_text for asset in assets)


def _dynamic_findings(
    repo_root: Path,
    tracked: set[str],
    policy: Mapping[str, object],
) -> list[Finding]:
    raw_contracts = policy.get("dynamic_resources", [])
    if not isinstance(raw_contracts, list):
        raise RuntimeResourceError(f"{POLICY_PATH} 的 dynamic_resources 必须是数组")
    findings: list[Finding] = []
    qrc_cache: dict[str, Mapping[str, str]] = {}
    for index, raw_contract in enumerate(raw_contracts):
        field = f"dynamic_resources[{index}]"
        contract = _mapping(raw_contract, field)
        contract_id = contract.get("id")
        if not isinstance(contract_id, str) or not re.fullmatch(r"[a-z0-9][a-z0-9-]*", contract_id):
            raise RuntimeResourceError(f"{POLICY_PATH} 的 {field}.id 值无效")
        source = _safe_path(contract.get("source"), f"{field}.source")
        source_root = _safe_path(contract.get("source_root"), f"{field}.source_root")
        raw_variants = _string_list(
            contract.get("variants"), f"{field}.variants", nonempty=True
        )
        variants = tuple(
            _safe_path(item, f"{field}.variants[{variant_index}]")
            for variant_index, item in enumerate(raw_variants)
        )
        if any("/" in variant for variant in variants):
            raise RuntimeResourceError(f"{POLICY_PATH} 的 {field}.variants 只能包含一级目录名")
        extensions = {
            item.lower()
            for item in _string_list(contract.get("extensions"), f"{field}.extensions", nonempty=True)
        }
        deliveries = set(
            _string_list(
                contract.get("required_delivery"), f"{field}.required_delivery", nonempty=True
            )
        )
        if not deliveries.issubset(DELIVERY_KINDS):
            raise RuntimeResourceError(f"{POLICY_PATH} 的 {field}.required_delivery 值无效")

        source_text = _read_index_text(repo_root, source) if source in tracked else ""
        source_literals = {literal for literal, _ in _source_literals(source_text)}
        source_root_literal = "/" + source_root.strip("/") + "/"
        has_source_root = any(
            literal == source_root.rstrip("/")
            or literal == source_root.rstrip("/") + "/"
            or literal.endswith(source_root_literal)
            for literal in source_literals
        )
        has_variants = all(variant in source_literals for variant in variants)
        has_qrc_root = True
        if "qrc" in deliveries:
            qrc_prefix = contract.get("qrc_prefix")
            has_qrc_root = isinstance(qrc_prefix, str) and any(
                literal.rstrip("/") in {qrc_prefix.rstrip("/"), ":" + qrc_prefix.rstrip("/")}
                for literal in source_literals
            )
        if source not in tracked or not (has_source_root and has_variants and has_qrc_root):
            findings.append(
                Finding(
                    code=f"dynamic:{contract_id}:source-contract",
                    title="动态资源策略与索引源码不一致",
                    detail="策略声明的文件系统根、QRC 根或变体没有全部出现在真实字符串字面量中。",
                    action=f"核对 {source} 的动态路径逻辑，并同步 {POLICY_PATH}。",
                )
            )

        assets = tuple(
            sorted(
                path
                for path in tracked
                if any(path.startswith(f"{source_root}/{variant}/") for variant in variants)
                and PurePosixPath(path).suffix.lower() in extensions
            )
        )
        missing_variants = tuple(
            variant
            for variant in variants
            if not any(path.startswith(f"{source_root}/{variant}/") for path in assets)
        )
        if missing_variants:
            findings.append(
                Finding(
                    code=f"dynamic:{contract_id}:source-files",
                    title="动态资源变体缺少索引文件",
                    detail=f"缺少 {_summarize_paths(missing_variants)}。",
                    action=f"补齐 {source_root} 下的资源，或从源码和策略中移除失效变体。",
                )
            )

        if "qrc" in deliveries:
            qrc_manifest = _safe_path(contract.get("qrc_manifest"), f"{field}.qrc_manifest")
            qrc_prefix = contract.get("qrc_prefix")
            if not isinstance(qrc_prefix, str) or not qrc_prefix.startswith("/") or ".." in qrc_prefix:
                raise RuntimeResourceError(f"{POLICY_PATH} 的 {field}.qrc_prefix 值无效")
            if qrc_manifest not in tracked:
                aliases: Mapping[str, str] = {}
            else:
                aliases = qrc_cache.setdefault(qrc_manifest, _qrc_aliases(repo_root, qrc_manifest))
            missing_qrc = []
            for asset in assets:
                relative = asset[len(source_root) + 1 :]
                expected_alias = _join_qrc_path(qrc_prefix, relative)
                if aliases.get(expected_alias) != asset:
                    missing_qrc.append(expected_alias)
            if missing_qrc:
                findings.append(
                    Finding(
                        code=f"dynamic:{contract_id}:qrc-delivery",
                        title=f"动态资源有 {len(missing_qrc)} 项未按约定进入 QRC",
                        detail="源码目录中的文件不会自动成为 QRC 资源。",
                        action=f"在 {qrc_manifest} 中为 {source_root} 建立与 {qrc_prefix} 一致的显式 alias。",
                    )
                )

        if "install" in deliveries:
            manifest = _safe_path(contract.get("install_manifest"), f"{field}.install_manifest")
            commands = (
                _cmake_install_commands(_read_index_text(repo_root, manifest))
                if manifest in tracked
                else ()
            )
            if not _has_install_delivery(commands, source_root, assets):
                findings.append(
                    Finding(
                        code=f"dynamic:{contract_id}:install-delivery",
                        title="动态资源没有安装树交付规则",
                        detail="源码目录和当前工作目录兜底不能证明安装包包含这些资源。",
                        action=f"在 {manifest} 中为 {source_root} 增加可复核的 install() 规则，并从源码树外验证。",
                    )
                )
    return findings


def audit_repository(repo_root: Path) -> AuditReport:
    tracked_paths = git_tracked_files(repo_root)
    tracked = set(tracked_paths)
    policy = _load_policy(repo_root)
    references = _sound_references(repo_root, tracked_paths, policy)

    deduplicated: dict[tuple[str, str | None], SoundReference] = {}
    for reference in references:
        deduplicated.setdefault((reference.asset_path, reference.qrc_path), reference)
    unique_references = tuple(deduplicated.values())

    missing, optional_missing, findings = _sound_findings(
        repo_root, tracked, policy, unique_references
    )
    findings.extend(_dynamic_findings(repo_root, tracked, policy))
    return AuditReport(
        tracked_file_count=len(tracked_paths),
        referenced_sound_count=len({item.asset_path for item in unique_references}),
        missing_sound_paths=missing,
        optional_missing_sound_paths=optional_missing,
        findings=tuple(findings),
    )


def render_report(report: AuditReport) -> str:
    lines = [
        "运行时资源检查结果",
        f"- Git 索引文件：{report.tracked_file_count}",
        f"- 代码引用音效：{report.referenced_sound_count}",
        f"- 缺失必需音效：{len(report.missing_sound_paths)}",
        f"- 未随源码提供的可选音效：{len(report.optional_missing_sound_paths)}",
    ]
    if report.passed:
        lines.append("结论：通过，索引中的资源引用与声明的交付契约一致。")
        return "\n".join(lines)

    lines.append(f"结论：未通过，共 {len(report.findings)} 项需要处理。")
    for index, finding in enumerate(report.findings, start=1):
        lines.extend(
            [
                f"{index}. [{finding.code}] {finding.title}",
                f"   事实：{finding.detail}",
                f"   建议：{finding.action}",
            ]
        )
    return "\n".join(lines)


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="待检查的 Git 仓库根目录",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _build_parser().parse_args(argv)
    try:
        report = audit_repository(args.repo_root.resolve())
    except RuntimeResourceError as error:
        print(f"运行时资源检查无法完成：{error}", file=sys.stderr)
        return 2
    print(render_report(report))
    return 0 if report.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
