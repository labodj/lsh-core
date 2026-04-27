"""One-shot conversion from old hand-shaped TOML to public schema v2.

This module is intentionally not used by the generator.  The build path accepts
schema v2 only; migration exists solely as an explicit maintenance tool.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

from .errors import fail
from .formatter import render_toml

if TYPE_CHECKING:
    from collections.abc import Callable
    from pathlib import Path

    from .models import TomlTable, TomlValue

import tomllib

SCHEMA_VERSION = 2

DEFINE_FEATURES = {
    "CONFIG_USE_FAST_CLICKABLES": "fast_buttons",
    "CONFIG_USE_FAST_ACTUATORS": "fast_actuators",
    "CONFIG_USE_FAST_INDICATORS": "fast_indicators",
    "CONFIG_LSH_BENCH": "bench",
}

DEFINE_TIMING = {
    "CONFIG_ACTUATOR_DEBOUNCE_TIME_MS": "actuator_debounce",
    "CONFIG_CLICKABLE_DEBOUNCE_TIME_MS": "button_debounce",
    "CONFIG_CLICKABLE_SCAN_INTERVAL_MS": "scan_interval",
    "CONFIG_CLICKABLE_LONG_CLICK_TIME_MS": "long_click",
    "CONFIG_CLICKABLE_SUPER_LONG_CLICK_TIME_MS": "super_long_click",
    "CONFIG_LCNB_TIMEOUT_MS": "network_click_timeout",
    "CONFIG_PING_INTERVAL_MS": "ping_interval",
    "CONFIG_CONNECTION_TIMEOUT_MS": "connection_timeout",
    "CONFIG_BRIDGE_BOOT_RETRY_INTERVAL_MS": "bridge_boot_retry",
    "CONFIG_BRIDGE_AWAIT_STATE_TIMEOUT_MS": "bridge_state_timeout",
    "CONFIG_DELAY_AFTER_RECEIVE_MS": "post_receive_delay",
    "CONFIG_NETWORK_CLICK_CHECK_INTERVAL_MS": "network_click_check_interval",
    "CONFIG_ACTUATORS_AUTO_OFF_CHECK_INTERVAL_MS": "auto_off_check_interval",
}

DEFINE_SERIAL = {
    "CONFIG_DEBUG_SERIAL_BAUD": "debug_baud",
    "CONFIG_COM_SERIAL_BAUD": "bridge_baud",
    "CONFIG_COM_SERIAL_TIMEOUT_MS": "timeout",
    "CONFIG_COM_SERIAL_MSGPACK_FRAME_IDLE_TIMEOUT_MS": "msgpack_frame_idle_timeout",
    "CONFIG_COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP": "max_rx_payloads_per_loop",
    "CONFIG_COM_SERIAL_MAX_RX_BYTES_PER_LOOP": "max_rx_bytes_per_loop",
    "CONFIG_COM_SERIAL_FLUSH_AFTER_SEND": "flush_after_send",
}

LONG_TYPE_ACTIONS = {
    "normal": "toggle",
    "on_only": "on",
    "on-only": "on",
    "off_only": "off",
    "off-only": "off",
}

FALLBACK_ACTIONS = {
    "local": "local",
    "local_fallback": "local",
    "local-fallback": "local",
    "do_nothing": "do_nothing",
    "do-nothing": "do_nothing",
    "none": "none",
}


def migrate_file(path: Path) -> str:
    """Return a schema v2 TOML document converted from one legacy profile."""
    root = _read_root(path)
    if root.get("schema_version") == SCHEMA_VERSION:
        fail(f"{path} is already schema v2.")
    migrated = _migrate_root(root)
    return render_toml(migrated)


def _read_root(path: Path) -> TomlTable:
    """Read a TOML file for one-shot migration."""
    try:
        with path.open("rb") as file_obj:
            root = tomllib.load(file_obj)
    except OSError as exc:
        fail(f"Cannot read {path}: {exc}")
    except tomllib.TOMLDecodeError as exc:
        fail(f"Invalid TOML in {path}: {exc}")
    return _table(root, "root")


def _migrate_root(root: TomlTable) -> TomlTable:
    """Convert the old public shape into canonical schema v2."""
    common = _table(root.get("common", {}), "common")
    generator = _table(root.get("generator", {}), "generator")
    common_defines = _table(common.get("defines", {}), "common.defines")

    migrated: TomlTable = {"schema_version": 2}
    if _looks_like_controllino_fast(common, common_defines):
        migrated["preset"] = (
            "controllino-maxi/fast-msgpack"
            if common_defines.get("CONFIG_MSG_PACK") is True
            else "controllino-maxi/fast-json"
        )
    else:
        migrated["controller"] = _migrate_controller(common)

    if generator:
        migrated["generator"] = _copy_generator(generator)

    _migrate_common_options(common, common_defines, migrated)
    migrated["devices"] = _migrate_devices(
        _table(root.get("devices"), "devices"),
        common_defines,
    )
    return migrated


def _migrate_controller(common: TomlTable) -> TomlTable:
    """Migrate the old common hardware/serial table."""
    controller: TomlTable = {}
    for old_key, new_key in (
        ("hardware_include", "hardware_include"),
        ("debug_serial", "debug_serial"),
        ("com_serial", "bridge_serial"),
    ):
        if old_key in common:
            controller[new_key] = common[old_key]
    return controller


def _copy_generator(generator: TomlTable) -> TomlTable:
    """Copy generator settings that still exist in schema v2."""
    copied: TomlTable = {}
    for key in (
        "output_dir",
        "config_dir",
        "user_config_header",
        "static_config_router_header",
    ):
        if key in generator:
            copied[key] = generator[key]
    return copied


def _migrate_common_options(
    common: TomlTable,
    common_defines: TomlTable,
    migrated: TomlTable,
) -> None:
    """Migrate old common defines and build flags into schema v2 sections."""
    features: TomlTable = {}
    timing: TomlTable = {}
    serial: TomlTable = {}
    advanced_defines = dict(common_defines)

    if "CONFIG_MSG_PACK" in advanced_defines:
        features["codec"] = (
            "msgpack" if advanced_defines.pop("CONFIG_MSG_PACK") else "json"
        )
    for define_name, field_name in DEFINE_FEATURES.items():
        if define_name in advanced_defines:
            features[field_name] = advanced_defines.pop(define_name)
    for define_name, field_name in DEFINE_TIMING.items():
        if define_name in advanced_defines:
            timing[field_name] = advanced_defines.pop(define_name)
    for define_name, field_name in DEFINE_SERIAL.items():
        if define_name in advanced_defines:
            serial[field_name] = advanced_defines.pop(define_name)

    _set_if_not_empty(migrated, "features", features)
    _set_if_not_empty(migrated, "timing", timing)
    _set_if_not_empty(migrated, "serial", serial)
    _migrate_advanced(
        migrated,
        common.get("raw_build_flags", []),
        advanced_defines,
    )


def _migrate_devices(devices: TomlTable, common_defines: TomlTable) -> TomlTable:
    """Migrate all old device profiles."""
    migrated: TomlTable = {}
    for device_key, raw_device in devices.items():
        device = _table(raw_device, f"devices.{device_key}")
        migrated[device_key] = _migrate_device(device, common_defines, device_key)
    return migrated


def _migrate_device(
    device: TomlTable,
    common_defines: TomlTable,
    device_key: str,
) -> TomlTable:
    """Migrate one legacy device table."""
    migrated: TomlTable = {}
    for key in (
        "name",
        "build_macro",
        "hardware_include",
        "debug_serial",
        "config_include",
        "static_config_include",
        "disable_rtc",
        "disable_eth",
    ):
        if key in device:
            migrated[key] = device[key]
    if "com_serial" in device:
        migrated["bridge_serial"] = device["com_serial"]

    device_defines = _table(device.get("defines", {}), f"devices.{device_key}.defines")
    _migrate_device_options(device, common_defines, device_defines, migrated)
    migrated["actuators"] = _migrate_named_resources(
        device.get("actuators", []),
        f"devices.{device_key}.actuators",
        _migrate_actuator,
    )
    migrated["buttons"] = _migrate_named_resources(
        device.get("clickables", []),
        f"devices.{device_key}.clickables",
        _migrate_button,
    )
    migrated["indicators"] = _migrate_named_resources(
        device.get("indicators", []),
        f"devices.{device_key}.indicators",
        _migrate_indicator,
    )
    return migrated


def _migrate_device_options(
    device: TomlTable,
    common_defines: TomlTable,
    device_defines: TomlTable,
    migrated: TomlTable,
) -> None:
    """Migrate device-level define overrides and raw build flags."""
    changed_defines = {
        key: value
        for key, value in device_defines.items()
        if common_defines.get(key) != value
    }
    nested: TomlTable = {}
    _migrate_common_options(
        {"raw_build_flags": device.get("raw_build_flags", [])},
        changed_defines,
        nested,
    )
    for key, value in nested.items():
        migrated[key] = value


def _migrate_named_resources(
    raw_resources: TomlValue,
    path: str,
    migrate_resource: Callable[[TomlTable], TomlTable],
) -> TomlTable:
    """Convert old array-of-table resources into named resource tables."""
    resources: TomlTable = {}
    if not isinstance(raw_resources, list):
        fail(f"{path} must be an array for migration.")
    for index, raw_resource in enumerate(raw_resources):
        resource = _table(raw_resource, f"{path}[{index}]")
        name = _string(resource.get("name"), f"{path}[{index}].name")
        resources[name] = migrate_resource(resource)
    return resources


def _migrate_actuator(resource: TomlTable) -> TomlTable:
    """Migrate one actuator."""
    migrated: TomlTable = {
        "id": resource["id"],
        "pin": resource["pin"],
    }
    if "default_state" in resource:
        migrated["default"] = resource["default_state"]
    for key in ("protected", "auto_off", "auto_off_ms"):
        if key in resource:
            migrated[key] = resource[key]
    return migrated


def _migrate_button(resource: TomlTable) -> TomlTable:
    """Migrate one clickable into a schema v2 button."""
    migrated: TomlTable = {
        "id": resource["id"],
        "pin": resource["pin"],
    }
    for key in ("short", "long", "super_long"):
        if key in resource:
            migrated[key] = _migrate_action(resource[key], key)
    return migrated


def _migrate_indicator(resource: TomlTable) -> TomlTable:
    """Migrate one indicator into a schema v2 indicator."""
    mode = str(resource.get("mode", "any")).lower()
    targets = resource.get("targets", resource.get("actuators", []))
    return {
        "pin": resource["pin"],
        "when": {mode: targets},
    }


def _migrate_action(value: TomlValue, action_name: str) -> TomlValue:
    """Migrate old click action shapes into canonical public actions."""
    if isinstance(value, (bool, str, list)):
        return value
    table = _table(value, f"{action_name} action")
    migrated: TomlTable = {}
    for key in ("enabled", "network", "time", "time_ms"):
        if key in table:
            migrated[key] = table[key]
    if "fallback" in table:
        fallback = _string(table["fallback"], f"{action_name}.fallback").lower()
        migrated["fallback"] = FALLBACK_ACTIONS.get(fallback, fallback)
    if "time" in migrated:
        migrated["after"] = migrated.pop("time")
    targets = table.get("targets", table.get("actuators"))
    if targets is not None:
        migrated["targets"] = targets
    if "type" in table:
        raw_type = _string(table["type"], f"{action_name}.type").lower()
        if action_name == "super_long" and raw_type == "selective" and targets is None:
            if migrated.get("network") is True:
                return migrated
            return False
        migrated["action"] = (
            _migrate_super_long_type(raw_type)
            if action_name == "super_long"
            else LONG_TYPE_ACTIONS.get(raw_type, raw_type)
        )
    return migrated


def _migrate_super_long_type(raw_type: str) -> str:
    """Map legacy super-long action types to schema v2 actions."""
    if raw_type == "normal":
        return "all_off"
    if raw_type == "selective":
        return "off"
    return raw_type


def _looks_like_controllino_fast(common: TomlTable, defines: TomlTable) -> bool:
    """Return true when old common options match a public Controllino preset."""
    include = str(common.get("hardware_include", "")).lower()
    return (
        "controllino" in include
        and common.get("com_serial", "Serial2") == "Serial2"
        and defines.get("CONFIG_USE_FAST_CLICKABLES") is True
        and defines.get("CONFIG_USE_FAST_ACTUATORS") is True
        and defines.get("CONFIG_USE_FAST_INDICATORS") is True
    )


def _migrate_advanced(
    parent: TomlTable,
    raw_flags: TomlValue,
    defines: TomlTable,
) -> None:
    """Attach advanced build flags and remaining raw defines when needed."""
    advanced: TomlTable = {}
    if isinstance(raw_flags, list) and raw_flags:
        advanced["build_flags"] = raw_flags
    if defines:
        advanced["defines"] = defines
    _set_if_not_empty(parent, "advanced", advanced)


def _set_if_not_empty(parent: TomlTable, key: str, value: TomlTable) -> None:
    """Set a table only when it contains values."""
    if value:
        parent[key] = value


def _table(value: TomlValue | None, path: str) -> TomlTable:
    """Return a parsed TOML table for migration."""
    if not isinstance(value, dict):
        fail(f"{path} must be a TOML table.")
    return value


def _string(value: TomlValue | None, path: str) -> str:
    """Return a string value for migration."""
    if not isinstance(value, str) or value == "":
        fail(f"{path} must be a non-empty string.")
    return value
