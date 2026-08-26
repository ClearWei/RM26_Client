#!/usr/bin/env python3
"""为 Git 索引中的源码快照生成文件级 SPDX 2.3 JSON。"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath
from typing import Sequence


class SbomGenerationError(RuntimeError):
    """源码 SBOM 无法安全生成。"""


def _run_git(repo_root: Path, *args: str) -> bytes:
    result = subprocess.run(
        ["git", *args],
        cwd=repo_root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        raise SbomGenerationError("无法读取 Git 索引，请在已建立索引的仓库中运行。")
    return result.stdout


def tracked_files(repo_root: Path) -> tuple[str, ...]:
    """返回索引中 stage 0 的普通路径，遇到未解决冲突时拒绝生成。"""
    unmerged = _run_git(repo_root, "ls-files", "-z", "--unmerged")
    if unmerged:
        raise SbomGenerationError("Git 索引存在未解决冲突，不能生成发布 SBOM。")

    raw = _run_git(repo_root, "ls-files", "-z", "--cached")
    paths = tuple(item.decode("utf-8") for item in raw.split(b"\0") if item)
    if not paths:
        raise SbomGenerationError("Git 索引为空，不能生成发布 SBOM。")
    return tuple(sorted(paths, key=lambda item: item.encode("utf-8")))


def _file_bytes(path: Path) -> bytes:
    if path.is_symlink():
        return os.readlink(path).encode("utf-8")
    if not path.is_file():
        raise SbomGenerationError("索引路径在工作区中不存在或不是普通文件。")
    return path.read_bytes()


def _created_time() -> str:
    epoch = os.environ.get("SOURCE_DATE_EPOCH")
    if epoch:
        try:
            value = datetime.fromtimestamp(int(epoch), tz=timezone.utc)
        except (ValueError, OverflowError) as exc:
            raise SbomGenerationError("SOURCE_DATE_EPOCH 不是有效整数时间戳。") from exc
    else:
        value = datetime.now(timezone.utc)
    return value.replace(microsecond=0).isoformat().replace("+00:00", "Z")


def build_spdx_document(
    repo_root: Path,
    *,
    package_name: str = "RM26CustomClient-source",
    declared_license: str = "NOASSERTION",
) -> dict[str, object]:
    """构建源码快照的 SPDX 2.3 文档对象。"""
    root = repo_root.resolve()
    files: list[dict[str, object]] = []
    file_sha1_values: list[str] = []
    relationships: list[dict[str, str]] = [
        {
            "spdxElementId": "SPDXRef-DOCUMENT",
            "relationshipType": "DESCRIBES",
            "relatedSpdxElement": "SPDXRef-Package",
        }
    ]

    for relative in tracked_files(root):
        posix_path = str(PurePosixPath(relative))
        data = _file_bytes(root / Path(*PurePosixPath(posix_path).parts))
        sha1_value = hashlib.sha1(data).hexdigest()
        sha256_value = hashlib.sha256(data).hexdigest()
        file_sha1_values.append(sha1_value)
        file_id = "SPDXRef-File-" + hashlib.sha1(posix_path.encode("utf-8")).hexdigest()
        files.append(
            {
                "fileName": "./" + posix_path,
                "SPDXID": file_id,
                "checksums": [
                    {"algorithm": "SHA1", "checksumValue": sha1_value},
                    {"algorithm": "SHA256", "checksumValue": sha256_value},
                ],
                "licenseConcluded": "NOASSERTION",
                "copyrightText": "NOASSERTION",
            }
        )
        relationships.append(
            {
                "spdxElementId": "SPDXRef-Package",
                "relationshipType": "CONTAINS",
                "relatedSpdxElement": file_id,
            }
        )

    verification_code = hashlib.sha1(
        "".join(sorted(file_sha1_values)).encode("ascii")
    ).hexdigest()
    namespace = (
        "https://fudan-robotega.github.io/rm26-custom-client/spdx/"
        + verification_code
    )
    return {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": package_name + "-sbom",
        "documentNamespace": namespace,
        "creationInfo": {
            "created": _created_time(),
            "creators": ["Tool: tools/release/generate_source_sbom.py"],
        },
        "packages": [
            {
                "name": package_name,
                "SPDXID": "SPDXRef-Package",
                "downloadLocation": "NOASSERTION",
                "filesAnalyzed": True,
                "packageVerificationCode": {
                    "packageVerificationCodeValue": verification_code
                },
                "licenseConcluded": "NOASSERTION",
                "licenseDeclared": declared_license,
                "copyrightText": "NOASSERTION",
            }
        ],
        "files": files,
        "relationships": relationships,
    }


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="仓库根目录")
    parser.add_argument(
        "--output", type=Path, default=Path("source.spdx.json"), help="输出文件"
    )
    parser.add_argument("--name", default="RM26CustomClient-source", help="SPDX 包名")
    parser.add_argument(
        "--license",
        default="NOASSERTION",
        dest="declared_license",
        help="项目许可证的 SPDX 标识；未确认前保持 NOASSERTION",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    try:
        document = build_spdx_document(
            args.root,
            package_name=args.name,
            declared_license=args.declared_license,
        )
        output = args.output
        if not output.is_absolute():
            output = args.root / output
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(
            json.dumps(document, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
    except (OSError, SbomGenerationError) as exc:
        print(f"源码 SBOM 生成失败：{exc}", file=sys.stderr)
        return 1

    print(f"源码 SBOM 已生成：{output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
