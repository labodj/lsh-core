"""Precomputed profile facts used by multiple renderers."""

from __future__ import annotations

from typing import TYPE_CHECKING

from .metrics import count_active_network_clicks
from .models import StaticProfileData
from .topology import target_indexes

if TYPE_CHECKING:
    from .models import ActionStep, DeviceConfig


def action_step_sets(
    steps: list[ActionStep],
    actuator_indexes: dict[str, int],
) -> list[tuple[str, list[int]]]:
    """Translate action steps into operation/index pairs for renderers."""
    return [
        (step.operation, target_indexes(step.targets, actuator_indexes))
        for step in steps
    ]


def count_step_links(step_sets: list[list[tuple[str, list[int]]]]) -> list[int]:
    """Count actuator links used by deterministic generated action steps."""
    return [sum(len(indexes) for _operation, indexes in steps) for steps in step_sets]


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
    pulse_indexes = [
        actuator_indexes[actuator.name]
        for actuator in device.actuators
        if actuator.pulse_ms is not None
    ]
    short_link_sets = [
        target_indexes(clickable.short_targets, actuator_indexes)
        for clickable in device.clickables
    ]
    short_step_sets = [
        action_step_sets(clickable.short_steps, actuator_indexes)
        for clickable in device.clickables
    ]
    long_link_sets = [
        target_indexes(clickable.long.targets, actuator_indexes)
        for clickable in device.clickables
    ]
    long_step_sets = [
        action_step_sets(clickable.long.steps, actuator_indexes)
        for clickable in device.clickables
    ]
    super_long_link_sets = [
        target_indexes(clickable.super_long.targets, actuator_indexes)
        for clickable in device.clickables
    ]
    super_long_step_sets = [
        action_step_sets(clickable.super_long.steps, actuator_indexes)
        for clickable in device.clickables
    ]
    indicator_link_sets = [
        target_indexes(indicator.targets, actuator_indexes)
        for indicator in device.indicators
    ]
    short_link_counts = [len(links) for links in short_link_sets]
    short_step_link_counts = count_step_links(short_step_sets)
    long_link_counts = [len(links) for links in long_link_sets]
    long_step_link_counts = count_step_links(long_step_sets)
    super_long_link_counts = [len(links) for links in super_long_link_sets]
    super_long_step_link_counts = count_step_links(super_long_step_sets)
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
        pulse_indexes=pulse_indexes,
        short_link_sets=short_link_sets,
        short_step_sets=short_step_sets,
        long_link_sets=long_link_sets,
        long_step_sets=long_step_sets,
        super_long_link_sets=super_long_link_sets,
        super_long_step_sets=super_long_step_sets,
        indicator_link_sets=indicator_link_sets,
        short_link_counts=short_link_counts,
        short_step_link_counts=short_step_link_counts,
        long_link_counts=long_link_counts,
        long_step_link_counts=long_step_link_counts,
        super_long_link_counts=super_long_link_counts,
        super_long_step_link_counts=super_long_step_link_counts,
        indicator_link_counts=indicator_link_counts,
        network_click_slots=network_click_slots,
        short_links=sum(short_link_counts) + sum(short_step_link_counts),
        long_links=sum(long_link_counts) + sum(long_step_link_counts),
        super_long_links=sum(super_long_link_counts) + sum(super_long_step_link_counts),
        indicator_links=sum(indicator_link_counts),
        active_network_clicks=count_active_network_clicks(device),
    )
