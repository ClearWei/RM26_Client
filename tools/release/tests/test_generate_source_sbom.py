from __future__ import annotations

import hashlib
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


RELEASE_DIR = Path(__file__).resolve().parents[1]
if str(RELEASE_DIR) not in sys.path:
    sys.path.insert(0, str(RELEASE_DIR))

import generate_source_sbom


class SourceSbomTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        subprocess.run(["git", "init", "--quiet"], cwd=self.root, check=True)

    def _write(self, path: str, content: str) -> None:
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content, encoding="utf-8")

    def test_document_contains_only_tracked_files_and_stable_hashes(self) -> None:
        self._write("README.md", "测试\n")
        self._write("src/main.cpp", "int main() {}\n")
        self._write("local.log", "不应进入 SBOM\n")
        subprocess.run(
            ["git", "add", "README.md", "src/main.cpp"],
            cwd=self.root,
            check=True,
        )
        os.environ["SOURCE_DATE_EPOCH"] = "0"
        self.addCleanup(os.environ.pop, "SOURCE_DATE_EPOCH", None)

        document = generate_source_sbom.build_spdx_document(self.root)

        file_names = {item["fileName"] for item in document["files"]}
        self.assertEqual({"./README.md", "./src/main.cpp"}, file_names)
        readme = next(
            item for item in document["files"] if item["fileName"] == "./README.md"
        )
        sha256 = next(
            item["checksumValue"]
            for item in readme["checksums"]
            if item["algorithm"] == "SHA256"
        )
        self.assertEqual(hashlib.sha256("测试\n".encode()).hexdigest(), sha256)
        self.assertEqual("1970-01-01T00:00:00Z", document["creationInfo"]["created"])
        self.assertEqual("NOASSERTION", document["packages"][0]["licenseDeclared"])

    def test_unmerged_index_is_rejected(self) -> None:
        self._write("conflict.txt", "base\n")
        blob = subprocess.run(
            ["git", "hash-object", "-w", "conflict.txt"],
            cwd=self.root,
            check=True,
            stdout=subprocess.PIPE,
            text=True,
        ).stdout.strip()
        subprocess.run(
            ["git", "update-index", "--index-info"],
            cwd=self.root,
            check=True,
            input=f"100644 {blob} 1\tconflict.txt\n",
            text=True,
        )

        with self.assertRaises(generate_source_sbom.SbomGenerationError):
            generate_source_sbom.build_spdx_document(self.root)


if __name__ == "__main__":
    unittest.main()
