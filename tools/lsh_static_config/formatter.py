"""Canonical TOML formatter for schema v2 configuration files."""

from __future__ import annotations

from typing import TYPE_CHECKING

from .errors import fail
from .public_schema import SCHEMA_VERSION

if TYPE_CHECKING:
    from pathlib import Path

    from .models import TomlTable, TomlValue

import tomllib

CLICK_ACTION_KEYS = {"short", "long", "super_long"}
BUTTON_ACTION_PATH_DEPTH = 4
CLICK_ACTION_FIELD_ORDER = (
    "enabled",
    "after",
    "time",
    "time_ms",
    "action",
    "target",
    "targets",
    "group",
    "groups",
    "scene",
    "network",
    "fallback",
)


def format_config_file(path: Path, *, check: bool) -> bool:
    """Format one schema v2 TOML file, returning true when it was stale."""
    root = _read_root(path)
    existing = path.read_text(encoding="utf-8")
    formatted = render_toml(root, schema_directive=_schema_directive(existing))
    if existing == formatted:
        return False
    if check:
        return True
    path.write_text(formatted, encoding="utf-8")
    return True


def render_toml(root: TomlTable, *, schema_directive: str | None = None) -> str:
    """Render a public schema v2 TOML tree in deterministic table order."""
    lines: list[str] = []
    if schema_directive is not None:
        lines.extend((schema_directive, ""))

    scalar_items = [(key, value) for key, value in root.items() if not _is_table(value)]
    table_items = [(key, value) for key, value in root.items() if _is_table(value)]

    for key, value in scalar_items:
        lines.append(f"{key} = {_render_value(value)}")
    if scalar_items and table_items:
        lines.append("")
    for index, (key, value) in enumerate(table_items):
        if index > 0:
            lines.append("")
        _render_table(lines, [key], _as_table(value))
    return "\n".join(lines).rstrip() + "\n"


def _schema_directive(existing: str) -> str | None:
    """Preserve Taplo/VS Code schema hints across canonical formatting."""
    for line in existing.splitlines():
        stripped = line.strip()
        if stripped.startswith("#:schema"):
            return stripped
        if stripped and not stripped.startswith("#"):
            return None
    return None


def _read_root(path: Path) -> TomlTable:
    """Read and minimally validate one schema v2 TOML file."""
    try:
        with path.open("rb") as file_obj:
            root = tomllib.load(file_obj)
    except OSError as exc:
        fail(f"Cannot read {path}: {exc}")
    except tomllib.TOMLDecodeError as exc:
        fail(f"Invalid TOML in {path}: {exc}")
    table = _as_table(root)
    if table.get("schema_version") != SCHEMA_VERSION:
        fail(f"schema_version must be {SCHEMA_VERSION}.")
    return table


def _render_table(lines: list[str], path: list[str], table: TomlTable) -> None:
    """Render one TOML table and its children."""
    scalar_items: list[tuple[str, TomlValue]] = []
    child_items: list[tuple[str, TomlTable]] = []
    for key, value in table.items():
        compact_value = _compact_click_action_value(path, key, value)
        if compact_value is not None:
            scalar_items.append((key, compact_value))
        elif _is_table(value):
            child_items.append((key, _as_table(value)))
        else:
            scalar_items.append((key, value))

    rendered_header = bool(scalar_items or not child_items)
    if rendered_header:
        lines.append(f"[{'.'.join(path)}]")
    for key, value in scalar_items:
        lines.append(f"{key} = {_render_value(value)}")
    for index, (key, value) in enumerate(child_items):
        if rendered_header or index > 0:
            lines.append("")
        _render_table(lines, [*path, key], value)


def _compact_click_action_value(
    path: list[str],
    key: str,
    value: TomlValue,
) -> TomlValue | None:
    """Render simple button actions inline instead of as child tables."""
    if (
        key not in CLICK_ACTION_KEYS
        or len(path) < BUTTON_ACTION_PATH_DEPTH
        or path[-2] != "buttons"
    ):
        return None
    if not isinstance(value, dict):
        return None

    table = _as_table(value)
    if table == {"enabled": False}:
        return False

    if key in {"short", "long"}:
        targets = _action_targets(table)
        if targets is not None and _only_default_toggle_fields(table):
            return targets[0] if len(targets) == 1 else targets

    return dict(_ordered_action_items(table))


def _action_targets(table: TomlTable) -> list[str] | None:
    """Return inline target lists when the action table uses one target spelling."""
    has_target = "target" in table
    has_targets = "targets" in table
    if has_target == has_targets:
        return None
    raw_targets = table["target"] if has_target else table["targets"]
    if isinstance(raw_targets, str):
        return [raw_targets]
    if (
        isinstance(raw_targets, list)
        and raw_targets
        and all(isinstance(target, str) for target in raw_targets)
    ):
        return raw_targets
    return None


def _only_default_toggle_fields(table: TomlTable) -> bool:
    """Return true when a short/long table is just a default toggle target."""
    allowed = {"target", "targets", "action", "enabled"}
    if any(key not in allowed for key in table):
        return False
    if table.get("enabled", True) is not True:
        return False
    action = table.get("action", "toggle")
    return action in {"toggle", "normal"}


def _ordered_action_items(table: TomlTable) -> list[tuple[str, TomlValue]]:
    """Use a stable, human-first order for inline action tables."""
    ordered: list[tuple[str, TomlValue]] = [
        (key, table[key]) for key in CLICK_ACTION_FIELD_ORDER if key in table
    ]
    ordered.extend(
        (key, value)
        for key, value in table.items()
        if key not in CLICK_ACTION_FIELD_ORDER
    )
    return ordered


def _render_value(value: TomlValue) -> str:
    """Render one TOML scalar or array value."""
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, str):
        return _quote(value)
    if isinstance(value, list):
        return "[" + ", ".join(_render_value(item) for item in value) + "]"
    if isinstance(value, dict):
        return _render_inline_table(_as_table(value))
    fail(f"Cannot format TOML value of type {type(value).__name__}.")
    raise AssertionError


def _render_inline_table(table: TomlTable) -> str:
    """Render one compact TOML inline table."""
    items = [f"{key} = {_render_value(value)}" for key, value in table.items()]
    return "{ " + ", ".join(items) + " }"


def _quote(value: str) -> str:
    """Return a TOML basic string."""
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def _is_table(value: TomlValue) -> bool:
    """Return true when a parsed TOML value is a table."""
    return isinstance(value, dict)


def _as_table(value: TomlValue) -> TomlTable:
    """Cast parsed TOML dictionaries for type checkers."""
    if not isinstance(value, dict):
        fail("expected a TOML table.")
    return value
