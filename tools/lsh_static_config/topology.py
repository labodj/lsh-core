"""Shared static-topology helpers for generated C++ renderers."""

from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from collections.abc import Sequence

    from .models import (
        ActuatorConfig,
        ClickableConfig,
        DeviceConfig,
        IndicatorConfig,
    )


def _object_suffix(logical_name: str) -> str:
    """Keep generated object names readable without depending on TOML uniqueness."""
    suffix = logical_name.strip("_")
    return suffix or "resource"


def actuator_object_name(actuator_index: int, actuator: ActuatorConfig) -> str:
    """Return the C++ object name for one generated actuator."""
    return f"actuator{actuator_index}_{_object_suffix(actuator.name)}"


def clickable_object_name(clickable_index: int, clickable: ClickableConfig) -> str:
    """Return the C++ object name for one generated clickable."""
    return f"button{clickable_index}_{_object_suffix(clickable.name)}"


def indicator_object_name(indicator_index: int, indicator: IndicatorConfig) -> str:
    """Return the C++ object name for one generated indicator."""
    return f"indicator{indicator_index}_{_object_suffix(indicator.name)}"


def actuator_name_at(device: DeviceConfig, actuator_index: int) -> str:
    """Return the generated C++ object name for one dense actuator index."""
    return actuator_object_name(actuator_index, device.actuators[actuator_index])


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
