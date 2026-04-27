"""Public TOML schema normalizer for ergonomic device profiles.

The renderer intentionally consumes one compact internal model.  This module
keeps the public, user-friendly TOML dialect separate from that internal model:
schema v2 profiles are first rewritten into the same normalized shape accepted
by the original parser, then the existing validation and code generation path
does the rest.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING, cast

from .constants import (
    DURATION_RE,
    INDICATOR_MODES,
    UINT8_MAX,
)
from .errors import fail
from .lockfile import apply_id_lock
from .presets import resolve_preset

if TYPE_CHECKING:
    from pathlib import Path

    from .models import TomlArray, TomlTable, TomlValue

SCHEMA_VERSION = 2

CODECS = {
    "json": False,
    "msgpack": True,
}

CONTROLLINO_PIN_PREFIXES = {"R", "A", "D", "IN"}

FEATURE_DEFINE_MAP = {
    "fast_buttons": "CONFIG_USE_FAST_CLICKABLES",
    "fast_actuators": "CONFIG_USE_FAST_ACTUATORS",
    "fast_indicators": "CONFIG_USE_FAST_INDICATORS",
    "bench": "CONFIG_LSH_BENCH",
}

TIMING_DEFINE_MAP = {
    "actuator_debounce": ("CONFIG_ACTUATOR_DEBOUNCE_TIME_MS", True),
    "button_debounce": ("CONFIG_CLICKABLE_DEBOUNCE_TIME_MS", True),
    "scan_interval": ("CONFIG_CLICKABLE_SCAN_INTERVAL_MS", False),
    "long_click": ("CONFIG_CLICKABLE_LONG_CLICK_TIME_MS", False),
    "super_long_click": ("CONFIG_CLICKABLE_SUPER_LONG_CLICK_TIME_MS", False),
    "network_click_timeout": ("CONFIG_LCNB_TIMEOUT_MS", False),
    "ping_interval": ("CONFIG_PING_INTERVAL_MS", False),
    "connection_timeout": ("CONFIG_CONNECTION_TIMEOUT_MS", False),
    "bridge_boot_retry": ("CONFIG_BRIDGE_BOOT_RETRY_INTERVAL_MS", False),
    "bridge_state_timeout": ("CONFIG_BRIDGE_AWAIT_STATE_TIMEOUT_MS", False),
    "post_receive_delay": ("CONFIG_DELAY_AFTER_RECEIVE_MS", True),
    "network_click_check_interval": (
        "CONFIG_NETWORK_CLICK_CHECK_INTERVAL_MS",
        False,
    ),
    "auto_off_check_interval": (
        "CONFIG_ACTUATORS_AUTO_OFF_CHECK_INTERVAL_MS",
        False,
    ),
}

SERIAL_DEFINE_MAP = {
    "debug_baud": "CONFIG_DEBUG_SERIAL_BAUD",
    "bridge_baud": "CONFIG_COM_SERIAL_BAUD",
    "timeout": "CONFIG_COM_SERIAL_TIMEOUT_MS",
    "msgpack_frame_idle_timeout": "CONFIG_COM_SERIAL_MSGPACK_FRAME_IDLE_TIMEOUT_MS",
    "max_rx_payloads_per_loop": "CONFIG_COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP",
    "max_rx_bytes_per_loop": "CONFIG_COM_SERIAL_MAX_RX_BYTES_PER_LOOP",
    "flush_after_send": "CONFIG_COM_SERIAL_FLUSH_AFTER_SEND",
}

SERIAL_BUFFER_FLAG_MAP = {
    "rx_buffer_size": "SERIAL_RX_BUFFER_SIZE",
    "tx_buffer_size": "SERIAL_TX_BUFFER_SIZE",
}

LONG_ACTION_MAP = {
    "toggle": "normal",
    "on": "on_only",
    "off": "off_only",
}

SUPER_LONG_ACTION_MAP = {
    "all_off": "normal",
    "off": "selective",
}

NETWORK_FALLBACK_ALIASES = {
    "none": "do_nothing",
    "do_nothing": "do_nothing",
    "local": "local",
}


@dataclass(frozen=True)
class PublicDefaults:
    """Resolved project-level defaults for one public schema v2 profile."""

    hardware_include: str | None = None
    debug_serial: str = "Serial"
    bridge_serial: str = "Serial"
    disable_rtc: bool = False
    disable_eth: bool = False
    controllino_pin_aliases: bool = False


@dataclass(frozen=True)
class TimingDefaults:
    """Resolved click timing defaults that affect generated static scanners."""

    long_click_ms: int | None = None
    super_long_click_ms: int | None = None


@dataclass(frozen=True)
class ActionAliasContext:
    """Resolved device-local aliases that actions may reference."""

    groups: dict[str, list[str]]
    scenes: dict[str, list[TomlTable]]


@dataclass(frozen=True)
class PublicSchemaResult:
    """Normalized public schema plus generated lockfile metadata."""

    normalized: TomlTable
    id_lock_content: str
    raw_define_paths: list[str]
    auto_id_paths: list[str]


def normalize_public_schema(root: TomlTable, lock_path: Path) -> PublicSchemaResult:
    """Rewrite schema v2 TOML into the internal code-generation model."""
    schema_version = root.get("schema_version")
    if isinstance(schema_version, bool) or schema_version != SCHEMA_VERSION:
        fail(f"schema_version must be {SCHEMA_VERSION}.")

    _reject_unknown_keys(
        root,
        {
            "schema_version",
            "preset",
            "generator",
            "controller",
            "features",
            "timing",
            "serial",
            "advanced",
            "devices",
        },
        "root",
    )

    raw_define_paths = _collect_raw_define_paths(root)
    lock_result = apply_id_lock(root, lock_path)

    common: TomlTable = {}
    common_defines: TomlTable = {}
    common_raw_flags: list[str] = []

    defaults = _apply_preset(root.get("preset"), common, common_defines)
    defaults = _apply_controller(
        _optional_table(root, "controller", "controller"),
        common,
        defaults,
        "controller",
    )
    _apply_features(
        _optional_table(root, "features", "features"),
        common_defines,
        "features",
    )
    timing_defaults = _apply_timing(
        _optional_table(root, "timing", "timing"),
        common_defines,
        "timing",
    )
    _apply_serial(
        _optional_table(root, "serial", "serial"),
        common_defines,
        common_raw_flags,
        "serial",
    )
    _apply_advanced(
        _optional_table(root, "advanced", "advanced"),
        common_defines,
        common_raw_flags,
        "advanced",
    )

    if defaults.hardware_include is not None:
        common.setdefault("hardware_include", defaults.hardware_include)
    common.setdefault("debug_serial", defaults.debug_serial)
    common.setdefault("com_serial", defaults.bridge_serial)
    if common_defines:
        common["defines"] = common_defines
    if common_raw_flags:
        common["raw_build_flags"] = common_raw_flags

    normalized: TomlTable = {}
    if "generator" in root:
        normalized["generator"] = root["generator"]
    normalized["common"] = common
    normalized["devices"] = _normalize_devices(
        _required_table(root, "devices", "devices"),
        defaults,
        timing_defaults,
    )
    return PublicSchemaResult(
        normalized=normalized,
        id_lock_content=lock_result.content,
        raw_define_paths=raw_define_paths,
        auto_id_paths=lock_result.auto_id_paths,
    )


def _collect_raw_define_paths(root: TomlTable) -> list[str]:
    """Collect user-authored expert defines before semantic fields are lowered."""
    raw_define_paths: list[str] = []
    _extend_raw_define_paths(
        raw_define_paths,
        root.get("advanced"),
        "advanced.defines",
    )

    devices_raw = root.get("devices")
    if isinstance(devices_raw, dict):
        for device_key, raw_device in devices_raw.items():
            if isinstance(raw_device, dict):
                _extend_raw_define_paths(
                    raw_define_paths,
                    raw_device.get("advanced"),
                    f"devices.{device_key}.advanced.defines",
                )
    return raw_define_paths


def _extend_raw_define_paths(
    raw_define_paths: list[str],
    raw_advanced: TomlValue,
    path: str,
) -> None:
    """Append `advanced.defines` keys without validating the table shape."""
    if not isinstance(raw_advanced, dict):
        return
    defines = raw_advanced.get("defines")
    if not isinstance(defines, dict):
        return
    raw_define_paths.extend(f"{path}.{define_name}" for define_name in defines)


def _required_table(parent: TomlTable, key: str, path: str) -> TomlTable:
    """Read a required TOML table."""
    return _expect_table(parent.get(key), path)


def _optional_table(parent: TomlTable, key: str, path: str) -> TomlTable:
    """Read an optional TOML table, using an empty table when omitted."""
    if key not in parent:
        return {}
    return _expect_table(parent[key], path)


def _expect_table(value: TomlValue | None, path: str) -> TomlTable:
    """Return a TOML table or fail with the public schema path."""
    if not isinstance(value, dict):
        fail(f"{path} must be a TOML table.")
    return cast("TomlTable", value)


def _expect_list(value: TomlValue | None, path: str) -> TomlArray:
    """Return a TOML array or fail with the public schema path."""
    if not isinstance(value, list):
        fail(f"{path} must be a TOML array.")
    return cast("TomlArray", value)


def _expect_string(value: TomlValue | None, path: str) -> str:
    """Return a non-empty TOML string."""
    if not isinstance(value, str) or value == "":
        fail(f"{path} must be a non-empty string.")
    return value


def _expect_bool(value: TomlValue | None, path: str) -> bool:
    """Return a TOML boolean without accepting integer lookalikes."""
    if not isinstance(value, bool):
        fail(f"{path} must be true or false.")
    return value


def _expect_int(value: TomlValue | None, path: str, minimum: int = 0) -> int:
    """Return a TOML integer in the requested lower-bounded range."""
    if isinstance(value, bool) or not isinstance(value, int):
        fail(f"{path} must be an integer.")
    if value < minimum:
        fail(f"{path} must be at least {minimum}.")
    return value


def _reject_unknown_keys(table: TomlTable, allowed: set[str], path: str) -> None:
    """Reject misspelled public schema keys before they silently do nothing."""
    for key in table:
        if key not in allowed:
            fail(f"{path}.{key} is not a supported schema v2 field.")


def _apply_preset(
    raw_preset: TomlValue | None,
    common: TomlTable,
    defines: TomlTable,
) -> PublicDefaults:
    """Apply one named adoption preset to common defaults and feature defines."""
    if raw_preset is None:
        return PublicDefaults()

    preset = resolve_preset(_expect_string(raw_preset, "preset"))
    common["hardware_include"] = preset.hardware_include
    defaults = PublicDefaults(
        hardware_include=preset.hardware_include,
        debug_serial=preset.debug_serial,
        bridge_serial=preset.bridge_serial,
        controllino_pin_aliases=preset.controllino_pin_aliases,
    )

    _set_codec(defines, use_msgpack=preset.codec == "msgpack")
    _set_fast_io(defines, enabled=preset.fast_io)
    return defaults


def _apply_controller(
    table: TomlTable,
    common: TomlTable,
    defaults: PublicDefaults,
    path: str,
) -> PublicDefaults:
    """Apply project-level hardware and serial object defaults."""
    _reject_unknown_keys(
        table,
        {
            "hardware_include",
            "debug_serial",
            "bridge_serial",
            "disable_rtc",
            "disable_eth",
            "controllino_pin_aliases",
        },
        path,
    )

    hardware_include = defaults.hardware_include
    debug_serial = defaults.debug_serial
    bridge_serial = defaults.bridge_serial
    controllino_pin_aliases = defaults.controllino_pin_aliases

    if "hardware_include" in table:
        hardware_include = _expect_string(
            table["hardware_include"],
            f"{path}.hardware_include",
        )
        common["hardware_include"] = hardware_include
        controllino_pin_aliases = "controllino" in hardware_include.lower()
    if "debug_serial" in table:
        debug_serial = _expect_string(table["debug_serial"], f"{path}.debug_serial")
        common["debug_serial"] = debug_serial
    if "bridge_serial" in table:
        bridge_serial = _expect_string(table["bridge_serial"], f"{path}.bridge_serial")
        common["com_serial"] = bridge_serial
    if "controllino_pin_aliases" in table:
        controllino_pin_aliases = _expect_bool(
            table["controllino_pin_aliases"],
            f"{path}.controllino_pin_aliases",
        )

    return PublicDefaults(
        hardware_include=hardware_include,
        debug_serial=debug_serial,
        bridge_serial=bridge_serial,
        disable_rtc=_get_bool(table, "disable_rtc", path, default=defaults.disable_rtc),
        disable_eth=_get_bool(table, "disable_eth", path, default=defaults.disable_eth),
        controllino_pin_aliases=controllino_pin_aliases,
    )


def _apply_features(table: TomlTable, defines: TomlTable, path: str) -> None:
    """Translate semantic feature toggles into lsh-core compile-time defines."""
    _reject_unknown_keys(
        table,
        {
            "codec",
            "fast_io",
            "fast_buttons",
            "fast_actuators",
            "fast_indicators",
            "bench",
            "bench_iterations",
            "aggressive_constexpr_ctors",
            "etl_profile_override_header",
        },
        path,
    )
    if "codec" in table:
        codec = _expect_string(table["codec"], f"{path}.codec").lower()
        if codec not in CODECS:
            choices = ", ".join(sorted(CODECS))
            fail(f"{path}.codec must be one of: {choices}.")
        _set_codec(defines, use_msgpack=CODECS[codec])
    if "fast_io" in table:
        _set_fast_io(
            defines,
            enabled=_expect_bool(table["fast_io"], f"{path}.fast_io"),
        )
    for field_name, define_name in FEATURE_DEFINE_MAP.items():
        if field_name in table:
            defines[define_name] = _expect_bool(
                table[field_name],
                f"{path}.{field_name}",
            )
    if "bench_iterations" in table:
        defines["CONFIG_BENCH_ITERATIONS"] = _expect_int(
            table["bench_iterations"],
            f"{path}.bench_iterations",
            1,
        )
    if "aggressive_constexpr_ctors" in table:
        _apply_constexpr_ctor_policy(
            table["aggressive_constexpr_ctors"],
            defines,
            f"{path}.aggressive_constexpr_ctors",
        )
    if "etl_profile_override_header" in table:
        defines["LSH_ETL_PROFILE_OVERRIDE_HEADER"] = _include_define_value(
            table["etl_profile_override_header"],
            f"{path}.etl_profile_override_header",
        )


def _set_codec(defines: TomlTable, *, use_msgpack: bool) -> None:
    """Set the generated serial codec flag."""
    defines["CONFIG_MSG_PACK"] = use_msgpack


def _set_fast_io(defines: TomlTable, *, enabled: bool) -> None:
    """Apply one coarse fast-I/O policy to all supported resource families."""
    defines["CONFIG_USE_FAST_CLICKABLES"] = enabled
    defines["CONFIG_USE_FAST_ACTUATORS"] = enabled
    defines["CONFIG_USE_FAST_INDICATORS"] = enabled


def _apply_constexpr_ctor_policy(
    value: TomlValue,
    defines: TomlTable,
    path: str,
) -> None:
    """Translate the public constexpr-constructor policy into expert defines."""
    if isinstance(value, str):
        if value.lower() != "auto":
            fail(f'{path} must be true, false, or "auto".')
        defines["LSH_ENABLE_AGGRESSIVE_CONSTEXPR_CTORS"] = False
        defines["LSH_DISABLE_AGGRESSIVE_CONSTEXPR_CTORS"] = False
        return
    enabled = _expect_bool(value, path)
    defines["LSH_ENABLE_AGGRESSIVE_CONSTEXPR_CTORS"] = enabled
    defines["LSH_DISABLE_AGGRESSIVE_CONSTEXPR_CTORS"] = not enabled


def _include_define_value(value: TomlValue, path: str) -> TomlValue:
    """Normalize a user header name into a C++ include-operand define value."""
    if value is False:
        return False
    header = _expect_string(value, path)
    if header.startswith(("<", '"')):
        return header
    return f'"{header}"'


def _apply_timing(
    table: TomlTable,
    defines: TomlTable,
    path: str,
) -> TimingDefaults:
    """Translate semantic timing knobs into millisecond compile-time defines."""
    _reject_unknown_keys(table, set(TIMING_DEFINE_MAP), path)
    long_click_ms: int | None = None
    super_long_click_ms: int | None = None
    for field_name, (define_name, allow_zero) in TIMING_DEFINE_MAP.items():
        if field_name in table:
            milliseconds = _duration_ms(
                table[field_name],
                f"{path}.{field_name}",
                allow_zero=allow_zero,
            )
            defines[define_name] = milliseconds
            if field_name == "long_click":
                long_click_ms = milliseconds
            elif field_name == "super_long_click":
                super_long_click_ms = milliseconds
    return TimingDefaults(
        long_click_ms=long_click_ms,
        super_long_click_ms=super_long_click_ms,
    )


def _apply_serial(
    table: TomlTable,
    defines: TomlTable,
    raw_flags: list[str],
    path: str,
) -> None:
    """Translate serial transport tuning into defines and build flags."""
    _reject_unknown_keys(
        table,
        set(SERIAL_DEFINE_MAP) | set(SERIAL_BUFFER_FLAG_MAP),
        path,
    )
    for field_name, define_name in SERIAL_DEFINE_MAP.items():
        if field_name not in table:
            continue
        value = table[field_name]
        if field_name == "flush_after_send":
            flush_after_send = _expect_bool(value, f"{path}.{field_name}")
            defines[define_name] = 1 if flush_after_send else 0
        elif field_name in {"timeout", "msgpack_frame_idle_timeout"}:
            defines[define_name] = _duration_ms(value, f"{path}.{field_name}")
        else:
            defines[define_name] = _expect_int(value, f"{path}.{field_name}", 1)
    for field_name, flag_name in SERIAL_BUFFER_FLAG_MAP.items():
        if field_name in table:
            buffer_size = _expect_int(
                table[field_name],
                f"{path}.{field_name}",
                1,
            )
            raw_flags.append(f"-D {flag_name}={buffer_size}")


def _apply_advanced(
    table: TomlTable,
    defines: TomlTable,
    raw_flags: list[str],
    path: str,
) -> None:
    """Apply expert-only escape hatches after semantic defaults."""
    _reject_unknown_keys(table, {"defines", "build_flags"}, path)
    if "defines" in table:
        defines.update(_expect_table(table["defines"], f"{path}.defines"))
    raw_value = table.get("build_flags")
    if raw_value is not None:
        raw_flags.extend(_string_list(raw_value, f"{path}.build_flags"))


def _normalize_devices(
    devices_table: TomlTable,
    defaults: PublicDefaults,
    timing_defaults: TimingDefaults,
) -> TomlTable:
    """Normalize all named device profiles into v1-compatible device tables."""
    normalized: TomlTable = {}
    for key, raw_device in devices_table.items():
        device_path = f"devices.{key}"
        table = _expect_table(raw_device, device_path)
        normalized[key] = _normalize_device(
            table,
            defaults,
            timing_defaults,
            device_path,
        )
    return normalized


def _normalize_device(
    table: TomlTable,
    defaults: PublicDefaults,
    inherited_timing: TimingDefaults,
    path: str,
) -> TomlTable:
    """Normalize one public schema v2 device profile."""
    _reject_unknown_keys(
        table,
        {
            "name",
            "build_macro",
            "hardware_include",
            "debug_serial",
            "bridge_serial",
            "config_include",
            "static_config_include",
            "disable_rtc",
            "disable_eth",
            "controllino_pin_aliases",
            "features",
            "timing",
            "serial",
            "advanced",
            "actuators",
            "groups",
            "scenes",
            "buttons",
            "indicators",
        },
        path,
    )

    device = _copy_device_scalar_fields(table, defaults, path)
    pin_aliases = _device_pin_aliases(table, defaults, path)

    device_defines: TomlTable = {}
    device_raw_flags: list[str] = []
    _apply_features(
        _optional_table(table, "features", f"{path}.features"),
        device_defines,
        f"{path}.features",
    )
    local_timing = _apply_timing(
        _optional_table(table, "timing", f"{path}.timing"),
        device_defines,
        f"{path}.timing",
    )
    _apply_serial(
        _optional_table(table, "serial", f"{path}.serial"),
        device_defines,
        device_raw_flags,
        f"{path}.serial",
    )
    _apply_advanced(
        _optional_table(table, "advanced", f"{path}.advanced"),
        device_defines,
        device_raw_flags,
        f"{path}.advanced",
    )
    if device_defines:
        device["defines"] = device_defines
    if device_raw_flags:
        device["raw_build_flags"] = device_raw_flags

    actuator_names = set(
        _public_resource_names(table.get("actuators"), f"{path}.actuators")
    )
    groups = _normalize_groups(
        table.get("groups"),
        f"{path}.groups",
        actuator_names=actuator_names,
    )
    scenes = _normalize_scenes(
        table.get("scenes"),
        f"{path}.scenes",
        groups=groups,
        actuator_names=actuator_names,
    )
    aliases = ActionAliasContext(groups=groups, scenes=scenes)

    device["actuators"] = _normalize_actuators(
        table.get("actuators"),
        f"{path}.actuators",
        pin_aliases=pin_aliases,
    )
    device["clickables"] = _normalize_clickables(
        table.get("buttons"),
        f"{path}.buttons",
        pin_aliases=pin_aliases,
        timing_defaults=_merge_timing_defaults(inherited_timing, local_timing),
        aliases=aliases,
    )
    device["indicators"] = _normalize_indicators(
        table.get("indicators"),
        f"{path}.indicators",
        pin_aliases=pin_aliases,
    )
    return device


def _copy_device_scalar_fields(
    table: TomlTable,
    defaults: PublicDefaults,
    path: str,
) -> TomlTable:
    """Copy device scalar fields and aliases into the internal table."""
    device: TomlTable = {}
    for key in (
        "name",
        "build_macro",
        "hardware_include",
        "debug_serial",
        "config_include",
        "static_config_include",
    ):
        if key in table:
            device[key] = table[key]
    if "bridge_serial" in table:
        device["com_serial"] = table["bridge_serial"]
    device["disable_rtc"] = _get_bool(
        table,
        "disable_rtc",
        path,
        default=defaults.disable_rtc,
    )
    device["disable_eth"] = _get_bool(
        table,
        "disable_eth",
        path,
        default=defaults.disable_eth,
    )
    return device


def _device_pin_aliases(
    table: TomlTable,
    defaults: PublicDefaults,
    path: str,
) -> bool:
    """Resolve whether this device expands public Controllino pin aliases."""
    pin_aliases = defaults.controllino_pin_aliases
    if "hardware_include" in table:
        pin_aliases = (
            "controllino"
            in _expect_string(
                table["hardware_include"],
                f"{path}.hardware_include",
            ).lower()
        )
    if "controllino_pin_aliases" in table:
        pin_aliases = _expect_bool(
            table["controllino_pin_aliases"],
            f"{path}.controllino_pin_aliases",
        )
    return pin_aliases


def _normalize_actuators(
    raw: TomlValue | None,
    path: str,
    *,
    pin_aliases: bool,
) -> list[TomlTable]:
    """Normalize named actuator tables and assign omitted public IDs."""
    resources = _named_resource_tables(raw, path)
    _assign_missing_ids(resources, "id", path)
    normalized: list[TomlTable] = []
    for name, table in resources:
        item_path = f"{path}.{name}"
        _reject_unknown_keys(
            table,
            {
                "id",
                "pin",
                "default",
                "protected",
                "auto_off",
                "auto_off_ms",
                "pulse",
                "pulse_ms",
                "interlock",
            },
            item_path,
        )
        item: TomlTable = {
            "name": name,
            "id": table["id"],
            "pin": _normalize_pin(
                _expect_string(table.get("pin"), f"{item_path}.pin"),
                controllino_aliases=pin_aliases,
            ),
        }
        if "default" in table:
            item["default_state"] = table["default"]
        for key in ("protected", "auto_off", "auto_off_ms", "pulse", "pulse_ms"):
            if key in table:
                item[key] = table[key]
        if "interlock" in table:
            item["interlock"] = _target_list(
                table["interlock"],
                f"{item_path}.interlock",
            )
        normalized.append(item)
    return normalized


def _normalize_clickables(
    raw: TomlValue | None,
    path: str,
    *,
    pin_aliases: bool,
    timing_defaults: TimingDefaults,
    aliases: ActionAliasContext,
) -> list[TomlTable]:
    """Normalize named button/clickable tables and assign omitted public IDs."""
    resources = _named_resource_tables(raw, path)
    _assign_missing_ids(resources, "id", path)
    normalized: list[TomlTable] = []
    for name, table in resources:
        item_path = f"{path}.{name}"
        _reject_unknown_keys(
            table,
            {"id", "pin", "short", "long", "super_long"},
            item_path,
        )
        normalized.append(
            {
                "name": name,
                "id": table["id"],
                "pin": _normalize_pin(
                    _expect_string(table.get("pin"), f"{item_path}.pin"),
                    controllino_aliases=pin_aliases,
                ),
                "short": _normalize_short_action(
                    table.get("short"),
                    f"{item_path}.short",
                    aliases=aliases,
                ),
                "long": _normalize_timed_action(
                    table.get("long"),
                    f"{item_path}.long",
                    "long",
                    default_ms=timing_defaults.long_click_ms,
                    aliases=aliases,
                ),
                "super_long": _normalize_timed_action(
                    table.get("super_long"),
                    f"{item_path}.super_long",
                    "super_long",
                    default_ms=timing_defaults.super_long_click_ms,
                    aliases=aliases,
                ),
            }
        )
    return normalized


def _normalize_indicators(
    raw: TomlValue | None,
    path: str,
    *,
    pin_aliases: bool,
) -> list[TomlTable]:
    """Normalize named indicator tables and user-friendly `when` expressions."""
    resources = _named_resource_tables(raw, path)
    normalized: list[TomlTable] = []
    for name, table in resources:
        item_path = f"{path}.{name}"
        _reject_unknown_keys(
            table,
            {"pin", "when"},
            item_path,
        )
        item: TomlTable = {
            "name": name,
            "pin": _normalize_pin(
                _expect_string(table.get("pin"), f"{item_path}.pin"),
                controllino_aliases=pin_aliases,
            ),
        }
        when_value = table.get("when")
        if when_value is None:
            fail(f"{item_path}.when is required.")
        targets, mode = _normalize_indicator_when(when_value, f"{item_path}.when")
        item["targets"] = targets
        item["mode"] = mode
        normalized.append(item)
    return normalized


def _named_resource_tables(
    raw: TomlValue | None,
    path: str,
) -> list[tuple[str, TomlTable]]:
    """Read schema v2 resources, which are always named TOML subtables."""
    if raw is None:
        return []
    table = _expect_table(raw, path)
    resources: list[tuple[str, TomlTable]] = []
    for name, raw_resource in table.items():
        resources.append((name, _expect_table(raw_resource, f"{path}.{name}")))
    return resources


def _public_resource_names(raw: TomlValue | None, path: str) -> list[str]:
    """Return resource table names without normalizing their content."""
    if raw is None:
        return []
    return [name for name, _table in _named_resource_tables(raw, path)]


def _normalize_groups(
    raw: TomlValue | None,
    path: str,
    *,
    actuator_names: set[str],
) -> dict[str, list[str]]:
    """Normalize local actuator groups used by click actions and scenes."""
    if raw is None:
        return {}
    table = _expect_table(raw, path)
    groups: dict[str, list[str]] = {}
    for group_name, raw_group in table.items():
        group_path = f"{path}.{group_name}"
        if isinstance(raw_group, dict):
            _reject_unknown_keys(raw_group, {"target", "targets"}, group_path)
            targets = _optional_targets(raw_group, group_path)
            if targets is None:
                fail(f"{group_path}.targets is required.")
        else:
            targets = _target_list(raw_group, group_path)
        if not targets:
            fail(f"{group_path} must contain at least one actuator.")
        _validate_public_targets(targets, actuator_names, group_path)
        groups[group_name] = targets
    return groups


def _normalize_scenes(
    raw: TomlValue | None,
    path: str,
    *,
    groups: dict[str, list[str]],
    actuator_names: set[str],
) -> dict[str, list[TomlTable]]:
    """Normalize deterministic scene steps into parser-ready action steps."""
    if raw is None:
        return {}
    table = _expect_table(raw, path)
    scenes: dict[str, list[TomlTable]] = {}
    for scene_name, raw_scene in table.items():
        scene_path = f"{path}.{scene_name}"
        scene_table = _expect_table(raw_scene, scene_path)
        _reject_unknown_keys(scene_table, {"off", "on", "toggle"}, scene_path)
        steps: list[TomlTable] = []
        assigned_targets: dict[str, str] = {}
        for field_name, operation in (
            ("off", "OFF"),
            ("on", "ON"),
            ("toggle", "TOGGLE"),
        ):
            if field_name not in scene_table:
                continue
            targets = _target_or_group_list(
                scene_table[field_name],
                f"{scene_path}.{field_name}",
                groups=groups,
                actuator_names=actuator_names,
            )
            for target in targets:
                previous_operation = assigned_targets.get(target)
                if previous_operation is not None:
                    fail(
                        f"{scene_path} assigns actuator {target!r} to both "
                        f"{previous_operation.lower()} and {field_name}."
                    )
                assigned_targets[target] = field_name
            steps.append({"operation": operation, "targets": targets})
        if not steps:
            fail(f"{scene_path} must contain at least one of off/on/toggle.")
        scenes[scene_name] = steps
    return scenes


def _assign_missing_ids(
    resources: list[tuple[str, TomlTable]],
    field_name: str,
    path: str,
) -> None:
    """Assign deterministic IDs to resources that omit their public wire ID."""
    used_ids: set[int] = set()
    for name, table in resources:
        if field_name not in table:
            continue
        item_path = f"{path}.{name}.{field_name}"
        resource_id = _expect_int(table[field_name], item_path, 1)
        if resource_id > UINT8_MAX:
            fail(f"{item_path} must be between 1 and {UINT8_MAX}.")
        if resource_id in used_ids:
            fail(f"{item_path} duplicates another {path} ID: {resource_id}.")
        used_ids.add(resource_id)

    next_id = 1
    for _, table in resources:
        if field_name in table:
            continue
        while next_id in used_ids:
            next_id += 1
        if next_id > UINT8_MAX:
            fail(f"{path} has more than {UINT8_MAX} public IDs.")
        table[field_name] = next_id
        used_ids.add(next_id)


def _normalize_short_action(
    raw: TomlValue | None,
    path: str,
    *,
    aliases: ActionAliasContext,
) -> TomlValue:
    """Normalize short-click sugar into the existing short action syntax."""
    if raw is None or isinstance(raw, bool):
        return raw if raw is not None else False
    if isinstance(raw, str):
        return [raw]
    if isinstance(raw, list):
        return raw
    table = _expect_table(raw, path)
    _reject_unknown_keys(
        table,
        {"enabled", "target", "targets", "group", "groups", "action", "scene"},
        path,
    )
    scene_steps = _optional_scene_steps(table, path, scenes=aliases.scenes)
    if scene_steps is not None:
        _reject_scene_conflicts(table, path)
        normalized_scene: TomlTable = {"steps": scene_steps}
        if "enabled" in table:
            normalized_scene["enabled"] = table["enabled"]
        return normalized_scene

    action = _get_optional_action(table, path)
    if action is not None and action not in {"toggle", "normal"}:
        fail(f'{path}.action must be "toggle" for short clicks.')
    normalized: TomlTable = {}
    if "enabled" in table:
        normalized["enabled"] = table["enabled"]
    targets = _optional_targets_or_groups(table, path, groups=aliases.groups)
    if targets is not None:
        normalized["targets"] = targets
    return normalized


def _normalize_timed_action(  # noqa: PLR0911
    raw: TomlValue | None,
    path: str,
    action_name: str,
    *,
    default_ms: int | None,
    aliases: ActionAliasContext,
) -> TomlValue:
    """Normalize long/super-long click sugar into the existing action syntax."""
    if raw is None:
        return False
    if isinstance(raw, bool):
        if raw and default_ms is not None:
            return {"enabled": True, "time_ms": default_ms}
        return raw
    if isinstance(raw, str):
        return _apply_action_default_time(
            _normalize_string_action(raw, action_name),
            default_ms,
        )
    if isinstance(raw, list):
        return _apply_action_default_time(raw, default_ms)

    table = _expect_table(raw, path)
    _reject_unknown_keys(
        table,
        {
            "enabled",
            "target",
            "targets",
            "group",
            "groups",
            "action",
            "after",
            "time",
            "time_ms",
            "network",
            "fallback",
            "scene",
        },
        path,
    )
    normalized = _copy_timed_action_fields(table, path)
    scene_steps = _optional_scene_steps(table, path, scenes=aliases.scenes)
    if scene_steps is not None:
        _reject_scene_conflicts(table, path)
        normalized["steps"] = scene_steps
        return _apply_action_default_time(normalized, default_ms)

    targets = _optional_targets_or_groups(table, path, groups=aliases.groups)
    if targets is not None:
        normalized["targets"] = targets
    action = _get_optional_action(table, path)
    if action is not None:
        normalized["type"] = _click_type_from_action(
            action,
            path,
            action_name,
            has_targets=bool(targets),
        )
    return _apply_action_default_time(normalized, default_ms)


def _merge_timing_defaults(
    inherited: TimingDefaults,
    local: TimingDefaults,
) -> TimingDefaults:
    """Merge project and device timing defaults, preferring device overrides."""
    return TimingDefaults(
        long_click_ms=(
            local.long_click_ms
            if local.long_click_ms is not None
            else inherited.long_click_ms
        ),
        super_long_click_ms=(
            local.super_long_click_ms
            if local.super_long_click_ms is not None
            else inherited.super_long_click_ms
        ),
    )


def _apply_action_default_time(
    action: TomlValue,
    default_ms: int | None,
) -> TomlValue:
    """Attach a schema-level click threshold to enabled actions without one."""
    if default_ms is None or isinstance(action, bool):
        return action
    if isinstance(action, list):
        return {"targets": action, "time_ms": default_ms}
    table = _expect_table(action, "click action")
    if table.get("enabled") is False or "time" in table or "time_ms" in table:
        return table
    table["time_ms"] = default_ms
    return table


def _copy_timed_action_fields(table: TomlTable, path: str) -> TomlTable:
    """Copy timed-action fields that do not need target/action translation."""
    normalized: TomlTable = {}
    for key in ("enabled", "network", "time", "time_ms", "type"):
        if key in table:
            normalized[key] = table[key]
    if "after" in table:
        if "time" in table or "time_ms" in table:
            fail(f"{path}.after cannot be combined with time/time_ms.")
        normalized["time"] = table["after"]
    if "fallback" in table:
        fallback = _expect_string(table["fallback"], f"{path}.fallback").lower()
        if fallback not in NETWORK_FALLBACK_ALIASES:
            choices = ", ".join(sorted(NETWORK_FALLBACK_ALIASES))
            fail(f"{path}.fallback must be one of: {choices}.")
        normalized["fallback"] = NETWORK_FALLBACK_ALIASES[fallback]
    return normalized


def _normalize_string_action(raw: str, action_name: str) -> TomlValue:
    """Normalize compact string click actions."""
    value = raw.lower()
    if action_name == "super_long":
        if value in SUPER_LONG_ACTION_MAP:
            return {"type": SUPER_LONG_ACTION_MAP[value]}
        if value in {"none", "disabled"}:
            return {"enabled": False}
    return [raw]


def _click_type_from_action(
    action: str,
    path: str,
    action_name: str,
    *,
    has_targets: bool,
) -> str:
    """Map public action names onto the internal click type vocabulary."""
    if action_name == "long":
        if action not in LONG_ACTION_MAP:
            choices = ", ".join(sorted(LONG_ACTION_MAP))
            fail(f"{path}.action must be one of: {choices}.")
        return LONG_ACTION_MAP[action]
    if action not in SUPER_LONG_ACTION_MAP:
        choices = ", ".join(sorted(SUPER_LONG_ACTION_MAP))
        fail(f"{path}.action must be one of: {choices}.")
    click_type = SUPER_LONG_ACTION_MAP[action]
    if click_type == "normal" and has_targets:
        fail(f'{path}.action="{action}" cannot also list targets.')
    return click_type


def _get_optional_action(table: TomlTable, path: str) -> str | None:
    """Return a lowercased public action name when one was supplied."""
    if "action" not in table:
        return None
    return _expect_string(table["action"], f"{path}.action").lower()


def _optional_scene_steps(
    table: TomlTable,
    path: str,
    *,
    scenes: dict[str, list[TomlTable]],
) -> list[TomlTable] | None:
    """Return normalized scene steps if this action references a scene."""
    if "scene" not in table:
        return None
    scene_name = _expect_string(table["scene"], f"{path}.scene")
    try:
        return scenes[scene_name]
    except KeyError:
        fail(f"{path}.scene references unknown scene {scene_name!r}.")


def _reject_scene_conflicts(table: TomlTable, path: str) -> None:
    """Keep scene actions deterministic by forbidding extra local target fields."""
    conflicting_fields = [
        key
        for key in ("target", "targets", "group", "groups", "action")
        if key in table
    ]
    if conflicting_fields:
        joined = "/".join(conflicting_fields)
        fail(f"{path}.scene cannot be combined with {joined}.")


def _optional_targets(table: TomlTable, path: str) -> list[str] | None:
    """Read any supported target alias, rejecting conflicting spellings."""
    target_keys = [key for key in ("target", "targets") if key in table]
    if not target_keys:
        return None
    if len(target_keys) > 1:
        fail(f"{path} must use only one of target/targets.")
    return _target_list(table[target_keys[0]], f"{path}.{target_keys[0]}")


def _optional_targets_or_groups(
    table: TomlTable,
    path: str,
    *,
    groups: dict[str, list[str]],
) -> list[str] | None:
    """Read explicit actuator targets plus explicit local groups."""
    targets = _optional_targets(table, path) or []
    group_names = _optional_groups(table, path)
    if not targets and group_names is None:
        return None
    expanded = list(targets)
    for group_name in group_names or []:
        try:
            expanded.extend(groups[group_name])
        except KeyError:
            fail(f"{path}.group references unknown group {group_name!r}.")
    return expanded


def _optional_groups(table: TomlTable, path: str) -> list[str] | None:
    """Read group/groups aliases from one action table."""
    group_keys = [key for key in ("group", "groups") if key in table]
    if not group_keys:
        return None
    if len(group_keys) > 1:
        fail(f"{path} must use only one of group/groups.")
    return _target_list(table[group_keys[0]], f"{path}.{group_keys[0]}")


def _target_list(value: TomlValue | None, path: str) -> list[str]:
    """Normalize one target string or target array into a string list."""
    if isinstance(value, str):
        return [value]
    return [
        _expect_string(item, f"{path}[{index}]")
        for index, item in enumerate(_expect_list(value, path))
    ]


def _target_or_group_list(
    value: TomlValue | None,
    path: str,
    *,
    groups: dict[str, list[str]],
    actuator_names: set[str],
) -> list[str]:
    """Expand scene entries that may name either an actuator or a group."""
    expanded: list[str] = []
    for item in _target_list(value, path):
        is_actuator = item in actuator_names
        is_group = item in groups
        if is_actuator and is_group:
            fail(f"{path} contains ambiguous actuator/group name {item!r}.")
        if is_group:
            expanded.extend(groups[item])
        elif is_actuator:
            expanded.append(item)
        else:
            fail(f"{path} references unknown actuator or group {item!r}.")
    if not expanded:
        fail(f"{path} must contain at least one actuator or group.")
    return expanded


def _validate_public_targets(
    targets: list[str],
    actuator_names: set[str],
    path: str,
) -> None:
    """Validate group target names while the public schema is still in scope."""
    seen: set[str] = set()
    for target in targets:
        if target not in actuator_names:
            fail(f"{path} references unknown actuator {target!r}.")
        if target in seen:
            fail(f"{path} references actuator {target!r} more than once.")
        seen.add(target)


def _normalize_indicator_when(value: TomlValue, path: str) -> tuple[list[str], str]:
    """Parse the public indicator `when` helper."""
    if isinstance(value, (str, list)):
        return _target_list(value, path), "any"
    table = _expect_table(value, path)
    _reject_unknown_keys(table, set(INDICATOR_MODES), path)
    if len(table) != 1:
        fail(f"{path} must contain exactly one of: any, all, majority.")
    mode, targets = next(iter(table.items()))
    return _target_list(targets, f"{path}.{mode}"), mode


def _normalize_pin(value: str, *, controllino_aliases: bool) -> str:
    """Expand public Controllino pin aliases while preserving raw C++ pins."""
    if value.startswith("raw:"):
        return value.removeprefix("raw:")
    if not controllino_aliases:
        return value
    upper = value.upper()
    for prefix in sorted(CONTROLLINO_PIN_PREFIXES, key=len, reverse=True):
        suffix = upper.removeprefix(prefix)
        if suffix != upper and suffix.isdigit():
            return f"CONTROLLINO_{prefix}{suffix}"
    return value


def _duration_ms(
    value: TomlValue,
    path: str,
    *,
    allow_zero: bool = False,
) -> int:
    """Parse a public duration value into integer milliseconds."""
    if isinstance(value, bool):
        fail(f"{path} must be a duration, not a boolean.")
    if isinstance(value, int):
        milliseconds = value
    elif isinstance(value, str):
        match = DURATION_RE.fullmatch(value)
        if match is None:
            fail(f"{path} must look like 250ms, 5s, 10m, 1h, or an integer.")
        amount = int(match.group(1))
        unit = (match.group(2) or "ms").lower()
        milliseconds = amount * {"ms": 1, "s": 1000, "m": 60000, "h": 3600000}[unit]
    else:
        fail(f"{path} must be a duration string or integer milliseconds.")
    minimum = 0 if allow_zero else 1
    if milliseconds < minimum:
        fail(f"{path} must be at least {minimum} ms.")
    return milliseconds


def _get_bool(table: TomlTable, key: str, path: str, *, default: bool) -> bool:
    """Read an optional boolean key."""
    if key not in table:
        return default
    return _expect_bool(table[key], f"{path}.{key}")


def _string_list(value: TomlValue, path: str) -> list[str]:
    """Return a TOML string array."""
    return [
        _expect_string(item, f"{path}[{index}]")
        for index, item in enumerate(_expect_list(value, path))
    ]
