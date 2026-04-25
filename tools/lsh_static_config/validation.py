"""Cross-resource validation for normalized static TOML profiles."""

from __future__ import annotations

from typing import TYPE_CHECKING, TypeVar

from .constants import UINT8_MAX
from .errors import fail

if TYPE_CHECKING:
    from collections.abc import Callable, Hashable, Iterable, Sequence

    from .models import DeviceConfig

TConfig = TypeVar("TConfig")


def validate_unique(
    values: Iterable[TConfig],
    field_name: str,
    path: str,
    value_of: Callable[[TConfig], Hashable],
) -> None:
    """Reject duplicate values while preserving source indexes in the error."""
    seen: dict[Hashable, int] = {}
    for index, item in enumerate(values):
        value = value_of(item)
        if value in seen:
            fail(
                f"{path}[{index}].{field_name} duplicates "
                f"{path}[{seen[value]}].{field_name}: {value!r}."
            )
        seen[value] = index


def _validate_resource_count(count: int, path: str) -> None:
    """Keep every generated index representable as a uint8_t."""
    if count > UINT8_MAX:
        fail(f"{path} cannot contain more than {UINT8_MAX} entries.")


def _validate_resource_counts(device: DeviceConfig) -> None:
    """Validate generated array/index limits for one device."""
    _validate_resource_count(
        len(device.actuators),
        f"devices.{device.key}.actuators",
    )
    _validate_resource_count(
        len(device.clickables),
        f"devices.{device.key}.clickables",
    )
    _validate_resource_count(
        len(device.indicators),
        f"devices.{device.key}.indicators",
    )


def _validate_cpp_object_name(
    device_key: str,
    group_name: str,
    object_name: str,
    used_names: set[str],
) -> None:
    """Reject cross-resource name reuse before generating C++ variables."""
    if object_name in used_names:
        fail(
            f"devices.{device_key}.{group_name}.{object_name} reuses a C++ "
            "object name already used by another resource."
        )
    used_names.add(object_name)


def _validate_unique_fields(device: DeviceConfig) -> None:
    """Validate all user IDs and C++ object names for one device."""
    validate_unique(
        device.actuators,
        "name",
        f"devices.{device.key}.actuators",
        lambda actuator: actuator.name,
    )
    validate_unique(
        device.actuators,
        "actuator_id",
        f"devices.{device.key}.actuators",
        lambda actuator: actuator.actuator_id,
    )
    validate_unique(
        device.clickables,
        "name",
        f"devices.{device.key}.clickables",
        lambda clickable: clickable.name,
    )
    validate_unique(
        device.clickables,
        "clickable_id",
        f"devices.{device.key}.clickables",
        lambda clickable: clickable.clickable_id,
    )
    validate_unique(
        device.indicators,
        "name",
        f"devices.{device.key}.indicators",
        lambda indicator: indicator.name,
    )

    object_names: set[str] = set()
    for actuator in device.actuators:
        _validate_cpp_object_name(device.key, "actuators", actuator.name, object_names)
    for clickable in device.clickables:
        _validate_cpp_object_name(
            device.key,
            "clickables",
            clickable.name,
            object_names,
        )
    for indicator in device.indicators:
        _validate_cpp_object_name(
            device.key,
            "indicators",
            indicator.name,
            object_names,
        )


def validate_target_set(targets: Sequence[str], allowed: set[str], path: str) -> None:
    """Validate target names and reject duplicate actuator links."""
    seen: set[str] = set()
    for target in targets:
        if target not in allowed:
            fail(f"{path} references unknown actuator {target!r}.")
        if target in seen:
            fail(f"{path} references actuator {target!r} more than once.")
        seen.add(target)


def _validate_clickable_targets(device: DeviceConfig) -> None:
    """Validate clickable local links and ensure every clickable can fire."""
    actuator_names = {actuator.name for actuator in device.actuators}
    for clickable in device.clickables:
        validate_target_set(
            clickable.short_targets,
            actuator_names,
            f"devices.{device.key}.clickables.{clickable.name}.short",
        )
        validate_target_set(
            clickable.long.targets,
            actuator_names,
            f"devices.{device.key}.clickables.{clickable.name}.long",
        )
        validate_target_set(
            clickable.super_long.targets,
            actuator_names,
            f"devices.{device.key}.clickables.{clickable.name}.super_long",
        )
        if (
            not clickable.short_enabled
            and not clickable.long.enabled
            and not clickable.super_long.enabled
        ):
            fail(
                f"devices.{device.key}.clickables.{clickable.name} has no "
                "enabled click action."
            )


def _validate_indicator_targets(device: DeviceConfig) -> None:
    """Validate indicator links and reject inert indicators."""
    actuator_names = {actuator.name for actuator in device.actuators}
    for indicator in device.indicators:
        validate_target_set(
            indicator.targets,
            actuator_names,
            f"devices.{device.key}.indicators.{indicator.name}.actuators",
        )
        if not indicator.targets:
            fail(
                f"devices.{device.key}.indicators.{indicator.name} must "
                "reference at least one actuator."
            )


def validate_device(device: DeviceConfig) -> None:
    """Validate cross-resource references and static resource limits."""
    _validate_resource_counts(device)
    _validate_unique_fields(device)
    _validate_clickable_targets(device)
    _validate_indicator_targets(device)
