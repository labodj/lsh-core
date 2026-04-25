"""Render the generated static_config runtime helpers."""

from __future__ import annotations

from typing import TYPE_CHECKING

from .cpp import (
    append_section,
    render_index_to_value_function,
    render_lookup_function,
    render_timer_function,
    render_u8_value_group_function,
)
from .runtime_paths import render_generated_action_accessors

if TYPE_CHECKING:
    from .models import DeviceConfig, StaticProfileData


def render_details_payload_accessors() -> list[str]:
    """Render the codec-selected DEVICE_DETAILS writer."""
    return [
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
    """Render generated ID, lookup and auto-off timer accessors."""
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
    return lines


def render_indicator_static_accessors(profile: StaticProfileData) -> list[str]:
    """Render generated indicator validation accessors."""
    lines: list[str] = []
    append_section(
        lines,
        render_u8_value_group_function(
            "getIndicatorActuatorLinkCount",
            "indicatorIndex",
            profile.indicator_link_counts,
            "0U",
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
        render_indicator_static_accessors(profile),
        render_details_payload_accessors(),
        render_generated_action_accessors(device, profile),
    ):
        append_section(lines, section)
    return lines
