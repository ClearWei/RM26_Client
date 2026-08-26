from __future__ import annotations

import hashlib
import sys
import tempfile
import unittest
from pathlib import Path


RELEASE_DIR = Path(__file__).resolve().parents[1]
if str(RELEASE_DIR) not in sys.path:
    sys.path.insert(0, str(RELEASE_DIR))

import generate_checksums


class GenerateChecksumsTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def test_lines_are_sorted_and_sha256_compatible(self) -> None:
        beta = self.root / "beta.zip"
        alpha = self.root / "alpha.tar.gz"
        beta.write_bytes(b"beta")
        alpha.write_bytes(b"alpha")

        lines = generate_checksums.checksum_lines([beta, alpha])

        self.assertEqual(
            (
                f"{hashlib.sha256(b'alpha').hexdigest()}  alpha.tar.gz",
                f"{hashlib.sha256(b'beta').hexdigest()}  beta.zip",
            ),
            lines,
        )

    def test_duplicate_names_are_rejected(self) -> None:
        first = self.root / "first" / "client.zip"
        second = self.root / "second" / "client.zip"
        first.parent.mkdir()
        second.parent.mkdir()
        first.write_bytes(b"first")
        second.write_bytes(b"second")

        with self.assertRaises(generate_checksums.ChecksumGenerationError):
            generate_checksums.checksum_lines([first, second])


if __name__ == "__main__":
    unittest.main()
