"""Stack-level configuration export derived from one lsh-core TOML profile."""

from __future__ import annotations

import json
from typing import TYPE_CHECKING, Any

from .models import DefineValue
from .platformio import merged_defines, render_escaped_build_flag_define
from .profile import collect_static_profile_data

if TYPE_CHECKING:
    from collections.abc import Mapping, Sequence

    from .models import DeviceConfig, ProjectConfig


LSH_BASE_PATH = "LSH/"
HOMIE_BASE_PATH = "homie/5/"
SERVICE_TOPIC = "LSH/Node-RED/SRV"
TOPIC_SUFFIX_INPUT = "IN"
TOPIC_SUFFIX_CONF = "conf"
TOPIC_SUFFIX_STATE = "state"
TOPIC_SUFFIX_EVENTS = "events"
TOPIC_SUFFIX_BRIDGE = "bridge"

BRIDGE_QOS_POLICY: Mapping[str, int] = {
    "deviceCommands": 2,
    "serviceCommands": 1,
    "confPublishes": 1,
    "statePublishes": 1,
    "eventsPublishes": 2,
    "bridgePublishes": 1,
}

COORDINATOR_SUBSCRIPTION_QOS: Mapping[str, int] = {
    TOPIC_SUFFIX_CONF: 2,
    TOPIC_SUFFIX_STATE: 2,
    TOPIC_SUFFIX_EVENTS: 2,
    TOPIC_SUFFIX_BRIDGE: 2,
    "homieState": 1,
}

CORE_TO_BRIDGE_DEFINE_MAP: Mapping[str, str] = {
    "CONFIG_COM_SERIAL_BAUD": "CONFIG_ARDCOM_SERIAL_BAUD",
    "CONFIG_COM_SERIAL_TIMEOUT_MS": "CONFIG_ARDCOM_SERIAL_TIMEOUT_MS",
    "CONFIG_COM_SERIAL_MSGPACK_FRAME_IDLE_TIMEOUT_MS": (
        "CONFIG_ARDCOM_SERIAL_MSGPACK_FRAME_IDLE_TIMEOUT_MS"
    ),
    "CONFIG_COM_SERIAL_MAX_RX_BYTES_PER_LOOP": (
        "CONFIG_ARDCOM_SERIAL_MAX_RX_BYTES_PER_LOOP"
    ),
    "CONFIG_CONNECTION_TIMEOUT_MS": "CONFIG_CONNECTION_TIMEOUT_CONTROLLINO_MS",
    "CONFIG_BRIDGE_BOOT_RETRY_INTERVAL_MS": "CONFIG_BOOTSTRAP_REQUEST_INTERVAL_MS",
}

CORE_DEFAULT_TIMING_DEFINES: Mapping[str, str] = {
    "CONFIG_COM_SERIAL_BAUD": "250000",
    "CONFIG_COM_SERIAL_TIMEOUT_MS": "5",
    "CONFIG_BRIDGE_BOOT_RETRY_INTERVAL_MS": "250",
}


def build_stack_config(
    project: ProjectConfig,
    selected_devices: Sequence[str],
) -> dict[str, Any]:
    """Build a machine-readable stack config for bridge/coordinator consumers."""
    devices = [project.devices[device_key] for device_key in selected_devices]
    mqtt_protocol = _stack_mqtt_protocol(project, devices)
    subscriptions = _coordinator_subscriptions(devices)
    device_exports = [
        _device_stack_export(project, device, mqtt_protocol) for device in devices
    ]
    unmapped_network_clicks = [
        click
        for device_export in device_exports
        for click in device_export["coordinator"]["unmappedNetworkClicks"]
    ]

    return {
        "schema": "lsh-stack-config/v1",
        "source": str(project.source_path),
        "lshBasePath": LSH_BASE_PATH,
        "homieBasePath": HOMIE_BASE_PATH,
        "serviceTopic": SERVICE_TOPIC,
        "protocol": mqtt_protocol,
        "qosPolicy": {
            "bridge": dict(BRIDGE_QOS_POLICY),
            "coordinatorSubscriptions": dict(COORDINATOR_SUBSCRIPTION_QOS),
        },
        "bridge": {
            "devices": {
                device_export["key"]: device_export["bridge"]
                for device_export in device_exports
            },
        },
        "controllers": {
            device_export["key"]: device_export["controller"]
            for device_export in device_exports
        },
        "coordinator": {
            "options": {
                "lshBasePath": LSH_BASE_PATH,
                "homieBasePath": HOMIE_BASE_PATH,
                "serviceTopic": SERVICE_TOPIC,
                "protocol": mqtt_protocol,
                "subscriptionQos": dict(COORDINATOR_SUBSCRIPTION_QOS),
            },
            "systemConfig": {
                "devices": [{"name": device.device_name} for device in devices],
            },
            "subscriptions": subscriptions,
            "unmappedNetworkClicks": unmapped_network_clicks,
        },
        "nodeRed": {
            "lshLogic": {
                "protocol": mqtt_protocol,
                "systemConfigJson": json.dumps(
                    {"devices": [{"name": device.device_name} for device in devices]},
                    indent=2,
                ),
            },
        },
        "footprint": {
            device_export["key"]: device_export["footprint"]
            for device_export in device_exports
        },
    }


def render_stack_config_json(
    project: ProjectConfig,
    selected_devices: Sequence[str],
) -> str:
    """Render the stack config as stable, pretty JSON."""
    return (
        json.dumps(
            build_stack_config(project, selected_devices),
            indent=2,
            sort_keys=True,
        )
        + "\n"
    )


def render_stack_report(project: ProjectConfig, selected_devices: Sequence[str]) -> str:
    """Render a human-readable stack footprint and integration report."""
    stack = build_stack_config(project, selected_devices)
    lines = [
        f"LSH stack export: {stack['source']}",
        (
            f"Protocol: {stack['protocol']} MQTT payloads, "
            f"LSH base {stack['lshBasePath']}, Homie base {stack['homieBasePath']}"
        ),
        "",
        "Devices",
    ]

    bridge_devices = stack["bridge"]["devices"]
    footprint = stack["footprint"]
    for device_key in selected_devices:
        device = project.devices[device_key]
        device_footprint = footprint[device_key]
        lines.extend(
            [
                (
                    f"- {device_key} ({device.device_name}): "
                    f"{device_footprint['actuators']} actuators, "
                    f"{device_footprint['buttons']} buttons, "
                    f"{device_footprint['indicators']} indicators"
                ),
                (
                    f"  state bytes: {device_footprint['packedStateBytes']}; "
                    f"network slots: {device_footprint['networkClickSlots']}; "
                    f"auto-off: {device_footprint['autoOffActuators']}; "
                    f"pulse: {device_footprint['pulseActuators']}; "
                    f"interlock edges: {device_footprint['interlockEdges']}"
                ),
                (
                    "  estimated core dynamic bytes: "
                    f"{device_footprint['estimatedCoreDynamicBytes']}"
                ),
                "  bridge flags:",
            ],
        )
        lines.extend(
            f"    {flag}" for flag in bridge_devices[device_key]["platformioBuildFlags"]
        )

    coordinator = stack["coordinator"]
    lines.extend(
        [
            "",
            "Coordinator",
            f"- devices: {len(coordinator['systemConfig']['devices'])}",
            f"- exact subscriptions: {len(coordinator['subscriptions'])}",
        ],
    )

    unmapped_clicks = coordinator["unmappedNetworkClicks"]
    if unmapped_clicks:
        lines.append("- unmapped network clicks needing coordinator actions:")
        lines.extend(
            (f"  {click['device']} button {click['buttonId']} {click['clickType']}")
            for click in unmapped_clicks
        )
    else:
        lines.append("- unmapped network clicks: none")

    return "\n".join(lines) + "\n"


def _device_stack_export(
    project: ProjectConfig,
    device: DeviceConfig,
    mqtt_protocol: str,
) -> dict[str, Any]:
    """Build all exported stack sections for one controller profile."""
    return {
        "key": device.key,
        "bridge": {
            "deviceName": device.device_name,
            "platformioBuildFlags": _bridge_build_flags(project, device, mqtt_protocol),
            "topics": _bridge_topics(device.device_name),
        },
        "controller": _controller_contract(device),
        "coordinator": {
            "device": {"name": device.device_name},
            "unmappedNetworkClicks": _network_click_placeholders(device),
        },
        "footprint": _footprint(device),
    }


def _stack_mqtt_protocol(
    project: ProjectConfig,
    devices: Sequence[DeviceConfig],
) -> str:
    """Return the MQTT payload protocol that keeps bridge and coordinator aligned."""
    uses_msgpack = any(
        "CONFIG_MSG_PACK" in _define_map(project, device) for device in devices
    )
    return "msgpack" if uses_msgpack else "json"


def _controller_contract(device: DeviceConfig) -> dict[str, object]:
    """Expose stable controller ids so stack composers can accept friendly names."""
    return {
        "deviceName": device.device_name,
        "actuators": [
            {"name": actuator.name, "id": actuator.actuator_id}
            for actuator in device.actuators
        ],
        "buttons": [
            {"name": clickable.name, "id": clickable.clickable_id}
            for clickable in device.clickables
        ],
        "indicators": [{"name": indicator.name} for indicator in device.indicators],
    }


def _define_map(project: ProjectConfig, device: DeviceConfig) -> dict[str, str | None]:
    """Return merged PlatformIO defines keyed by macro name."""
    return {define.name: define.value for define in merged_defines(project, device)}


def _bridge_build_flags(
    project: ProjectConfig,
    device: DeviceConfig,
    mqtt_protocol: str,
) -> list[str]:
    """Return per-device lsh-bridge PlatformIO build flags."""
    defines = _define_map(project, device)
    bridge_defines = [
        DefineValue("CONFIG_MAX_ACTUATORS", _uint_literal(len(device.actuators))),
        DefineValue("CONFIG_MAX_BUTTONS", _uint_literal(len(device.clickables))),
        DefineValue("CONFIG_MAX_NAME_LENGTH", _uint_literal(len(device.device_name))),
        DefineValue("CONFIG_MQTT_TOPIC_BASE", _quoted(LSH_BASE_PATH.rstrip("/"))),
        DefineValue("CONFIG_MQTT_TOPIC_INPUT", _quoted(TOPIC_SUFFIX_INPUT)),
        DefineValue("CONFIG_MQTT_TOPIC_STATE", _quoted(TOPIC_SUFFIX_STATE)),
        DefineValue("CONFIG_MQTT_TOPIC_CONF", _quoted(TOPIC_SUFFIX_CONF)),
        DefineValue("CONFIG_MQTT_TOPIC_EVENTS", _quoted(TOPIC_SUFFIX_EVENTS)),
        DefineValue("CONFIG_MQTT_TOPIC_BRIDGE", _quoted(TOPIC_SUFFIX_BRIDGE)),
        DefineValue("CONFIG_MQTT_TOPIC_SERVICE", _quoted(SERVICE_TOPIC)),
        DefineValue(
            "CONFIG_MQTT_QOS_DEVICE_COMMANDS",
            _uint_literal(BRIDGE_QOS_POLICY["deviceCommands"]),
        ),
        DefineValue(
            "CONFIG_MQTT_QOS_SERVICE_COMMANDS",
            _uint_literal(BRIDGE_QOS_POLICY["serviceCommands"]),
        ),
        DefineValue(
            "CONFIG_MQTT_QOS_CONF",
            _uint_literal(BRIDGE_QOS_POLICY["confPublishes"]),
        ),
        DefineValue(
            "CONFIG_MQTT_QOS_STATE",
            _uint_literal(BRIDGE_QOS_POLICY["statePublishes"]),
        ),
        DefineValue(
            "CONFIG_MQTT_QOS_EVENTS",
            _uint_literal(BRIDGE_QOS_POLICY["eventsPublishes"]),
        ),
        DefineValue(
            "CONFIG_MQTT_QOS_BRIDGE",
            _uint_literal(BRIDGE_QOS_POLICY["bridgePublishes"]),
        ),
    ]

    for core_define, bridge_define in CORE_TO_BRIDGE_DEFINE_MAP.items():
        value = defines.get(core_define, CORE_DEFAULT_TIMING_DEFINES.get(core_define))
        if value is not None:
            bridge_defines.append(DefineValue(bridge_define, _uint_literal(value)))

    if "CONFIG_MSG_PACK" in defines:
        bridge_defines.append(DefineValue("CONFIG_MSG_PACK_ARDUINO"))
    if mqtt_protocol == "msgpack":
        bridge_defines.append(DefineValue("CONFIG_MSG_PACK_MQTT"))

    return [render_escaped_build_flag_define(define) for define in bridge_defines]


def _coordinator_subscriptions(
    devices: Sequence[DeviceConfig],
) -> dict[str, dict[str, int]]:
    """Return exact coordinator MQTT subscriptions for the selected devices."""
    subscriptions: dict[str, dict[str, int]] = {}
    for device in devices:
        prefix = f"{LSH_BASE_PATH}{device.device_name}"
        subscriptions[f"{prefix}/{TOPIC_SUFFIX_CONF}"] = {
            "qos": COORDINATOR_SUBSCRIPTION_QOS[TOPIC_SUFFIX_CONF],
        }
        subscriptions[f"{prefix}/{TOPIC_SUFFIX_STATE}"] = {
            "qos": COORDINATOR_SUBSCRIPTION_QOS[TOPIC_SUFFIX_STATE],
        }
        subscriptions[f"{prefix}/{TOPIC_SUFFIX_EVENTS}"] = {
            "qos": COORDINATOR_SUBSCRIPTION_QOS[TOPIC_SUFFIX_EVENTS],
        }
        subscriptions[f"{prefix}/{TOPIC_SUFFIX_BRIDGE}"] = {
            "qos": COORDINATOR_SUBSCRIPTION_QOS[TOPIC_SUFFIX_BRIDGE],
        }
        subscriptions[f"{HOMIE_BASE_PATH}{device.device_name}/$state"] = {
            "qos": COORDINATOR_SUBSCRIPTION_QOS["homieState"],
        }
    return subscriptions


def _bridge_topics(device_name: str) -> dict[str, str]:
    """Return the concrete LSH MQTT topics used by one bridge/device pair."""
    prefix = f"{LSH_BASE_PATH}{device_name}"
    return {
        "input": f"{prefix}/{TOPIC_SUFFIX_INPUT}",
        "conf": f"{prefix}/{TOPIC_SUFFIX_CONF}",
        "state": f"{prefix}/{TOPIC_SUFFIX_STATE}",
        "events": f"{prefix}/{TOPIC_SUFFIX_EVENTS}",
        "bridge": f"{prefix}/{TOPIC_SUFFIX_BRIDGE}",
        "service": SERVICE_TOPIC,
    }


def _network_click_placeholders(device: DeviceConfig) -> list[dict[str, object]]:
    """List network clicks that still need coordinator-side actor targets."""
    clicks: list[dict[str, object]] = []
    for clickable in device.clickables:
        if clickable.long.enabled and clickable.long.network:
            clicks.append(
                {
                    "device": device.device_name,
                    "buttonId": clickable.clickable_id,
                    "button": clickable.name,
                    "clickType": "long",
                }
            )
        if clickable.super_long.enabled and clickable.super_long.network:
            clicks.append(
                {
                    "device": device.device_name,
                    "buttonId": clickable.clickable_id,
                    "button": clickable.name,
                    "clickType": "superLong",
                }
            )
    return clicks


def _footprint(device: DeviceConfig) -> dict[str, int]:
    """Return compact footprint facts for one static profile."""
    profile = collect_static_profile_data(device)
    packed_state_bytes = (len(device.actuators) + 7) >> 3
    auto_off_bytes = len(profile.auto_off_indexes) * 4
    pulse_bytes = len(profile.pulse_indexes) * 2
    network_click_bytes = len(profile.network_click_slots) * 6
    interlock_edges = sum(
        len(actuator.interlock_targets) for actuator in device.actuators
    )
    return {
        "actuators": len(device.actuators),
        "buttons": len(device.clickables),
        "indicators": len(device.indicators),
        "packedStateBytes": packed_state_bytes,
        "networkClickSlots": len(profile.network_click_slots),
        "autoOffActuators": len(profile.auto_off_indexes),
        "pulseActuators": len(profile.pulse_indexes),
        "interlockEdges": interlock_edges,
        "shortLinks": profile.short_links,
        "longLinks": profile.long_links,
        "superLongLinks": profile.super_long_links,
        "indicatorLinks": profile.indicator_links,
        "estimatedCoreDynamicBytes": (
            packed_state_bytes + auto_off_bytes + pulse_bytes + network_click_bytes
        ),
    }


def _quoted(value: str) -> str:
    """Return a quoted C preprocessor string literal."""
    return f'"{value}"'


def _uint_literal(value: object) -> str:
    """Render a non-negative integer-like value as a C++ unsigned literal."""
    text = str(value).strip()
    if text.endswith(("U", "u")):
        text = text[:-1]
    return f"{int(text)}U"
