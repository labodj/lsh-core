"""Direct generated action bodies for hot runtime paths."""

from __future__ import annotations

from typing import TYPE_CHECKING

from .constants import CLANG_FORMAT_COLUMN_LIMIT, MAX_INLINE_SUM_TERMS
from .cpp import append_section, u8
from .topology import actuator_name_at, unprotected_actuator_indexes

if TYPE_CHECKING:
    from collections.abc import Sequence

    from .models import DeviceConfig, StaticProfileData


def render_bool_accumulator(
    calls: Sequence[str],
    *,
    variable_name: str = "anyActuatorChangedState",
    indent: str = "        ",
) -> list[str]:
    """Render a bool OR accumulator for direct generated actuator calls."""
    if not calls:
        return [f"{indent}return false;"]
    if len(calls) == 1:
        return [f"{indent}return {calls[0]};"]
    lines = [f"{indent}bool {variable_name} = false;"]
    lines.extend(f"{indent}{variable_name} |= {call};" for call in calls)
    lines.append(f"{indent}return {variable_name};")
    return lines


def render_u8_sum_declaration(
    variable_name: str,
    terms: Sequence[str],
    *,
    indent: str = "        ",
) -> list[str]:
    """Render a clang-format-stable uint8_t sum declaration."""
    if not terms:
        return [f"{indent}const uint8_t {variable_name} = 0U;"]
    joined_terms = " + ".join(terms)
    one_line = f"{indent}const uint8_t {variable_name} = {joined_terms};"
    if (
        len(terms) <= MAX_INLINE_SUM_TERMS
        and len(one_line) <= CLANG_FORMAT_COLUMN_LIMIT
    ):
        return [one_line]

    return [f"{indent}const uint8_t {variable_name} =", f"{indent}    {joined_terms};"]


def render_switch_case_with_body(
    lines: list[str],
    case_index: int,
    body: Sequence[str],
) -> None:
    """Append one uint8_t switch case with a scoped multi-line body."""
    lines.append(f"    case {u8(case_index)}:")
    lines.append("    {")
    lines.extend(body)
    lines.append("    }")


def render_direct_short_clicks(
    device: DeviceConfig,
    profile: StaticProfileData,
) -> list[str]:
    """Render direct short-click actions with no runtime topology lookup."""
    lines = ["auto runShortClick(uint8_t clickableIndex) noexcept -> bool", "{"]
    cases: list[tuple[int, list[str]]] = []
    for clickable_index, links in enumerate(profile.short_link_sets):
        if not links:
            continue
        calls = [
            f"{actuator_name_at(device, actuator_index)}.toggleState()"
            for actuator_index in links
        ]
        cases.append((clickable_index, render_bool_accumulator(calls)))

    if not cases:
        lines.extend(
            ["    static_cast<void>(clickableIndex);", "    return false;", "}"]
        )
        return lines

    lines.extend(["    switch (clickableIndex)", "    {"])
    for clickable_index, body in cases:
        render_switch_case_with_body(lines, clickable_index, body)
    lines.extend(["    default:", "        return false;", "    }", "}"])
    return lines


def render_long_normal_body(
    device: DeviceConfig,
    links: Sequence[int],
) -> list[str]:
    """Render the generated NORMAL long-click action body."""
    if not links:
        return ["        return false;"]

    state_terms = [
        f"static_cast<uint8_t>({actuator_name_at(device, actuator_index)}.getState())"
        for actuator_index in links
    ]
    calls = [
        f"{actuator_name_at(device, actuator_index)}.setState(stateToSet)"
        for actuator_index in links
    ]
    lines = render_u8_sum_declaration("actuatorsLongOn", state_terms)
    lines.append(
        "        const bool stateToSet = "
        f"(static_cast<uint8_t>(actuatorsLongOn << 1U) < {u8(len(links))});"
    )
    lines.extend(render_bool_accumulator(calls))
    return lines


def render_direct_long_clicks(
    device: DeviceConfig,
    profile: StaticProfileData,
) -> list[str]:
    """Render direct long-click actions with generated action bodies."""
    lines = ["auto runLongClick(uint8_t clickableIndex) noexcept -> bool", "{"]
    cases: list[tuple[int, list[str]]] = []
    for clickable_index, clickable in enumerate(device.clickables):
        if not clickable.long.enabled:
            continue
        links = profile.long_link_sets[clickable_index]
        if clickable.long.click_type == "NORMAL":
            body = render_long_normal_body(device, links)
        else:
            state = "true" if clickable.long.click_type == "ON_ONLY" else "false"
            calls = [
                f"{actuator_name_at(device, actuator_index)}.setState({state})"
                for actuator_index in links
            ]
            body = render_bool_accumulator(calls)
        cases.append((clickable_index, body))

    if not cases:
        lines.extend(
            ["    static_cast<void>(clickableIndex);", "    return false;", "}"]
        )
        return lines

    lines.extend(["    switch (clickableIndex)", "    {"])
    for clickable_index, body in cases:
        render_switch_case_with_body(lines, clickable_index, body)
    lines.extend(["    default:", "        return false;", "    }", "}"])
    return lines


def render_direct_turn_off_function(
    function_name: str,
    device: DeviceConfig,
    actuator_indexes: Sequence[int],
) -> list[str]:
    """Render a direct all/off subset function over generated actuator objects."""
    lines = [f"auto {function_name}() noexcept -> bool", "{"]
    calls = [
        f"{actuator_name_at(device, actuator_index)}.setState(false)"
        for actuator_index in actuator_indexes
    ]
    lines.extend(render_bool_accumulator(calls, indent="    "))
    lines.append("}")
    return lines


def render_direct_super_long_clicks(
    device: DeviceConfig,
    profile: StaticProfileData,
) -> list[str]:
    """Render direct super-long actions while preserving NORMAL/SELECTIVE semantics."""
    lines = ["auto runSuperLongClick(uint8_t clickableIndex) noexcept -> bool", "{"]
    cases: list[tuple[int, list[str]]] = []
    protected_indexes = {
        actuator_index
        for actuator_index, actuator in enumerate(device.actuators)
        if actuator.protected
    }
    for clickable_index, clickable in enumerate(device.clickables):
        if not clickable.super_long.enabled:
            continue
        if clickable.super_long.click_type == "NORMAL":
            cases.append(
                (clickable_index, ["        return turnOffUnprotectedActuators();"])
            )
            continue

        links = [
            actuator_index
            for actuator_index in profile.super_long_link_sets[clickable_index]
            if actuator_index not in protected_indexes
        ]
        calls = [
            f"{actuator_name_at(device, actuator_index)}.setState(false)"
            for actuator_index in links
        ]
        cases.append((clickable_index, render_bool_accumulator(calls)))

    if not cases:
        lines.extend(
            ["    static_cast<void>(clickableIndex);", "    return false;", "}"]
        )
        return lines

    lines.extend(["    switch (clickableIndex)", "    {"])
    for clickable_index, body in cases:
        render_switch_case_with_body(lines, clickable_index, body)
    lines.extend(["    default:", "        return false;", "    }", "}"])
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
        calls = [
            (
                f"{actuator_name_at(device, actuator_index)}.setState("
                f"(packedByte & {u8(1 << (actuator_index - start))}) != 0U)"
            )
            for actuator_index in range(start, end)
        ]
        render_switch_case_with_body(
            lines,
            byte_index,
            render_bool_accumulator(calls),
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
        expression = " || ".join(
            f"{actuator_name_at(device, actuator_index)}.getState()"
            for actuator_index in links
        )
        return [f"        return {expression};"]

    if mode == "ALL":
        expression = " && ".join(
            f"{actuator_name_at(device, actuator_index)}.getState()"
            for actuator_index in links
        )
        return [f"        return {expression};"]

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


def render_generated_action_accessors(
    device: DeviceConfig,
    profile: StaticProfileData,
) -> list[str]:
    """Render all direct generated action helpers used by hot runtime paths."""
    lines: list[str] = []
    for section in (
        render_direct_short_clicks(device, profile),
        render_direct_long_clicks(device, profile),
        render_direct_turn_off_function(
            "turnOffAllActuators",
            device,
            list(range(len(device.actuators))),
        ),
        render_direct_turn_off_function(
            "turnOffUnprotectedActuators",
            device,
            unprotected_actuator_indexes(device),
        ),
        render_direct_super_long_clicks(device, profile),
        render_apply_packed_state_byte(device),
        render_compute_indicator_state(device, profile),
    ):
        append_section(lines, section)
    return lines
