#!/usr/bin/env python3
"""检查模拟器 Protobuf 生成代码与 Python 运行时是否相容。"""

from __future__ import annotations

import argparse
import ast
import importlib.util
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence

from google.protobuf import descriptor_pb2

try:
    import tomllib
except ModuleNotFoundError:  # macOS 系统 Python 3.9 仍可运行仓库门禁。
    tomllib = None

TOML_DECODE_ERROR = tomllib.TOMLDecodeError if tomllib is not None else ValueError


@dataclass(frozen=True)
class ProtobufVersion:
    major: int
    minor: int
    patch: int
    suffix: str = ""

    @property
    def core(self) -> tuple[int, int, int]:
        return self.major, self.minor, self.patch

    def __str__(self) -> str:
        return f"{self.major}.{self.minor}.{self.patch}{self.suffix}"


@dataclass(frozen=True)
class RuntimeIssue:
    code: str
    path: str
    message: str


@dataclass(frozen=True)
class AuditResult:
    generated: ProtobufVersion | None
    runtime: ProtobufVersion | None
    issues: tuple[RuntimeIssue, ...]


class RuntimeContractError(RuntimeError):
    """表示版本契约无法被可靠读取。"""


def _call_name(node: ast.Call) -> str | None:
    function = node.func
    if not isinstance(function, ast.Attribute):
        return None
    if not isinstance(function.value, ast.Name):
        return None
    if function.value.id != "_runtime_version":
        return None
    return function.attr


def parse_generated_runtime(path: Path) -> ProtobufVersion:
    """从 pb2 的自带校验调用中读取最低运行时版本。"""

    try:
        tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    except (OSError, SyntaxError) as error:
        raise RuntimeContractError(f"无法解析生成代码：{error}") from error

    calls = [
        node
        for node in ast.walk(tree)
        if isinstance(node, ast.Call)
        and _call_name(node) == "ValidateProtobufRuntimeVersion"
    ]
    if len(calls) != 1:
        raise RuntimeContractError(
            "应当有且仅有一个 ValidateProtobufRuntimeVersion 调用"
        )

    call = calls[0]
    if len(call.args) < 6:
        raise RuntimeContractError("生成代码的版本校验参数不完整")
    try:
        major, minor, patch = (ast.literal_eval(arg) for arg in call.args[1:4])
        suffix = ast.literal_eval(call.args[4])
    except (ValueError, TypeError) as error:
        raise RuntimeContractError("生成代码的版本参数不是常量") from error
    if not all(isinstance(value, int) for value in (major, minor, patch)):
        raise RuntimeContractError("生成代码的版本号必须是整数")
    if not isinstance(suffix, str):
        raise RuntimeContractError("生成代码的版本后缀必须是字符串")
    return ProtobufVersion(major, minor, patch, suffix)


def expected_requirement(generated: ProtobufVersion) -> str:
    """根据生成代码给出可安装的同主版本范围。"""

    return f"protobuf>={generated.major}.{generated.minor}.{generated.patch},<{generated.major + 1}"


def _package_name(specification: str) -> str | None:
    match = re.match(r"\s*([A-Za-z0-9_.-]+)", specification)
    if match is None:
        return None
    return match.group(1).lower().replace("_", "-")


def validate_dependency_specs(
    specifications: Sequence[str], *, expected: str, path: str
) -> tuple[RuntimeIssue, ...]:
    protobuf_specs = [
        specification.strip()
        for specification in specifications
        if _package_name(specification) == "protobuf"
    ]
    if not protobuf_specs:
        return (
            RuntimeIssue(
                "dependency:missing",
                path,
                f"缺少与生成代码对应的 {expected}",
            ),
        )

    issues: list[RuntimeIssue] = []
    if len(protobuf_specs) > 1:
        issues.append(
            RuntimeIssue(
                "dependency:duplicate",
                path,
                "protobuf 依赖只应声明一次",
            )
        )
    for specification in protobuf_specs:
        normalized = re.sub(r"\s+", "", specification).lower()
        if normalized != expected.lower():
            issues.append(
                RuntimeIssue(
                    "dependency:range",
                    path,
                    f"当前为 {specification!r}，应与生成代码同步为 {expected!r}",
                )
            )
    return tuple(issues)


def _read_pyproject_dependencies(path: Path) -> list[str]:
    try:
        source = path.read_text(encoding="utf-8")
    except OSError as error:
        raise RuntimeContractError(f"无法读取 project.dependencies：{error}") from error

    try:
        if tomllib is not None:
            dependencies = tomllib.loads(source)["project"]["dependencies"]
        else:
            # 兼容解析只读取 project.dependencies，不尝试替代完整 TOML 解析器。
            project = re.search(
                r"(?ms)^\[project\]\s*$\n(.*?)(?=^\[|\Z)", source
            )
            if project is None:
                raise KeyError("project")
            declaration = re.search(
                r"(?ms)^\s*dependencies\s*=\s*(\[.*?\])\s*$",
                project.group(1),
            )
            if declaration is None:
                raise KeyError("dependencies")
            dependencies = ast.literal_eval(declaration.group(1))
    except (SyntaxError, TOML_DECODE_ERROR, KeyError, TypeError) as error:
        raise RuntimeContractError(f"无法读取 project.dependencies：{error}") from error
    if not isinstance(dependencies, list) or not all(
        isinstance(item, str) for item in dependencies
    ):
        raise RuntimeContractError("project.dependencies 必须是字符串数组")
    return dependencies


def _read_requirements(path: Path) -> list[str]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise RuntimeContractError(f"无法读取依赖文件：{error}") from error
    return [
        content
        for raw_line in lines
        if (content := raw_line.split("#", 1)[0].strip())
    ]


def _metadata_version(value: str) -> ProtobufVersion:
    match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(.*)", value)
    if match is None:
        raise RuntimeContractError(f"无法识别 protobuf.__version__={value!r}")
    return ProtobufVersion(
        int(match.group(1)),
        int(match.group(2)),
        int(match.group(3)),
        match.group(4),
    )


def import_runtime_version() -> ProtobufVersion:
    """导入当前 Python 环境中真实的 protobuf runtime。"""

    try:
        import google.protobuf
        from google.protobuf import runtime_version
    except (ImportError, AttributeError) as error:
        raise RuntimeContractError(f"无法导入 protobuf runtime：{error}") from error

    metadata = _metadata_version(google.protobuf.__version__)
    constants = ProtobufVersion(
        int(runtime_version.MAJOR),
        int(runtime_version.MINOR),
        int(runtime_version.PATCH),
        str(runtime_version.SUFFIX),
    )
    if metadata != constants:
        raise RuntimeContractError(
            f"protobuf 版本元数据不一致：__version__={metadata}，"
            f"runtime_version={constants}"
        )
    return constants


def validate_runtime_version(
    generated: ProtobufVersion, runtime: ProtobufVersion
) -> tuple[RuntimeIssue, ...]:
    issues: list[RuntimeIssue] = []
    if runtime.core < generated.core:
        issues.append(
            RuntimeIssue(
                "runtime:too-old",
                "google.protobuf",
                f"已导入 {runtime}，但生成代码至少需要 {generated}",
            )
        )
    if runtime.major >= generated.major + 1:
        issues.append(
            RuntimeIssue(
                "runtime:major-range",
                "google.protobuf",
                f"已导入 {runtime}，应保持在 {generated.major}.x 主版本内",
            )
        )
    return tuple(issues)


def import_generated_module(path: Path) -> str:
    """真实执行 pb2 导入，让 Protobuf 自带的版本校验生效。"""

    spec = importlib.util.spec_from_file_location("_rm26_sim_pb2_runtime_check", path)
    if spec is None or spec.loader is None:
        raise RuntimeContractError("无法为生成代码创建导入器")
    module = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(module)
    except Exception as error:
        raise RuntimeContractError(f"导入生成代码失败：{error}") from error

    descriptor = getattr(module, "DESCRIPTOR", None)
    descriptor_name = getattr(descriptor, "name", None)
    if not isinstance(descriptor_name, str) or not descriptor_name:
        raise RuntimeContractError("生成模块没有有效的 DESCRIPTOR")
    return descriptor_name


def extract_generated_descriptor(path: Path) -> descriptor_pb2.FileDescriptorProto:
    """从提交的 pb2 中提取 protoc 写入的文件描述符。"""

    try:
        tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    except (OSError, SyntaxError) as error:
        raise RuntimeContractError(f"无法解析生成代码描述符：{error}") from error

    serialized_values: list[bytes] = []
    for node in ast.walk(tree):
        if not isinstance(node, ast.Call) or not isinstance(node.func, ast.Attribute):
            continue
        if node.func.attr != "AddSerializedFile" or len(node.args) != 1:
            continue
        try:
            value = ast.literal_eval(node.args[0])
        except (ValueError, TypeError):
            continue
        if isinstance(value, bytes):
            serialized_values.append(value)

    if len(serialized_values) != 1:
        raise RuntimeContractError("应当有且仅有一个 AddSerializedFile 字节常量")

    descriptor = descriptor_pb2.FileDescriptorProto()
    try:
        descriptor.ParseFromString(serialized_values[0])
    except Exception as error:
        raise RuntimeContractError(f"生成代码描述符无法解析：{error}") from error
    if not descriptor.name:
        raise RuntimeContractError("生成代码描述符缺少文件名")
    return descriptor


def generate_canonical_descriptor(
    project_root: Path, *, protoc: str | None = None
) -> descriptor_pb2.FileDescriptorProto:
    """让 protoc 从 canonical schema 生成描述符，供 CI 比对。"""

    executable = protoc or shutil.which("protoc")
    if not executable:
        raise RuntimeContractError("找不到 protoc，无法复核 canonical schema")

    proto_root = project_root / "src/network/proto"
    canonical = proto_root / "robomaster.proto"
    if not canonical.is_file():
        raise RuntimeContractError("缺少 src/network/proto/robomaster.proto")

    with tempfile.TemporaryDirectory(prefix="rm26-protobuf-") as temp_dir:
        descriptor_path = Path(temp_dir) / "canonical.pb"
        command = [
            executable,
            f"--proto_path={proto_root}",
            f"--descriptor_set_out={descriptor_path}",
            str(canonical),
        ]
        completed = subprocess.run(
            command,
            capture_output=True,
            text=True,
            check=False,
        )
        if completed.returncode != 0:
            details = (completed.stderr or completed.stdout).strip()
            raise RuntimeContractError(
                f"protoc 生成 canonical descriptor 失败：{details or completed.returncode}"
            )
        descriptor_set = descriptor_pb2.FileDescriptorSet()
        try:
            descriptor_set.ParseFromString(descriptor_path.read_bytes())
        except Exception as error:
            raise RuntimeContractError(f"无法读取 canonical descriptor：{error}") from error

    matches = [item for item in descriptor_set.file if item.name == "robomaster.proto"]
    if len(matches) != 1:
        raise RuntimeContractError("canonical descriptor 中未找到唯一的 robomaster.proto")
    return matches[0]


def descriptors_equal(
    generated: descriptor_pb2.FileDescriptorProto,
    canonical: descriptor_pb2.FileDescriptorProto,
) -> bool:
    """忽略生成器补充的 json_name，比较其余 schema 语义。"""

    def normalized(
        source: descriptor_pb2.FileDescriptorProto,
    ) -> descriptor_pb2.FileDescriptorProto:
        result = descriptor_pb2.FileDescriptorProto()
        result.CopyFrom(source)

        def clear_message(message: descriptor_pb2.DescriptorProto) -> None:
            for field in message.field:
                field.ClearField("json_name")
            for field in message.extension:
                field.ClearField("json_name")
            for nested in message.nested_type:
                clear_message(nested)

        for field in result.extension:
            field.ClearField("json_name")
        for message in result.message_type:
            clear_message(message)
        return result

    return normalized(generated).SerializeToString(
        deterministic=True
    ) == normalized(canonical).SerializeToString(deterministic=True)


def audit_repository(project_root: Path) -> AuditResult:
    generated_path = project_root / "sim/robomaster_pb2.py"
    pyproject_path = project_root / "sim/pyproject.toml"
    requirements_path = project_root / "sim/requirements.txt"
    issues: list[RuntimeIssue] = []

    duplicate_schema = project_root / "sim/robomaster.proto"
    if duplicate_schema.exists():
        issues.append(
            RuntimeIssue(
                "schema:duplicate",
                "sim/robomaster.proto",
                "模拟器不得再手工维护第二份 RoboMaster schema",
            )
        )

    try:
        generated = parse_generated_runtime(generated_path)
    except RuntimeContractError as error:
        return AuditResult(
            None,
            None,
            (RuntimeIssue("gencode:version", str(generated_path), str(error)),),
        )

    expected = expected_requirement(generated)
    try:
        dependencies = _read_pyproject_dependencies(pyproject_path)
    except RuntimeContractError as error:
        issues.append(RuntimeIssue("dependency:pyproject", str(pyproject_path), str(error)))
    else:
        issues.extend(
            validate_dependency_specs(
                dependencies, expected=expected, path="sim/pyproject.toml"
            )
        )

    try:
        requirements = _read_requirements(requirements_path)
    except RuntimeContractError as error:
        issues.append(
            RuntimeIssue("dependency:requirements", str(requirements_path), str(error))
        )
    else:
        issues.extend(
            validate_dependency_specs(
                requirements, expected=expected, path="sim/requirements.txt"
            )
        )

    runtime: ProtobufVersion | None = None
    try:
        runtime = import_runtime_version()
    except RuntimeContractError as error:
        issues.append(RuntimeIssue("runtime:import", "google.protobuf", str(error)))
    else:
        issues.extend(validate_runtime_version(generated, runtime))

    try:
        descriptor_name = import_generated_module(generated_path)
    except RuntimeContractError as error:
        issues.append(RuntimeIssue("gencode:import", str(generated_path), str(error)))
    else:
        if descriptor_name != "robomaster.proto":
            issues.append(
                RuntimeIssue(
                    "gencode:descriptor",
                    str(generated_path),
                    f"descriptor 名称应为 'robomaster.proto'，当前为 {descriptor_name!r}",
                )
            )

    try:
        generated_descriptor = extract_generated_descriptor(generated_path)
        canonical_descriptor = generate_canonical_descriptor(project_root)
    except RuntimeContractError as error:
        issues.append(
            RuntimeIssue(
                "codegen:descriptor",
                "src/network/proto/robomaster.proto",
                str(error),
            )
        )
    else:
        if not descriptors_equal(generated_descriptor, canonical_descriptor):
            issues.append(
                RuntimeIssue(
                    "codegen:drift",
                    "sim/robomaster_pb2.py",
                    "提交的 Python 描述符不是由 canonical schema 生成",
                )
            )

    return AuditResult(generated, runtime, tuple(issues))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="检查模拟器 Protobuf 运行时契约")
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="仓库根目录，默认由检查器位置推导",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    result = audit_repository(args.project_root.resolve())
    if result.issues:
        print(f"模拟器 Protobuf 兼容性检查：未通过，共 {len(result.issues)} 项")
        for issue in result.issues:
            print(f"- {issue.path}: {issue.message} [{issue.code}]")
        return 1

    print(
        "模拟器 Protobuf 兼容性检查：通过"
        f"（生成代码 {result.generated}，实际运行时 {result.runtime}）"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
