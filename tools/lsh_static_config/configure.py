"""Render the generated Configurator::configure implementation."""

from __future__ import annotations

from typing import TYPE_CHECKING

from .cpp import u8
from .topology import target_indexes

if TYPE_CHECKING:
    from collections.abc import Sequence

    from .models import ActuatorConfig, ClickableConfig, ClickAction, DeviceConfig


def render_action_calls(action: ClickAction, action_name: str) -> list[str]:
    """Render runtime flag method calls for one click action."""
    if not action.enabled:
        return []

    setter = "setClickableLong" if action_name == "long" else "setClickableSuperLong"
    calls: list[str] = []
    if action.network or action.fallback != "LOCAL_FALLBACK":
        calls.append(
            f"{setter}(true, {str(action.network).lower()}, "
            f"NoNetworkClickType::{action.fallback})"
        )
    else:
        calls.append(f"{setter}(true)")

    return calls


def render_fluent_line(object_name: str, calls: Sequence[str]) -> str:
    """Join generated fluent C++ calls for one object."""
    return f"{object_name}." + ".".join(calls) + ";"


def render_link_chain(
    object_name: str, method_name: str, target_indexes: Sequence[int]
) -> str:
    """Render a fluent actuator-link chain."""
    return render_fluent_line(
        object_name,
        [f"{method_name}({u8(target_index)})" for target_index in target_indexes],
    )


def _click_action_needs_network_alias(action: ClickAction) -> bool:
    """Return true when generated code must name NoNetworkClickType."""
    return action.enabled and (action.network or action.fallback != "LOCAL_FALLBACK")


def _needs_network_alias(device: DeviceConfig) -> bool:
    """Return true when configure() needs the network fallback enum alias."""
    return any(
        _click_action_needs_network_alias(action)
        for clickable in device.clickables
        for action in (clickable.long, clickable.super_long)
    )


def _needs_indicator_mode_alias(device: DeviceConfig) -> bool:
    """Return true when configure() needs the indicator mode enum alias."""
    return any(indicator.mode != "ANY" for indicator in device.indicators)


def _append_configure_section(lines: list[str], statements: Sequence[str]) -> None:
    """Append a blank-separated indented configure() section."""
    if not statements:
        return
    lines.append("")
    lines.extend(f"    {statement}" for statement in statements)


def _actuator_registration_line(index: int, actuator: ActuatorConfig) -> str:
    """Render one Actuators::addActuator call."""
    return (
        f"Actuators::addActuator(&{actuator.name}, "
        f"{u8(actuator.actuator_id)}, {u8(index)});"
    )


def _clickable_registration_line(index: int, clickable: ClickableConfig) -> str:
    """Render one Clickables::addClickable call."""
    return (
        f"Clickables::addClickable(&{clickable.name}, "
        f"{u8(clickable.clickable_id)}, {u8(index)});"
    )


def _actuator_registration_lines(device: DeviceConfig) -> list[str]:
    """Render actuator registration statements."""
    return [
        _actuator_registration_line(index, actuator)
        for index, actuator in enumerate(device.actuators)
    ]


def _clickable_registration_lines(device: DeviceConfig) -> list[str]:
    """Render clickable registration statements."""
    return [
        _clickable_registration_line(index, clickable)
        for index, clickable in enumerate(device.clickables)
    ]


def _indicator_registration_lines(device: DeviceConfig) -> list[str]:
    """Render indicator registration statements."""
    return [
        f"Indicators::addIndicator(&{indicator.name}, {u8(index)});"
        for index, indicator in enumerate(device.indicators)
    ]


def _protected_actuator_lines(device: DeviceConfig) -> list[str]:
    """Render actuator protection setup statements."""
    return [
        f"{actuator.name}.setProtected(true);"
        for actuator in device.actuators
        if actuator.protected
    ]


def _disabled_short_lines(device: DeviceConfig) -> list[str]:
    """Render explicit short-click disable statements."""
    return [
        f"{clickable.name}.setClickableShort(false);"
        for clickable in device.clickables
        if not clickable.short_enabled
    ]


def _short_link_lines(
    device: DeviceConfig,
    actuator_indexes: dict[str, int],
) -> list[str]:
    """Render short-click local actuator links."""
    return [
        render_link_chain(
            clickable.name,
            "addActuatorShort",
            target_indexes(clickable.short_targets, actuator_indexes),
        )
        for clickable in device.clickables
        if clickable.short_targets
    ]


def _click_action_lines(device: DeviceConfig) -> list[str]:
    """Render long and super-long click setup chains."""
    lines: list[str] = []
    for clickable in device.clickables:
        calls = render_action_calls(clickable.long, "long")
        calls.extend(render_action_calls(clickable.super_long, "super_long"))
        if calls:
            lines.append(render_fluent_line(clickable.name, calls))
    return lines


def _long_link_lines(
    device: DeviceConfig,
    actuator_indexes: dict[str, int],
) -> list[str]:
    """Render long-click local actuator links."""
    return [
        render_link_chain(
            clickable.name,
            "addActuatorLong",
            target_indexes(clickable.long.targets, actuator_indexes),
        )
        for clickable in device.clickables
        if clickable.long.targets
    ]


def _super_long_link_lines(
    device: DeviceConfig,
    actuator_indexes: dict[str, int],
) -> list[str]:
    """Render super-long-click local actuator links."""
    return [
        render_link_chain(
            clickable.name,
            "addActuatorSuperLong",
            target_indexes(clickable.super_long.targets, actuator_indexes),
        )
        for clickable in device.clickables
        if clickable.super_long.targets
    ]


def _indicator_mode_lines(device: DeviceConfig) -> list[str]:
    """Render non-default indicator mode setup statements."""
    return [
        f"{indicator.name}.setMode(IndicatorMode::{indicator.mode});"
        for indicator in device.indicators
        if indicator.mode != "ANY"
    ]


def _indicator_link_lines(
    device: DeviceConfig,
    actuator_indexes: dict[str, int],
) -> list[str]:
    """Render indicator actuator links."""
    return [
        render_link_chain(
            indicator.name,
            "addActuator",
            target_indexes(indicator.targets, actuator_indexes),
        )
        for indicator in device.indicators
        if indicator.targets
    ]


def render_configure(device: DeviceConfig) -> list[str]:
    """Render Configurator::configure for the normalized device profile."""
    lines = ["void Configurator::configure()", "{", "    using namespace Debug;"]
    if _needs_network_alias(device):
        lines.append("    using constants::NoNetworkClickType;")
    lines.extend(["", "    DP_CONTEXT();"])

    _append_configure_section(lines, ["disableRtc();"] if device.disable_rtc else [])
    _append_configure_section(lines, ["disableEth();"] if device.disable_eth else [])
    _append_configure_section(lines, _actuator_registration_lines(device))
    _append_configure_section(lines, _clickable_registration_lines(device))
    _append_configure_section(lines, _indicator_registration_lines(device))
    _append_configure_section(lines, _protected_actuator_lines(device))
    _append_configure_section(lines, _disabled_short_lines(device))
    _append_configure_section(lines, _click_action_lines(device))

    lines.append("}")
    return lines
