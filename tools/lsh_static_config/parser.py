"""TOML loading, normalization and cross-resource validation."""

from __future__ import annotations

import re
from pathlib import Path, PurePosixPath
from typing import TYPE_CHECKING, TypeVar, cast

from .constants import (
    DURATION_RE,
    IDENTIFIER_RE,
    INDICATOR_MODES,
    LONG_CLICK_TYPES,
    MACRO_RE,
    NETWORK_FALLBACKS,
    SAFE_CPP_EXPR_FORBIDDEN,
    SUPER_LONG_CLICK_TYPES,
    UINT8_MAX,
    UINT16_MAX,
    UINT32_MAX,
)
from .errors import fail
from .metrics import count_active_network_clicks
from .models import (
    ActuatorConfig,
    ClickableConfig,
    ClickAction,
    DefineMap,
    DeviceConfig,
    GeneratorSettings,
    IndicatorConfig,
    ProjectConfig,
    TomlArray,
    TomlTable,
    TomlValue,
)

if TYPE_CHECKING:
    from collections.abc import Callable, Hashable, Iterable, Sequence

try:
    import tomllib
except ModuleNotFoundError:  # pragma: no cover - exercised only on Python < 3.11
    try:
        import tomli as tomllib  # type: ignore[import-not-found,no-redef]
    except ModuleNotFoundError:
        message = "TOML support requires Python 3.11+ or `pip install tomli`."
        raise SystemExit(message) from None

TConfig = TypeVar("TConfig")


def expect_table(value: TomlValue | None, path: str) -> TomlTable:
    """Return a TOML table or fail with the precise source path."""
    if not isinstance(value, dict):
        fail(f"{path} must be a TOML table.")
    return cast("TomlTable", value)


def expect_list(value: TomlValue | None, path: str) -> TomlArray:
    """Return a TOML array or fail with the precise source path."""
    if not isinstance(value, list):
        fail(f"{path} must be a TOML array.")
    return cast("TomlArray", value)


def expect_bool(value: TomlValue | None, path: str) -> bool:
    """Return a TOML boolean without accepting integer lookalikes."""
    if not isinstance(value, bool):
        fail(f"{path} must be true or false.")
    return value


def expect_string(value: TomlValue | None, path: str) -> str:
    """Return a non-empty TOML string."""
    if not isinstance(value, str) or value == "":
        fail(f"{path} must be a non-empty string.")
    return value


def expect_int(value: TomlValue | None, path: str, minimum: int, maximum: int) -> int:
    """Return an integer in the inclusive range, rejecting booleans."""
    if isinstance(value, bool) or not isinstance(value, int):
        fail(f"{path} must be an integer.")
    if value < minimum or value > maximum:
        fail(f"{path} must be between {minimum} and {maximum}.")
    return value


def get_string(
    table: TomlTable,
    key: str,
    path: str,
    default: str | None = None,
) -> str:
    """Read an optional string key from a TOML table."""
    if key not in table:
        if default is None:
            fail(f"{path}.{key} is required.")
        return default
    return expect_string(table[key], f"{path}.{key}")


def get_bool(
    table: TomlTable,
    key: str,
    path: str,
    *,
    default: bool = False,
) -> bool:
    """Read an optional boolean key from a TOML table."""
    if key not in table:
        return default
    return expect_bool(table[key], f"{path}.{key}")


def validate_identifier(value: str, path: str) -> str:
    """Validate a user-facing name that becomes a C++ object identifier."""
    if not IDENTIFIER_RE.fullmatch(value):
        fail(f"{path} must be a valid C++ identifier.")
    return value


def validate_macro(value: str, path: str) -> str:
    """Validate a preprocessor macro name from TOML."""
    if not MACRO_RE.fullmatch(value):
        fail(f"{path} must be a valid preprocessor macro name.")
    return value


def validate_cpp_expr(value: str, path: str) -> str:
    """Reject obvious code-injection characters from pin and serial expressions."""
    if any(ch in SAFE_CPP_EXPR_FORBIDDEN for ch in value):
        fail(
            f"{path} contains characters that are not allowed in generated "
            "C++ expressions."
        )
    if value.strip() != value or value == "":
        fail(f"{path} must not be empty or padded with spaces.")
    return value


def normalize_include(value: str) -> str:
    """Normalize bare hardware include names into angle-bracket includes."""
    if value.startswith(("<", '"')):
        return value
    return f"<{value}>"


def normalize_serial(value: str, path: str) -> str:
    """Normalize serial names into pointer expressions expected by lsh-core."""
    value = validate_cpp_expr(value, path)
    if value.startswith("&"):
        return value
    return f"&{value}"


def safe_path_fragment(value: str, path: str) -> str:
    """Validate one generated directory or file name component."""
    if value == "" or value.startswith(".") or "/" in value or "\\" in value:
        fail(f"{path} must be a simple file or directory name.")
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", value):
        fail(f"{path} contains unsupported path characters.")
    return value


def safe_relative_dir(value: str, path: str) -> Path:
    """Validate a generated-output directory under the TOML directory."""
    if value == "" or "\\" in value:
        fail(f"{path} must be a relative directory path.")
    posix_path = PurePosixPath(value)
    if posix_path.is_absolute() or any(
        part in ("", ".", "..") or part.startswith(".") for part in posix_path.parts
    ):
        fail(f"{path} must stay inside the TOML directory.")
    for part in posix_path.parts:
        if not re.fullmatch(r"[A-Za-z0-9_.-]+", part):
            fail(f"{path} contains unsupported path characters.")
    return Path(*posix_path.parts)


def safe_header_include_path(value: str, path: str) -> str:
    """Validate a relative generated header include path."""
    if "\\" in value:
        fail(f"{path} must use '/' separators.")
    posix_path = PurePosixPath(value)
    if posix_path.is_absolute() or any(
        part in ("", ".", "..") for part in posix_path.parts
    ):
        fail(
            f"{path} must be a relative header path inside the generated "
            "include directory."
        )
    if posix_path.suffix not in (".h", ".hpp"):
        fail(f"{path} must point to a .h or .hpp header.")
    return str(posix_path)


def sanitized_macro_suffix(value: str) -> str:
    """Convert a device key into a safe default macro suffix."""
    return re.sub(r"[^A-Za-z0-9]+", "_", value).strip("_").upper()


def parse_duration_ms(
    value: TomlValue | None, path: str, maximum: int = UINT32_MAX
) -> int:
    """Parse a TOML duration into milliseconds with an explicit upper bound."""
    if isinstance(value, bool):
        fail(f"{path} must be a duration, not a boolean.")
    if isinstance(value, int):
        ms = value
    elif isinstance(value, str):
        match = DURATION_RE.fullmatch(value)
        if match is None:
            fail(
                f"{path} must look like 250ms, 5s, 10m, 1h, or an integer "
                "millisecond value."
            )
        amount = int(match.group(1))
        unit = (match.group(2) or "ms").lower()
        factor = {"ms": 1, "s": 1000, "m": 60000, "h": 3600000}[unit]
        ms = amount * factor
    else:
        fail(f"{path} must be a duration string or integer milliseconds.")
    if ms <= 0 or ms > maximum:
        fail(f"{path} must be between 1 and {maximum} ms.")
    return ms


def parse_targets(value: TomlValue | None, path: str) -> list[str]:
    """Parse a list of actuator names."""
    targets = expect_list(value, path)
    parsed: list[str] = []
    for index, target in enumerate(targets):
        parsed.append(
            validate_identifier(
                expect_string(target, f"{path}[{index}]"), f"{path}[{index}]"
            )
        )
    return parsed


def parse_short_action(raw: TomlValue | None, path: str) -> tuple[bool, list[str]]:
    """Parse the short-click shorthand/table syntax."""
    if raw is None:
        return False, []
    if isinstance(raw, bool):
        if raw:
            fail(
                f'{path}=true is ambiguous; use {path} = ["actuator_name"] '
                "or a table with targets."
            )
        return False, []
    if isinstance(raw, list):
        targets = parse_targets(raw, path)
        if not targets:
            fail(f"{path} must contain at least one actuator when enabled.")
        return True, targets
    table = expect_table(raw, path)
    enabled = get_bool(table, "enabled", path, default=True)
    targets = parse_targets(table.get("targets", []), f"{path}.targets")
    if enabled and not targets:
        fail(f"{path} is enabled but has no targets.")
    if not enabled and targets:
        fail(f"{path} has targets but is disabled.")
    return enabled, targets


def _parse_click_type(raw_type: str, path: str, action_name: str) -> str:
    """Translate a user click type into the generated enum suffix."""
    if action_name == "long":
        if raw_type not in LONG_CLICK_TYPES:
            choices = ", ".join(sorted(LONG_CLICK_TYPES))
            fail(f"{path}.type must be one of: {choices}.")
        return LONG_CLICK_TYPES[raw_type]

    if raw_type not in SUPER_LONG_CLICK_TYPES:
        choices = ", ".join(sorted(SUPER_LONG_CLICK_TYPES))
        fail(f"{path}.type must be one of: {choices}.")
    return SUPER_LONG_CLICK_TYPES[raw_type]


def _apply_click_action_table(
    action: ClickAction,
    table: TomlTable,
    path: str,
    action_name: str,
) -> None:
    """Apply table-only click options to an already-created action."""
    if "targets" in table:
        action.targets = parse_targets(table["targets"], f"{path}.targets")
    elif "actuators" in table:
        action.targets = parse_targets(table["actuators"], f"{path}.actuators")

    if "type" in table:
        raw_type = expect_string(table["type"], f"{path}.type").lower()
        action.click_type = _parse_click_type(raw_type, path, action_name)

    if "network" in table:
        action.network = expect_bool(table["network"], f"{path}.network")

    if "fallback" in table:
        raw_fallback = expect_string(table["fallback"], f"{path}.fallback").lower()
        if raw_fallback not in NETWORK_FALLBACKS:
            choices = ", ".join(sorted(NETWORK_FALLBACKS))
            fail(f"{path}.fallback must be one of: {choices}.")
        action.fallback = NETWORK_FALLBACKS[raw_fallback]

    if "time" in table:
        action.time_ms = parse_duration_ms(table["time"], f"{path}.time", UINT16_MAX)
    if "time_ms" in table:
        action.time_ms = parse_duration_ms(
            table["time_ms"], f"{path}.time_ms", UINT16_MAX
        )


def _validate_click_action(action: ClickAction, path: str, action_name: str) -> None:
    """Reject actions that would be enabled but inert or contradictory."""
    if not action.enabled:
        if action.targets or action.network or action.time_ms is not None:
            fail(f"{path} is disabled but still contains active options.")
        return

    if action_name == "long" and not action.targets and not action.network:
        fail(f"{path} is enabled but has neither local targets nor network=true.")

    # A selective super-long with no local/network target is a deliberate no-op.
    # It preserves legacy profiles that used SELECTIVE to suppress the default
    # global-off action without attaching any secondary actuator.
    if (
        action_name == "super_long"
        and action.click_type == "SELECTIVE"
        and not action.targets
        and not action.network
    ):
        return


def parse_click_action(
    raw: TomlValue | None, path: str, action_name: str
) -> ClickAction:
    """Parse long/super-long shorthand, boolean and table syntaxes."""
    if raw is None:
        return ClickAction()
    if isinstance(raw, bool):
        if raw and action_name == "long":
            fail(
                f"{path}=true is ambiguous; long clicks need local targets "
                "or network=true."
            )
        return ClickAction(enabled=raw)
    if isinstance(raw, list):
        targets = parse_targets(raw, path)
        if action_name == "long" and not targets:
            fail(f"{path} must contain at least one actuator when enabled.")
        return ClickAction(enabled=True, targets=targets)

    table = expect_table(raw, path)
    action = ClickAction(enabled=get_bool(table, "enabled", path, default=True))
    _apply_click_action_table(action, table, path, action_name)
    _validate_click_action(action, path, action_name)
    return action


def parse_define_table(raw: TomlValue | None, path: str) -> DefineMap:
    """Parse a define table while validating only macro names early."""
    if raw is None:
        return {}
    table = expect_table(raw, path)
    parsed: DefineMap = {}
    for name, value in table.items():
        parsed[validate_macro(name, f"{path}.{name}")] = value
    return parsed


def parse_raw_build_flags(raw: TomlValue | None, path: str) -> list[str]:
    """Parse extra raw PlatformIO build flags."""
    if raw is None:
        return []
    flags = expect_list(raw, path)
    parsed: list[str] = []
    for index, flag in enumerate(flags):
        parsed.append(expect_string(flag, f"{path}[{index}]"))
    return parsed


def parse_actuators(raw: TomlValue | None, path: str) -> list[ActuatorConfig]:
    """Parse all actuator entries for one device."""
    actuators: list[ActuatorConfig] = []
    for index, item in enumerate(expect_list(raw or [], path)):
        table = expect_table(item, f"{path}[{index}]")
        item_path = f"{path}[{index}]"
        actuator = ActuatorConfig(
            name=validate_identifier(
                get_string(table, "name", item_path), f"{item_path}.name"
            ),
            actuator_id=expect_int(table.get("id"), f"{item_path}.id", 1, UINT8_MAX),
            pin=validate_cpp_expr(
                get_string(table, "pin", item_path), f"{item_path}.pin"
            ),
            default_state=get_bool(table, "default_state", item_path, default=False),
            protected=get_bool(table, "protected", item_path, default=False),
        )
        if "auto_off" in table:
            actuator.auto_off_ms = parse_duration_ms(
                table["auto_off"], f"{item_path}.auto_off"
            )
        if "auto_off_ms" in table:
            actuator.auto_off_ms = parse_duration_ms(
                table["auto_off_ms"], f"{item_path}.auto_off_ms"
            )
        actuators.append(actuator)
    return actuators


def parse_clickables(raw: TomlValue | None, path: str) -> list[ClickableConfig]:
    """Parse all clickable entries for one device."""
    clickables: list[ClickableConfig] = []
    for index, item in enumerate(expect_list(raw or [], path)):
        table = expect_table(item, f"{path}[{index}]")
        item_path = f"{path}[{index}]"
        clickable = ClickableConfig(
            name=validate_identifier(
                get_string(table, "name", item_path), f"{item_path}.name"
            ),
            clickable_id=expect_int(table.get("id"), f"{item_path}.id", 1, UINT8_MAX),
            pin=validate_cpp_expr(
                get_string(table, "pin", item_path), f"{item_path}.pin"
            ),
        )
        clickable.short_enabled, clickable.short_targets = parse_short_action(
            table.get("short"), f"{item_path}.short"
        )
        clickable.long = parse_click_action(
            table.get("long"), f"{item_path}.long", "long"
        )
        clickable.super_long = parse_click_action(
            table.get("super_long"), f"{item_path}.super_long", "super_long"
        )
        clickables.append(clickable)
    return clickables


def parse_indicators(raw: TomlValue | None, path: str) -> list[IndicatorConfig]:
    """Parse all indicator entries for one device."""
    indicators: list[IndicatorConfig] = []
    for index, item in enumerate(expect_list(raw or [], path)):
        table = expect_table(item, f"{path}[{index}]")
        item_path = f"{path}[{index}]"
        raw_mode = get_string(table, "mode", item_path, "any").lower()
        if raw_mode not in INDICATOR_MODES:
            choices = ", ".join(sorted(INDICATOR_MODES))
            fail(f"{item_path}.mode must be one of: {choices}.")
        indicators.append(
            IndicatorConfig(
                name=validate_identifier(
                    get_string(table, "name", item_path), f"{item_path}.name"
                ),
                pin=validate_cpp_expr(
                    get_string(table, "pin", item_path), f"{item_path}.pin"
                ),
                targets=parse_targets(
                    table.get("actuators", table.get("targets", [])),
                    f"{item_path}.actuators",
                ),
                mode=INDICATOR_MODES[raw_mode],
            ),
        )
    return indicators


def validate_unique(
    values: Iterable[TConfig],
    field_name: str,
    path: str,
    value_of: Callable[[TConfig], Hashable],
) -> None:
    """Reject duplicate values while preserving source indexes in the error."""
    seen: dict[Hashable, int] = {}
    for index, item in enumerate(values):
        value = value_of(item)
        if value in seen:
            fail(
                f"{path}[{index}].{field_name} duplicates "
                f"{path}[{seen[value]}].{field_name}: {value!r}."
            )
        seen[value] = index


def _validate_resource_count(count: int, path: str) -> None:
    """Keep every generated index representable as a uint8_t."""
    if count > UINT8_MAX:
        fail(f"{path} cannot contain more than {UINT8_MAX} entries.")


def _validate_resource_counts(device: DeviceConfig) -> None:
    """Validate generated array/index limits for one device."""
    _validate_resource_count(
        len(device.actuators),
        f"devices.{device.key}.actuators",
    )
    _validate_resource_count(
        len(device.clickables),
        f"devices.{device.key}.clickables",
    )
    _validate_resource_count(
        len(device.indicators),
        f"devices.{device.key}.indicators",
    )


def _validate_unique_fields(device: DeviceConfig) -> None:
    """Validate all user IDs and C++ object names for one device."""
    validate_unique(
        device.actuators,
        "name",
        f"devices.{device.key}.actuators",
        lambda actuator: actuator.name,
    )
    validate_unique(
        device.actuators,
        "actuator_id",
        f"devices.{device.key}.actuators",
        lambda actuator: actuator.actuator_id,
    )
    validate_unique(
        device.clickables,
        "name",
        f"devices.{device.key}.clickables",
        lambda clickable: clickable.name,
    )
    validate_unique(
        device.clickables,
        "clickable_id",
        f"devices.{device.key}.clickables",
        lambda clickable: clickable.clickable_id,
    )
    validate_unique(
        device.indicators,
        "name",
        f"devices.{device.key}.indicators",
        lambda indicator: indicator.name,
    )

    object_names: set[str] = set()
    for actuator in device.actuators:
        _validate_cpp_object_name(device.key, "actuators", actuator.name, object_names)
    for clickable in device.clickables:
        _validate_cpp_object_name(
            device.key,
            "clickables",
            clickable.name,
            object_names,
        )
    for indicator in device.indicators:
        _validate_cpp_object_name(
            device.key,
            "indicators",
            indicator.name,
            object_names,
        )


def _validate_cpp_object_name(
    device_key: str,
    group_name: str,
    object_name: str,
    used_names: set[str],
) -> None:
    """Reject cross-resource name reuse before generating C++ variables."""
    if object_name in used_names:
        fail(
            f"devices.{device_key}.{group_name}.{object_name} reuses a C++ "
            "object name already used by another resource."
        )
    used_names.add(object_name)


def _validate_clickable_targets(device: DeviceConfig) -> None:
    """Validate clickable local links and ensure every clickable can fire."""
    actuator_names = {actuator.name for actuator in device.actuators}
    for clickable in device.clickables:
        validate_target_set(
            clickable.short_targets,
            actuator_names,
            f"devices.{device.key}.clickables.{clickable.name}.short",
        )
        validate_target_set(
            clickable.long.targets,
            actuator_names,
            f"devices.{device.key}.clickables.{clickable.name}.long",
        )
        validate_target_set(
            clickable.super_long.targets,
            actuator_names,
            f"devices.{device.key}.clickables.{clickable.name}.super_long",
        )
        if (
            not clickable.short_enabled
            and not clickable.long.enabled
            and not clickable.super_long.enabled
        ):
            fail(
                f"devices.{device.key}.clickables.{clickable.name} has no "
                "enabled click action."
            )


def _validate_indicator_targets(device: DeviceConfig) -> None:
    """Validate indicator links and reject inert indicators."""
    actuator_names = {actuator.name for actuator in device.actuators}
    for indicator in device.indicators:
        validate_target_set(
            indicator.targets,
            actuator_names,
            f"devices.{device.key}.indicators.{indicator.name}.actuators",
        )
        if not indicator.targets:
            fail(
                f"devices.{device.key}.indicators.{indicator.name} must "
                "reference at least one actuator."
            )


def _validate_network_click_settings(device: DeviceConfig) -> None:
    """Validate device-level network-click defines against click actions."""
    active_network_clicks = count_active_network_clicks(device)
    network_setting = device.defines.get("LSH_NETWORK_CLICKS")
    if network_setting is False and active_network_clicks != 0:
        fail(
            f"devices.{device.key} disables network clicks but contains "
            "network=true click actions."
        )


def validate_device(device: DeviceConfig) -> None:
    """Validate cross-resource references and static resource limits."""
    _validate_resource_counts(device)
    _validate_unique_fields(device)
    _validate_clickable_targets(device)
    _validate_indicator_targets(device)
    _validate_network_click_settings(device)


def validate_target_set(targets: Sequence[str], allowed: set[str], path: str) -> None:
    """Validate target names and reject duplicate actuator links."""
    seen: set[str] = set()
    for target in targets:
        if target not in allowed:
            fail(f"{path} references unknown actuator {target!r}.")
        if target in seen:
            fail(f"{path} references actuator {target!r} more than once.")
        seen.add(target)


def parse_project(path: Path) -> ProjectConfig:
    """Load, validate and normalize one TOML project file."""
    try:
        with path.open("rb") as file_obj:
            root = tomllib.load(file_obj)
    except OSError as exc:
        fail(f"Cannot read {path}: {exc}")
    except tomllib.TOMLDecodeError as exc:
        fail(f"Invalid TOML in {path}: {exc}")

    root_table = expect_table(root, "root")
    generator_table = expect_table(root_table.get("generator", {}), "generator")
    common_table = expect_table(root_table.get("common", {}), "common")
    devices_table = expect_table(root_table.get("devices"), "devices")

    output_dir = safe_relative_dir(
        get_string(generator_table, "output_dir", "generator", "include"),
        "generator.output_dir",
    )
    settings = GeneratorSettings(
        output_dir=(path.parent / output_dir).resolve(),
        config_dir=safe_path_fragment(
            get_string(generator_table, "config_dir", "generator", "lsh_configs"),
            "generator.config_dir",
        ),
        user_config_header=safe_path_fragment(
            get_string(
                generator_table,
                "user_config_header",
                "generator",
                "lsh_user_config.hpp",
            ),
            "generator.user_config_header",
        ),
    )

    common_defines = parse_define_table(common_table.get("defines"), "common.defines")
    common_raw_build_flags = parse_raw_build_flags(
        common_table.get("raw_build_flags"), "common.raw_build_flags"
    )

    devices: dict[str, DeviceConfig] = {}
    for key, raw_device in devices_table.items():
        device_path = f"devices.{key}"
        table = expect_table(raw_device, device_path)
        safe_key = safe_path_fragment(key.lower(), device_path)
        config_include = f"{settings.config_dir}/{safe_key}_config.hpp"
        static_include = f"{settings.config_dir}/{safe_key}_static_config.hpp"
        hardware_include = table.get(
            "hardware_include", common_table.get("hardware_include")
        )
        if hardware_include is None:
            fail(
                f"{device_path}.hardware_include is required when "
                "common.hardware_include is not set."
            )

        device = DeviceConfig(
            key=safe_key,
            device_name=get_string(table, "name", device_path, key),
            build_macro=validate_macro(
                get_string(
                    table,
                    "build_macro",
                    device_path,
                    f"LSH_BUILD_{sanitized_macro_suffix(key)}",
                ),
                f"{device_path}.build_macro",
            ),
            hardware_include=normalize_include(
                expect_string(hardware_include, f"{device_path}.hardware_include")
            ),
            com_serial=normalize_serial(
                get_string(
                    table,
                    "com_serial",
                    device_path,
                    get_string(common_table, "com_serial", "common", "Serial"),
                ),
                f"{device_path}.com_serial",
            ),
            debug_serial=normalize_serial(
                get_string(
                    table,
                    "debug_serial",
                    device_path,
                    get_string(common_table, "debug_serial", "common", "Serial"),
                ),
                f"{device_path}.debug_serial",
            ),
            config_include=safe_header_include_path(
                get_string(table, "config_include", device_path, config_include),
                f"{device_path}.config_include",
            ),
            static_config_include=safe_header_include_path(
                get_string(table, "static_config_include", device_path, static_include),
                f"{device_path}.static_config_include",
            ),
            disable_rtc=get_bool(table, "disable_rtc", device_path, default=False),
            disable_eth=get_bool(table, "disable_eth", device_path, default=False),
            defines=parse_define_table(table.get("defines"), f"{device_path}.defines"),
            raw_build_flags=parse_raw_build_flags(
                table.get("raw_build_flags"), f"{device_path}.raw_build_flags"
            ),
            actuators=parse_actuators(
                table.get("actuators"), f"{device_path}.actuators"
            ),
            clickables=parse_clickables(
                table.get("clickables"), f"{device_path}.clickables"
            ),
            indicators=parse_indicators(
                table.get("indicators"), f"{device_path}.indicators"
            ),
        )
        validate_device(device)
        devices[safe_key] = device

    if not devices:
        fail("devices must contain at least one device profile.")

    return ProjectConfig(
        source_path=path.resolve(),
        settings=settings,
        common_defines=common_defines,
        common_raw_build_flags=common_raw_build_flags,
        devices=devices,
    )
