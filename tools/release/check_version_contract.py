#!/usr/bin/env python3
"""核对 Git 索引中的应用、模拟器、协议和可执行名称版本契约。"""

from __future__ import annotations

import argparse
import json
import plistlib
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Callable, Mapping, Sequence


POLICY_PATH = "tools/release/version_policy.json"

APPLICATION_SOURCE_LABELS = {
    "cmake_project": "CMake 工程",
    "example_config": "公开示例配置",
    "runtime_metadata": "Qt 运行时元数据",
    "config_fallback": "配置读取兜底",
    "docker_image": "Docker 镜像元数据",
    "macos_bundle": "macOS 应用包",
}
SIMULATOR_SOURCE_LABELS = {"pyproject": "模拟器 pyproject"}
PROTOCOL_SOURCE_LABELS = {"manifest": "协议清单"}
EXECUTABLE_SOURCE_LABELS = {
    "cmake_targets": "CMake 可执行目标",
    "docker_entrypoint": "Docker 启动入口",
    "macos_bundle": "macOS 可执行入口",
}

APPLICATION_STRATEGIES = {"undecided", "single-source"}
SIMULATOR_STRATEGIES = {"undecided", "independent", "shared"}
EXECUTABLE_STRATEGIES = {"undecided", "legacy-compatible", "canonical-only"}

# 兼容项目现有的三段 SemVer 和官方客户端风格的四段构建号。
VERSION_RE = re.compile(
    r"^(?:0|[1-9]\d*)(?:\.(?:0|[1-9]\d*)){2,3}"
    r"(?:-[0-9A-Za-z](?:[0-9A-Za-z.-]*[0-9A-Za-z])?)?"
    r"(?:\+[0-9A-Za-z](?:[0-9A-Za-z.-]*[0-9A-Za-z])?)?$"
)
TARGET_NAME_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_.+-]{0,127}$")
WINDOWS_ABSOLUTE_RE = re.compile(r"^[A-Za-z]:[\\/]")


@dataclass(frozen=True)
class VersionSource:
    category: str
    key: str
    label: str
    version: str


@dataclass(frozen=True)
class NameSource:
    key: str
    label: str
    names: tuple[str, ...]


@dataclass(frozen=True)
class Finding:
    code: str
    title: str
    detail: str
    action: str


@dataclass(frozen=True)
class AuditReport:
    tracked_file_count: int
    application_strategy: str | None
    simulator_strategy: str | None
    executable_strategy: str | None
    version_sources: tuple[VersionSource, ...]
    name_sources: tuple[NameSource, ...]
    findings: tuple[Finding, ...]

    @property
    def passed(self) -> bool:
        return not self.findings


class VersionContractError(RuntimeError):
    """版本策略或 Git 索引无法被可靠读取。"""


def git_tracked_files(repo_root: Path) -> tuple[str, ...]:
    completed = subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=repo_root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        raise VersionContractError("无法读取 Git 索引")
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
        raise VersionContractError("无法读取索引中的版本来源")
    return completed.stdout


def _read_index_text(repo_root: Path, relative_path: str) -> str:
    try:
        return _read_index_blob(repo_root, relative_path).decode("utf-8")
    except UnicodeDecodeError as error:
        raise VersionContractError("索引中的版本来源不是 UTF-8 文本") from error


def _mapping(parent: Mapping[str, object], key: str, field: str) -> Mapping[str, object]:
    value = parent.get(key)
    if not isinstance(value, dict):
        raise VersionContractError(f"{POLICY_PATH} 的 {field} 必须是对象")
    return value


def _string(parent: Mapping[str, object], key: str, field: str) -> str:
    value = parent.get(key)
    if not isinstance(value, str):
        raise VersionContractError(f"{POLICY_PATH} 的 {field} 必须是字符串")
    return value


def _safe_policy_path(value: object, field: str) -> str:
    if not isinstance(value, str) or not value:
        raise VersionContractError(f"{POLICY_PATH} 的 {field} 必须是仓库相对路径")
    candidate = PurePosixPath(value)
    if (
        candidate.is_absolute()
        or value.startswith("~")
        or WINDOWS_ABSOLUTE_RE.match(value)
        or "\\" in value
        or ".." in candidate.parts
    ):
        # 不回显策略中的原始值，避免本机目录出现在公开 CI 日志中。
        raise VersionContractError(f"{POLICY_PATH} 的 {field} 必须是仓库相对路径")
    return value


def _source_paths(
    section: Mapping[str, object],
    section_name: str,
    labels: Mapping[str, str],
) -> dict[str, str]:
    raw_sources = _mapping(section, "sources", f"{section_name}.sources")
    paths: dict[str, str] = {}
    for key in labels:
        paths[key] = _safe_policy_path(
            raw_sources.get(key), f"{section_name}.sources.{key}"
        )
    unknown = set(raw_sources) - set(labels)
    if unknown:
        raise VersionContractError(
            f"{POLICY_PATH} 的 {section_name}.sources 包含未支持的来源类型"
        )
    return paths


def _valid_version(value: str) -> bool:
    return bool(VERSION_RE.fullmatch(value))


def _validate_declared_version(value: str, field: str, *, required: bool) -> None:
    if required and not value:
        raise VersionContractError(f"{POLICY_PATH} 的 {field} 必须填写版本号")
    if value and not _valid_version(value):
        raise VersionContractError(f"{POLICY_PATH} 的 {field} 版本格式无效")


def _safe_target_name(value: object, field: str) -> str:
    if not isinstance(value, str) or not TARGET_NAME_RE.fullmatch(value):
        # 名称也可能来自私有构建约定，格式错误时只报告字段名。
        raise VersionContractError(f"{POLICY_PATH} 的 {field} 目标名称格式无效")
    return value


def _load_policy(repo_root: Path) -> dict[str, object]:
    try:
        payload = json.loads(_read_index_text(repo_root, POLICY_PATH))
    except json.JSONDecodeError as error:
        raise VersionContractError(
            f"{POLICY_PATH} JSON 格式错误（第 {error.lineno} 行）"
        ) from error
    if not isinstance(payload, dict):
        raise VersionContractError(f"{POLICY_PATH} 顶层必须是对象")
    if payload.get("schema_version") != 1:
        raise VersionContractError(f"{POLICY_PATH} schema_version 必须为 1")

    application = _mapping(payload, "application", "application")
    simulator = _mapping(payload, "simulator", "simulator")
    protocol = _mapping(payload, "protocol", "protocol")
    executable = _mapping(payload, "executable", "executable")

    app_strategy = _string(application, "strategy", "application.strategy")
    sim_strategy = _string(simulator, "strategy", "simulator.strategy")
    protocol_strategy = _string(protocol, "strategy", "protocol.strategy")
    executable_strategy = _string(executable, "strategy", "executable.strategy")
    if app_strategy not in APPLICATION_STRATEGIES:
        raise VersionContractError(
            f"{POLICY_PATH} 的 application.strategy 值无效"
        )
    if sim_strategy not in SIMULATOR_STRATEGIES:
        raise VersionContractError(f"{POLICY_PATH} 的 simulator.strategy 值无效")
    if protocol_strategy != "independent":
        raise VersionContractError(
            f"{POLICY_PATH} 的 protocol.strategy 必须为 independent；协议版本不得复用应用版本策略"
        )
    if executable_strategy not in EXECUTABLE_STRATEGIES:
        raise VersionContractError(
            f"{POLICY_PATH} 的 executable.strategy 值无效"
        )

    app_declared = _string(
        application, "declared_version", "application.declared_version"
    )
    sim_declared = _string(
        simulator, "declared_version", "simulator.declared_version"
    )
    _validate_declared_version(
        app_declared,
        "application.declared_version",
        required=app_strategy == "single-source",
    )
    _validate_declared_version(
        sim_declared,
        "simulator.declared_version",
        required=sim_strategy == "independent",
    )
    if sim_strategy == "shared" and sim_declared:
        raise VersionContractError(
            f"{POLICY_PATH} 的 simulator.declared_version 在 shared 策略下必须留空"
        )

    canonical_name = _safe_target_name(
        executable.get("canonical_name"), "executable.canonical_name"
    )
    raw_legacy_names = executable.get("legacy_names")
    if not isinstance(raw_legacy_names, list):
        raise VersionContractError(
            f"{POLICY_PATH} 的 executable.legacy_names 必须是数组"
        )
    legacy_names = [
        _safe_target_name(value, f"executable.legacy_names[{index}]")
        for index, value in enumerate(raw_legacy_names)
    ]
    if len(set(legacy_names)) != len(legacy_names) or canonical_name in legacy_names:
        raise VersionContractError(
            f"{POLICY_PATH} 的 executable 名称存在重复或分类冲突"
        )

    compatibility_through = _string(
        executable,
        "legacy_compatibility_through",
        "executable.legacy_compatibility_through",
    )
    _validate_declared_version(
        compatibility_through,
        "executable.legacy_compatibility_through",
        required=executable_strategy == "legacy-compatible",
    )

    normalized = dict(payload)
    normalized["application"] = {
        **application,
        "sources": _source_paths(
            application, "application", APPLICATION_SOURCE_LABELS
        ),
    }
    normalized["simulator"] = {
        **simulator,
        "sources": _source_paths(simulator, "simulator", SIMULATOR_SOURCE_LABELS),
    }
    normalized["protocol"] = {
        **protocol,
        "sources": _source_paths(protocol, "protocol", PROTOCOL_SOURCE_LABELS),
    }
    normalized["executable"] = {
        **executable,
        "canonical_name": canonical_name,
        "legacy_names": legacy_names,
        "sources": _source_paths(
            executable, "executable", EXECUTABLE_SOURCE_LABELS
        ),
    }
    return normalized


def _strip_c_family_comments(text: str) -> str:
    """去掉 C/C++ 注释，同时保留字符串中的注释符号。"""

    output: list[str] = []
    index = 0
    quote = ""
    while index < len(text):
        char = text[index]
        following = text[index + 1] if index + 1 < len(text) else ""
        if quote:
            output.append(char)
            if char == "\\" and following:
                output.append(following)
                index += 2
                continue
            if char == quote:
                quote = ""
            index += 1
            continue
        if char in {'"', "'"}:
            quote = char
            output.append(char)
            index += 1
            continue
        if char == "/" and following == "/":
            newline = text.find("\n", index + 2)
            if newline == -1:
                break
            output.append("\n")
            index = newline + 1
            continue
        if char == "/" and following == "*":
            end = text.find("*/", index + 2)
            if end == -1:
                break
            output.extend("\n" for item in text[index : end + 2] if item == "\n")
            index = end + 2
            continue
        output.append(char)
        index += 1
    return "".join(output)


def _strip_cmake_comments(text: str) -> str:
    lines: list[str] = []
    for line in text.splitlines():
        quote = ""
        escaped = False
        kept: list[str] = []
        for char in line:
            if escaped:
                kept.append(char)
                escaped = False
                continue
            if char == "\\" and quote:
                kept.append(char)
                escaped = True
                continue
            if char in {'"', "'"}:
                quote = "" if quote == char else char if not quote else quote
                kept.append(char)
                continue
            if char == "#" and not quote:
                break
            kept.append(char)
        lines.append("".join(kept))
    return "\n".join(lines)


def _extract_cmake_project(text: str) -> tuple[str, str]:
    cleaned = _strip_cmake_comments(text)
    match = re.search(
        r"\bproject\s*\(\s*([A-Za-z][A-Za-z0-9_.+-]*)"
        r"(?:(?!\)\s*(?:\n|$)).)*?\bVERSION\s+([^\s\)]+)",
        cleaned,
        flags=re.IGNORECASE | re.DOTALL,
    )
    if match is None:
        raise ValueError("没有找到带 VERSION 的 project()")
    return match.group(1), match.group(2).strip('"\'')


def _extract_example_config(text: str) -> str:
    payload = json.loads(text)
    if not isinstance(payload, dict):
        raise ValueError("顶层不是对象")
    app_settings = payload.get("app_settings")
    if not isinstance(app_settings, dict) or not isinstance(
        app_settings.get("version"), str
    ):
        raise ValueError("缺少 app_settings.version")
    return str(app_settings["version"])


def _extract_runtime_metadata(text: str) -> str:
    cleaned = _strip_c_family_comments(text)
    match = re.search(
        r"\bsetApplicationVersion\s*\(\s*(?:QStringLiteral\s*\(\s*)?"
        r'"([^"\r\n]+)"',
        cleaned,
    )
    if match is None:
        raise ValueError("没有找到 setApplicationVersion()")
    return match.group(1)


def _extract_config_fallback(text: str) -> str:
    cleaned = _strip_c_family_comments(text)
    function = re.search(
        r"\bQString\s+ConfigManager::getAppVersion\s*\(\s*\)\s*const\s*"
        r"\{(?P<body>.*?)\n\}",
        cleaned,
        flags=re.DOTALL,
    )
    if function is None:
        raise ValueError("没有找到 getAppVersion()")
    match = re.search(r'\.toString\s*\(\s*"([^"\r\n]+)"\s*\)', function.group("body"))
    if match is None:
        raise ValueError("没有找到配置版本兜底")
    return match.group(1)


def _extract_docker_image_version(text: str) -> str:
    """读取 OCI 镜像版本；兼容早期内嵌配置的历史测试仓库。"""

    label_match = re.search(
        r"org\.opencontainers\.image\.version\s*=\s*[\"']([^\"'\r\n]+)[\"']",
        text,
        flags=re.IGNORECASE,
    )
    if label_match is not None:
        return label_match.group(1)

    match = re.search(
        r'"app_settings"\s*:\s*\{(?:(?!\}\s*,).)*?'
        r'"version"\s*:\s*"([^"\r\n]+)"',
        text,
        flags=re.DOTALL,
    )
    if match is None:
        raise ValueError("没有找到 Docker 镜像版本")
    return match.group(1)


def _extract_macos_versions(blob: bytes) -> tuple[tuple[str, str], ...]:
    payload = plistlib.loads(blob)
    if not isinstance(payload, dict):
        raise ValueError("plist 顶层不是对象")
    values: list[tuple[str, str]] = []
    for key, label in (
        ("CFBundleShortVersionString", "macOS 短版本"),
        ("CFBundleVersion", "macOS 构建版本"),
    ):
        value = payload.get(key)
        if not isinstance(value, str):
            raise ValueError(f"缺少 {key}")
        values.append((label, value))
    return tuple(values)


def _extract_pyproject_version(text: str) -> str:
    match = re.search(
        r"(?ms)^\s*\[project\]\s*$"
        r"(?P<body>.*?)(?=^\s*\[[^\]\r\n]+\]\s*$|\Z)",
        text,
    )
    if match is None:
        raise ValueError("缺少 [project]")
    version = re.search(
        r'(?m)^\s*version\s*=\s*["\']([^"\'\r\n]+)["\']\s*(?:#.*)?$',
        match.group("body"),
    )
    if version is None:
        raise ValueError("缺少 project.version")
    return version.group(1)


def _extract_protocol_manifest(text: str) -> str:
    payload = json.loads(text)
    if not isinstance(payload, dict):
        raise ValueError("顶层不是对象")
    protocol = payload.get("protocol")
    if not isinstance(protocol, dict) or not isinstance(protocol.get("version"), str):
        raise ValueError("缺少 protocol.version")
    return str(protocol["version"])


def _extract_cmake_targets(text: str) -> tuple[str, ...]:
    cleaned = _strip_cmake_comments(text)
    names = re.findall(
        r"\badd_executable\s*\(\s*([A-Za-z][A-Za-z0-9_.+-]*)",
        cleaned,
        flags=re.IGNORECASE,
    )
    return tuple(dict.fromkeys(names))


def _extract_docker_entrypoint(text: str) -> tuple[str, ...]:
    cleaned = _strip_c_family_comments(text)
    match = re.search(r"(?m)^\s*exec\s+([^\s\"']+)", cleaned)
    if match is None:
        return ()
    return (PurePosixPath(match.group(1)).name,)


def _extract_macos_executable(blob: bytes) -> tuple[str, ...]:
    payload = plistlib.loads(blob)
    if not isinstance(payload, dict):
        raise ValueError("plist 顶层不是对象")
    value = payload.get("CFBundleExecutable")
    return (value,) if isinstance(value, str) else ()


def _source_missing_finding(category: str, key: str, label: str) -> Finding:
    return Finding(
        code=f"source:untracked:{category}:{key}",
        title=f"{label}未进入发布快照",
        detail="策略声明的来源不在 Git 索引中，无法参与版本契约复核。",
        action="将该来源纳入 Git 索引，或由维护者修订机器可读策略中的来源映射。",
    )


def _source_extract_finding(category: str, key: str, label: str) -> Finding:
    return Finding(
        code=f"source:extract-failed:{category}:{key}",
        title=f"无法提取{label}版本",
        detail="来源格式与受支持的公开版本契约不一致；原始内容未写入日志。",
        action="恢复该来源的标准版本声明，或先调整检查器和策略再发布。",
    )


def _invalid_version_finding(source: VersionSource) -> Finding:
    return Finding(
        code=f"source:invalid-version:{source.category}:{source.key}",
        title=f"{source.label}版本格式无效",
        detail="提取值不是受支持的三段或四段版本号；原始值未写入日志。",
        action="改为明确的三段 SemVer，或保留三段版本并追加一段数字构建号。",
    )


def _collect_single_version_source(
    *,
    repo_root: Path,
    tracked: set[str],
    category: str,
    key: str,
    label: str,
    path: str,
    extractor: Callable[[str], str],
    findings: list[Finding],
) -> VersionSource | None:
    if path not in tracked:
        findings.append(_source_missing_finding(category, key, label))
        return None
    try:
        version = extractor(_read_index_text(repo_root, path))
    except (ValueError, TypeError, json.JSONDecodeError):
        findings.append(_source_extract_finding(category, key, label))
        return None
    source = VersionSource(category, key, label, version)
    if not _valid_version(version):
        findings.append(_invalid_version_finding(source))
        return None
    return source


def _collect_application_sources(
    repo_root: Path,
    tracked: set[str],
    paths: Mapping[str, str],
    findings: list[Finding],
) -> list[VersionSource]:
    extractors: dict[str, Callable[[str], str]] = {
        "cmake_project": lambda text: _extract_cmake_project(text)[1],
        "example_config": _extract_example_config,
        "runtime_metadata": _extract_runtime_metadata,
        "config_fallback": _extract_config_fallback,
        "docker_image": _extract_docker_image_version,
    }
    sources: list[VersionSource] = []
    for key, extractor in extractors.items():
        source = _collect_single_version_source(
            repo_root=repo_root,
            tracked=tracked,
            category="application",
            key=key,
            label=APPLICATION_SOURCE_LABELS[key],
            path=paths[key],
            extractor=extractor,
            findings=findings,
        )
        if source is not None:
            sources.append(source)

    bundle_path = paths["macos_bundle"]
    if bundle_path not in tracked:
        findings.append(
            _source_missing_finding(
                "application",
                "macos_bundle",
                APPLICATION_SOURCE_LABELS["macos_bundle"],
            )
        )
    else:
        try:
            bundle_versions = _extract_macos_versions(
                _read_index_blob(repo_root, bundle_path)
            )
        except (ValueError, TypeError, plistlib.InvalidFileException):
            findings.append(
                _source_extract_finding(
                    "application",
                    "macos_bundle",
                    APPLICATION_SOURCE_LABELS["macos_bundle"],
                )
            )
        else:
            for index, (label, version) in enumerate(bundle_versions):
                source = VersionSource(
                    "application", f"macos_bundle.{index}", label, version
                )
                if _valid_version(version):
                    sources.append(source)
                else:
                    findings.append(_invalid_version_finding(source))
    return sources


def _collect_name_source(
    *,
    repo_root: Path,
    tracked: set[str],
    key: str,
    label: str,
    path: str,
    extractor: Callable[[bytes], tuple[str, ...]],
    findings: list[Finding],
) -> NameSource | None:
    if path not in tracked:
        findings.append(_source_missing_finding("executable", key, label))
        return None
    try:
        raw_names = extractor(_read_index_blob(repo_root, path))
    except (UnicodeDecodeError, ValueError, TypeError, plistlib.InvalidFileException):
        findings.append(_source_extract_finding("executable", key, label))
        return None
    if not raw_names:
        findings.append(
            Finding(
                code=f"source:name-missing:{key}",
                title=f"{label}没有可识别的可执行名称",
                detail="来源存在，但没有提取到可复核的目标名称。",
                action="补充标准的可执行目标或入口声明。",
            )
        )
        return None
    if any(not TARGET_NAME_RE.fullmatch(name) for name in raw_names):
        findings.append(
            Finding(
                code=f"source:invalid-name:{key}",
                title=f"{label}包含非法目标名称",
                detail="名称未回显，避免把非公开构建信息写入日志。",
                action="将目标名改为可移植的字母、数字、点、下划线、加号或连字符组合。",
            )
        )
        return None
    return NameSource(key, label, tuple(dict.fromkeys(raw_names)))


def _group_versions(sources: Sequence[VersionSource]) -> dict[str, list[str]]:
    groups: dict[str, list[str]] = {}
    for source in sources:
        groups.setdefault(source.version, []).append(source.label)
    return groups


def _version_group_summary(groups: Mapping[str, Sequence[str]]) -> str:
    return "；".join(
        f"{version}（{'、'.join(labels)}）"
        for version, labels in sorted(groups.items())
    )


def _resolved_application_version(
    strategy: str, declared: str, sources: Sequence[VersionSource]
) -> str | None:
    if strategy == "single-source":
        return declared
    versions = {source.version for source in sources}
    return next(iter(versions)) if len(versions) == 1 else None


def _numeric_version(value: str) -> tuple[int, ...]:
    core = value.split("+", 1)[0].split("-", 1)[0]
    parts = tuple(int(item) for item in core.split("."))
    return parts + (0,) * (4 - len(parts))


def _strategy_findings(
    policy: Mapping[str, object],
    version_sources: Sequence[VersionSource],
    name_sources: Sequence[NameSource],
) -> list[Finding]:
    findings: list[Finding] = []
    application = _mapping(policy, "application", "application")
    simulator = _mapping(policy, "simulator", "simulator")
    executable = _mapping(policy, "executable", "executable")
    app_strategy = str(application["strategy"])
    sim_strategy = str(simulator["strategy"])
    executable_strategy = str(executable["strategy"])
    app_declared = str(application["declared_version"])
    sim_declared = str(simulator["declared_version"])

    app_sources = [
        source for source in version_sources if source.category == "application"
    ]
    app_groups = _group_versions(app_sources)
    if len(app_groups) > 1:
        findings.append(
            Finding(
                code="application:version-drift",
                title="应用版本来源发生漂移",
                detail=f"仅汇总标准版本号和来源标签：{_version_group_summary(app_groups)}。",
                action="先确定应用版本的唯一来源，再同步 CMake、示例配置、运行时兜底、Docker 镜像和应用包元数据。",
            )
        )
    if app_strategy == "undecided":
        findings.append(
            Finding(
                code="application:strategy-undecided",
                title="应用版本策略尚未决定",
                detail="策略没有声明公开发行版本的唯一真值。",
                action="由维护者确认首个公开应用版本，并将 strategy 改为 single-source。",
            )
        )
    else:
        mismatches = [
            source.label for source in app_sources if source.version != app_declared
        ]
        if mismatches:
            findings.append(
                Finding(
                    code="application:declared-mismatch",
                    title="应用来源未同步声明版本",
                    detail=f"未同步来源：{'、'.join(mismatches)}。",
                    action=f"将这些来源同步到策略声明的 {app_declared}，再生成公开发行快照。",
                )
            )

    sim_sources = [
        source for source in version_sources if source.category == "simulator"
    ]
    if sim_strategy == "undecided":
        findings.append(
            Finding(
                code="simulator:strategy-undecided",
                title="模拟器版本策略尚未决定",
                detail="尚未确认模拟器与桌面应用共享版本，还是独立演进。",
                action="由维护者选择 shared 或 independent，并同步 pyproject 版本。",
            )
        )
    elif sim_sources:
        simulator_version = sim_sources[0].version
        if sim_strategy == "independent" and simulator_version != sim_declared:
            findings.append(
                Finding(
                    code="simulator:declared-mismatch",
                    title="模拟器版本未同步独立版本声明",
                    detail="模拟器 pyproject 与策略声明不一致。",
                    action=f"将模拟器版本同步到独立声明 {sim_declared}。",
                )
            )
        elif sim_strategy == "shared":
            app_version = _resolved_application_version(
                app_strategy, app_declared, app_sources
            )
            if app_version is None:
                findings.append(
                    Finding(
                        code="simulator:shared-version-unresolved",
                        title="无法解析模拟器要共享的应用版本",
                        detail="应用版本仍有冲突，不能据此验证共享版本。",
                        action="先消除应用版本漂移，再复核模拟器共享版本。",
                    )
                )
            elif simulator_version != app_version:
                findings.append(
                    Finding(
                        code="simulator:shared-version-drift",
                        title="模拟器没有跟随应用版本",
                        detail="shared 策略要求模拟器 pyproject 与应用公开版本相同。",
                        action=f"将模拟器版本同步到应用版本 {app_version}，或改为 independent 并单独声明。",
                    )
                )

    canonical = str(executable["canonical_name"])
    legacy = set(executable["legacy_names"])
    observed = {
        name for source in name_sources for name in source.names
    }
    unexpected = sorted(observed - legacy - {canonical})
    observed_legacy = sorted(observed & legacy)
    if unexpected:
        findings.append(
            Finding(
                code="executable:unclassified-name",
                title="发现未分类的可执行名称",
                detail=f"已验证为安全目标标识的未分类名称：{'、'.join(unexpected)}。",
                action="将其归入 canonical_name 或 legacy_names，或修正构建和启动入口。",
            )
        )
    if executable_strategy == "undecided":
        findings.append(
            Finding(
                code="executable:strategy-undecided",
                title="可执行名称迁移策略尚未决定",
                detail="现有名称已完成分类，但旧名保留多久尚无可执行约束。",
                action="选择 legacy-compatible 并给出兼容截止版本，或完成迁移后选择 canonical-only。",
            )
        )
    elif executable_strategy == "canonical-only":
        if canonical not in observed:
            findings.append(
                Finding(
                    code="executable:canonical-missing",
                    title="规范可执行名称没有落地",
                    detail="构建目标、Docker 入口和 macOS 入口均未使用规范名称。",
                    action="同步重命名目标和各平台启动入口，同时提供必要的升级说明。",
                )
            )
        if observed_legacy:
            findings.append(
                Finding(
                    code="executable:legacy-forbidden",
                    title="canonical-only 策略仍引用旧可执行名称",
                    detail=f"仍被引用的旧名：{'、'.join(observed_legacy)}。",
                    action="移除旧名引用，或恢复有明确截止版本的兼容策略。",
                )
            )
    elif executable_strategy == "legacy-compatible":
        app_version = _resolved_application_version(
            app_strategy, app_declared, app_sources
        )
        if app_version is None:
            findings.append(
                Finding(
                    code="executable:compatibility-version-unresolved",
                    title="无法判断旧名兼容期",
                    detail="应用公开版本没有唯一结果，不能与兼容截止版本比较。",
                    action="先确定并同步应用公开版本。",
                )
            )
        elif observed_legacy and _numeric_version(app_version) > _numeric_version(
            str(executable["legacy_compatibility_through"])
        ):
            findings.append(
                Finding(
                    code="executable:legacy-expired",
                    title="旧可执行名称已经超过兼容期",
                    detail=f"仍被引用的旧名：{'、'.join(observed_legacy)}。",
                    action="迁移构建和各平台入口到规范名称，或由维护者重新评估兼容截止版本。",
                )
            )
    return findings


def audit_repository(repo_root: Path) -> AuditReport:
    root = repo_root.resolve()
    tracked_files = git_tracked_files(root)
    tracked = set(tracked_files)
    if POLICY_PATH not in tracked:
        return AuditReport(
            len(tracked_files),
            None,
            None,
            None,
            (),
            (),
            (
                Finding(
                    code="policy:missing",
                    title="缺少机器可读版本策略",
                    detail="Git 索引中没有应用、模拟器、协议和旧名的版本契约。",
                    action=f"新增 {POLICY_PATH}，并如实保留尚未决定的策略。",
                ),
            ),
        )

    policy = _load_policy(root)
    application = _mapping(policy, "application", "application")
    simulator = _mapping(policy, "simulator", "simulator")
    protocol = _mapping(policy, "protocol", "protocol")
    executable = _mapping(policy, "executable", "executable")
    findings: list[Finding] = []
    version_sources = _collect_application_sources(
        root,
        tracked,
        application["sources"],  # type: ignore[arg-type]
        findings,
    )

    sim_paths = simulator["sources"]
    assert isinstance(sim_paths, dict)
    sim_source = _collect_single_version_source(
        repo_root=root,
        tracked=tracked,
        category="simulator",
        key="pyproject",
        label=SIMULATOR_SOURCE_LABELS["pyproject"],
        path=str(sim_paths["pyproject"]),
        extractor=_extract_pyproject_version,
        findings=findings,
    )
    if sim_source is not None:
        version_sources.append(sim_source)

    protocol_paths = protocol["sources"]
    assert isinstance(protocol_paths, dict)
    protocol_source = _collect_single_version_source(
        repo_root=root,
        tracked=tracked,
        category="protocol",
        key="manifest",
        label=PROTOCOL_SOURCE_LABELS["manifest"],
        path=str(protocol_paths["manifest"]),
        extractor=_extract_protocol_manifest,
        findings=findings,
    )
    if protocol_source is not None:
        version_sources.append(protocol_source)

    executable_paths = executable["sources"]
    assert isinstance(executable_paths, dict)
    name_extractors: dict[str, Callable[[bytes], tuple[str, ...]]] = {
        "cmake_targets": lambda blob: _extract_cmake_targets(blob.decode("utf-8")),
        "docker_entrypoint": lambda blob: _extract_docker_entrypoint(
            blob.decode("utf-8")
        ),
        "macos_bundle": _extract_macos_executable,
    }
    name_sources: list[NameSource] = []
    for key, extractor in name_extractors.items():
        source = _collect_name_source(
            repo_root=root,
            tracked=tracked,
            key=key,
            label=EXECUTABLE_SOURCE_LABELS[key],
            path=str(executable_paths[key]),
            extractor=extractor,
            findings=findings,
        )
        if source is not None:
            name_sources.append(source)

    findings.extend(_strategy_findings(policy, version_sources, name_sources))
    return AuditReport(
        len(tracked_files),
        str(application["strategy"]),
        str(simulator["strategy"]),
        str(executable["strategy"]),
        tuple(version_sources),
        tuple(name_sources),
        tuple(findings),
    )


def _render_sources(report: AuditReport) -> list[str]:
    lines = ["", "已提取的版本来源："]
    for category, title in (
        ("application", "应用"),
        ("simulator", "模拟器"),
        ("protocol", "协议（独立维度，不参与应用版本比较）"),
    ):
        sources = [
            source for source in report.version_sources if source.category == category
        ]
        if sources:
            summary = "、".join(
                f"{source.label}={source.version}" for source in sources
            )
            lines.append(f"- {title}：{summary}")
    if report.name_sources:
        lines.extend(("", "已提取的可执行名称来源："))
        for source in report.name_sources:
            lines.append(f"- {source.label}：{'、'.join(source.names)}")
    return lines


def render_report(report: AuditReport) -> str:
    if report.passed:
        lines = [
            "版本契约检查：通过",
            f"已审计 Git 索引中的 {report.tracked_file_count} 个文件。",
        ]
        lines.extend(_render_sources(report))
        return "\n".join(lines)

    lines = [
        "版本契约检查：未通过",
        f"已审计 Git 索引中的 {report.tracked_file_count} 个文件，发现 {len(report.findings)} 个阻断项。",
    ]
    lines.extend(_render_sources(report))
    for index, finding in enumerate(report.findings, start=1):
        lines.extend(
            (
                "",
                f"{index}. [{finding.code}] {finding.title}",
                f"   说明：{finding.detail}",
                f"   处理：{finding.action}",
            )
        )
    lines.extend(
        (
            "",
            "说明：本工具只读取 Git 索引；未暂存改动和未跟踪文件不参与公开版本判断。",
        )
    )
    return "\n".join(lines)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="检查公开发行快照的版本与旧名契约")
    parser.add_argument(
        "--repo", type=Path, default=Path.cwd(), help="待检查的 Git 仓库"
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        report = audit_repository(args.repo)
    except VersionContractError as error:
        print(f"版本契约检查无法执行：{error}", file=sys.stderr)
        return 2
    print(render_report(report))
    return 0 if report.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
