"""TOML loading, normalization and cross-resource validation."""

from __future__ import annotations

import re
import tomllib
from pathlib import Path, PurePosixPath
from typing import cast

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
from .models import (
    ActionStep,
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
from .public_schema import normalize_public_schema
from .validation import validate_device, validate_project


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


def normalize_include(value: str, path: str) -> str:
    """Normalize bare hardware include names into angle-bracket includes."""
    if value.strip() != value:
        fail(f"{path} must not be padded with spaces.")
    if value.startswith("<"):
        if not value.endswith(">") or value == "<>":
            fail(f"{path} must use a balanced <header.h> include.")
        return value
    if value.startswith('"'):
        if not value.endswith('"') or value == '""':
            fail(f"{path} must use a balanced quoted include.")
        return value
    if value.endswith((">", '"')):
        fail(f"{path} has an include delimiter without a matching opener.")
    value = validate_cpp_expr(value, path)
    if "/" in value or "\\" in value:
        fail(f"{path} must be a bare include name or an explicit include operand.")
    if value == "":
        fail(f"{path} must not be empty.")
    return f"<{value}>"


def hardware_include_looks_controllino(value: str) -> bool:
    """Return true if the include operand clearly targets Controllino headers."""
    return "controllino" in value.lower()


def validate_controllino_options(device: DeviceConfig, device_path: str) -> None:
    """Reject Controllino-only setup switches on non-Controllino profiles."""
    if not (device.disable_rtc or device.disable_eth):
        return
    if hardware_include_looks_controllino(device.hardware_include):
        return
    fail(
        f"{device_path}.disable_rtc/disable_eth require a Controllino hardware_include."
    )


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


def parse_action_steps(raw: TomlValue | None, path: str) -> list[ActionStep]:
    """Parse deterministic generated action steps for scenes and advanced actions."""
    if raw is None:
        return []
    steps = expect_list(raw, path)
    parsed: list[ActionStep] = []
    for index, raw_step in enumerate(steps):
        step_path = f"{path}[{index}]"
        table = expect_table(raw_step, step_path)
        operation = expect_string(
            table.get("operation"), f"{step_path}.operation"
        ).upper()
        if operation not in {"TOGGLE", "ON", "OFF"}:
            fail(f"{step_path}.operation must be one of: TOGGLE, ON, OFF.")
        parsed.append(
            ActionStep(
                operation=operation,
                targets=parse_targets(table.get("targets", []), f"{step_path}.targets"),
            )
        )
        if not parsed[-1].targets:
            fail(f"{step_path}.targets must contain at least one actuator.")
    return parsed


def parse_short_action(
    raw: TomlValue | None,
    path: str,
) -> tuple[bool, list[str], list[ActionStep]]:
    """Parse the short-click shorthand/table syntax."""
    if raw is None:
        return False, [], []
    if isinstance(raw, bool):
        if raw:
            fail(
                f'{path}=true is ambiguous; use {path} = ["actuator_name"] '
                "or a table with targets."
            )
        return False, [], []
    if isinstance(raw, list):
        targets = parse_targets(raw, path)
        if not targets:
            fail(f"{path} must contain at least one actuator when enabled.")
        return True, targets, []
    table = expect_table(raw, path)
    enabled = get_bool(table, "enabled", path, default=True)
    targets = parse_targets(table.get("targets", []), f"{path}.targets")
    steps = parse_action_steps(table.get("steps"), f"{path}.steps")
    if enabled and not targets and not steps:
        fail(f"{path} is enabled but has no targets.")
    if not enabled and targets:
        fail(f"{path} has targets but is disabled.")
    if not enabled and steps:
        fail(f"{path} has steps but is disabled.")
    return enabled, targets, steps


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


def _apply_click_targets(action: ClickAction, table: TomlTable, path: str) -> None:
    """Apply the normalized local target list to a click action."""
    if "targets" in table:
        action.targets = parse_targets(table["targets"], f"{path}.targets")


def _apply_click_timing(action: ClickAction, table: TomlTable, path: str) -> None:
    """Apply long/super-long threshold aliases to a click action."""
    if "time" in table:
        action.time_ms = parse_duration_ms(table["time"], f"{path}.time", UINT16_MAX)
    if "time_ms" in table:
        action.time_ms = parse_duration_ms(
            table["time_ms"], f"{path}.time_ms", UINT16_MAX
        )


def _apply_click_action_table(
    action: ClickAction,
    table: TomlTable,
    path: str,
    action_name: str,
) -> None:
    """Apply table-only click options to an already-created action."""
    _apply_click_targets(action, table, path)
    action.steps = parse_action_steps(table.get("steps"), f"{path}.steps")

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

    _apply_click_timing(action, table, path)


def _validate_click_action(action: ClickAction, path: str, action_name: str) -> None:
    """Reject actions that would be enabled but inert or contradictory."""
    if not action.enabled:
        if (
            action.targets
            or action.steps
            or action.network
            or action.time_ms is not None
        ):
            fail(f"{path} is disabled but still contains active options.")
        return

    if (
        action_name == "long"
        and not action.targets
        and not action.steps
        and not action.network
    ):
        fail(f"{path} is enabled but has neither local targets nor network=true.")

    if (
        action_name == "super_long"
        and action.click_type == "SELECTIVE"
        and not action.targets
        and not action.steps
        and not action.network
    ):
        fail(f"{path} is selective but has no targets.")


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
    """Parse a define table while validating lsh-core define names early."""
    if raw is None:
        return {}
    table = expect_table(raw, path)
    parsed: DefineMap = {}
    for name, value in table.items():
        macro = validate_macro(name, f"{path}.{name}")
        if macro == "LSH_NETWORK_CLICKS":
            fail(
                f"{path}.{name} is no longer supported; remove it and use "
                "network=true only on the click actions that need the bridge."
            )
        if macro == "LSH_COMPACT_ACTUATOR_SWITCH_TIMES":
            fail(
                f"{path}.{name} is no longer supported; compact actuator "
                "switch-time storage is selected automatically when "
                "CONFIG_ACTUATOR_DEBOUNCE_TIME_MS is 0."
            )
        parsed[macro] = value
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
        if "pulse" in table:
            actuator.pulse_ms = parse_duration_ms(
                table["pulse"], f"{item_path}.pulse", UINT16_MAX
            )
        if "pulse_ms" in table:
            actuator.pulse_ms = parse_duration_ms(
                table["pulse_ms"], f"{item_path}.pulse_ms", UINT16_MAX
            )
        if "interlock" in table:
            actuator.interlock_targets = parse_targets(
                table["interlock"], f"{item_path}.interlock"
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
        (
            clickable.short_enabled,
            clickable.short_targets,
            clickable.short_steps,
        ) = parse_short_action(table.get("short"), f"{item_path}.short")
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
                    table.get("targets", []),
                    f"{item_path}.targets",
                ),
                mode=INDICATOR_MODES[raw_mode],
            ),
        )
    return indicators


def parse_project(path: Path) -> ProjectConfig:
    """Load, validate and normalize one TOML project file."""
    try:
        with path.open("rb") as file_obj:
            root = tomllib.load(file_obj)
    except OSError as exc:
        fail(f"Cannot read {path}: {exc}")
    except tomllib.TOMLDecodeError as exc:
        fail(f"Invalid TOML in {path}: {exc}")

    raw_root_table = expect_table(root, "root")
    raw_generator_table = expect_table(raw_root_table.get("generator", {}), "generator")
    lock_file = safe_path_fragment(
        get_string(
            raw_generator_table,
            "id_lock_file",
            "generator",
            "lsh_devices.lock.toml",
        ),
        "generator.id_lock_file",
    )
    public_schema = normalize_public_schema(
        raw_root_table,
        (path.parent / lock_file).resolve(),
    )
    root_table = public_schema.normalized
    generator_table = expect_table(root_table.get("generator", {}), "generator")
    common_table = expect_table(root_table.get("common", {}), "common")
    devices_table = expect_table(root_table.get("devices"), "devices")

    output_dir = safe_relative_dir(
        get_string(generator_table, "output_dir", "generator", "include"),
        "generator.output_dir",
    )
    settings = GeneratorSettings(
        output_dir=(path.parent / output_dir).resolve(),
        id_lock_file=(path.parent / lock_file).resolve(),
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
        static_config_router_header=safe_path_fragment(
            get_string(
                generator_table,
                "static_config_router_header",
                "generator",
                "lsh_static_config_router.hpp",
            ),
            "generator.static_config_router_header",
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
                expect_string(hardware_include, f"{device_path}.hardware_include"),
                f"{device_path}.hardware_include",
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
        validate_controllino_options(device, device_path)
        if safe_key in devices:
            fail(f"{device_path} normalizes to duplicate device key {safe_key!r}.")
        devices[safe_key] = device

    if not devices:
        fail("devices must contain at least one device profile.")

    project = ProjectConfig(
        source_path=path.resolve(),
        settings=settings,
        common_defines=common_defines,
        common_raw_build_flags=common_raw_build_flags,
        devices=devices,
        id_lock_content=public_schema.id_lock_content,
        raw_define_paths=public_schema.raw_define_paths,
        auto_id_paths=public_schema.auto_id_paths,
    )
    validate_project(project)
    return project
