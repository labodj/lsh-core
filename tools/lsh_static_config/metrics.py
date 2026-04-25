"""Small derived facts about a normalized device profile."""

from __future__ import annotations

from typing import TYPE_CHECKING

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
