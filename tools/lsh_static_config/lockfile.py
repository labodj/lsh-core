"""Stable public-ID lockfile support for schema v2 profiles.

The user-facing TOML may omit actuator and button IDs while sketching a device.
This module keeps those IDs stable across later edits by storing the assigned
wire IDs in a small generated lockfile beside `lsh_devices.toml`.
"""

from __future__ import annotations

import tomllib
from dataclasses import dataclass, field
from typing import TYPE_CHECKING, cast

from .constants import UINT8_MAX
from .errors import fail

if TYPE_CHECKING:
    from pathlib import Path

    from .models import TomlTable, TomlValue

LOCK_SCHEMA_VERSION = 1
LOCKED_RESOURCE_GROUPS = ("actuators", "buttons")


@dataclass(frozen=True)
class IdLockResult:
    """Result of applying the stable-ID lockfile to one public TOML tree."""

    content: str
    auto_id_paths: list[str] = field(default_factory=list)


def apply_id_lock(root: TomlTable, lock_path: Path) -> IdLockResult:
    """Apply stable IDs to resources that omit an explicit public ID."""
    lock_root = _read_lock(lock_path)
    devices = _table(root.get("devices"), "devices")
    locked_devices = _table(lock_root.get("devices", {}), "lock.devices")

    rendered_devices: dict[str, dict[str, dict[str, int]]] = {}
    auto_id_paths: list[str] = []
    for device_key, raw_device in devices.items():
        device = _table(raw_device, f"devices.{device_key}")
        locked_device = _table(
            locked_devices.get(device_key, {}),
            f"lock.devices.{device_key}",
        )
        rendered_devices[device_key] = {}
        for group_name in LOCKED_RESOURCE_GROUPS:
            rendered_group, group_auto_paths = _apply_group_lock(
                device.get(group_name),
                _table(
                    locked_device.get(group_name, {}),
                    f"lock.devices.{device_key}.{group_name}",
                ),
                f"devices.{device_key}.{group_name}",
            )
            if rendered_group:
                rendered_devices[device_key][group_name] = rendered_group
                auto_id_paths.extend(group_auto_paths)

    return IdLockResult(
        content=_render_lock(rendered_devices),
        auto_id_paths=auto_id_paths,
    )


def _read_lock(path: Path) -> TomlTable:
    """Read an existing lockfile, or return an empty lock when none exists."""
    if not path.exists():
        return {}
    try:
        with path.open("rb") as file_obj:
            root = tomllib.load(file_obj)
    except OSError as exc:
        fail(f"Cannot read {path}: {exc}")
    except tomllib.TOMLDecodeError as exc:
        fail(f"Invalid TOML in {path}: {exc}")
    table = _table(root, "lock")
    version = table.get("schema_version")
    if version is not None and version != LOCK_SCHEMA_VERSION:
        fail(f"{path.name}.schema_version must be {LOCK_SCHEMA_VERSION}.")
    return table


def _apply_group_lock(
    raw_resources: TomlValue | None,
    locked_group: TomlTable,
    path: str,
) -> tuple[dict[str, int], list[str]]:
    """Apply one actuator/button lock table and return the new lock entries."""
    if raw_resources is None:
        return {}, []
    resources = _table(raw_resources, path)
    used_ids: set[int] = set()
    rendered: dict[str, int] = {}
    auto_paths: list[str] = []

    for resource_name, raw_resource in resources.items():
        resource = _table(raw_resource, f"{path}.{resource_name}")
        if "id" not in resource:
            continue
        explicit_id = _public_id(resource["id"], f"{path}.{resource_name}.id")
        if explicit_id in used_ids:
            fail(f"{path}.{resource_name}.id duplicates another ID: {explicit_id}.")
        used_ids.add(explicit_id)
        rendered[resource_name] = explicit_id

    for resource_name, raw_resource in resources.items():
        resource = _table(raw_resource, f"{path}.{resource_name}")
        if "id" in resource:
            continue
        auto_paths.append(f"{path}.{resource_name}.id")
        locked_id = _locked_id(locked_group, resource_name, path)
        if locked_id is not None:
            if locked_id in used_ids:
                fail(
                    f"{path}.{resource_name} lock ID {locked_id} conflicts "
                    "with another ID in the current profile."
                )
            resource["id"] = locked_id
            used_ids.add(locked_id)
            rendered[resource_name] = locked_id
            continue
        assigned_id = _next_free_id(used_ids, path)
        resource["id"] = assigned_id
        used_ids.add(assigned_id)
        rendered[resource_name] = assigned_id

    return rendered, auto_paths


def _locked_id(locked_group: TomlTable, resource_name: str, path: str) -> int | None:
    """Return a locked ID for one resource when the lockfile contains it."""
    if resource_name not in locked_group:
        return None
    return _public_id(locked_group[resource_name], f"lock.{path}.{resource_name}")


def _next_free_id(used_ids: set[int], path: str) -> int:
    """Return the smallest free public ID in the uint8 wire range."""
    for candidate in range(1, UINT8_MAX + 1):
        if candidate not in used_ids:
            return candidate
    fail(f"{path} cannot allocate more than {UINT8_MAX} public IDs.")
    raise AssertionError


def _public_id(value: TomlValue, path: str) -> int:
    """Validate one public ID value."""
    if isinstance(value, bool) or not isinstance(value, int):
        fail(f"{path} must be an integer.")
    if value < 1 or value > UINT8_MAX:
        fail(f"{path} must be between 1 and {UINT8_MAX}.")
    return value


def _table(value: TomlValue | None, path: str) -> TomlTable:
    """Return a TOML table from the user config or lockfile."""
    if not isinstance(value, dict):
        fail(f"{path} must be a TOML table.")
    return cast("TomlTable", value)


def _render_lock(devices: dict[str, dict[str, dict[str, int]]]) -> str:
    """Render the canonical generated ID lockfile."""
    lines = [
        "# Generated by lsh-core. Keep this file committed with lsh_devices.toml.",
        f"schema_version = {LOCK_SCHEMA_VERSION}",
        "",
    ]
    for device_key, groups in devices.items():
        for group_name in LOCKED_RESOURCE_GROUPS:
            entries = groups.get(group_name, {})
            if not entries:
                continue
            lines.append(f"[devices.{device_key}.{group_name}]")
            for resource_name, resource_id in entries.items():
                lines.append(f"{resource_name} = {resource_id}")
            lines.append("")
    return "\n".join(lines).rstrip() + "\n"
