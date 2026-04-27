"""Guided starter TOML scaffolding for new lsh-core consumer projects."""

from __future__ import annotations

import json
from dataclasses import dataclass
from typing import TYPE_CHECKING

from .constants import IDENTIFIER_RE
from .errors import fail
from .json_schema import write_project_schema
from .presets import PRESETS

if TYPE_CHECKING:
    from pathlib import Path

MAX_SCAFFOLD_RESOURCES = 64
MIN_GROUP_RELAYS = 2


@dataclass(frozen=True)
class ScaffoldOptions:
    """User-facing knobs for the non-interactive config scaffold."""

    preset: str = "controllino-maxi/fast-msgpack"
    device_key: str = "my_device"
    device_name: str | None = None
    relays: int = 4
    buttons: int = 4
    indicators: int = 1
    force: bool = False
    write_schema: bool = True


def render_scaffold(options: ScaffoldOptions) -> str:
    """Render a small, valid, production-shaped schema v2 TOML profile."""
    _validate_options(options)
    device_name = options.device_name or options.device_key
    relay_names = [f"relay{index}" for index in range(1, options.relays + 1)]
    button_names = [f"button{index}" for index in range(1, options.buttons + 1)]
    indicator_names = [
        f"indicator{index}" for index in range(1, options.indicators + 1)
    ]

    lines = [
        "#:schema ./.vscode/lsh_devices.schema.json",
        "",
        "schema_version = 2",
        f"preset = {_toml_string(options.preset)}",
        "",
        "[generator]",
        'output_dir = "include"',
        'config_dir = "lsh_configs"',
        "",
        f"[devices.{options.device_key}]",
        f"name = {_toml_string(device_name)}",
    ]
    _append_actuators(lines, options, relay_names)
    _append_groups(lines, options, relay_names)
    _append_buttons(lines, options, relay_names, button_names)
    _append_indicators(lines, options, relay_names, indicator_names)
    return "\n".join(lines).rstrip() + "\n"


def write_scaffold(path: Path, options: ScaffoldOptions) -> Path:
    """Write a starter config and its editor schema, returning the TOML path."""
    config_path = path.resolve()
    if config_path.exists() and not options.force:
        fail(f"{config_path} already exists; pass --force to overwrite it.")
    config_path.parent.mkdir(parents=True, exist_ok=True)
    config_path.write_text(render_scaffold(options), encoding="utf-8")
    if options.write_schema:
        write_project_schema(config_path)
    return config_path


def _validate_options(options: ScaffoldOptions) -> None:
    """Validate scaffold knobs before writing files."""
    if options.preset not in PRESETS:
        choices = ", ".join(sorted(PRESETS))
        fail(f"--preset must be one of: {choices}.")
    _validate_identifier(options.device_key, "--device-key")
    for field_name, value in (
        ("--relays", options.relays),
        ("--buttons", options.buttons),
        ("--indicators", options.indicators),
    ):
        if isinstance(value, bool) or value < 0 or value > MAX_SCAFFOLD_RESOURCES:
            fail(f"{field_name} must be between 0 and {MAX_SCAFFOLD_RESOURCES}.")


def _validate_identifier(value: str, path: str) -> None:
    """Keep generated table keys compatible with C++ object names."""
    if not IDENTIFIER_RE.fullmatch(value):
        fail(f"{path} must be a valid C++ identifier.")


def _append_actuators(
    lines: list[str],
    options: ScaffoldOptions,
    relay_names: list[str],
) -> None:
    """Append relay actuator tables."""
    for index, relay_name in enumerate(relay_names):
        lines.extend(
            [
                "",
                f"[devices.{options.device_key}.actuators.{relay_name}]",
                f"pin = {_toml_string(_relay_pin(options.preset, index))}",
            ]
        )


def _append_groups(
    lines: list[str],
    options: ScaffoldOptions,
    relay_names: list[str],
) -> None:
    """Append a useful room-level group when there is more than one relay."""
    if len(relay_names) < MIN_GROUP_RELAYS:
        return
    lines.extend(
        [
            "",
            f"[devices.{options.device_key}.groups.all_relays]",
            f"targets = {_toml_array(relay_names)}",
        ]
    )


def _append_buttons(
    lines: list[str],
    options: ScaffoldOptions,
    relay_names: list[str],
    button_names: list[str],
) -> None:
    """Append button tables with safe defaults and a group-off example."""
    for index, button_name in enumerate(button_names):
        lines.extend(
            [
                "",
                f"[devices.{options.device_key}.buttons.{button_name}]",
                f"pin = {_toml_string(_button_pin(options.preset, index))}",
            ]
        )
        if not relay_names:
            lines.append("short = false")
            continue
        target = relay_names[index % len(relay_names)]
        lines.append(f"short = {_toml_string(target)}")
        if index == 0 and len(relay_names) > 1:
            lines.append('long = { action = "off", group = "all_relays" }')


def _append_indicators(
    lines: list[str],
    options: ScaffoldOptions,
    relay_names: list[str],
    indicator_names: list[str],
) -> None:
    """Append indicator tables only when there is a relay state to mirror."""
    if not relay_names:
        return
    for index, indicator_name in enumerate(indicator_names):
        lines.extend(
            [
                "",
                f"[devices.{options.device_key}.indicators.{indicator_name}]",
                f"pin = {_toml_string(_indicator_pin(options.preset, index))}",
            ]
        )
        if index < len(relay_names):
            lines.append(f"when = {_toml_string(relay_names[index])}")
        else:
            lines.append(f"when = {{ any = {_toml_array(relay_names)} }}")


def _relay_pin(preset: str, index: int) -> str:
    """Return a readable relay pin for the selected board family."""
    if preset.startswith("controllino-"):
        return f"R{index}"
    return str(6 + index)


def _button_pin(preset: str, index: int) -> str:
    """Return a readable input pin for the selected board family."""
    if preset.startswith("controllino-"):
        return f"A{index}"
    return str(2 + index)


def _indicator_pin(preset: str, index: int) -> str:
    """Return a readable indicator pin for the selected board family."""
    if preset.startswith("controllino-"):
        return f"D{index}"
    return str(13 - index)


def _toml_string(value: str) -> str:
    """Render a TOML basic string."""
    return json.dumps(value)


def _toml_array(values: list[str]) -> str:
    """Render a compact TOML string array."""
    return "[" + ", ".join(_toml_string(value) for value in values) + "]"
