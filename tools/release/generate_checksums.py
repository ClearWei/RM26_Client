#!/usr/bin/env python3
"""为最终发行文件生成稳定排序的 SHA256SUMS。"""

from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path
from typing import Iterable, Sequence


class ChecksumGenerationError(RuntimeError):
    """发行文件无法安全计算校验和。"""


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def checksum_lines(paths: Iterable[Path]) -> tuple[str, ...]:
    """返回按发行文件名排序的 sha256sum 兼容文本。"""
    resolved = [path.resolve() for path in paths]
    if not resolved:
        raise ChecksumGenerationError("至少需要一个发行文件。")

    names = [path.name for path in resolved]
    if len(names) != len(set(names)):
        raise ChecksumGenerationError("发行文件名重复，无法生成无歧义的校验清单。")
    for path in resolved:
        if not path.is_file() or path.is_symlink():
            raise ChecksumGenerationError("校验对象必须是存在的普通文件。")

    return tuple(
        f"{_sha256(path)}  {path.name}"
        for path in sorted(resolved, key=lambda item: item.name.encode("utf-8"))
    )


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifacts", nargs="+", type=Path, help="最终发行文件")
    parser.add_argument(
        "--output", type=Path, default=Path("SHA256SUMS"), help="校验清单路径"
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    try:
        output = args.output.resolve()
        if any(path.resolve() == output for path in args.artifacts):
            raise ChecksumGenerationError("输出文件不能同时作为校验对象。")
        lines = checksum_lines(args.artifacts)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    except (OSError, ChecksumGenerationError) as exc:
        print(f"校验和生成失败：{exc}", file=sys.stderr)
        return 1

    print(f"SHA256SUMS 已生成：{output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
