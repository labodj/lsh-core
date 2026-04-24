"""PlatformIO-facing define and environment helpers."""

from __future__ import annotations

import re
from typing import TYPE_CHECKING

from .errors import fail
from .models import DefineValue

if TYPE_CHECKING:
    from .models import DeviceConfig, ProjectConfig


def merged_defines(project: ProjectConfig, device: DeviceConfig) -> list[DefineValue]:
    """Merge common and device defines into PlatformIO-compatible values."""
    merged = dict(project.common_defines)
    merged.update(device.defines)
    defines = [DefineValue(device.build_macro)]
    for name in sorted(merged):
        value = merged[name]
        if value is False:
            continue
        if value is True:
            defines.append(DefineValue(name))
        elif isinstance(value, int) and not isinstance(value, bool):
            defines.append(DefineValue(name, str(value)))
        elif isinstance(value, str):
            defines.append(DefineValue(name, value))
        else:
            fail(f"define {name} must be boolean, integer, or string.")
    return defines


def raw_build_flags(project: ProjectConfig, device: DeviceConfig) -> list[str]:
    """Return common plus device-specific raw build flags."""
    return list(project.common_raw_build_flags) + list(device.raw_build_flags)


def resolve_device_key(project: ProjectConfig, requested: str) -> str:
    """Resolve a CLI/PlatformIO device selector to a normalized device key."""
    normalized = requested.lower()
    if normalized in project.devices:
        return normalized
    for key, device in project.devices.items():
        if requested == device.build_macro:
            return key
    choices = ", ".join(project.devices)
    fail(f"Unknown lsh-core device {requested!r}. Available devices: {choices}.")
    raise AssertionError


def infer_device_from_env(project: ProjectConfig, env_name: str) -> str:
    """Infer a device key from common PlatformIO environment naming schemes."""
    candidates = [
        env_name,
        env_name.lower(),
        re.sub(r"_(release|debug)$", "", env_name.lower()),
        re.sub(r"-(release|debug)$", "", env_name.lower()),
    ]
    for candidate in candidates:
        if candidate in project.devices:
            return candidate
    fail(f"Cannot infer custom_lsh_device from PlatformIO environment {env_name!r}.")
    raise AssertionError
