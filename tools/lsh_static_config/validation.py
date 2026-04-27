"""Cross-resource validation for normalized static TOML profiles."""

from __future__ import annotations

from typing import TYPE_CHECKING, TypeVar

from .constants import DEFAULT_LONG_CLICK_MS, DEFAULT_SUPER_LONG_CLICK_MS, UINT8_MAX
from .errors import fail

if TYPE_CHECKING:
    from collections.abc import Callable, Hashable, Iterable, Sequence

    from .models import ActionStep, ClickableConfig, DeviceConfig, ProjectConfig

TConfig = TypeVar("TConfig")
GENERATED_TOP_LEVEL_HEADER_COUNT = 2


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


def _validate_unique_fields(device: DeviceConfig) -> None:
    """Validate wire IDs and names inside each resource family."""
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


def validate_target_set(targets: Sequence[str], allowed: set[str], path: str) -> None:
    """Validate target names and reject duplicate actuator links."""
    seen: set[str] = set()
    for target in targets:
        if target not in allowed:
            fail(f"{path} references unknown actuator {target!r}.")
        if target in seen:
            fail(f"{path} references actuator {target!r} more than once.")
        seen.add(target)


def validate_action_steps(
    steps: Sequence[ActionStep],
    allowed: set[str],
    path: str,
) -> None:
    """Validate every target inside deterministic generated action steps."""
    for step_index, step in enumerate(steps):
        validate_target_set(
            step.targets,
            allowed,
            f"{path}.steps[{step_index}].targets",
        )


def _validate_actuator_options(device: DeviceConfig) -> None:
    """Validate advanced actuator features after all actuator names are known."""
    actuator_names = {actuator.name for actuator in device.actuators}
    for actuator in device.actuators:
        if actuator.pulse_ms is not None and actuator.auto_off_ms is not None:
            fail(
                f"devices.{device.key}.actuators.{actuator.name} cannot combine "
                "pulse with auto_off; use pulse for momentary outputs or "
                "auto_off for latched outputs with a guard timer."
            )
        validate_target_set(
            actuator.interlock_targets,
            actuator_names,
            f"devices.{device.key}.actuators.{actuator.name}.interlock",
        )
        if actuator.name in actuator.interlock_targets:
            fail(
                f"devices.{device.key}.actuators.{actuator.name}.interlock "
                "cannot include the actuator itself."
            )


def _validate_clickable_targets(device: DeviceConfig) -> None:
    """Validate clickable local links and ensure every clickable can fire."""
    actuator_names = {actuator.name for actuator in device.actuators}
    protected_actuator_names = {
        actuator.name for actuator in device.actuators if actuator.protected
    }
    for clickable in device.clickables:
        validate_target_set(
            clickable.short_targets,
            actuator_names,
            f"devices.{device.key}.clickables.{clickable.name}.short",
        )
        validate_action_steps(
            clickable.short_steps,
            actuator_names,
            f"devices.{device.key}.clickables.{clickable.name}.short",
        )
        validate_target_set(
            clickable.long.targets,
            actuator_names,
            f"devices.{device.key}.clickables.{clickable.name}.long",
        )
        validate_action_steps(
            clickable.long.steps,
            actuator_names,
            f"devices.{device.key}.clickables.{clickable.name}.long",
        )
        validate_target_set(
            clickable.super_long.targets,
            actuator_names,
            f"devices.{device.key}.clickables.{clickable.name}.super_long",
        )
        validate_action_steps(
            clickable.super_long.steps,
            actuator_names,
            f"devices.{device.key}.clickables.{clickable.name}.super_long",
        )
        if clickable.super_long.click_type == "SELECTIVE":
            for target in clickable.super_long.targets:
                if target in protected_actuator_names:
                    fail(
                        f"devices.{device.key}.clickables.{clickable.name}"
                        ".super_long targets protected actuator "
                        f"{target!r}; protected actuators cannot be switched "
                        "off by super-long actions."
                    )
        _validate_click_timing(device, clickable)
        if (
            not _has_effective_short_action(clickable)
            and not _has_effective_long_action(clickable)
            and not _has_effective_super_long_action(device, clickable)
        ):
            fail(
                f"devices.{device.key}.clickables.{clickable.name} has no "
                "effective click action."
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
    _validate_actuator_options(device)
    _validate_clickable_targets(device)
    _validate_indicator_targets(device)


def validate_project(project: ProjectConfig) -> None:
    """Validate cross-device generated paths and macro selectors."""
    header_names = {
        project.settings.user_config_header,
        project.settings.static_config_router_header,
    }
    if len(header_names) != GENERATED_TOP_LEVEL_HEADER_COUNT:
        fail(
            "generator.user_config_header and "
            "generator.static_config_router_header must be different."
        )

    build_macros: dict[str, str] = {}
    generated_headers: dict[str, str] = {
        project.settings.user_config_header: "generator.user_config_header",
    }
    generated_headers[project.settings.static_config_router_header] = (
        "generator.static_config_router_header"
    )
    for device in project.devices.values():
        previous_macro = build_macros.get(device.build_macro)
        if previous_macro is not None:
            fail(
                f"devices.{device.key}.build_macro duplicates "
                f"devices.{previous_macro}.build_macro: {device.build_macro}."
            )
        build_macros[device.build_macro] = device.key

        for field_name, include_path in (
            ("config_include", device.config_include),
            ("static_config_include", device.static_config_include),
        ):
            previous_owner = generated_headers.get(include_path)
            if previous_owner is not None:
                fail(
                    f"devices.{device.key}.{field_name} collides with "
                    f"{previous_owner}: {include_path}."
                )
            generated_headers[include_path] = f"devices.{device.key}.{field_name}"


def _has_effective_short_action(clickable: ClickableConfig) -> bool:
    """Return true when the short-click action can change at least one actuator."""
    return clickable.short_enabled and (
        bool(clickable.short_targets) or bool(clickable.short_steps)
    )


def _has_effective_long_action(clickable: ClickableConfig) -> bool:
    """Return true when the long-click action has a local or network effect."""
    return clickable.long.enabled and (
        clickable.long.network
        or bool(clickable.long.targets)
        or bool(clickable.long.steps)
    )


def _has_effective_super_long_action(
    device: DeviceConfig,
    clickable: ClickableConfig,
) -> bool:
    """Return true when the super-long action has a real local or network effect."""
    if not clickable.super_long.enabled:
        return False
    if clickable.super_long.network:
        return True
    if clickable.super_long.steps:
        return True
    if clickable.super_long.click_type == "NORMAL":
        return any(not actuator.protected for actuator in device.actuators)
    return bool(clickable.super_long.targets)


def _effective_click_time_ms(clickable: ClickableConfig, action_name: str) -> int:
    """Return the effective generated threshold for one timed click action."""
    if action_name == "long":
        return (
            clickable.long.time_ms
            if clickable.long.time_ms is not None
            else DEFAULT_LONG_CLICK_MS
        )
    return (
        clickable.super_long.time_ms
        if clickable.super_long.time_ms is not None
        else DEFAULT_SUPER_LONG_CLICK_MS
    )


def _validate_click_timing(device: DeviceConfig, clickable: ClickableConfig) -> None:
    """Reject timed click thresholds that would make event ordering ambiguous."""
    if not (clickable.long.enabled and clickable.super_long.enabled):
        return
    long_ms = _effective_click_time_ms(clickable, "long")
    super_long_ms = _effective_click_time_ms(clickable, "super_long")
    if super_long_ms <= long_ms:
        fail(
            f"devices.{device.key}.clickables.{clickable.name}.super_long "
            "must be greater than the long-click threshold."
        )
