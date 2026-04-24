"""Shared static-topology helpers for generated C++ renderers."""

from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from collections.abc import Sequence

    from .models import DeviceConfig


def actuator_name_at(device: DeviceConfig, actuator_index: int) -> str:
    """Return the generated C++ object name for one dense actuator index."""
    return device.actuators[actuator_index].name


def unprotected_actuator_indexes(device: DeviceConfig) -> list[int]:
    """Return dense indexes for actuators not marked as globally protected."""
    return [
        actuator_index
        for actuator_index, actuator in enumerate(device.actuators)
        if not actuator.protected
    ]


def target_indexes(
    target_names: Sequence[str],
    actuator_indexes: dict[str, int],
) -> list[int]:
    """Translate already-validated actuator names to generated indexes."""
    return [actuator_indexes[target] for target in target_names]
