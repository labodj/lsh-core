"""Render the generated clickable scan hot path."""

from __future__ import annotations

from typing import TYPE_CHECKING

from .action_bodies import (
    render_cached_action_time_declaration,
    render_u8_sum_declaration,
)
from .constants import (
    CLANG_FORMAT_COLUMN_LIMIT,
    DEFAULT_LONG_CLICK_MS,
    DEFAULT_SUPER_LONG_CLICK_MS,
)
from .cpp import u8, u16
from .topology import actuator_name_at, unprotected_actuator_indexes

if TYPE_CHECKING:
    from collections.abc import Callable, Sequence

    from .models import ClickableConfig, ClickAction, DeviceConfig, StaticProfileData


def click_action_time_ms(action: ClickAction, default_ms: int) -> int:
    """Return the generated threshold for one timed click action."""
    return action.time_ms if action.time_ms is not None else default_ms


def render_detection_flags(clickable: ClickableConfig) -> str:
    """Render a compile-time clickable FSM flag expression."""
    return (
        "constants::clickDetection::makeFlags("
        f"{str(clickable.short_enabled).lower()}, "
        f"{str(clickable.long.enabled).lower()}, "
        f"{str(clickable.super_long.enabled).lower()})"
    )


def render_mark_state_changed(call: str, *, indent: str = "        ") -> list[str]:
    """Render a conditional scan-result update from one bool action call."""
    return [
        f"{indent}if ({call})",
        f"{indent}{{",
        f"{indent}    scanResultFlags |= CLICK_SCAN_STATE_CHANGED;",
        f"{indent}}}",
    ]


def _toggle_call(
    device: DeviceConfig,
    actuator_index: int,
    *,
    cached_time: bool,
) -> str:
    """Render one generated static-index toggle call for scan-local actions."""
    object_name = actuator_name_at(device, actuator_index)
    if cached_time:
        return f"{object_name}.toggleStateStatic<{u8(actuator_index)}>(actionNow)"
    return f"{object_name}.toggleStateStatic<{u8(actuator_index)}>()"


def _set_state_call(
    device: DeviceConfig,
    actuator_index: int,
    state: str,
    *,
    cached_time: bool,
) -> str:
    """Render one generated static-index setState call for scan-local actions."""
    object_name = actuator_name_at(device, actuator_index)
    if cached_time:
        return f"{object_name}.setStateStatic<{u8(actuator_index)}>({state}, actionNow)"
    return f"{object_name}.setStateStatic<{u8(actuator_index)}>({state})"


def render_mark_any_state_changed(
    calls: Sequence[str],
    *,
    indent: str = "        ",
    with_cached_time: bool = False,
) -> list[str]:
    """Render scan-result updates for one or more generated actuator calls."""
    if not calls:
        return []

    lines: list[str] = []
    if with_cached_time:
        lines.extend(render_cached_action_time_declaration(indent))
    for call in calls:
        lines.extend(render_mark_state_changed(call, indent=indent))
    return lines


def render_short_local_action(
    device: DeviceConfig,
    profile: StaticProfileData,
    clickable_index: int,
    *,
    indent: str = "        ",
) -> list[str]:
    """Render the scan-local short-click action without a dispatcher call."""
    links = profile.short_link_sets[clickable_index]
    cached_time = len(links) > 1
    calls = [
        _toggle_call(device, actuator_index, cached_time=cached_time)
        for actuator_index in links
    ]
    return render_mark_any_state_changed(
        calls,
        indent=indent,
        with_cached_time=cached_time,
    )


def render_long_local_action(
    device: DeviceConfig,
    profile: StaticProfileData,
    clickable_index: int,
    *,
    indent: str = "        ",
) -> list[str]:
    """Render the scan-local long-click action without a dispatcher call."""
    clickable = device.clickables[clickable_index]
    links = profile.long_link_sets[clickable_index]
    if not links:
        return []

    lines: list[str] = []
    if clickable.long.click_type == "NORMAL":
        state_terms = [
            (
                "static_cast<uint8_t>"
                f"({actuator_name_at(device, actuator_index)}.getState())"
            )
            for actuator_index in links
        ]
        lines.extend(
            render_u8_sum_declaration(
                "actuatorsLongOn",
                state_terms,
                indent=indent,
            )
        )
        lines.append(
            f"{indent}const bool stateToSet = "
            f"(static_cast<uint8_t>(actuatorsLongOn << 1U) < {u8(len(links))});"
        )
        state = "stateToSet"
    else:
        state = "true" if clickable.long.click_type == "ON_ONLY" else "false"

    cached_time = len(links) > 1
    calls = [
        _set_state_call(device, actuator_index, state, cached_time=cached_time)
        for actuator_index in links
    ]
    lines.extend(
        render_mark_any_state_changed(
            calls,
            indent=indent,
            with_cached_time=cached_time,
        )
    )
    return lines


def render_super_long_local_action(
    device: DeviceConfig,
    profile: StaticProfileData,
    clickable_index: int,
    *,
    indent: str = "        ",
) -> list[str]:
    """Render the scan-local super-long action without a dispatcher call."""
    clickable = device.clickables[clickable_index]
    if clickable.super_long.click_type == "NORMAL":
        links = unprotected_actuator_indexes(device)
    else:
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
    return render_mark_any_state_changed(
        calls,
        indent=indent,
        with_cached_time=cached_time,
    )


def render_click_debug_log(
    clickable: ClickableConfig,
    debug_token: str,
    *,
    indent: str = "        ",
) -> list[str]:
    """Render a generated DPL line for one detected click event."""
    return [
        (
            f"{indent}DPL(FPSTR(dStr::CLICKABLE), FPSTR(dStr::SPACE), "
            f"{u8(clickable.clickable_id)}, FPSTR(dStr::SPACE), "
            f"FPSTR(dStr::{debug_token}), FPSTR(dStr::SPACE),"
        ),
        f"{indent}    FPSTR(dStr::CLICKED));",
    ]


def render_network_or_local_event(
    clickable_index: int,
    action: ClickAction,
    click_type: str,
    local_renderer: Callable[[str], list[str]],
    *,
    indent: str = "        ",
) -> list[str]:
    """Render generated routing for one long/super-long click event."""
    if not action.network:
        return local_renderer(indent)

    click_type_expr = f"constants::ClickType::{click_type}"
    lines = [
        f"{indent}if (BridgeSerial::isConnected())",
        f"{indent}{{",
        (
            f"{indent}    const auto requestResult = "
            f"NetworkClicks::request({u8(clickable_index)}, {click_type_expr});"
        ),
        f"{indent}    if (requestResult == NetworkClicks::RequestResult::Accepted)",
        f"{indent}    {{",
        f"{indent}        scanResultFlags |= CLICK_SCAN_NETWORK_PENDING;",
        f"{indent}    }}",
    ]
    if action.fallback == "LOCAL_FALLBACK":
        lines.extend(
            [
                (
                    f"{indent}    else if (requestResult == "
                    "NetworkClicks::RequestResult::TransportRejected)"
                ),
                f"{indent}    {{",
                *local_renderer(f"{indent}        "),
                f"{indent}    }}",
            ]
        )
    lines.append(f"{indent}}}")

    if action.fallback == "LOCAL_FALLBACK":
        lines.extend(
            [
                f"{indent}else",
                f"{indent}{{",
                *local_renderer(f"{indent}    "),
                f"{indent}}}",
            ]
        )
    return lines


def render_scan_clickable(
    device: DeviceConfig,
    profile: StaticProfileData,
    clickable_index: int,
) -> list[str]:
    """Render one generated clickable scan block."""
    clickable = device.clickables[clickable_index]
    result_name = f"{clickable.name}ClickResult"
    long_time_ms = click_action_time_ms(clickable.long, DEFAULT_LONG_CLICK_MS)
    super_long_time_ms = click_action_time_ms(
        clickable.super_long,
        DEFAULT_SUPER_LONG_CLICK_MS,
    )
    click_detection_call = (
        f"{clickable.name}.clickDetection<"
        f"{render_detection_flags(clickable)}, {u16(long_time_ms)}, "
        f"{u16(super_long_time_ms)}>(elapsed_ms);"
    )
    assignment_line = f"    const auto {result_name} = {click_detection_call}"
    if len(f"    {assignment_line}") <= CLANG_FORMAT_COLUMN_LIMIT:
        detection_lines = [assignment_line]
    else:
        detection_lines = [
            f"    const auto {result_name} =",
            f"        {click_detection_call}",
        ]
    lines = ["{", *detection_lines, f"    switch ({result_name})", "    {"]
    if clickable.short_enabled:
        lines.extend(
            [
                "    case ClickResult::SHORT_CLICK:",
                "    case ClickResult::SHORT_CLICK_QUICK:",
                "    {",
                *render_click_debug_log(clickable, "SHORT"),
                *render_short_local_action(device, profile, clickable_index),
                "    }",
                "    break;",
                "",
            ]
        )
    if clickable.long.enabled:
        lines.extend(
            [
                "    case ClickResult::LONG_CLICK:",
                "    {",
                *render_click_debug_log(clickable, "LONG"),
                *render_network_or_local_event(
                    clickable_index,
                    clickable.long,
                    "LONG",
                    lambda action_indent: render_long_local_action(
                        device,
                        profile,
                        clickable_index,
                        indent=action_indent,
                    ),
                ),
                "    }",
                "    break;",
                "",
            ]
        )
    if clickable.super_long.enabled:
        lines.extend(
            [
                "    case ClickResult::SUPER_LONG_CLICK:",
                "    {",
                *render_click_debug_log(clickable, "SUPER_LONG"),
                *render_network_or_local_event(
                    clickable_index,
                    clickable.super_long,
                    "SUPER_LONG",
                    lambda action_indent: render_super_long_local_action(
                        device,
                        profile,
                        clickable_index,
                        indent=action_indent,
                    ),
                ),
                "    }",
                "    break;",
                "",
            ]
        )
    lines.extend(["    default:", "        break;", "    }", "}"])
    return lines


def render_scan_clickables(
    device: DeviceConfig, profile: StaticProfileData
) -> list[str]:
    """Render the generated, topology-specialized clickable scanner."""
    lines = ["auto scanClickables(uint16_t elapsed_ms) noexcept -> uint8_t", "{"]
    if not device.clickables:
        lines.extend(["    static_cast<void>(elapsed_ms);", "    return 0U;", "}"])
        return lines

    lines.extend(
        [
            "    using constants::ClickResult;",
            "    using namespace Debug;",
            "    uint8_t scanResultFlags = 0U;",
            "",
        ]
    )
    for clickable_index in range(len(device.clickables)):
        if clickable_index != 0:
            lines.append("")
        lines.extend(
            line if line.startswith("#") else f"    {line}" if line else ""
            for line in render_scan_clickable(device, profile, clickable_index)
        )
    lines.extend(["", "    return scanResultFlags;", "}"])
    return lines
