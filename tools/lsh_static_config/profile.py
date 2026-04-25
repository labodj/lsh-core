"""Precomputed profile facts used by multiple renderers."""

from __future__ import annotations

from typing import TYPE_CHECKING

from .metrics import count_active_network_clicks
from .models import StaticProfileData
from .topology import target_indexes

if TYPE_CHECKING:
    from .models import DeviceConfig


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
    network_click_slots: list[tuple[int, str]] = []
    for clickable_index, clickable in enumerate(device.clickables):
        if clickable.long.enabled and clickable.long.network:
            network_click_slots.append((clickable_index, "LONG"))
        if clickable.super_long.enabled and clickable.super_long.network:
            network_click_slots.append((clickable_index, "SUPER_LONG"))
    return StaticProfileData(
        actuator_ids=actuator_ids,
        clickable_ids=clickable_ids,
        actuator_indexes=actuator_indexes,
        auto_off_indexes=auto_off_indexes,
        short_link_sets=short_link_sets,
        long_link_sets=long_link_sets,
        super_long_link_sets=super_long_link_sets,
        indicator_link_sets=indicator_link_sets,
        short_link_counts=short_link_counts,
        long_link_counts=long_link_counts,
        super_long_link_counts=super_long_link_counts,
        indicator_link_counts=indicator_link_counts,
        network_click_slots=network_click_slots,
        short_links=sum(short_link_counts),
        long_links=sum(long_link_counts),
        super_long_links=sum(super_long_link_counts),
        indicator_links=sum(indicator_link_counts),
        active_network_clicks=count_active_network_clicks(device),
    )
