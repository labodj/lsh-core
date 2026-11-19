"""JSON Schema for editor autocomplete of `lsh_devices.toml` schema v2."""

from __future__ import annotations

import json
import tomllib
from typing import TYPE_CHECKING, cast

from .constants import DURATION_UNIT, MAX_AUTO_OFF_MS, UINT8_MAX, UINT32_MAX
from .errors import fail
from .parser import parse_project
from .presets import PRESETS
from .public_schema import (
    SERIAL_BUFFER_FLAG_MAP,
    SERIAL_INTEGER_LIMITS,
    TIMING_DEFINE_MAP,
)
from .validation import validate_output_paths

if TYPE_CHECKING:
    from pathlib import Path

    from .models import ProjectConfig, TomlTable

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
    return write_schema_for_project(parse_project(config_path), output_path)


def write_schema_for_project(
    project: ProjectConfig, output_path: Path | None = None
) -> Path:
    """Write the schema beside an already validated project."""
    config_path = project.source_path
    schema_path = output_path or default_vscode_schema_path(config_path)
    validate_output_paths(project, schema_path)
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
    except (OSError, UnicodeError) as exc:
        fail(f"Cannot read {path}: {exc}")
    except tomllib.TOMLDecodeError as exc:
        fail(f"Invalid TOML in {path}: {exc}")
    if not isinstance(root, dict):
        fail("root must be a TOML table.")
    return cast("TomlTable", root)


def _root_schema(devices_schema: JsonObject) -> JsonObject:
    """Return the top-level project schema with a supplied devices schema."""
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
            "timing": _timing_schema(),
            "serial": _serial_schema(),
            "advanced": _advanced_schema(),
            "devices": devices_schema,
        },
    }


def _duration_schema() -> JsonObject:
    """Return a duration schema that accepts zero milliseconds."""
    return {
        "oneOf": [
            {"type": "integer", "minimum": 0},
            {"type": "string", "pattern": rf"^\s*\d+\s*{DURATION_UNIT}\s*$"},
        ]
    }


def _positive_duration_schema() -> JsonObject:
    """Return a duration schema that rejects zero milliseconds."""
    return {
        "oneOf": [
            {"type": "integer", "minimum": 1},
            {"type": "string", "pattern": rf"^\s*0*[1-9]\d*\s*{DURATION_UNIT}\s*$"},
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
    selectors = ("target", "targets", "group", "groups")
    local = [{"required": [key]} for key in (*selectors, "scene")]
    common = {
        "enabled": {"type": "boolean"},
        "target": target_name,
        "targets": target_value,
        "group": group_name,
        "groups": resolved_group_value,
        "scene": scene_name,
    }
    timed = {
        "after": positive_duration,
        "time": positive_duration,
        "time_ms": {"type": "integer", "minimum": 1, "maximum": 65535},
        "network": {"type": "boolean"},
        "fallback": {"enum": ["local", "do_nothing", "none"]},
    }
    network = {"required": ["network"], "properties": {"network": {"const": True}}}
    disabled = {"required": ["enabled"], "properties": {"enabled": {"const": False}}}
    actions = {
        "short": ["toggle", "normal"],
        "long": ["toggle", "on", "off"],
        "super_long": ["off", "all_off"],
    }
    result: JsonObject = {}
    for kind, operations in actions.items():
        properties = {
            **common,
            **(timed if kind != "short" else {}),
            "action": {"enum": operations},
        }
        conflicts = [
            ("target", "targets"),
            ("group", "groups"),
            *(("scene", k) for k in (*selectors, "action")),
        ]
        if kind != "short":
            conflicts.extend(
                [("after", "time"), ("after", "time_ms"), ("time", "time_ms")]
            )
        rules: list[JsonObject] = [
            {"not": {"required": list(pair)}} for pair in conflicts
        ]
        rules.append(
            {
                "if": disabled,
                "then": {
                    "not": {
                        "anyOf": [
                            *local,
                            network,
                            *(
                                {"required": [key]}
                                for key in ("after", "time", "time_ms")
                            ),
                        ]
                    }
                },
            }
        )
        compact = target_value
        if kind == "super_long":
            rules.append(
                {
                    "if": {
                        "required": ["action"],
                        "properties": {"action": {"const": "all_off"}},
                    },
                    "then": {"not": {"anyOf": local}},
                }
            )
            rules.append(
                {
                    "if": {
                        "required": ["action"],
                        "properties": {"action": {"const": "off"}},
                    },
                    "then": {"anyOf": [*local, network, disabled]},
                }
            )
            keywords = ("all_off", "off", "disabled", "none")
            known = target_name.get("enum")
            if isinstance(known, list):
                reserved = {name.lower() for name in known}.intersection(keywords)
                names = [name for name in known if name.lower() not in reserved]
                words = [
                    "".join(
                        f"[{c.lower()}{c.upper()}]" if c.isalpha() else c for c in word
                    )
                    for word in keywords
                    if word not in reserved
                ]
                compact = {
                    "anyOf": [
                        _target_name_schema(tuple(names)),
                        *(
                            [
                                {
                                    "type": "string",
                                    "pattern": "^(?:" + "|".join(words) + ")$",
                                }
                            ]
                            if words
                            else []
                        ),
                        cast("list[object]", target_value["oneOf"])[1],
                    ]
                }

        else:
            rules.append(
                {"anyOf": [*local, disabled, *([network] if kind == "long" else [])]}
            )
        result[kind] = {
            "oneOf": [
                {"type": "boolean"} if kind == "super_long" else {"const": False},
                compact,
                {
                    "type": "object",
                    "additionalProperties": False,
                    "properties": properties,
                    "allOf": rules,
                },
            ]
        }
    return result


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
            "interlocks": _resource_names(device.get("interlocks"), tables_only=True),
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
    positive_duration = _positive_duration_schema()
    names = resource_names or {}
    return {
        "type": "object",
        "additionalProperties": False,
        "properties": {
            "name": {
                "type": "string",
                "minLength": 1,
                "maxLength": 255,
                "pattern": r"^[A-Za-z0-9_-]+$",
            },
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
            "timing": _timing_schema(),
            "serial": _serial_schema(),
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
            "interlocks": _named_resource_map(
                names.get("interlocks"),
                _interlock_schema(actuator_value),
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
                "pattern": r"^[A-Za-z0-9_-][A-Za-z0-9_.-]*\.h(pp)?$",
            },
            "static_config_router_header": {
                "type": "string",
                "default": "lsh_static_config_router.hpp",
                "pattern": r"^[A-Za-z0-9_-][A-Za-z0-9_.-]*\.h(pp)?$",
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
            "bench_iterations": {
                "type": "integer",
                "minimum": 1,
                "maximum": UINT32_MAX,
            },
            "aggressive_constexpr_ctors": {
                "oneOf": [{"type": "boolean"}, {"const": "auto"}]
            },
            "etl_profile_override_header": {
                "oneOf": [{"type": "string"}, {"const": False}]
            },
        },
    }


def _timing_schema() -> JsonObject:
    """Use the generator's AVR field widths for editor feedback."""
    return {
        "type": "object",
        "additionalProperties": False,
        "properties": {
            field: _bounded_duration_schema(allow_zero=allow_zero, maximum=maximum)
            for field, (_, allow_zero, maximum) in TIMING_DEFINE_MAP.items()
        },
    }


def _bounded_duration_schema(*, allow_zero: bool, maximum: int) -> JsonObject:
    """Bound millisecond integers and document the limit for unit strings."""
    duration = _duration_schema() if allow_zero else _positive_duration_schema()
    return {
        **duration,
        "maximum": maximum,
        "description": f"Maximum duration: {maximum} ms.",
    }


def _serial_schema() -> JsonObject:
    """Return the serial-section schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "properties": {
            **{
                field: {
                    "type": "integer",
                    "minimum": 2 if field in SERIAL_BUFFER_FLAG_MAP else 1,
                    "maximum": maximum,
                }
                for field, maximum in SERIAL_INTEGER_LIMITS.items()
            },
            "timeout": _bounded_duration_schema(allow_zero=True, maximum=UINT8_MAX),
            "msgpack_frame_idle_timeout": _bounded_duration_schema(
                allow_zero=True, maximum=UINT8_MAX
            ),
            "flush_after_send": {"type": "boolean"},
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
            "auto_off": _bounded_duration_schema(
                allow_zero=False, maximum=MAX_AUTO_OFF_MS
            ),
            "auto_off_ms": {
                "type": "integer",
                "minimum": 1,
                "maximum": MAX_AUTO_OFF_MS,
            },
            "pulse": positive_duration,
            "pulse_ms": {"type": "integer", "minimum": 1, "maximum": 65535},
            "interlock": target_value,
            "directed_interlock": {"type": "boolean"},
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


def _interlock_schema(target_value: JsonObject) -> JsonObject:
    """Return the reciprocal interlock-group schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": ["actuators"],
        "properties": {
            "actuators": target_value,
        },
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


def _button_schema(click_action: JsonObject) -> JsonObject:
    """Return the button-resource schema."""
    return {
        "type": "object",
        "additionalProperties": False,
        "required": ["pin"],
        "properties": {
            "id": {"type": "integer", "minimum": 1, "maximum": 255},
            "pin": {"type": "string", "minLength": 1},
            **click_action,
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
