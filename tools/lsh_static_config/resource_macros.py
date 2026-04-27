"""Render generated resource macros consumed during the resource pass."""

from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .models import DeviceConfig, StaticProfileData


def static_resource_macros(
    device: DeviceConfig,
    profile: StaticProfileData,
) -> dict[str, int]:
    """Return the exact resource macros for one generated static profile."""
    return {
        "LSH_STATIC_CONFIG_CLICKABLES": len(device.clickables),
        "LSH_STATIC_CONFIG_ACTUATORS": len(device.actuators),
        "LSH_STATIC_CONFIG_INDICATORS": len(device.indicators),
        "LSH_STATIC_CONFIG_MAX_CLICKABLE_ID": max(profile.clickable_ids)
        if profile.clickable_ids
        else 1,
        "LSH_STATIC_CONFIG_MAX_ACTUATOR_ID": max(profile.actuator_ids)
        if profile.actuator_ids
        else 1,
        "LSH_STATIC_CONFIG_SHORT_CLICK_ACTUATOR_LINKS": profile.short_links,
        "LSH_STATIC_CONFIG_LONG_CLICK_ACTUATOR_LINKS": profile.long_links,
        "LSH_STATIC_CONFIG_SUPER_LONG_CLICK_ACTUATOR_LINKS": profile.super_long_links,
        "LSH_STATIC_CONFIG_INDICATOR_ACTUATOR_LINKS": profile.indicator_links,
        "LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS": len(profile.auto_off_indexes),
        "LSH_STATIC_CONFIG_PULSE_ACTUATORS": len(profile.pulse_indexes),
        "LSH_STATIC_CONFIG_ACTIVE_NETWORK_CLICKS": profile.active_network_clicks,
        "LSH_STATIC_CONFIG_DISABLE_NETWORK_CLICKS": 1
        if profile.active_network_clicks == 0
        else 0,
    }


def render_static_resource_macros(
    device: DeviceConfig,
    profile: StaticProfileData,
) -> list[str]:
    """Render the resource-pass macro block for one generated profile."""
    return [
        f"#define {macro} {value}"
        for macro, value in static_resource_macros(device, profile).items()
    ]
