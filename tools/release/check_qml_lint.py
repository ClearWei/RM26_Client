#!/usr/bin/env python3
"""运行 qmllint，并阻止 QML 静态质量基线回退。"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from collections import Counter
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Mapping, Sequence


DEFAULT_POLICY_PATH = Path("tools/release/qml_lint_policy.json")
DIAGNOSTIC_ID_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_.-]*$")
WINDOWS_ABSOLUTE_RE = re.compile(r"^[A-Za-z]:[\\/]")
QT64_CALL_LATER_FALSE_POSITIVE = (
    'Property "callLater" is a QJSValue property. It may or may not be a method. '
    "Use a regular Q_INVOKABLE instead."
)
QT64_CALL_LATER_MAX_IGNORES = 3
QT_CALL_LATER_RE = re.compile(r"\bQt\s*\.\s*callLater\s*\(")


class QmlLintCheckError(RuntimeError):
    """策略、工具或输出无法被可靠读取。"""


@dataclass(frozen=True)
class DiagnosticRule:
    diagnostic_type: str
    max_count: int


@dataclass(frozen=True)
class Finding:
    code: str
    detail: str


@dataclass(frozen=True)
class AuditReport:
    file_count: int
    counts: Mapping[tuple[str, str], int]
    findings: tuple[Finding, ...]
    compatibility_ignores: int = 0

    @property
    def passed(self) -> bool:
        return not self.findings


def _safe_relative_path(value: object, field: str) -> Path:
    if not isinstance(value, str) or not value:
        raise QmlLintCheckError(f"{field} 必须是仓库相对路径")
    candidate = PurePosixPath(value)
    if (
        candidate.is_absolute()
        or value.startswith("~")
        or WINDOWS_ABSOLUTE_RE.match(value)
        or "\\" in value
        or ".." in candidate.parts
    ):
        raise QmlLintCheckError(f"{field} 必须是仓库相对路径")
    return Path(*candidate.parts)


def parse_policy(payload: object) -> tuple[Path, dict[str, DiagnosticRule]]:
    if not isinstance(payload, dict):
        raise QmlLintCheckError("QML lint 策略顶层必须是对象")
    if payload.get("schema_version") != 1:
        raise QmlLintCheckError("QML lint 策略 schema_version 必须为 1")

    supported_fields = {"schema_version", "qml_root", "allowed_diagnostics"}
    if set(payload) - supported_fields:
        raise QmlLintCheckError("QML lint 策略包含未支持的字段")

    qml_root = _safe_relative_path(payload.get("qml_root"), "qml_root")
    raw_rules = payload.get("allowed_diagnostics")
    if not isinstance(raw_rules, dict):
        raise QmlLintCheckError("allowed_diagnostics 必须是对象")

    rules: dict[str, DiagnosticRule] = {}
    for diagnostic_id, raw_rule in raw_rules.items():
        if not isinstance(diagnostic_id, str) or not DIAGNOSTIC_ID_RE.fullmatch(
            diagnostic_id
        ):
            raise QmlLintCheckError("allowed_diagnostics 包含无效诊断 ID")
        if not isinstance(raw_rule, dict):
            raise QmlLintCheckError(f"诊断 {diagnostic_id} 的规则必须是对象")
        if set(raw_rule) != {"type", "max_count"}:
            raise QmlLintCheckError(f"诊断 {diagnostic_id} 的规则字段不完整")

        diagnostic_type = raw_rule.get("type")
        max_count = raw_rule.get("max_count")
        if diagnostic_type not in {"warning", "info"}:
            raise QmlLintCheckError(f"诊断 {diagnostic_id} 的 type 无效")
        if isinstance(max_count, bool) or not isinstance(max_count, int) or max_count < 0:
            raise QmlLintCheckError(f"诊断 {diagnostic_id} 的 max_count 无效")
        rules[diagnostic_id] = DiagnosticRule(diagnostic_type, max_count)

    return qml_root, rules


def load_policy(path: Path) -> tuple[Path, dict[str, DiagnosticRule]]:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except OSError as error:
        raise QmlLintCheckError("无法读取 QML lint 策略") from error
    except json.JSONDecodeError as error:
        raise QmlLintCheckError(
            f"QML lint 策略 JSON 格式错误（第 {error.lineno} 行）"
        ) from error
    return parse_policy(payload)


def collect_qml_files(repo_root: Path, qml_root: Path) -> tuple[Path, ...]:
    source_root = repo_root / qml_root
    if not source_root.is_dir():
        raise QmlLintCheckError("QML 源码目录不存在")
    files = tuple(sorted(path for path in source_root.rglob("*.qml") if path.is_file()))
    if not files:
        raise QmlLintCheckError("QML 源码目录中没有 .qml 文件")
    return files


def find_qmllint() -> str | None:
    for executable_name in ("qmllint", "qmllint6"):
        executable = shutil.which(executable_name)
        if executable:
            return executable

    ubuntu_qmllint = Path("/usr/lib/qt6/bin/qmllint")
    if ubuntu_qmllint.is_file():
        return str(ubuntu_qmllint)

    # Debian/Ubuntu 会把 Qt 6 工具放在专用 bin 目录，未必建立全局链接。
    for qtpaths_name in ("qtpaths6", "qtpaths"):
        qtpaths = shutil.which(qtpaths_name)
        if not qtpaths:
            continue
        completed = subprocess.run(
            [qtpaths, "--query", "QT_INSTALL_BINS"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
            text=True,
        )
        if completed.returncode != 0:
            continue
        candidate = Path(completed.stdout.strip()) / "qmllint"
        if candidate.is_file():
            return str(candidate)
    return None


def supports_qmllint_option(executable: str, option: str) -> bool:
    """按当前 qmllint 的帮助文本判断可用参数，兼容不同 Qt 版本。"""
    try:
        completed = subprocess.run(
            [executable, "--help"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
            text=True,
        )
    except OSError:
        return False
    return option in f"{completed.stdout}\n{completed.stderr}"


def run_qmllint(
    repo_root: Path,
    qml_files: Sequence[Path],
    *,
    qmllint: str | None = None,
) -> object:
    executable = qmllint if qmllint is not None else find_qmllint()
    if not executable:
        raise QmlLintCheckError(
            "未找到 qmllint；请安装 Qt Declarative 开发工具并确认其位于 PATH"
        )

    compatibility_arguments: list[str] = []
    if supports_qmllint_option(executable, "--deferred-property-id"):
        # Qt 6.4 默认启用该检查，新版 Qt 已默认关闭；显式关闭以统一结果。
        compatibility_arguments = ["--deferred-property-id", "disable"]

    with tempfile.TemporaryDirectory(prefix="rm26-qmllint-") as temporary:
        report_path = Path(temporary) / "report.json"
        relative_files = [str(path.relative_to(repo_root)) for path in qml_files]
        try:
            completed = subprocess.run(
                [
                    executable,
                    "--json",
                    str(report_path),
                    *compatibility_arguments,
                    *relative_files,
                ],
                cwd=repo_root,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
                text=True,
            )
        except OSError as error:
            raise QmlLintCheckError("无法启动 qmllint") from error
        if not report_path.is_file():
            detail = (completed.stderr or completed.stdout).strip()
            suffix = f"：{detail.splitlines()[0]}" if detail else ""
            raise QmlLintCheckError(
                f"qmllint 未生成 JSON 报告（退出码 {completed.returncode}）{suffix}"
            )
        try:
            return json.loads(report_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise QmlLintCheckError("qmllint JSON 报告无法解析") from error


def is_qt64_call_later_false_positive(
    diagnostic: Mapping[str, object],
    filename: str,
    source_lines: Mapping[str, Sequence[str]] | None,
) -> bool:
    """只忽略能回溯到 Qt.callLater 源码行的 Qt 6.4 已知误报。"""
    if (
        "id" in diagnostic
        or diagnostic.get("type") != "warning"
        or diagnostic.get("message") != QT64_CALL_LATER_FALSE_POSITIVE
        or source_lines is None
    ):
        return False

    line_number = diagnostic.get("line")
    lines = source_lines.get(filename)
    if (
        isinstance(line_number, bool)
        or not isinstance(line_number, int)
        or line_number < 1
        or lines is None
        or line_number > len(lines)
    ):
        return False
    return QT_CALL_LATER_RE.search(lines[line_number - 1]) is not None


def audit_payload(
    payload: object,
    rules: Mapping[str, DiagnosticRule],
    *,
    expected_file_count: int | None = None,
    source_lines: Mapping[str, Sequence[str]] | None = None,
) -> AuditReport:
    if not isinstance(payload, dict) or not isinstance(payload.get("files"), list):
        raise QmlLintCheckError("qmllint JSON 报告缺少 files 数组")

    counts: Counter[tuple[str, str]] = Counter()
    samples: dict[tuple[str, str], str] = {}
    compatibility_ignores = 0
    file_count = 0
    filenames: set[str] = set()
    for file_report in payload["files"]:
        if not isinstance(file_report, dict) or not isinstance(
            file_report.get("warnings"), list
        ):
            raise QmlLintCheckError("qmllint 文件报告格式无效")
        filename = file_report.get("filename")
        if not isinstance(filename, str) or not filename:
            raise QmlLintCheckError("qmllint 文件报告缺少文件名")
        if filename in filenames:
            raise QmlLintCheckError("qmllint JSON 报告包含重复文件")
        filenames.add(filename)
        file_count += 1
        for diagnostic in file_report["warnings"]:
            if not isinstance(diagnostic, dict):
                raise QmlLintCheckError("qmllint 诊断条目格式无效")
            if is_qt64_call_later_false_positive(
                diagnostic, filename, source_lines
            ):
                # Qt 6.4 无诊断 ID，且会把 Qt.callLater 误判为普通 QJSValue 属性。
                compatibility_ignores += 1
                continue
            diagnostic_id = diagnostic.get("id")
            diagnostic_type = diagnostic.get("type")
            if not isinstance(diagnostic_id, str) or not diagnostic_id:
                diagnostic_id = "unknown"
            if diagnostic_type not in {"warning", "info"}:
                diagnostic_type = "unknown"
            key = (diagnostic_id, diagnostic_type)
            counts[key] += 1
            message = diagnostic.get("message")
            if key not in samples and isinstance(message, str) and message.strip():
                # 旧版 Qt 的 JSON 没有诊断 ID，保留一条短消息便于定位兼容问题。
                samples[key] = " ".join(message.split())[:160]

    if expected_file_count is not None and file_count != expected_file_count:
        raise QmlLintCheckError(
            f"qmllint 仅返回 {file_count}/{expected_file_count} 个文件，扫描结果不完整"
        )

    findings: list[Finding] = []
    for (diagnostic_id, diagnostic_type), count in sorted(counts.items()):
        rule = rules.get(diagnostic_id)
        if rule is None:
            sample_text = samples.get((diagnostic_id, diagnostic_type))
            sample = f"；示例：{sample_text}" if sample_text else ""
            findings.append(
                Finding(
                    "diagnostic:not-allowed",
                    f"{diagnostic_id} ({diagnostic_type}) 新增 {count} 条{sample}",
                )
            )
            continue
        if diagnostic_type != rule.diagnostic_type:
            findings.append(
                Finding(
                    "diagnostic:type-drift",
                    f"{diagnostic_id} 类型由 {rule.diagnostic_type} 变为 {diagnostic_type}",
                )
            )
            continue
        if count > rule.max_count:
            findings.append(
                Finding(
                    "diagnostic:count-regression",
                    f"{diagnostic_id} 为 {count} 条，超过基线 {rule.max_count}",
                )
            )

    if compatibility_ignores > QT64_CALL_LATER_MAX_IGNORES:
        findings.append(
            Finding(
                "diagnostic:compatibility-regression",
                "Qt 6.4 的 callLater 已知误报为 "
                f"{compatibility_ignores} 条，超过兼容基线 "
                f"{QT64_CALL_LATER_MAX_IGNORES}",
            )
        )

    return AuditReport(
        file_count,
        dict(counts),
        tuple(findings),
        compatibility_ignores,
    )


def render_report(report: AuditReport) -> str:
    summary = ", ".join(
        f"{diagnostic_id}={count}"
        for (diagnostic_id, _diagnostic_type), count in sorted(report.counts.items())
    ) or "无诊断"
    compatibility_note = (
        f"；忽略 Qt 6.4 的 callLater 已知误报 {report.compatibility_ignores} 条"
        if report.compatibility_ignores
        else ""
    )
    if report.passed:
        return (
            f"QML lint 基线检查：通过（{report.file_count} 个文件，"
            f"{summary}{compatibility_note}）"
        )

    lines = [
        "QML lint 基线检查：未通过",
        f"扫描 {report.file_count} 个文件，当前诊断：{summary}{compatibility_note}",
    ]
    for index, finding in enumerate(report.findings, start=1):
        lines.append(f"{index}. [{finding.code}] {finding.detail}")
    lines.append("请修复新增诊断；基线只允许随清理结果下调，不应为通过检查而上调。")
    return "\n".join(lines)


def audit_repository(
    repo_root: Path,
    policy_path: Path = DEFAULT_POLICY_PATH,
    *,
    qmllint: str | None = None,
) -> AuditReport:
    resolved_policy = policy_path if policy_path.is_absolute() else repo_root / policy_path
    qml_root, rules = load_policy(resolved_policy)
    qml_files = collect_qml_files(repo_root, qml_root)
    payload = run_qmllint(repo_root, qml_files, qmllint=qmllint)
    try:
        source_lines: dict[str, tuple[str, ...]] = {}
        for path in qml_files:
            lines = tuple(path.read_text(encoding="utf-8").splitlines())
            source_lines[str(path)] = lines
            source_lines[path.relative_to(repo_root).as_posix()] = lines
    except OSError as error:
        raise QmlLintCheckError("无法读取 QML 源码以校验兼容诊断") from error
    return audit_payload(
        payload,
        rules,
        expected_file_count=len(qml_files),
        source_lines=source_lines,
    )


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="仓库根目录")
    parser.add_argument(
        "--policy", type=Path, default=DEFAULT_POLICY_PATH, help="QML lint 策略"
    )
    parser.add_argument("--qmllint", help="显式指定 qmllint 可执行文件")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        report = audit_repository(
            args.root.resolve(), args.policy, qmllint=args.qmllint
        )
    except QmlLintCheckError as error:
        print(f"QML lint 基线检查：无法完成\n{error}", file=sys.stderr)
        return 2
    print(render_report(report))
    return 0 if report.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
