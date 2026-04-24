"""Precomputed profile facts used by multiple renderers."""

from __future__ import annotations

from typing import TYPE_CHECKING

from .constants import DEFAULT_LONG_CLICK_MS, DEFAULT_SUPER_LONG_CLICK_MS
from .metrics import count_active_network_clicks
from .models import StaticProfileData
from .topology import target_indexes

if TYPE_CHECKING:
    from .models import ClickableConfig, DeviceConfig


def click_action_time(clickable: ClickableConfig, action_name: str) -> int:
    """Return the generated threshold for one timed click action."""
    if action_name == "long":
        default = DEFAULT_LONG_CLICK_MS
        action = clickable.long
    else:
        default = DEFAULT_SUPER_LONG_CLICK_MS
        action = clickable.super_long
    return action.time_ms if action.enabled and action.time_ms is not None else default


def collect_static_profile_data(device: DeviceConfig) -> StaticProfileData:
    """Precompute repeated generated-code values for one device."""
    actuator_ids = [actuator.actuator_id for actuator in device.actuators]
    clickable_ids = [clickable.clickable_id for clickable in device.clickables]
    actuator_indexes = {
        actuator.name: index for index, actuator in enumerate(device.actuators)
    }
    auto_off_indexes = [
        actuator_indexes[actuator.name]
        for actuator in device.actuators
        if actuator.auto_off_ms is not None
    ]
    short_link_sets = [
        target_indexes(clickable.short_targets, actuator_indexes)
        for clickable in device.clickables
    ]
    long_link_sets = [
        target_indexes(clickable.long.targets, actuator_indexes)
        for clickable in device.clickables
    ]
    super_long_link_sets = [
        target_indexes(clickable.super_long.targets, actuator_indexes)
        for clickable in device.clickables
    ]
    indicator_link_sets = [
        target_indexes(indicator.targets, actuator_indexes)
        for indicator in device.indicators
    ]
    short_link_counts = [len(links) for links in short_link_sets]
    long_link_counts = [len(links) for links in long_link_sets]
    super_long_link_counts = [len(links) for links in super_long_link_sets]
    indicator_link_counts = [len(links) for links in indicator_link_sets]
    return StaticProfileData(
        actuator_ids=actuator_ids,
        clickable_ids=clickable_ids,
        actuator_indexes=actuator_indexes,
        auto_off_indexes=auto_off_indexes,
        long_types=[
            clickable.long.click_type if clickable.long.enabled else "NORMAL"
            for clickable in device.clickables
        ],
        super_long_types=[
            clickable.super_long.click_type
            if clickable.super_long.enabled
            else "NORMAL"
            for clickable in device.clickables
        ],
        long_times=[
            click_action_time(clickable, "long") for clickable in device.clickables
        ],
        super_long_times=[
            click_action_time(clickable, "super_long")
            for clickable in device.clickables
        ],
        short_link_sets=short_link_sets,
        long_link_sets=long_link_sets,
        super_long_link_sets=super_long_link_sets,
        indicator_link_sets=indicator_link_sets,
        short_link_counts=short_link_counts,
        long_link_counts=long_link_counts,
        super_long_link_counts=super_long_link_counts,
        indicator_link_counts=indicator_link_counts,
        indicator_modes=[indicator.mode for indicator in device.indicators],
        short_links=sum(short_link_counts),
        long_links=sum(long_link_counts),
        super_long_links=sum(super_long_link_counts),
        indicator_links=sum(indicator_link_counts),
        active_network_clicks=count_active_network_clicks(device),
    )
