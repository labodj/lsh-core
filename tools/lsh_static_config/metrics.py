"""Small derived facts about a normalized device profile."""

from __future__ import annotations

from typing import TYPE_CHECKING

from .constants import DEFAULT_LONG_CLICK_MS, DEFAULT_SUPER_LONG_CLICK_MS

if TYPE_CHECKING:
    from .models import DeviceConfig


def count_active_network_clicks(device: DeviceConfig) -> int:
    """Count bridge-assisted click transactions declared by one device."""
    total = 0
    for clickable in device.clickables:
        if clickable.long.enabled and clickable.long.network:
            total += 1
        if clickable.super_long.enabled and clickable.super_long.network:
            total += 1
    return total


def count_timing_overrides(device: DeviceConfig) -> int:
    """Count clickables with non-default long or super-long timing."""
    total = 0
    for clickable in device.clickables:
        has_override = False
        if (
            clickable.long.enabled
            and clickable.long.time_ms is not None
            and clickable.long.time_ms != DEFAULT_LONG_CLICK_MS
        ):
            has_override = True
        if (
            clickable.super_long.enabled
            and clickable.super_long.time_ms is not None
            and clickable.super_long.time_ms != DEFAULT_SUPER_LONG_CLICK_MS
        ):
            has_override = True
        if has_override:
            total += 1
    return total
