#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
HEVC Annex-B 码流访问单元提取器。

将 FFmpeg 输出的连续 HEVC Annex-B 裸码流按访问单元（近似一帧）
切分，避免把任意大小的 stdout 数据块误当成完整视频帧发送。
"""

from __future__ import annotations

from typing import List, Union


class HevcAnnexBFrameExtractor:
    def __init__(self) -> None:
        self._buffer = bytearray()
        self._current_access_unit = bytearray()
        self._has_vcl = False

    def reset(self) -> None:
        self._buffer.clear()
        self._current_access_unit.clear()
        self._has_vcl = False

    def push(self, data: bytes) -> List[bytes]:
        if not data:
            return []

        self._buffer.extend(data)
        frames: List[bytes] = []

        while True:
            start = self._find_start_code(self._buffer, 0)
            if start < 0:
                if len(self._buffer) > 3:
                    del self._buffer[:-3]
                break

            if start > 0:
                del self._buffer[:start]

            next_start = self._find_start_code(
                self._buffer, self._start_code_len(self._buffer, 0)
            )
            if next_start < 0:
                break

            nal = bytes(self._buffer[:next_start])
            del self._buffer[:next_start]
            frames.extend(self._consume_nal(nal))

        return frames

    def flush(self) -> List[bytes]:
        frames: List[bytes] = []

        start = self._find_start_code(self._buffer, 0)
        if start >= 0:
            if start > 0:
                del self._buffer[:start]
            if self._buffer:
                frames.extend(self._consume_nal(bytes(self._buffer)))

        self._buffer.clear()

        if self._current_access_unit:
            frames.append(bytes(self._current_access_unit))
            self._current_access_unit.clear()
            self._has_vcl = False

        return frames

    def _consume_nal(self, nal: bytes) -> List[bytes]:
        frames: List[bytes] = []
        start_len = self._start_code_len(nal, 0)
        payload = nal[start_len:]
        if len(payload) < 2:
            return frames

        nal_type = (payload[0] >> 1) & 0x3F
        is_vcl = nal_type <= 31
        first_slice = is_vcl and len(payload) >= 3 and (payload[2] & 0x80) != 0

        if self._has_vcl and (
            nal_type in {32, 33, 34, 35, 39} or (is_vcl and first_slice)
        ):
            frames.append(bytes(self._current_access_unit))
            self._current_access_unit.clear()
            self._has_vcl = False

        self._current_access_unit.extend(nal)
        if is_vcl:
            self._has_vcl = True

        return frames

    @staticmethod
    def _find_start_code(data: bytearray, start: int) -> int:
        limit = len(data) - 3
        index = max(0, start)
        while index <= limit:
            if data[index] == 0x00 and data[index + 1] == 0x00:
                if data[index + 2] == 0x01:
                    return index
                if (
                    index + 3 < len(data)
                    and data[index + 2] == 0x00
                    and data[index + 3] == 0x01
                ):
                    return index
            index += 1
        return -1

    @staticmethod
    def _start_code_len(data: Union[bytes, bytearray], index: int) -> int:
        if index + 3 < len(data) and data[index : index + 4] == b"\x00\x00\x00\x01":
            return 4
        return 3
