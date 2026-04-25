"""Render generated click action dispatchers and fallback helpers."""

from __future__ import annotations

from typing import TYPE_CHECKING

from .action_bodies import (
    render_bool_accumulator,
    render_switch_case_with_body,
    render_u8_sum_declaration,
)
from .cpp import render_values_condition, u8
from .topology import actuator_name_at, unprotected_actuator_indexes

if TYPE_CHECKING:
    from collections.abc import Sequence

    from .models import DeviceConfig, StaticProfileData


def _toggle_call(
    device: DeviceConfig,
    actuator_index: int,
    *,
    cached_time: bool,
) -> str:
    """Render one generated timestamp-aware toggle call."""
    object_name = actuator_name_at(device, actuator_index)
    if not cached_time:
        return f"{object_name}.toggleStateStatic<{u8(actuator_index)}>()"
    return f"{object_name}.toggleStateStatic<{u8(actuator_index)}>(actionNow)"


def _set_state_call(
    device: DeviceConfig,
    actuator_index: int,
    state: str,
    *,
    cached_time: bool,
) -> str:
    """Render one generated timestamp-aware setState call."""
    object_name = actuator_name_at(device, actuator_index)
    if not cached_time:
        return f"{object_name}.setStateStatic<{u8(actuator_index)}>({state})"
    return f"{object_name}.setStateStatic<{u8(actuator_index)}>({state}, actionNow)"


def render_short_click_body(
    device: DeviceConfig,
    profile: StaticProfileData,
    clickable_index: int,
    *,
    indent: str = "        ",
) -> list[str]:
    """Render the direct short-click action body for one clickable."""
    links = profile.short_link_sets[clickable_index]
    cached_time = len(links) > 1
    calls = [
        _toggle_call(device, actuator_index, cached_time=cached_time)
        for actuator_index in links
    ]
    return render_bool_accumulator(
        calls,
        indent=indent,
        with_cached_time=cached_time,
    )


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
    cached_time = len(links) > 1
    calls = [
        _set_state_call(
            device,
            actuator_index,
            "stateToSet",
            cached_time=cached_time,
        )
        for actuator_index in links
    ]
    lines = render_u8_sum_declaration("actuatorsLongOn", state_terms)
    lines.append(
        "        const bool stateToSet = "
        f"(static_cast<uint8_t>(actuatorsLongOn << 1U) < {u8(len(links))});"
    )
    lines.extend(render_bool_accumulator(calls, with_cached_time=cached_time))
    return lines


def render_long_click_body(
    device: DeviceConfig,
    profile: StaticProfileData,
    clickable_index: int,
    *,
    indent: str = "        ",
) -> list[str]:
    """Render the direct long-click action body for one clickable."""
    clickable = device.clickables[clickable_index]
    links = profile.long_link_sets[clickable_index]
    if clickable.long.click_type == "NORMAL":
        body = render_long_normal_body(device, links)
        if indent == "        ":
            return body
        return [
            line.replace("        ", indent, 1) if line.startswith("        ") else line
            for line in body
        ]

    state = "true" if clickable.long.click_type == "ON_ONLY" else "false"
    cached_time = len(links) > 1
    calls = [
        _set_state_call(device, actuator_index, state, cached_time=cached_time)
        for actuator_index in links
    ]
    return render_bool_accumulator(
        calls,
        indent=indent,
        with_cached_time=cached_time,
    )


def render_super_long_click_body(
    device: DeviceConfig,
    profile: StaticProfileData,
    clickable_index: int,
    *,
    indent: str = "        ",
) -> list[str]:
    """Render the direct super-long-click action body for one clickable."""
    clickable = device.clickables[clickable_index]
    if clickable.super_long.click_type == "NORMAL":
        return [f"{indent}return turnOffUnprotectedActuators();"]

    protected_indexes = {
        actuator_index
        for actuator_index, actuator in enumerate(device.actuators)
        if actuator.protected
    }
    links = [
        actuator_index
        for actuator_index in profile.super_long_link_sets[clickable_index]
        if actuator_index not in protected_indexes
    ]
    cached_time = len(links) > 1
    calls = [
        _set_state_call(device, actuator_index, "false", cached_time=cached_time)
        for actuator_index in links
    ]
    return render_bool_accumulator(
        calls,
        indent=indent,
        with_cached_time=cached_time,
    )


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
        cases.append(
            (
                clickable_index,
                render_short_click_body(device, profile, clickable_index),
            )
        )

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
        cases.append(
            (
                clickable_index,
                render_long_click_body(device, profile, clickable_index),
            )
        )

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
    cached_time = len(actuator_indexes) > 1
    calls = [
        _set_state_call(device, actuator_index, "false", cached_time=cached_time)
        for actuator_index in actuator_indexes
    ]
    lines.extend(
        render_bool_accumulator(calls, indent="    ", with_cached_time=cached_time)
    )
    lines.append("}")
    return lines


def render_direct_super_long_clicks(
    device: DeviceConfig,
    profile: StaticProfileData,
) -> list[str]:
    """Render direct super-long actions while preserving NORMAL/SELECTIVE semantics."""
    lines = ["auto runSuperLongClick(uint8_t clickableIndex) noexcept -> bool", "{"]
    cases: list[tuple[int, list[str]]] = []
    for clickable_index, clickable in enumerate(device.clickables):
        if not clickable.super_long.enabled:
            continue
        cases.append(
            (
                clickable_index,
                render_super_long_click_body(device, profile, clickable_index),
            )
        )

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


def render_run_click() -> list[str]:
    """Render a generic generated click dispatcher used by cold external paths."""
    return [
        (
            "auto runClick(uint8_t clickableIndex, "
            "constants::ClickType clickType) noexcept -> bool"
        ),
        "{",
        "    switch (clickType)",
        "    {",
        "    case constants::ClickType::SHORT:",
        "        return runShortClick(clickableIndex);",
        "    case constants::ClickType::LONG:",
        "        return runLongClick(clickableIndex);",
        "    case constants::ClickType::SUPER_LONG:",
        "        return runSuperLongClick(clickableIndex);",
        "    default:",
        "        return false;",
        "    }",
        "}",
    ]


def render_network_fallback_cases(
    click_type: str,
    cases: Sequence[int],
) -> list[str]:
    """Render one click-type branch for generated local network fallback."""
    lines = [f"    case constants::ClickType::{click_type}:", "    {"]
    if cases:
        lines.extend(["        switch (clickableIndex)", "        {"])
        for clickable_index in cases:
            function_name = f"run{click_type.title().replace('_', '')}Click"
            lines.extend(
                [
                    f"        case {u8(clickable_index)}:",
                    f"            return {function_name}(clickableIndex);",
                ]
            )
        lines.extend(["        default:", "            return false;", "        }"])
    else:
        lines.append("        return false;")
    lines.extend(["    }"])
    return lines


def render_run_network_click_fallback(device: DeviceConfig) -> list[str]:
    """Render local fallback for expired or rejected network clicks."""
    long_cases = [
        index
        for index, clickable in enumerate(device.clickables)
        if clickable.long.enabled
        and clickable.long.network
        and clickable.long.fallback == "LOCAL_FALLBACK"
    ]
    super_long_cases = [
        index
        for index, clickable in enumerate(device.clickables)
        if clickable.super_long.enabled
        and clickable.super_long.network
        and clickable.super_long.fallback == "LOCAL_FALLBACK"
    ]
    lines = [
        (
            "auto runNetworkClickFallback(uint8_t clickableIndex, "
            "constants::ClickType clickType) noexcept -> bool"
        ),
        "{",
    ]
    if not long_cases and not super_long_cases:
        lines.extend(
            [
                "    static_cast<void>(clickableIndex);",
                "    static_cast<void>(clickType);",
                "    return false;",
                "}",
            ]
        )
        return lines

    lines.extend(
        [
            "    switch (clickType)",
            "    {",
        ]
    )
    lines.extend(render_network_fallback_cases("LONG", long_cases))
    lines.extend(render_network_fallback_cases("SUPER_LONG", super_long_cases))
    lines.extend(["    default:", "        return false;", "    }", "}"])
    return lines


def render_get_network_click_slot_count(device: DeviceConfig) -> list[str]:
    """Render max concurrent network-click slots needed by one held clickable."""
    values = [
        int(clickable.long.enabled and clickable.long.network)
        + int(clickable.super_long.enabled and clickable.super_long.network)
        for clickable in device.clickables
    ]
    lines = [
        "auto getNetworkClickSlotCount(uint8_t clickableIndex) noexcept -> uint8_t",
        "{",
    ]
    if not values or all(value == 0 for value in values):
        lines.extend(["    static_cast<void>(clickableIndex);", "    return 0U;", "}"])
        return lines

    for value in sorted(set(values), reverse=True):
        if value == 0:
            continue
        indexes = [index for index, item in enumerate(values) if item == value]
        lines.extend(
            [
                f"    if ({render_values_condition('clickableIndex', indexes)})",
                "    {",
                f"        return {u8(value)};",
                "    }",
                "",
            ]
        )
    lines.extend(["    return 0U;", "}"])
    return lines


def render_get_network_click_slot_index(profile: StaticProfileData) -> list[str]:
    """Render direct clickable/type to fixed network-click slot lookup."""
    lines = [
        (
            "auto getNetworkClickSlotIndex(uint8_t clickableIndex, "
            "constants::ClickType clickType) noexcept -> uint8_t"
        ),
        "{",
    ]
    if not profile.network_click_slots:
        lines.extend(
            [
                "    static_cast<void>(clickableIndex);",
                "    static_cast<void>(clickType);",
                "    return UINT8_MAX;",
                "}",
            ]
        )
        return lines

    lines.extend(["    switch (clickType)", "    {"])
    for click_type in ("LONG", "SUPER_LONG"):
        slots = [
            (slot_index, clickable_index)
            for slot_index, (clickable_index, slot_type) in enumerate(
                profile.network_click_slots
            )
            if slot_type == click_type
        ]
        lines.extend([f"    case constants::ClickType::{click_type}:", "    {"])
        if slots:
            lines.extend(["        switch (clickableIndex)", "        {"])
            for slot_index, clickable_index in slots:
                lines.extend(
                    [
                        f"        case {u8(clickable_index)}:",
                        f"            return {u8(slot_index)};",
                    ]
                )
            lines.extend(
                ["        default:", "            return UINT8_MAX;", "        }"]
            )
        else:
            lines.append("        return UINT8_MAX;")
        lines.append("    }")
    lines.extend(["    default:", "        return UINT8_MAX;", "    }", "}"])
    return lines


def render_get_network_click_clickable_index(profile: StaticProfileData) -> list[str]:
    """Render fixed network-click slot to clickable-index lookup."""
    values = [
        clickable_index for clickable_index, _click_type in profile.network_click_slots
    ]
    lines = [
        "auto getNetworkClickClickableIndex(uint8_t slotIndex) noexcept -> uint8_t",
        "{",
    ]
    if not values:
        lines.extend(
            ["    static_cast<void>(slotIndex);", "    return UINT8_MAX;", "}"]
        )
        return lines
    for slot_index, clickable_index in enumerate(values):
        lines.extend(
            [
                f"    if (slotIndex == {u8(slot_index)})",
                "    {",
                f"        return {u8(clickable_index)};",
                "    }",
                "",
            ]
        )
    lines.extend(["    return UINT8_MAX;", "}"])
    return lines


def render_get_network_click_type(profile: StaticProfileData) -> list[str]:
    """Render fixed network-click slot to click type lookup."""
    lines = [
        "auto getNetworkClickType(uint8_t slotIndex) noexcept -> constants::ClickType",
        "{",
    ]
    if not profile.network_click_slots:
        lines.extend(
            [
                "    static_cast<void>(slotIndex);",
                "    return constants::ClickType::NONE;",
                "}",
            ]
        )
        return lines
    for slot_index, (_clickable_index, click_type) in enumerate(
        profile.network_click_slots
    ):
        lines.extend(
            [
                f"    if (slotIndex == {u8(slot_index)})",
                "    {",
                f"        return constants::ClickType::{click_type};",
                "    }",
                "",
            ]
        )
    lines.extend(["    return constants::ClickType::NONE;", "}"])
    return lines


def render_is_clickable_configuration_valid(
    device: DeviceConfig,
    profile: StaticProfileData,
) -> list[str]:
    """Render debug/runtime validation for generated clickable topology."""
    valid_indexes: list[int] = []
    for clickable_index, clickable in enumerate(device.clickables):
        has_enabled_click = (
            clickable.short_enabled
            or clickable.long.enabled
            or clickable.super_long.enabled
        )
        has_local_action = (
            bool(profile.short_link_sets[clickable_index])
            or bool(profile.long_link_sets[clickable_index])
            or bool(profile.super_long_link_sets[clickable_index])
        )
        has_network_action = (clickable.long.enabled and clickable.long.network) or (
            clickable.super_long.enabled and clickable.super_long.network
        )
        if has_enabled_click and (has_local_action or has_network_action):
            valid_indexes.append(clickable_index)

    lines = [
        "auto isClickableConfigurationValid(uint8_t clickableIndex) noexcept -> bool",
        "{",
    ]
    if not valid_indexes:
        lines.extend(
            ["    static_cast<void>(clickableIndex);", "    return false;", "}"]
        )
        return lines
    lines.extend(
        [
            (f"    return {render_values_condition('clickableIndex', valid_indexes)};"),
            "}",
        ]
    )
    return lines


def render_turn_off_all_actuators(
    device: DeviceConfig,
) -> list[str]:
    """Render direct global turn-off over all generated actuators."""
    return render_direct_turn_off_function(
        "turnOffAllActuators",
        device,
        list(range(len(device.actuators))),
    )


def render_turn_off_unprotected_actuators(
    device: DeviceConfig,
) -> list[str]:
    """Render direct global turn-off excluding protected actuators."""
    return render_direct_turn_off_function(
        "turnOffUnprotectedActuators",
        device,
        unprotected_actuator_indexes(device),
    )
