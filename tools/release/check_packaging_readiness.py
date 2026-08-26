#!/usr/bin/env python3
"""检查 Git 索引中的快照是否具备声明发布方式所需的静态契约。"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Mapping, Sequence


POLICY_PATH = "tools/release/packaging_policy.json"
CMAKE_PATH = "CMakeLists.txt"
RELEASE_MODES = {"undecided", "source", "binary"}
SOURCE_ARTIFACTS = {"source-archive", "source-tarball", "source-zip"}
PLATFORMS = {"linux", "macos", "windows"}

GENERATED_DIRECTORY_NAMES = {
    "__pycache__",
    ".pytest_cache",
    ".mypy_cache",
    "build",
    "dist",
    "output",
    "CMakeFiles",
}
GENERATED_FILE_NAMES = {
    ".DS_Store",
    "CMakeCache.txt",
    "cmake_install.cmake",
    "compile_commands.json",
    "install_manifest.txt",
}
GENERATED_SUFFIXES = {
    ".a",
    ".appimage",
    ".deb",
    ".dll",
    ".dmg",
    ".dylib",
    ".exe",
    ".lib",
    ".msi",
    ".o",
    ".obj",
    ".pdb",
    ".pyc",
    ".pyo",
    ".rpm",
    ".so",
    ".whl",
}


@dataclass(frozen=True)
class Finding:
    code: str
    title: str
    detail: str
    action: str


@dataclass(frozen=True)
class AuditReport:
    tracked_file_count: int
    release_mode: str | None
    findings: tuple[Finding, ...]

    @property
    def passed(self) -> bool:
        return not self.findings


class PackagingReadinessError(RuntimeError):
    """检查器无法可靠读取发布快照时抛出。"""


def git_tracked_files(repo_root: Path) -> tuple[str, ...]:
    completed = subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=repo_root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        raise PackagingReadinessError("无法读取 Git 索引")
    decoded = completed.stdout.decode("utf-8", errors="surrogateescape")
    return tuple(path for path in decoded.split("\0") if path)


def _read_index_text(repo_root: Path, relative_path: str) -> str:
    completed = subprocess.run(
        ["git", "cat-file", "blob", f":{relative_path}"],
        cwd=repo_root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        raise PackagingReadinessError(f"无法读取索引中的 {relative_path}")
    try:
        return completed.stdout.decode("utf-8")
    except UnicodeDecodeError as error:
        raise PackagingReadinessError(
            f"索引中的 {relative_path} 不是 UTF-8 文本"
        ) from error


def _load_policy(repo_root: Path) -> Mapping[str, object]:
    text = _read_index_text(repo_root, POLICY_PATH)
    try:
        payload = json.loads(text)
    except json.JSONDecodeError as error:
        raise PackagingReadinessError(
            f"{POLICY_PATH} JSON 格式错误（第 {error.lineno} 行）"
        ) from error
    if not isinstance(payload, dict):
        raise PackagingReadinessError(f"{POLICY_PATH} 顶层必须是 JSON 对象")
    if payload.get("schema_version") != 1:
        raise PackagingReadinessError(f"{POLICY_PATH} schema_version 必须为 1")
    mode = payload.get("release_mode")
    if mode not in RELEASE_MODES:
        raise PackagingReadinessError(
            f"{POLICY_PATH} release_mode 只能是 undecided、source 或 binary"
        )
    return payload


def _mapping(payload: Mapping[str, object], key: str) -> Mapping[str, object]:
    value = payload.get(key, {})
    if not isinstance(value, dict):
        raise PackagingReadinessError(f"{POLICY_PATH} 的 {key} 必须是对象")
    return value


def _string_list(value: object, field: str) -> tuple[str, ...]:
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        raise PackagingReadinessError(f"{POLICY_PATH} 的 {field} 必须是字符串数组")
    return tuple(value)


def _policy_path(value: object, field: str) -> str:
    if not isinstance(value, str):
        raise PackagingReadinessError(f"{POLICY_PATH} 的 {field} 必须是字符串")
    if not value:
        return ""
    candidate = PurePosixPath(value)
    if candidate.is_absolute() or ".." in candidate.parts or "\\" in value:
        # 不回显非法值，避免把本机路径带入发布日志。
        raise PackagingReadinessError(f"{POLICY_PATH} 的 {field} 必须是仓库相对路径")
    return value


def _generated_artifact_findings(paths: Sequence[str]) -> list[Finding]:
    matches: list[str] = []
    for path in paths:
        candidate = PurePosixPath(path)
        parts = candidate.parts
        suffix = candidate.suffix.lower()
        if (
            any(part in GENERATED_DIRECTORY_NAMES or part.endswith(".egg-info") for part in parts)
            or candidate.name in GENERATED_FILE_NAMES
            or suffix in GENERATED_SUFFIXES
        ):
            matches.append(path)
    if not matches:
        return []
    return [
        Finding(
            code="artifact:tracked-generated",
            title="Git 索引包含构建或打包产物",
            detail=f"发现 {len(matches)} 个不应进入发布源码快照的生成文件。",
            action="从 Git 索引移除这些产物，并由忽略规则或发布工作流单独管理。",
        )
    ]


def _tracked_evidence_finding(
    paths: set[str], value: object, field: str, code: str, title: str
) -> Finding | None:
    relative_path = _policy_path(value, field)
    if relative_path and relative_path in paths:
        return None
    return Finding(
        code=code,
        title=title,
        detail="策略中尚未填写可由 Git 索引复核的仓库相对路径。",
        action=f"补充 {POLICY_PATH} 的 {field}，并将对应证据纳入发布快照。",
    )


def _supply_chain_findings(
    policy: Mapping[str, object], paths: set[str]
) -> list[Finding]:
    supply = _mapping(policy, "supply_chain")
    findings: list[Finding] = []
    for field, code, title in (
        ("sbom_generator", "supply:sbom", "缺少 SBOM 生成入口"),
        ("checksum_generator", "supply:checksums", "缺少校验和生成入口"),
    ):
        finding = _tracked_evidence_finding(
            paths, supply.get(field, ""), f"supply_chain.{field}", code, title
        )
        if finding is not None:
            findings.append(finding)

    signing_status = supply.get("signing_status", "undecided")
    if signing_status not in {"required", "not-required", "undecided"}:
        raise PackagingReadinessError(
            f"{POLICY_PATH} 的 supply_chain.signing_status 值无效"
        )
    if signing_status == "undecided":
        findings.append(
            Finding(
                code="supply:signing-undecided",
                title="产物签名策略尚未决定",
                detail="签名、公证或明确不签名的公开说明均未形成可复核结论。",
                action="由维护者确定 signing_status，并补充签名策略证据。",
            )
        )
    else:
        finding = _tracked_evidence_finding(
            paths,
            supply.get("signing_evidence", ""),
            "supply_chain.signing_evidence",
            "supply:signing-evidence",
            "缺少签名策略证据",
        )
        if finding is not None:
            findings.append(finding)
    return findings


def _source_findings(
    policy: Mapping[str, object], paths: set[str]
) -> list[Finding]:
    source = _mapping(policy, "source")
    artifacts = _string_list(source.get("artifacts", []), "source.artifacts")
    findings: list[Finding] = []
    if not artifacts or any(item not in SOURCE_ARTIFACTS for item in artifacts):
        findings.append(
            Finding(
                code="source:binary-artifact-claim",
                title="源码发布模式包含安装包声明",
                detail="source 模式只允许声明源码归档，不代表已有可安装二进制。",
                action="仅保留 source-archive、source-tarball 或 source-zip；二进制产物应切换到 binary 模式验收。",
            )
        )

    binary = _mapping(policy, "binary")
    binary_platforms = _string_list(
        binary.get("platforms", []), "binary.platforms"
    )
    binary_artifacts = binary.get("artifacts", {})
    if not isinstance(binary_artifacts, dict):
        raise PackagingReadinessError(
            f"{POLICY_PATH} 的 binary.artifacts 必须是对象"
        )
    if binary_platforms or binary_artifacts:
        findings.append(
            Finding(
                code="source:binary-contract-present",
                title="源码发布模式仍声明二进制产物",
                detail="策略同时声明了二进制平台或安装包，发布方式存在冲突。",
                action="清空 binary 平台与产物声明，或改为 binary 模式并满足完整验收。",
            )
        )

    finding = _tracked_evidence_finding(
        paths,
        source.get("build_evidence", ""),
        "source.build_evidence",
        "source:build-evidence",
        "缺少源码构建证据",
    )
    if finding is not None:
        findings.append(finding)
    findings.extend(_supply_chain_findings(policy, paths))
    return findings


def _strip_cmake_comments(text: str) -> str:
    return "\n".join(line.split("#", 1)[0] for line in text.splitlines())


def _binary_findings(
    repo_root: Path, policy: Mapping[str, object], paths: set[str]
) -> list[Finding]:
    binary = _mapping(policy, "binary")
    platforms = _string_list(binary.get("platforms", []), "binary.platforms")
    if any(platform not in PLATFORMS for platform in platforms):
        raise PackagingReadinessError(
            f"{POLICY_PATH} 的 binary.platforms 包含不支持的平台"
        )
    findings: list[Finding] = []
    if not platforms:
        findings.append(
            Finding(
                code="binary:platforms",
                title="未声明二进制发布平台",
                detail="binary 模式必须明确 Linux、macOS 或 Windows 支持范围。",
                action="在策略中填写已经准备验收的平台，不把仅能源码构建的平台列为可下载产物。",
            )
        )

    if CMAKE_PATH not in paths or not re.search(
        r"(?m)^\s*install\s*\(",
        _strip_cmake_comments(_read_index_text(repo_root, CMAKE_PATH))
        if CMAKE_PATH in paths
        else "",
    ):
        findings.append(
            Finding(
                code="binary:install-rules",
                title="缺少 CMake 安装规则",
                detail="Git 索引中的 CMakeLists.txt 没有可识别的 install() 入口。",
                action="先定义可验证的安装树，再声明二进制发行。",
            )
        )

    artifacts = binary.get("artifacts", {})
    metadata = binary.get("platform_metadata", {})
    if not isinstance(artifacts, dict) or not isinstance(metadata, dict):
        raise PackagingReadinessError(
            f"{POLICY_PATH} 的 binary.artifacts 和 binary.platform_metadata 必须是对象"
        )
    for platform in platforms:
        formats = artifacts.get(platform, [])
        if not isinstance(formats, list) or not formats or not all(
            isinstance(item, str) and item for item in formats
        ):
            findings.append(
                Finding(
                    code="binary:artifact-format",
                    title=f"{platform} 未声明发行格式",
                    detail="平台没有对应的可下载产物格式。",
                    action="填写实际生成并经过安装验证的格式，例如 dmg、zip 或 appimage。",
                )
            )
        finding = _tracked_evidence_finding(
            paths,
            metadata.get(platform, ""),
            f"binary.platform_metadata.{platform}",
            "binary:platform-metadata",
            f"{platform} 缺少平台元数据",
        )
        if finding is not None:
            findings.append(finding)

    for field, code, title in (
        (
            "runtime_dependency_evidence",
            "binary:runtime-dependencies",
            "缺少运行时依赖部署证据",
        ),
        ("install_smoke_test", "binary:install-smoke", "缺少安装后启动测试"),
    ):
        finding = _tracked_evidence_finding(
            paths, binary.get(field, ""), f"binary.{field}", code, title
        )
        if finding is not None:
            findings.append(finding)
    findings.extend(_supply_chain_findings(policy, paths))
    return findings


def audit_repository(repo_root: Path) -> AuditReport:
    root = repo_root.resolve()
    tracked = git_tracked_files(root)
    path_set = set(tracked)
    findings = _generated_artifact_findings(tracked)
    if POLICY_PATH not in path_set:
        findings.append(
            Finding(
                code="policy:missing",
                title="缺少打包发布策略",
                detail="Git 索引中没有机器可读的发布方式声明。",
                action=f"新增 {POLICY_PATH}，并将 release_mode 设为真实状态。",
            )
        )
        return AuditReport(len(tracked), None, tuple(findings))

    policy = _load_policy(root)
    mode = str(policy["release_mode"])
    if mode == "undecided":
        findings.append(
            Finding(
                code="mode:undecided",
                title="发布方式尚未决定",
                detail="当前没有把仓库声明为源码发布版或可安装二进制版。",
                action="由维护者选择 source 或 binary；选择前不要发布现有 .app、wheel 或其他开发机构建产物。",
            )
        )
    elif mode == "source":
        findings.extend(_source_findings(policy, path_set))
    else:
        findings.extend(_binary_findings(root, policy, path_set))
    return AuditReport(len(tracked), mode, tuple(findings))


def render_report(report: AuditReport) -> str:
    mode = report.release_mode or "未声明"
    if report.passed:
        return (
            "打包发布就绪检查：通过\n"
            f"发布模式：{mode}\n"
            f"已审计 Git 索引中的 {report.tracked_file_count} 个文件。"
        )
    lines = [
        "打包发布就绪检查：未通过",
        f"发布模式：{mode}",
        f"已审计 Git 索引中的 {report.tracked_file_count} 个文件，发现 {len(report.findings)} 个阻断项：",
    ]
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
            "说明：本工具只读取 Git 索引，不使用未暂存改动或未跟踪文件替发布快照背书。",
        )
    )
    return "\n".join(lines)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="检查发布快照的打包静态契约")
    parser.add_argument(
        "--repo", type=Path, default=Path.cwd(), help="待检查的 Git 仓库"
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        report = audit_repository(args.repo)
    except PackagingReadinessError as error:
        print(f"打包发布就绪检查无法执行：{error}", file=sys.stderr)
        return 2
    print(render_report(report))
    return 0 if report.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
