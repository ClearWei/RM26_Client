#!/usr/bin/env python3
"""检查公开示例配置是否完整、脱敏并适合本地体验。"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Sequence
from urllib.parse import urlsplit


REQUIRED_ROOT_OBJECTS = (
    "app_settings",
    "window",
    "ui_text",
    "network",
    "video",
    "robots",
    "ar_overlay",
)

REQUIRED_UI_TEXT = (
    "window_title",
    "start_game",
    "pause_game",
    "quit_game",
    "connect_server",
    "disconnect_server",
    "status_connected",
    "status_disconnected",
)

SENSITIVE_KEYS = {
    "api_key",
    "access_token",
    "credential",
    "credentials",
    "identity_file",
    "password",
    "passwd",
    "private_key",
    "secret",
    "ssh_key",
    "token",
}

WINDOWS_ABSOLUTE_RE = re.compile(r"^[A-Za-z]:[\\/]")
RESOLUTION_RE = re.compile(r"^[1-9]\d*x[1-9]\d*$")


@dataclass(frozen=True)
class ConfigIssue:
    code: str
    path: str
    message: str


def render_issue(issue: ConfigIssue) -> str:
    """把单条问题整理成稳定、便于 CI 阅读的文本。"""

    return f"{issue.path}: {issue.message} [{issue.code}]"


def _object(
    parent: dict[str, object],
    key: str,
    issues: list[ConfigIssue],
    *,
    path: str = "",
) -> dict[str, object] | None:
    value = parent.get(key)
    current_path = f"{path}.{key}" if path else key
    if not isinstance(value, dict):
        issues.append(ConfigIssue("type:object", current_path, "必须是 JSON 对象"))
        return None
    return value


def _string(
    parent: dict[str, object],
    key: str,
    issues: list[ConfigIssue],
    *,
    path: str,
    allow_empty: bool = False,
) -> str | None:
    value = parent.get(key)
    current_path = f"{path}.{key}"
    if not isinstance(value, str):
        issues.append(ConfigIssue("type:string", current_path, "必须是字符串"))
        return None
    if not allow_empty and not value.strip():
        issues.append(ConfigIssue("value:empty", current_path, "不能为空"))
        return None
    return value


def _integer(
    parent: dict[str, object],
    key: str,
    issues: list[ConfigIssue],
    *,
    path: str,
    minimum: int,
    maximum: int,
) -> int | None:
    value = parent.get(key)
    current_path = f"{path}.{key}"
    if isinstance(value, bool) or not isinstance(value, int):
        issues.append(ConfigIssue("type:integer", current_path, "必须是整数"))
        return None
    if not minimum <= value <= maximum:
        issues.append(
            ConfigIssue(
                "value:range",
                current_path,
                f"必须位于 {minimum} 到 {maximum} 之间",
            )
        )
        return None
    return value


def _number_between_zero_and_one(
    parent: dict[str, object], key: str, issues: list[ConfigIssue], *, path: str
) -> None:
    value = parent.get(key)
    current_path = f"{path}.{key}"
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        issues.append(ConfigIssue("type:number", current_path, "必须是数字"))
        return
    if not 0.0 <= float(value) <= 1.0:
        issues.append(ConfigIssue("value:range", current_path, "必须位于 0 到 1 之间"))


def _safe_relative_path(
    value: str | None, path: str, issues: list[ConfigIssue]
) -> None:
    if value is None or not value:
        return
    normalized = value.replace("\\", "/")
    if (
        normalized.startswith(("/", "~", "//"))
        or WINDOWS_ABSOLUTE_RE.match(value)
        or ".." in PurePosixPath(normalized).parts
        or urlsplit(value).scheme
    ):
        issues.append(
            ConfigIssue(
                "path:unsafe",
                path,
                "只能使用不含 .. 的仓库相对路径，或留空",
            )
        )


def _sensitive_key_issues(
    value: object, issues: list[ConfigIssue], *, path: str = "root"
) -> None:
    if isinstance(value, dict):
        for raw_key, child in value.items():
            key = str(raw_key)
            normalized = key.strip().lower().replace("-", "_")
            current_path = f"{path}.{key}"
            if normalized in SENSITIVE_KEYS or any(
                normalized.endswith(f"_{candidate}") for candidate in SENSITIVE_KEYS
            ):
                issues.append(
                    ConfigIssue(
                        "secret:field",
                        current_path,
                        "公开示例不得包含凭据或密钥字段",
                    )
                )
            _sensitive_key_issues(child, issues, path=current_path)
    elif isinstance(value, list):
        for index, child in enumerate(value):
            _sensitive_key_issues(child, issues, path=f"{path}[{index}]")


def _validate_window(window: dict[str, object], issues: list[ConfigIssue]) -> None:
    min_width = _integer(
        window, "min_width", issues, path="window", minimum=320, maximum=16384
    )
    min_height = _integer(
        window, "min_height", issues, path="window", minimum=240, maximum=16384
    )
    default_width = _integer(
        window, "default_width", issues, path="window", minimum=320, maximum=16384
    )
    default_height = _integer(
        window, "default_height", issues, path="window", minimum=240, maximum=16384
    )
    if min_width is not None and default_width is not None and default_width < min_width:
        issues.append(
            ConfigIssue("window:width", "window.default_width", "不能小于 min_width")
        )
    if (
        min_height is not None
        and default_height is not None
        and default_height < min_height
    ):
        issues.append(
            ConfigIssue(
                "window:height", "window.default_height", "不能小于 min_height"
            )
        )
    if not isinstance(window.get("fullscreen"), bool):
        issues.append(
            ConfigIssue("type:boolean", "window.fullscreen", "必须是布尔值")
        )


def _validate_network(
    network: dict[str, object], issues: list[ConfigIssue]
) -> int | None:
    for key in ("server_ip", "mqtt_broker"):
        value = _string(network, key, issues, path="network")
        if value is not None and value != "127.0.0.1":
            issues.append(
                ConfigIssue(
                    "network:not-loopback",
                    f"network.{key}",
                    "公开示例只能连接 127.0.0.1",
                )
            )

    for key in ("server_port", "client_port", "mqtt_port"):
        _integer(network, key, issues, path="network", minimum=1, maximum=65535)
    video_port = _integer(
        network, "video_port", issues, path="network", minimum=1, maximum=65535
    )
    robot_id = _integer(
        network,
        "client_robot_id",
        issues,
        path="network",
        minimum=1,
        maximum=255,
    )
    if robot_id is not None and not (
        1 <= robot_id <= 7 or 101 <= robot_id <= 107
    ):
        issues.append(
            ConfigIssue(
                "network:robot-id",
                "network.client_robot_id",
                "必须是红方 1..7 或蓝方 101..107",
            )
        )
    return video_port


def _validate_video(
    video: dict[str, object], video_port: int | None, issues: list[ConfigIssue]
) -> None:
    default_path = _string(
        video, "default_path", issues, path="video", allow_empty=True
    )
    _safe_relative_path(default_path, "video.default_path", issues)

    stream_url = _string(video, "stream_url", issues, path="video")
    if stream_url is not None:
        try:
            parsed = urlsplit(stream_url)
            parsed_port = parsed.port
        except ValueError:
            parsed = None
            parsed_port = None
        if (
            parsed is None
            or parsed.scheme != "udp"
            or parsed.hostname not in {"0.0.0.0", "127.0.0.1"}
            or parsed.username is not None
            or parsed.password is not None
            or parsed.path not in {"", "/"}
            or parsed.query
            or parsed.fragment
            or parsed_port is None
        ):
            issues.append(
                ConfigIssue(
                    "video:url",
                    "video.stream_url",
                    "必须是 udp://0.0.0.0:<端口> 或回环地址",
                )
            )
        elif video_port is not None and parsed_port != video_port:
            issues.append(
                ConfigIssue(
                    "video:port-mismatch",
                    "video.stream_url",
                    "端口必须与 network.video_port 一致",
                )
            )

    resolution = _string(video, "resolution", issues, path="video")
    if resolution is not None and not RESOLUTION_RE.fullmatch(resolution):
        issues.append(
            ConfigIssue(
                "video:resolution",
                "video.resolution",
                "必须使用 1280x720 这类宽x高格式",
            )
        )
    _integer(video, "fps", issues, path="video", minimum=1, maximum=240)


def _validate_robots(
    robots: dict[str, object], issues: list[ConfigIssue]
) -> None:
    if not robots:
        issues.append(ConfigIssue("robots:empty", "robots", "至少需要一个机器人配置"))
        return
    for robot_name, raw_config in robots.items():
        path = f"robots.{robot_name}"
        if not isinstance(raw_config, dict):
            issues.append(ConfigIssue("type:object", path, "必须是 JSON 对象"))
            continue
        _string(raw_config, "description", issues, path=path)
        _string(raw_config, "ui_layout", issues, path=path)

        modules = raw_config.get("modules")
        if not isinstance(modules, list) or not modules or not all(
            isinstance(item, str) and item.strip() for item in modules
        ):
            issues.append(
                ConfigIssue(
                    "robots:modules",
                    f"{path}.modules",
                    "必须是非空字符串数组",
                )
            )

        bindings = raw_config.get("key_bindings")
        if not isinstance(bindings, dict) or not all(
            isinstance(key, str)
            and key.strip()
            and isinstance(value, str)
            and value.strip()
            for key, value in bindings.items()
        ):
            issues.append(
                ConfigIssue(
                    "robots:key-bindings",
                    f"{path}.key_bindings",
                    "必须是字符串到字符串的对象",
                )
            )


def _validate_ar(ar_overlay: dict[str, object], issues: list[ConfigIssue]) -> None:
    if not isinstance(ar_overlay.get("enabled"), bool):
        issues.append(
            ConfigIssue("type:boolean", "ar_overlay.enabled", "必须是布尔值")
        )
    model_path = _string(
        ar_overlay, "model_path", issues, path="ar_overlay", allow_empty=True
    )
    _safe_relative_path(model_path, "ar_overlay.model_path", issues)
    for key in ("confidence_threshold", "nms_threshold", "smoothing_factor"):
        _number_between_zero_and_one(ar_overlay, key, issues, path="ar_overlay")
    _integer(
        ar_overlay,
        "max_missed_frames",
        issues,
        path="ar_overlay",
        minimum=0,
        maximum=10000,
    )
    _integer(
        ar_overlay,
        "detection_interval_ms",
        issues,
        path="ar_overlay",
        minimum=1,
        maximum=60000,
    )


def validate_example_config(payload: object) -> tuple[ConfigIssue, ...]:
    """返回示例配置中的全部问题；空元组表示通过。"""

    issues: list[ConfigIssue] = []
    if not isinstance(payload, dict):
        return (ConfigIssue("type:root", "root", "顶层必须是 JSON 对象"),)

    _sensitive_key_issues(payload, issues)
    sections: dict[str, dict[str, object]] = {}
    for key in REQUIRED_ROOT_OBJECTS:
        section = _object(payload, key, issues)
        if section is not None:
            sections[key] = section

    app_settings = sections.get("app_settings")
    if app_settings is not None:
        _string(app_settings, "client_name", issues, path="app_settings")
        _string(app_settings, "version", issues, path="app_settings")

    window = sections.get("window")
    if window is not None:
        _validate_window(window, issues)

    ui_text = sections.get("ui_text")
    if ui_text is not None:
        for key in REQUIRED_UI_TEXT:
            _string(ui_text, key, issues, path="ui_text")

    video_port: int | None = None
    network = sections.get("network")
    if network is not None:
        video_port = _validate_network(network, issues)

    video = sections.get("video")
    if video is not None:
        _validate_video(video, video_port, issues)

    robots = sections.get("robots")
    if robots is not None:
        _validate_robots(robots, issues)

    ar_overlay = sections.get("ar_overlay")
    if ar_overlay is not None:
        _validate_ar(ar_overlay, issues)

    return tuple(issues)


def load_json(path: Path) -> object:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except OSError as error:
        raise RuntimeError(f"无法读取 {path}：{error}") from error
    except json.JSONDecodeError as error:
        raise RuntimeError(f"{path} 不是合法 JSON：{error}") from error


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="检查 RM26 公开示例配置")
    parser.add_argument(
        "config",
        nargs="?",
        type=Path,
        default=Path("config.example.json"),
        help="示例配置路径，默认使用仓库根目录 config.example.json",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        payload = load_json(args.config)
    except RuntimeError as error:
        print(f"示例配置检查无法执行：{error}", file=sys.stderr)
        return 2

    issues = validate_example_config(payload)
    if not issues:
        print(f"示例配置检查：通过（{args.config}）")
        return 0

    print(f"示例配置检查：未通过，共 {len(issues)} 项")
    for issue in issues:
        print(f"- {render_issue(issue)}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
