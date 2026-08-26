#!/usr/bin/env python3
"""检查 Git 索引中 Markdown 的本地链接和 depends_on 元数据。"""

from __future__ import annotations

import argparse
import posixpath
import re
import sys
from dataclasses import dataclass
from pathlib import PurePosixPath
from typing import Sequence
from urllib.parse import unquote, urlsplit

from check_public_readiness import ReadinessError, _read_index_blob, git_tracked_files


MARKDOWN_LINK_RE = re.compile(r"!?\[[^\]]*\]\((?P<target>[^)]+)\)")
EXTERNAL_SCHEMES = {"data", "http", "https", "mailto"}


@dataclass(frozen=True)
class BrokenReference:
    source: str
    line: int
    target: str
    kind: str


def _front_matter_dependencies(text: str) -> list[tuple[int, str]]:
    lines = text.splitlines()
    if not lines or lines[0].strip() != "---":
        return []

    dependencies: list[tuple[int, str]] = []
    in_dependencies = False
    for line_number, line in enumerate(lines[1:], start=2):
        if line.strip() == "---":
            break
        if line == "depends_on:":
            in_dependencies = True
            continue
        if in_dependencies and line.startswith("  - "):
            dependencies.append((line_number, line[4:].strip()))
            continue
        if in_dependencies and line and not line.startswith(" "):
            in_dependencies = False
    return dependencies


def _markdown_links(text: str) -> list[tuple[int, str]]:
    links: list[tuple[int, str]] = []
    for line_number, line in enumerate(text.splitlines(), start=1):
        for match in MARKDOWN_LINK_RE.finditer(line):
            target = match.group("target").strip()
            if target.startswith("<") and target.endswith(">"):
                target = target[1:-1]
            links.append((line_number, target))
    return links


def _resolve_target(source: str, target: str) -> str | None:
    value = target.strip()
    if not value or value.startswith("#"):
        return None

    parsed = urlsplit(value)
    if parsed.scheme.lower() in EXTERNAL_SCHEMES or parsed.netloc:
        return None

    path = unquote(parsed.path)
    if not path:
        return None
    if path.startswith("/"):
        resolved = posixpath.normpath(path.lstrip("/"))
    else:
        resolved = posixpath.normpath(
            posixpath.join(str(PurePosixPath(source).parent), path)
        )
    if resolved == ".." or resolved.startswith("../"):
        return ""
    return resolved


def _target_exists(target: str, tracked: set[str]) -> bool:
    if target in tracked:
        return True
    prefix = target.rstrip("/") + "/"
    return any(path.startswith(prefix) for path in tracked)


def audit_docs(repo_root) -> tuple[int, tuple[BrokenReference, ...]]:
    paths = git_tracked_files(repo_root)
    tracked = set(paths)
    markdown_paths = sorted(path for path in paths if path.lower().endswith(".md"))
    broken: list[BrokenReference] = []

    for source in markdown_paths:
        data = _read_index_blob(repo_root, source)
        if b"\0" in data:
            raise ReadinessError(f"Markdown 文件包含二进制内容：{source}")
        text = data.decode("utf-8", errors="replace")
        references = [
            (line, target, "Markdown 链接")
            for line, target in _markdown_links(text)
        ]
        references.extend(
            (line, target, "depends_on")
            for line, target in _front_matter_dependencies(text)
        )

        for line, target, kind in references:
            resolved = _resolve_target(source, target)
            if resolved is None:
                continue
            if not resolved or not _target_exists(resolved, tracked):
                broken.append(BrokenReference(source, line, target, kind))

    return len(markdown_paths), tuple(broken)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="检查 Markdown 本地引用")
    parser.add_argument("--repo", default=".", help="待检查的 Git 仓库")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        count, broken = audit_docs(args.repo)
    except ReadinessError as error:
        print(f"文档引用检查无法执行：{error}", file=sys.stderr)
        return 2

    if not broken:
        print(f"文档引用检查：通过（{count} 个 Markdown 文件）")
        return 0

    print(f"文档引用检查：未通过，共 {len(broken)} 个断链")
    for item in broken:
        print(f"- {item.source}:{item.line} [{item.kind}] {item.target}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
