"""Render the generated Configurator::configure implementation."""

from __future__ import annotations

from typing import TYPE_CHECKING

from .cpp import u8

if TYPE_CHECKING:
    from collections.abc import Sequence

    from .models import (
        ActuatorConfig,
        ClickableConfig,
        DeviceConfig,
        IndicatorConfig,
    )


def _append_configure_section(lines: list[str], statements: Sequence[str]) -> None:
    """Append a blank-separated indented configure() section."""
    if not statements:
        return
    lines.append("")
    lines.extend(
        statement if statement.startswith("#") else f"    {statement}"
        for statement in statements
    )


def _packed_initial_state_lines(device: DeviceConfig) -> list[str]:
    """Render direct initialization for the packed actuator-state shadow."""
    byte_count = max(1, (len(device.actuators) + 7) // 8)
    packed_bytes = [0 for _ in range(byte_count)]
    for actuator_index, actuator in enumerate(device.actuators):
        if actuator.default_state:
            packed_bytes[actuator_index >> 3] |= 1 << (actuator_index & 0x07)
    return [
        f"Actuators::packedActuatorStates[{u8(byte_index)}] = {u8(packed_byte)};"
        for byte_index, packed_byte in enumerate(packed_bytes)
    ]


def _actuator_registration_lines_for(index: int, actuator: ActuatorConfig) -> list[str]:
    """Render direct static registration for one generated actuator."""
    return [
        "#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)",
        f"{actuator.name}.setIndex({u8(index)});",
        f"Actuators::actuators[{u8(index)}] = &{actuator.name};",
        "#endif",
    ]


def _clickable_registration_lines_for(
    index: int,
    clickable: ClickableConfig,
) -> list[str]:
    """Render direct static registration for one generated clickable."""
    return [
        "#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)",
        f"{clickable.name}.setIndex({u8(index)});",
        f"Clickables::clickables[{u8(index)}] = &{clickable.name};",
        "#endif",
    ]


def _indicator_registration_lines_for(
    index: int,
    indicator: IndicatorConfig,
) -> list[str]:
    """Render direct static registration for one generated indicator."""
    return [
        "#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)",
        f"{indicator.name}.setIndex({u8(index)});",
        f"Indicators::indicators[{u8(index)}] = &{indicator.name};",
        "#endif",
    ]


def _actuator_registration_lines(device: DeviceConfig) -> list[str]:
    """Render actuator registration statements."""
    lines: list[str] = []
    for index, actuator in enumerate(device.actuators):
        lines.extend(_actuator_registration_lines_for(index, actuator))
    return lines


def _clickable_registration_lines(device: DeviceConfig) -> list[str]:
    """Render clickable registration statements."""
    lines: list[str] = []
    for index, clickable in enumerate(device.clickables):
        lines.extend(_clickable_registration_lines_for(index, clickable))
    return lines


def _indicator_registration_lines(device: DeviceConfig) -> list[str]:
    """Render indicator registration statements."""
    lines: list[str] = []
    for index, indicator in enumerate(device.indicators):
        lines.extend(_indicator_registration_lines_for(index, indicator))
    return lines


def _protected_actuator_lines(device: DeviceConfig) -> list[str]:
    """Render actuator protection setup statements."""
    return [
        f"{actuator.name}.setProtected(true);"
        for actuator in device.actuators
        if actuator.protected
    ]


def render_configure(device: DeviceConfig) -> list[str]:
    """Render Configurator::configure for the normalized device profile."""
    lines = ["void Configurator::configure()", "{", "    using namespace Debug;"]
    lines.extend(["", "    DP_CONTEXT();"])

    _append_configure_section(lines, ["disableRtc();"] if device.disable_rtc else [])
    _append_configure_section(lines, ["disableEth();"] if device.disable_eth else [])
    _append_configure_section(lines, _packed_initial_state_lines(device))
    _append_configure_section(lines, _actuator_registration_lines(device))
    _append_configure_section(lines, _clickable_registration_lines(device))
    _append_configure_section(lines, _indicator_registration_lines(device))
    _append_configure_section(lines, _protected_actuator_lines(device))

    lines.append("}")
    return lines
