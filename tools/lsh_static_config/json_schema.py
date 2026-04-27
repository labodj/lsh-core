"""JSON Schema for editor autocomplete of `lsh_devices.toml` schema v2."""

from __future__ import annotations

import json
import tomllib
from typing import TYPE_CHECKING, cast

from .errors import fail
from .presets import PRESETS

if TYPE_CHECKING:
    from pathlib import Path

    from .models import TomlTable

JsonObject = dict[str, object]


def schema() -> JsonObject:
    """Build the generic JSON Schema suitable for Taplo and VS Code tooling."""
    return _root_schema(_generic_devices_schema())


def schema_json() -> str:
    """Return the generic public TOML schema as formatted JSON."""
    return _schema_to_json(schema())


def project_schema(config_path: Path) -> JsonObject:
    """Build a JSON Schema with autocomplete enums from one TOML project.

    The generic schema remains permissive about resource names.  This project
    schema keeps the same structural rules but specializes known devices so an
    editor can suggest the actual actuator, group and scene names authored in
    that project.
    """
    root = _load_toml_table(config_path)
    devices_raw = root.get("devices")
    if not isinstance(devices_raw, dict):
        return schema()

    known_devices: JsonObject = {}
    for device_key, raw_device in devices_raw.items():
        if isinstance(device_key, str) and isinstance(raw_device, dict):
            known_devices[device_key] = _project_device_schema(
                cast("TomlTable", raw_device)
            )

    return _root_schema(
        {
            "type": "object",
            "minProperties": 1,
            "properties": known_devices,
            # New devices stay writable before the project schema is refreshed.
            "additionalProperties": _generic_device_schema(),
        }
    )


def project_schema_json(config_path: Path) -> str:
    """Return the project-specific TOML schema as formatted JSON."""
    return _schema_to_json(project_schema(config_path))


def default_vscode_schema_path(config_path: Path) -> Path:
    """Return the conventional VS Code schema path beside a TOML project."""
    return config_path.parent / ".vscode" / "lsh_devices.schema.json"


def write_project_schema(config_path: Path, output_path: Path | None = None) -> Path:
    """Write the project-specific schema and return the path used."""
    schema_path = output_path or default_vscode_schema_path(config_path)
    schema_path.parent.mkdir(parents=True, exist_ok=True)
    schema_path.write_text(project_schema_json(config_path), encoding="utf-8")
    return schema_path


def _schema_to_json(value: JsonObject) -> str:
    """Serialize JSON Schema in a stable, review-friendly layout."""
    return json.dumps(value, indent=2, sort_keys=True) + "\n"


def _load_toml_table(path: Path) -> TomlTable:
    """Load raw TOML for editor-schema extraction."""
    try:
        with path.open("rb") as file_obj:
            root = tomllib.load(file_obj)
    except OSError as exc:
        fail(f"Cannot read {path}: {exc}")
    except tomllib.TOMLDecodeError as exc:
        fail(f"Invalid TOML in {path}: {exc}")
    if not isinstance(root, dict):
        fail("root must be a TOML table.")
    return cast("TomlTable", root)


def _root_schema(devices_schema: JsonObject) -> JsonObject:
    """Return the top-level project schema with a supplied devices schema."""
    duration = _duration_schema()
    positive_duration = _positive_duration_schema()
    return {
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "$id": "https://labodj.github.io/lsh-core/lsh_devices.schema.json",
        "title": "lsh-core static device configuration",
        "type": "object",
        "additionalProperties": False,
        "required": ["schema_version", "devices"],
        "properties": {
            "schema_version": {"const": 2},
            "preset": {"enum": sorted(PRESETS)},
            "generator": _generator_schema(),
            "controller": _controller_schema(),
            "features": _features_schema(),
            "timing": _timing_schema(duration, positive_duration),
            "serial": _serial_schema(duration, positive_duration),
            "advanced": _advanced_schema(),
            "devices": devices_schema,
        },
    }


def _duration_schema() -> JsonObject:
    """Return a duration schema that accepts zero milliseconds."""
    return {
        "oneOf": [
            {"type": "integer", "minimum": 0},
            {"type": "string", "pattern": r"^\s*\d+\s*(ms|s|m|h)?\s*$"},
        ]
    }


def _positive_duration_schema() -> JsonObject:
    """Return a duration schema that rejects zero milliseconds."""
    return {
        "oneOf": [
            {"type": "integer", "minimum": 1},
            {"type": "string", "pattern": r"^\s*[1-9]\d*\s*(ms|s|m|h)?\s*$"},
        ]
    }


def _target_name_schema(values: tuple[str, ...] | None) -> JsonObject:
    """Return a single target-name schema, optionally specialized as an enum."""
    if values is None:
        return {"type": "string", "minLength": 1}
    return {"enum": sorted(values)}


def _target_value_schema(values: tuple[str, ...] | None = None) -> JsonObject:
    """Return a string-or-list target schema with optional enum suggestions."""
    item = _target_name_schema(values)
    return {
        "oneOf": [
            item,
            {
                "type": "array",
                "items": item,
                "minItems": 1,
                "uniqueItems": True,
            },
        ]
    }


def _target_item_schema(target_value: JsonObject) -> JsonObject:
    """Return the scalar branch from a target string-or-list schema."""
    target_choices = cast("list[object]", target_value["oneOf"])
    return cast("JsonObject", target_choices[0])


def _click_action_schema(
    target_value: JsonObject,
    group_value: JsonObject | None = None,
    scene_names: tuple[str, ...] | None = None,
) -> JsonObject:
    """Return the click-action schema, specializing aliases when available."""
    positive_duration = _positive_duration_schema()
    target_name = _target_item_schema(target_value)
    resolved_group_value = group_value if group_value is not None else target_value
    group_name = _target_item_schema(resolved_group_value)
    scene_name = _target_name_schema(scene_names)
    return {
        "oneOf": [
            {"type": "boolean"},
            target_value,
            {
                "type": "object",
                "additionalProperties": False,
                "properties": {
                    "enabled": {"type": "boolean"},
                    "target": target_name,
                    "targets": target_value,
                    "group": group_name,
                    "groups": resolved_group_value,
                    "scene": scene_name,
                    "action": {"enum": ["toggle", "on", "off", "all_off"]},
                    "after": positive_duration,
                    "time": positive_duration,
                    "time_ms": {"type": "integer", "minimum": 1},
                    "network": {"type": "boolean"},
                    "fallback": {"enum": ["local", "do_nothing", "none"]},
                },
            },
        ]
    }


def _generic_devices_schema() -> JsonObject:
    """Return the generic devices-map schema."""
    return {
        "type": "object",
        "minProperties": 1,
        "additionalProperties": _generic_device_schema(),
    }


def _generic_device_schema() -> JsonObject:
    """Return a device schema without project-specific resource enums."""
    target_value = _target_value_schema()
    return _device_schema(
        actuator_value=target_value,
        scene_target_value=target_value,
        click_action=_click_action_schema(target_value),
        resource_names=None,
    )


def _project_device_schema(device: TomlTable) -> JsonObject:
    """Return a device schema specialized for one raw device table."""
    actuator_names = _resource_names(device.get("actuators"), tables_only=True)
    group_names = _resource_names(device.get("groups"), tables_only=False)
    scene_names = _resource_names(device.get("scenes"), tables_only=True)
    actuator_value = _target_value_schema(actuator_names)
    group_value = _target_value_schema(group_names)
    scene_target_value = _target_value_schema(
        tuple(sorted(actuator_names + group_names))
    )
    return _device_schema(
        actuator_value=actuator_value,
        scene_target_value=scene_target_value,
        click_action=_click_action_schema(
            actuator_value,
            group_value=group_value,
            scene_names=scene_names,
        ),
        resource_names={
            "actuators": actuator_names,
            "groups": group_names,
            "scenes": scene_names,
            "buttons": _resource_names(device.get("buttons"), tables_only=True),
            "indicators": _resource_names(device.get("indicators"), tables_only=True),
        },
    )


def _resource_names(raw: object, *, tables_only: bool) -> tuple[str, ...]:
    """Return public resource keys from a TOML object."""
    if not isinstance(raw, dict):
        return ()
    names = [
        name
        for name, value in raw.items()
        if isinstance(name, str) and (not tables_only or isinstance(value, dict))
    ]
    return tuple(sorted(names))


def _named_resource_map(
    values: tuple[str, ...] | None,
    value_schema: JsonObject,
) -> JsonObject:
    """Return a named-resource map, keeping unknown future keys writable."""
    resource_map: JsonObject = {
        "type": "object",
        "additionalProperties": value_schema,
    }
    if values:
        resource_map["properties"] = dict.fromkeys(values, value_schema)
    return resource_map


def _device_schema(
    *,
    actuator_value: JsonObject,
    scene_target_value: JsonObject,
    click_action: JsonObject,
    resource_names: dict[str, tuple[str, ...]] | None,
) -> JsonObject:
    """Return the device-resource schema."""
    duration = _duration_schema()
    positive_duration = _positive_duration_schema()
    names = resource_names or {}
    return {
        "type": "object",
        "additionalProperties": False,
        "properties": {
            "name": {"type": "string", "minLength": 1},
            "build_macro": {"type": "string", "pattern": r"^[A-Za-z_][A-Za-z0-9_]*$"},
            "hardware_include": {"type": "string", "minLength": 1},
            "debug_serial": {"type": "string", "minLength": 1},
            "bridge_serial": {"type": "string", "minLength": 1},
            "config_include": {"type": "string", "minLength": 1},
            "static_config_include": {"type": "string", "minLength": 1},
            "disable_rtc": {"type": "boolean"},
            "disable_eth": {"type": "boolean"},
            "controllino_pin_aliases": {"type": "boolean"},
            "features": _features_schema(),
            "timing": _timing_schema(duration, positive_duration),
            "serial": _serial_schema(duration, positive_duration),
            "advanced": _advanced_schema(),
            "actuators": {
                "type": "object",
                "additionalProperties": _actuator_schema(
                    positive_duration,
                    actuator_value,
                ),
                "properties": dict.fromkeys(
                    names.get("actuators", ()),
                    _actuator_schema(positive_duration, actuator_value),
                ),
            },
            "groups": _named_resource_map(
                names.get("groups"),
                _group_schema(actuator_value),
            ),
            "scenes": _named_resource_map(
                names.get("scenes"),
                _scene_schema(scene_target_value),
            ),
            "buttons": _named_resource_map(
                names.get("buttons"),
                _button_schema(click_action),
            ),
            "indicators": _named_resource_map(
                names.get("indicators"),
                _indicator_schema(actuator_value),
            ),
        },
    }


def _generator_schema() -> JsonObject:
    """Return the generator-section schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "properties": {
            "output_dir": {"type": "string", "default": "include"},
            "config_dir": {"type": "string", "default": "lsh_configs"},
            "user_config_header": {
                "type": "string",
                "default": "lsh_user_config.hpp",
            },
            "static_config_router_header": {
                "type": "string",
                "default": "lsh_static_config_router.hpp",
            },
            "id_lock_file": {
                "type": "string",
                "default": "lsh_devices.lock.toml",
            },
        },
    }


def _controller_schema() -> JsonObject:
    """Return the controller-section schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "properties": {
            "hardware_include": {"type": "string"},
            "debug_serial": {"type": "string"},
            "bridge_serial": {"type": "string"},
            "disable_rtc": {"type": "boolean"},
            "disable_eth": {"type": "boolean"},
            "controllino_pin_aliases": {"type": "boolean"},
        },
    }


def _features_schema() -> JsonObject:
    """Return the features-section schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "properties": {
            "codec": {"enum": ["json", "msgpack"]},
            "fast_io": {"type": "boolean"},
            "fast_buttons": {"type": "boolean"},
            "fast_actuators": {"type": "boolean"},
            "fast_indicators": {"type": "boolean"},
            "bench": {"type": "boolean"},
            "bench_iterations": {"type": "integer", "minimum": 1},
            "aggressive_constexpr_ctors": {
                "oneOf": [{"type": "boolean"}, {"const": "auto"}]
            },
            "etl_profile_override_header": {
                "oneOf": [{"type": "string"}, {"const": False}]
            },
        },
    }


def _timing_schema(duration: object, positive_duration: object) -> JsonObject:
    """Return the timing-section schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "properties": {
            "actuator_debounce": duration,
            "button_debounce": duration,
            "scan_interval": positive_duration,
            "long_click": positive_duration,
            "super_long_click": positive_duration,
            "network_click_timeout": positive_duration,
            "ping_interval": positive_duration,
            "connection_timeout": positive_duration,
            "bridge_boot_retry": positive_duration,
            "bridge_state_timeout": positive_duration,
            "post_receive_delay": duration,
            "network_click_check_interval": positive_duration,
            "auto_off_check_interval": positive_duration,
        },
    }


def _serial_schema(duration: object, positive_duration: object) -> JsonObject:
    """Return the serial-section schema."""
    del positive_duration
    return {
        "type": "object",
        "additionalProperties": False,
        "properties": {
            "debug_baud": {"type": "integer", "minimum": 1},
            "bridge_baud": {"type": "integer", "minimum": 1},
            "timeout": duration,
            "msgpack_frame_idle_timeout": duration,
            "max_rx_payloads_per_loop": {"type": "integer", "minimum": 1},
            "max_rx_bytes_per_loop": {"type": "integer", "minimum": 1},
            "flush_after_send": {"type": "boolean"},
            "rx_buffer_size": {"type": "integer", "minimum": 1},
            "tx_buffer_size": {"type": "integer", "minimum": 1},
        },
    }


def _advanced_schema() -> JsonObject:
    """Return the expert advanced-section schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "properties": {
            "build_flags": {
                "type": "array",
                "items": {"type": "string"},
            },
            "defines": {
                "type": "object",
                "additionalProperties": {
                    "oneOf": [
                        {"type": "boolean"},
                        {"type": "integer"},
                        {"type": "string"},
                    ]
                },
            },
        },
    }


def _actuator_schema(positive_duration: object, target_value: JsonObject) -> JsonObject:
    """Return the actuator-resource schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": ["pin"],
        "properties": {
            "id": {"type": "integer", "minimum": 1, "maximum": 255},
            "pin": {"type": "string", "minLength": 1},
            "default": {"type": "boolean"},
            "protected": {"type": "boolean"},
            "auto_off": positive_duration,
            "auto_off_ms": {"type": "integer", "minimum": 1},
            "pulse": positive_duration,
            "pulse_ms": {"type": "integer", "minimum": 1, "maximum": 65535},
            "interlock": target_value,
        },
    }


def _group_schema(target_value: JsonObject) -> JsonObject:
    """Return the local actuator-group schema."""
    return {
        "oneOf": [
            target_value,
            {
                "type": "object",
                "additionalProperties": False,
                "properties": {
                    "target": _target_item_schema(target_value),
                    "targets": target_value,
                },
            },
        ]
    }


def _scene_schema(target_value: JsonObject) -> JsonObject:
    """Return the deterministic scene schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "minProperties": 1,
        "properties": {
            "off": target_value,
            "on": target_value,
            "toggle": target_value,
        },
    }


def _button_schema(click_action: object) -> JsonObject:
    """Return the button-resource schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": ["pin"],
        "properties": {
            "id": {"type": "integer", "minimum": 1, "maximum": 255},
            "pin": {"type": "string", "minLength": 1},
            "short": click_action,
            "long": click_action,
            "super_long": click_action,
        },
    }


def _indicator_schema(target_value: JsonObject) -> JsonObject:
    """Return the indicator-resource schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": ["pin", "when"],
        "properties": {
            "pin": {"type": "string", "minLength": 1},
            "when": {
                "oneOf": [
                    target_value,
                    {
                        "type": "object",
                        "minProperties": 1,
                        "maxProperties": 1,
                        "additionalProperties": False,
                        "properties": {
                            "any": target_value,
                            "all": target_value,
                            "majority": target_value,
                        },
                    },
                ]
            },
        },
    }
