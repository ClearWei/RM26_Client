from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


RELEASE_DIR = Path(__file__).resolve().parents[1]
if str(RELEASE_DIR) not in sys.path:
    sys.path.insert(0, str(RELEASE_DIR))

import check_docs


class DocumentationReferenceTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        subprocess.run(["git", "init", "--quiet"], cwd=self.root, check=True)

    def _write(self, path: str, content: str) -> None:
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content, encoding="utf-8")

    def _track_all(self) -> None:
        subprocess.run(["git", "add", "--all"], cwd=self.root, check=True)

    def test_valid_link_and_dependency_pass(self) -> None:
        self._write("README.md", "[架构](docs/architecture.md)\n")
        self._write("docs/architecture.md", "---\ndepends_on:\n  - ../README.md\n---\n")
        self._track_all()

        count, broken = check_docs.audit_docs(self.root)

        self.assertEqual(2, count)
        self.assertEqual((), broken)

    def test_broken_markdown_link_is_reported(self) -> None:
        self._write("README.md", "[不存在](docs/missing.md)\n")
        self._track_all()

        _, broken = check_docs.audit_docs(self.root)

        self.assertEqual(1, len(broken))
        self.assertEqual("Markdown 链接", broken[0].kind)

    def test_broken_dependency_is_reported(self) -> None:
        self._write("docs/page.md", "---\ndepends_on:\n  - missing.md\n---\n")
        self._track_all()

        _, broken = check_docs.audit_docs(self.root)

        self.assertEqual(1, len(broken))
        self.assertEqual("depends_on", broken[0].kind)

    def test_external_and_anchor_links_are_ignored(self) -> None:
        self._write(
            "README.md",
            "[官网](https://example.com) [章节](#section) [邮件](mailto:test@example.com)\n",
        )
        self._track_all()

        _, broken = check_docs.audit_docs(self.root)

        self.assertEqual((), broken)


if __name__ == "__main__":
    unittest.main()
