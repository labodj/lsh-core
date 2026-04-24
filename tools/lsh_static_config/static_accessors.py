"""Render static_config namespace accessors and dispatchers."""

from __future__ import annotations

from typing import TYPE_CHECKING

from .actions import render_generated_action_accessors
from .constants import DEFAULT_LONG_CLICK_MS, DEFAULT_SUPER_LONG_CLICK_MS
from .cpp import (
    append_section,
    consecutive_runs,
    render_enum_accessor,
    render_index_to_value_function,
    render_lookup_function,
    render_range_condition,
    render_run_value_expression,
    render_timer_function,
    render_u8_value_group_function,
    render_values_condition,
    u8,
    u16,
)

if TYPE_CHECKING:
    from collections.abc import Sequence

    from .models import DeviceConfig, StaticProfileData


def render_click_time_function(
    function_name: str,
    values: Sequence[int],
    default_value: int,
) -> list[str]:
    """Render a uint16_t per-clickable timing accessor."""
    lines = [
        f"auto {function_name}(uint8_t clickableIndex) noexcept -> uint16_t",
        "{",
    ]
    non_default: dict[int, list[int]] = {}
    for index, value in enumerate(values):
        if value != default_value:
            non_default.setdefault(value, []).append(index)

    if not non_default:
        default_symbol = default_timing_symbol(function_name)
        lines.extend(
            [
                "    static_cast<void>(clickableIndex);",
                f"    return constants::timings::{default_symbol};",
                "}",
            ]
        )
        return lines

    groups = sorted(non_default.items(), key=lambda item: min(item[1]))
    default = f"constants::timings::{default_timing_symbol(function_name)}"
    for group_index, (timer, indexes) in enumerate(groups):
        condition = render_values_condition("clickableIndex", indexes)
        if group_index == len(groups) - 1:
            if group_index != 0:
                lines.append("")
            lines.append(f"    return {condition} ? {u16(timer)} : {default};")
            break
        if group_index != 0:
            lines.append("")
        lines.extend(
            [f"    if ({condition})", "    {", f"        return {u16(timer)};", "    }"]
        )
    lines.append("}")
    return lines


def default_timing_symbol(function_name: str) -> str:
    """Return the C++ constant used by one generated timing accessor."""
    if function_name == "getLongClickTime":
        return "CLICKABLE_LONG_CLICK_TIME_MS"
    return "CLICKABLE_SUPER_LONG_CLICK_TIME_MS"


def render_case_index_to_value(
    param_name: str,
    values: Sequence[int],
    invalid: str,
    indent: str,
) -> list[str]:
    """Render branch code for a small index-to-value mapping inside a switch case."""
    pairs = list(enumerate(values))
    runs = consecutive_runs(pairs)
    lines: list[str] = []
    for run_index, (start, end, offset) in enumerate(runs):
        condition = render_range_condition(param_name, start, end)
        value = render_run_value_expression(param_name, start, end, offset)
        if run_index == len(runs) - 1:
            if run_index != 0:
                lines.append("")
            lines.append(f"{indent}return {condition} ? {value} : {invalid};")
            break
        if run_index != 0:
            lines.append("")
        lines.extend(
            [
                f"{indent}if ({condition})",
                f"{indent}{{",
                f"{indent}    return {value};",
                f"{indent}}}",
            ]
        )
    return lines


def render_single_link_lookup_function(
    function_name: str,
    owner_param_name: str,
    link_sets: Sequence[Sequence[int]],
) -> list[str]:
    """Render an owner lookup optimized for link sets with at most one entry."""
    pairs = [
        (owner_index, links[0])
        for owner_index, links in enumerate(link_sets)
        if len(links) == 1
    ]
    signature = (
        f"auto {function_name}(uint8_t {owner_param_name}, "
        "uint8_t linkIndex) noexcept -> uint8_t"
    )
    lines = [signature, "{"]
    if not pairs:
        lines.extend(
            [
                f"    static_cast<void>({owner_param_name});",
                "    static_cast<void>(linkIndex);",
                "    return UINT8_MAX;",
                "}",
            ]
        )
        return lines

    lines.extend(
        [
            "    if (linkIndex != 0U)",
            "    {",
            "        return UINT8_MAX;",
            "    }",
            "",
        ]
    )
    runs = consecutive_runs(pairs)
    for run_index, (start, end, offset) in enumerate(runs):
        condition = render_range_condition(owner_param_name, start, end)
        value = render_run_value_expression(owner_param_name, start, end, offset)
        if run_index == len(runs) - 1:
            if run_index != 0:
                lines.append("")
            lines.append(f"    return {condition} ? {value} : UINT8_MAX;")
            break
        if run_index != 0:
            lines.append("")
        lines.extend(
            [f"    if ({condition})", "    {", f"        return {value};", "    }"]
        )
    lines.append("}")
    return lines


def render_link_lookup_function(
    function_name: str,
    owner_param_name: str,
    link_sets: Sequence[Sequence[int]],
) -> list[str]:
    """Render a generated owner/link-index to actuator-index accessor."""
    if all(len(links) <= 1 for links in link_sets):
        return render_single_link_lookup_function(
            function_name,
            owner_param_name,
            link_sets,
        )

    signature = (
        f"auto {function_name}(uint8_t {owner_param_name}, "
        "uint8_t linkIndex) noexcept -> uint8_t"
    )
    lines = [signature, "{"]
    if not any(link_sets):
        lines.extend(
            [
                f"    static_cast<void>({owner_param_name});",
                "    static_cast<void>(linkIndex);",
                "    return UINT8_MAX;",
                "}",
            ]
        )
        return lines

    lines.extend([f"    switch ({owner_param_name})", "    {"])
    for owner_index, links in enumerate(link_sets):
        if not links:
            continue
        lines.append(f"    case {u8(owner_index)}:")
        lines.extend(
            render_case_index_to_value(
                "linkIndex",
                links,
                "UINT8_MAX",
                "        ",
            )
        )
    lines.extend(
        [
            "    default:",
            "        return UINT8_MAX;",
            "    }",
            "}",
        ]
    )
    return lines


def render_click_link_count_dispatcher() -> list[str]:
    """Render the public click-link-count dispatcher over generated helpers."""
    signature = (
        "auto getClickActuatorLinkCount(constants::ClickType clickType, "
        "uint8_t clickableIndex) noexcept -> uint8_t"
    )
    return [
        signature,
        "{",
        "    switch (clickType)",
        "    {",
        "    case constants::ClickType::SHORT:",
        "        return getShortClickActuatorLinkCount(clickableIndex);",
        "    case constants::ClickType::LONG:",
        "        return getLongClickActuatorLinkCount(clickableIndex);",
        "    case constants::ClickType::SUPER_LONG:",
        "        return getSuperLongClickActuatorLinkCount(clickableIndex);",
        "    default:",
        "        return 0U;",
        "    }",
        "}",
    ]


def render_click_link_dispatcher() -> list[str]:
    """Render the public click-link accessor dispatcher over generated helpers."""
    signature = (
        "auto getClickActuatorLink(constants::ClickType clickType, "
        "uint8_t clickableIndex, uint8_t linkIndex) noexcept -> uint8_t"
    )
    return [
        signature,
        "{",
        "    switch (clickType)",
        "    {",
        "    case constants::ClickType::SHORT:",
        "        return getShortClickActuatorLink(clickableIndex, linkIndex);",
        "    case constants::ClickType::LONG:",
        "        return getLongClickActuatorLink(clickableIndex, linkIndex);",
        "    case constants::ClickType::SUPER_LONG:",
        "        return getSuperLongClickActuatorLink(clickableIndex, linkIndex);",
        "    default:",
        "        return UINT8_MAX;",
        "    }",
        "}",
    ]


def render_details_payload_accessors() -> list[str]:
    """Render codec-selected DEVICE_DETAILS payload accessors."""
    return [
        "auto getDetailsPayloadSize() noexcept -> uint16_t",
        "{",
        "#ifdef CONFIG_MSG_PACK",
        "    return sizeof(DETAILS_MSGPACK_PAYLOAD);",
        "#else",
        "    return sizeof(DETAILS_JSON_PAYLOAD);",
        "#endif",
        "}",
        "",
        "auto getDetailsPayloadByte(uint16_t byteIndex) noexcept -> uint8_t",
        "{",
        "#ifdef CONFIG_MSG_PACK",
        "    if (byteIndex >= sizeof(DETAILS_MSGPACK_PAYLOAD))",
        "    {",
        "        return 0U;",
        "    }",
        "    return LSH_STATIC_CONFIG_READ_BYTE(&DETAILS_MSGPACK_PAYLOAD[byteIndex]);",
        "#else",
        "    if (byteIndex >= sizeof(DETAILS_JSON_PAYLOAD))",
        "    {",
        "        return 0U;",
        "    }",
        "    return LSH_STATIC_CONFIG_READ_BYTE(&DETAILS_JSON_PAYLOAD[byteIndex]);",
        "#endif",
        "}",
        "",
        "auto writeDetailsPayload() noexcept -> bool",
        "{",
        "#ifdef CONFIG_MSG_PACK",
        "    return writeGeneratedPayload(DETAILS_MSGPACK_PAYLOAD);",
        "#else",
        "    return writeGeneratedPayload(DETAILS_JSON_PAYLOAD);",
        "#endif",
        "}",
    ]


def render_core_static_accessors(
    device: DeviceConfig,
    profile: StaticProfileData,
) -> list[str]:
    """Render generated ID, lookup, timer and click-type accessors."""
    lines: list[str] = []
    append_section(
        lines,
        render_index_to_value_function(
            "getActuatorId",
            "actuatorIndex",
            profile.actuator_ids,
            "0U",
        ),
    )
    append_section(
        lines,
        render_index_to_value_function(
            "getClickableId",
            "clickableIndex",
            profile.clickable_ids,
            "0U",
        ),
    )
    append_section(
        lines,
        render_lookup_function(
            "getActuatorIndexById",
            "actuatorId",
            {value: index for index, value in enumerate(profile.actuator_ids)},
        ),
    )
    append_section(
        lines,
        render_lookup_function(
            "getClickableIndexById",
            "clickableId",
            {value: index for index, value in enumerate(profile.clickable_ids)},
        ),
    )
    append_section(
        lines,
        render_index_to_value_function(
            "getAutoOffActuatorIndex",
            "autoOffIndex",
            profile.auto_off_indexes,
            "UINT8_MAX",
        ),
    )
    append_section(
        lines,
        render_lookup_function(
            "getAutoOffIndexByActuatorIndex",
            "actuatorIndex",
            {
                actuator_index: auto_off_index
                for auto_off_index, actuator_index in enumerate(
                    profile.auto_off_indexes
                )
            },
        ),
    )
    append_section(lines, render_timer_function(device, profile.actuator_indexes))
    append_section(
        lines,
        render_enum_accessor(
            "getLongClickType",
            "clickableIndex",
            profile.long_types,
            "LongClickType",
            "NORMAL",
        ),
    )
    append_section(
        lines,
        render_enum_accessor(
            "getSuperLongClickType",
            "clickableIndex",
            profile.super_long_types,
            "SuperLongClickType",
            "NORMAL",
        ),
    )
    append_section(
        lines,
        render_click_time_function(
            "getLongClickTime",
            profile.long_times,
            DEFAULT_LONG_CLICK_MS,
        ),
    )
    append_section(
        lines,
        render_click_time_function(
            "getSuperLongClickTime",
            profile.super_long_times,
            DEFAULT_SUPER_LONG_CLICK_MS,
        ),
    )
    return lines


def render_click_link_static_accessors(profile: StaticProfileData) -> list[str]:
    """Render generated clickable-to-actuator topology accessors."""
    lines: list[str] = []
    append_section(
        lines,
        render_u8_value_group_function(
            "getShortClickActuatorLinkCount",
            "clickableIndex",
            profile.short_link_counts,
            "0U",
        ),
    )
    append_section(
        lines,
        render_u8_value_group_function(
            "getLongClickActuatorLinkCount",
            "clickableIndex",
            profile.long_link_counts,
            "0U",
        ),
    )
    append_section(
        lines,
        render_u8_value_group_function(
            "getSuperLongClickActuatorLinkCount",
            "clickableIndex",
            profile.super_long_link_counts,
            "0U",
        ),
    )
    append_section(lines, render_click_link_count_dispatcher())
    append_section(
        lines,
        render_link_lookup_function(
            "getShortClickActuatorLink",
            "clickableIndex",
            profile.short_link_sets,
        ),
    )
    append_section(
        lines,
        render_link_lookup_function(
            "getLongClickActuatorLink",
            "clickableIndex",
            profile.long_link_sets,
        ),
    )
    append_section(
        lines,
        render_link_lookup_function(
            "getSuperLongClickActuatorLink",
            "clickableIndex",
            profile.super_long_link_sets,
        ),
    )
    append_section(lines, render_click_link_dispatcher())
    return lines


def render_indicator_static_accessors(profile: StaticProfileData) -> list[str]:
    """Render generated indicator topology accessors."""
    lines: list[str] = []
    append_section(
        lines,
        render_enum_accessor(
            "getIndicatorMode",
            "indicatorIndex",
            profile.indicator_modes,
            "IndicatorMode",
            "ANY",
        ),
    )
    append_section(
        lines,
        render_u8_value_group_function(
            "getIndicatorActuatorLinkCount",
            "indicatorIndex",
            profile.indicator_link_counts,
            "0U",
        ),
    )
    append_section(
        lines,
        render_link_lookup_function(
            "getIndicatorActuatorLink",
            "indicatorIndex",
            profile.indicator_link_sets,
        ),
    )
    return lines


def render_static_config_accessors(
    device: DeviceConfig,
    profile: StaticProfileData,
) -> list[str]:
    """Render all functions in the static_config namespace."""
    lines: list[str] = []
    for section in (
        render_core_static_accessors(device, profile),
        render_click_link_static_accessors(profile),
        render_indicator_static_accessors(profile),
        render_details_payload_accessors(),
        render_generated_action_accessors(device, profile),
    ):
        append_section(lines, section)
    return lines
