"""Render generated runtime helpers outside the parser/configuration layer."""

from __future__ import annotations

from typing import TYPE_CHECKING

from .action_bodies import (
    render_bool_accumulator,
    render_switch_case_with_body,
    render_u8_sum_declaration,
)
from .action_calls import (
    render_generated_actuator_action_helpers,
    render_set_state_call,
)
from .click_actions import (
    render_direct_long_clicks,
    render_direct_short_clicks,
    render_direct_super_long_clicks,
    render_get_network_click_clickable_index,
    render_get_network_click_slot_count,
    render_get_network_click_slot_index,
    render_get_network_click_type,
    render_is_clickable_configuration_valid,
    render_run_click,
    render_run_network_click_fallback,
    render_turn_off_all_actuators,
    render_turn_off_unprotected_actuators,
)
from .click_scan import render_scan_clickables
from .constants import CLANG_FORMAT_COLUMN_LIMIT
from .cpp import append_section, u8, u32
from .topology import actuator_name_at, indicator_object_name

if TYPE_CHECKING:
    from collections.abc import Sequence

    from .models import DeviceConfig, StaticProfileData


def _indicator_state_terms(device: DeviceConfig, links: Sequence[int]) -> list[str]:
    """Return direct getState() terms for one generated indicator expression."""
    return [
        f"{actuator_name_at(device, actuator_index)}.getState()"
        for actuator_index in links
    ]


def _render_chained_bool_assignment(
    variable_name: str,
    terms: Sequence[str],
    operator: str,
    *,
    indent: str,
) -> list[str]:
    """Render a clang-format-stable short-circuit bool chain."""
    first_term, *remaining_terms = terms
    lines = [f"{indent}bool {variable_name} = {first_term};"]
    lines.extend(
        f"{indent}{variable_name} = {variable_name} {operator} {term};"
        for term in remaining_terms
    )
    return lines


def _render_indicator_apply_from_terms(
    indicator_name: str,
    terms: Sequence[str],
    operator: str,
) -> list[str]:
    """Render applyComputedState() without creating clang-format churn."""
    expression = f" {operator} ".join(terms)
    direct_line = f"    {indicator_name}.applyComputedState({expression});"
    if len(direct_line) <= CLANG_FORMAT_COLUMN_LIMIT:
        return [direct_line]

    variable_name = f"{indicator_name}State"
    variable_line = f"    const bool {variable_name} = {expression};"
    if len(variable_line) <= CLANG_FORMAT_COLUMN_LIMIT:
        return [
            variable_line,
            f"    {indicator_name}.applyComputedState({variable_name});",
        ]

    lines = ["    {"]
    lines.extend(
        _render_chained_bool_assignment(
            "indicatorState", terms, operator, indent="        "
        )
    )
    lines.extend(
        [
            f"        {indicator_name}.applyComputedState(indicatorState);",
            "    }",
        ]
    )
    return lines


def render_apply_packed_state_byte(
    device: DeviceConfig,
) -> list[str]:
    """Render direct packed-state application with known byte/bit boundaries."""
    lines = [
        (
            "auto applyPackedActuatorStateByte(uint8_t byteIndex, "
            "uint8_t packedByte) noexcept -> bool"
        ),
        "{",
    ]
    if not device.actuators:
        lines.extend(
            [
                "    static_cast<void>(byteIndex);",
                "    static_cast<void>(packedByte);",
                "    return false;",
                "}",
            ]
        )
        return lines

    lines.extend(["    switch (byteIndex)", "    {"])
    packed_byte_count = (len(device.actuators) + 7) // 8
    for byte_index in range(packed_byte_count):
        start = byte_index * 8
        end = min(start + 8, len(device.actuators))
        cached_time = (end - start) > 1
        calls = [
            render_set_state_call(
                device,
                actuator_index,
                f"(packedByte & {u8(1 << (actuator_index - start))}) != 0U",
                cached_time=cached_time,
            )
            for actuator_index in range(start, end)
        ]
        render_switch_case_with_body(
            lines,
            byte_index,
            render_bool_accumulator(calls, with_cached_time=cached_time),
        )
    lines.extend(["    default:", "        return false;", "    }", "}"])
    return lines


def render_set_actuator_state_by_id(device: DeviceConfig) -> list[str]:
    """Render direct inbound actuator command dispatch by wire ID."""
    lines = [
        "auto setActuatorStateById(uint8_t actuatorId, bool state) noexcept -> bool",
        "{",
    ]
    if not device.actuators:
        lines.extend(
            [
                "    static_cast<void>(actuatorId);",
                "    static_cast<void>(state);",
                "    return false;",
                "}",
            ]
        )
        return lines

    lines.extend(["    switch (actuatorId)", "    {"])
    for actuator_index, actuator in enumerate(device.actuators):
        set_call = render_set_state_call(
            device,
            actuator_index,
            "state",
            cached_time=False,
        )
        render_switch_case_with_body(
            lines,
            actuator.actuator_id,
            [f"        return {set_call};"],
        )
    lines.extend(["    default:", "        return false;", "    }", "}"])
    return lines


def render_indicator_case_body(
    device: DeviceConfig,
    links: Sequence[int],
    mode: str,
) -> list[str]:
    """Render the direct state expression for one generated indicator."""
    if not links:
        return ["        return false;"]

    if mode == "ANY":
        terms = _indicator_state_terms(device, links)
        expression = " || ".join(terms)
        line = f"        return {expression};"
        if len(line) <= CLANG_FORMAT_COLUMN_LIMIT:
            return [line]
        lines = _render_chained_bool_assignment(
            "indicatorState", terms, "||", indent="        "
        )
        lines.append("        return indicatorState;")
        return lines

    if mode == "ALL":
        terms = _indicator_state_terms(device, links)
        expression = " && ".join(terms)
        line = f"        return {expression};"
        if len(line) <= CLANG_FORMAT_COLUMN_LIMIT:
            return [line]
        lines = _render_chained_bool_assignment(
            "indicatorState", terms, "&&", indent="        "
        )
        lines.append("        return indicatorState;")
        return lines

    terms = [
        f"static_cast<uint8_t>({actuator_name_at(device, actuator_index)}.getState())"
        for actuator_index in links
    ]
    lines = render_u8_sum_declaration("totalControlledActuatorsOn", terms)
    lines.append(
        "        return "
        f"static_cast<uint8_t>(totalControlledActuatorsOn << 1U) > {u8(len(links))};"
    )
    return lines


def render_compute_indicator_state(
    device: DeviceConfig,
    profile: StaticProfileData,
) -> list[str]:
    """Render direct indicator state computation from generated topology."""
    lines = [
        "auto computeIndicatorState(uint8_t indicatorIndex) noexcept -> bool",
        "{",
    ]
    cases = [
        (
            indicator_index,
            render_indicator_case_body(
                device,
                profile.indicator_link_sets[indicator_index],
                indicator.mode,
            ),
        )
        for indicator_index, indicator in enumerate(device.indicators)
    ]
    if not cases:
        lines.extend(
            ["    static_cast<void>(indicatorIndex);", "    return false;", "}"]
        )
        return lines

    lines.extend(["    switch (indicatorIndex)", "    {"])
    for indicator_index, body in cases:
        render_switch_case_with_body(lines, indicator_index, body)
    lines.extend(["    default:", "        return false;", "    }", "}"])
    return lines


def render_check_auto_off_timers(device: DeviceConfig) -> list[str]:
    """Render the generated auto-off sweep over known actuator/timer pairs."""
    entries = [
        (actuator_index, actuator)
        for actuator_index, actuator in enumerate(device.actuators)
        if actuator.auto_off_ms is not None
    ]
    lines = ["auto checkAutoOffTimers(uint32_t now_ms) noexcept -> bool", "{"]
    if not entries:
        lines.extend(["    static_cast<void>(now_ms);", "    return false;", "}"])
        return lines

    lines.append("    bool anyActuatorChangedState = false;")
    for auto_off_index, (actuator_index, actuator) in enumerate(entries):
        timer_ms = actuator.auto_off_ms if actuator.auto_off_ms is not None else 0
        lines.extend(
            [
                "#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES",
                (
                    "    anyActuatorChangedState |= "
                    f"Actuators::checkCompactAutoOffTimer({u8(auto_off_index)}, "
                    f"{u8(actuator_index)}, "
                    f"{actuator_name_at(device, actuator_index)}, "
                    f"now_ms, {u32(timer_ms)});"
                ),
                "#else",
                (
                    "    anyActuatorChangedState |= "
                    f"{actuator_name_at(device, actuator_index)}"
                    f".checkAutoOffTimerForIndex({u8(actuator_index)}, now_ms, "
                    f"{u32(timer_ms)});"
                ),
                "#endif",
            ]
        )
    lines.extend(["    return anyActuatorChangedState;", "}"])
    return lines


def render_check_pulse_timers(device: DeviceConfig) -> list[str]:
    """Render the generated pulse countdown sweep for momentary actuators."""
    entries = [
        (actuator_index, actuator)
        for actuator_index, actuator in enumerate(device.actuators)
        if actuator.pulse_ms is not None
    ]
    lines = ["auto checkPulseTimers(uint16_t elapsed_ms) noexcept -> bool", "{"]
    if not entries:
        lines.extend(["    static_cast<void>(elapsed_ms);", "    return false;", "}"])
        return lines

    lines.extend(
        [
            "    if (activePulseActuators == 0U)",
            "    {",
            "        return false;",
            "    }",
            "",
            "    bool anyActuatorChangedState = false;",
        ]
    )
    for pulse_index, (actuator_index, _actuator) in enumerate(entries):
        off_call = render_set_state_call(
            device,
            actuator_index,
            "false",
            cached_time=False,
        )
        lines.extend(
            [
                f"    if (pulseRemaining_ms[{u8(pulse_index)}] != 0U)",
                "    {",
                f"        if (pulseRemaining_ms[{u8(pulse_index)}] <= elapsed_ms)",
                "        {",
                f"            pulseRemaining_ms[{u8(pulse_index)}] = 0U;",
                "            --activePulseActuators;",
                f"            anyActuatorChangedState |= {off_call};",
                "        }",
                "        else",
                "        {",
                (
                    f"            pulseRemaining_ms[{u8(pulse_index)}] = "
                    "static_cast<uint16_t>("
                    f"pulseRemaining_ms[{u8(pulse_index)}] - elapsed_ms);"
                ),
                "        }",
                "    }",
            ]
        )
    lines.extend(["    return anyActuatorChangedState;", "}"])
    return lines


def render_indicator_refresh_lines(
    device: DeviceConfig,
    indicator_index: int,
    links: Sequence[int],
    mode: str,
) -> list[str]:
    """Render a direct generated indicator refresh expression."""
    indicator = device.indicators[indicator_index]
    indicator_name = indicator_object_name(indicator_index, indicator)
    if not links:
        return [f"    {indicator_name}.applyComputedState(false);"]

    if mode in {"ANY", "ALL"}:
        terms = _indicator_state_terms(device, links)
        return _render_indicator_apply_from_terms(
            indicator_name,
            terms,
            "||" if mode == "ANY" else "&&",
        )

    terms = [
        f"static_cast<uint8_t>({actuator_name_at(device, actuator_index)}.getState())"
        for actuator_index in links
    ]
    variable_name = f"{indicator_name}ActuatorsOn"
    lines = render_u8_sum_declaration(variable_name, terms, indent="    ")
    lines.append(
        f"    {indicator_name}.applyComputedState("
        f"static_cast<uint8_t>({variable_name} << 1U) > {u8(len(links))});"
    )
    return lines


def render_refresh_indicators(
    device: DeviceConfig,
    profile: StaticProfileData,
) -> list[str]:
    """Render a direct indicator refresh without walking the manager array."""
    lines = ["void refreshIndicators() noexcept", "{"]
    if not device.indicators:
        lines.append("    return;")
    else:
        for indicator_index, indicator in enumerate(device.indicators):
            lines.extend(
                render_indicator_refresh_lines(
                    device,
                    indicator_index,
                    profile.indicator_link_sets[indicator_index],
                    indicator.mode,
                )
            )
    lines.append("}")
    return lines


def render_generated_action_accessors(
    device: DeviceConfig,
    profile: StaticProfileData,
) -> list[str]:
    """Render all direct generated action helpers used by hot runtime paths."""
    lines: list[str] = []
    for section in (
        render_generated_actuator_action_helpers(device),
        render_direct_short_clicks(device, profile),
        render_direct_long_clicks(device, profile),
        render_turn_off_all_actuators(device),
        render_turn_off_unprotected_actuators(device),
        render_direct_super_long_clicks(device, profile),
        render_run_click(),
        render_run_network_click_fallback(device),
        render_get_network_click_slot_count(device),
        render_get_network_click_slot_index(profile),
        render_get_network_click_clickable_index(profile),
        render_get_network_click_type(profile),
        render_is_clickable_configuration_valid(device, profile),
        render_set_actuator_state_by_id(device),
        render_scan_clickables(device, profile),
        render_check_pulse_timers(device),
        render_check_auto_off_timers(device),
        render_apply_packed_state_byte(device),
        render_compute_indicator_state(device, profile),
        render_refresh_indicators(device, profile),
    ):
        append_section(lines, section)
    return lines
