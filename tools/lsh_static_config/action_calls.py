"""Shared renderers for generated actuator method calls."""

from __future__ import annotations

from typing import TYPE_CHECKING

from .cpp import u8, u16
from .topology import actuator_name_at

if TYPE_CHECKING:
    from .models import DeviceConfig

INLINE_PREFIX = "[[nodiscard]] inline auto"
ALWAYS_INLINE_PREFIX = "[[nodiscard]] __attribute__((always_inline)) inline auto"


def actuator_action_function_name(device: DeviceConfig, actuator_index: int) -> str:
    """Return the generated helper prefix for one actuator action wrapper."""
    return f"{actuator_name_at(device, actuator_index)}Action"


def pulse_slot_map(device: DeviceConfig) -> dict[int, int]:
    """Return dense actuator index to pulse scheduler slot."""
    return {
        actuator_index: pulse_index
        for pulse_index, (actuator_index, actuator) in enumerate(
            item for item in enumerate(device.actuators) if item[1].pulse_ms is not None
        )
    }


def interlock_indexes(device: DeviceConfig, actuator_index: int) -> list[int]:
    """Return generated dense indexes switched OFF before this actuator turns ON."""
    actuator_indexes = {
        actuator.name: index for index, actuator in enumerate(device.actuators)
    }
    return [
        actuator_indexes[target]
        for target in device.actuators[actuator_index].interlock_targets
    ]


def actuator_needs_generated_wrapper(device: DeviceConfig, actuator_index: int) -> bool:
    """Return true when direct object calls cannot express generated semantics."""
    actuator = device.actuators[actuator_index]
    return actuator.pulse_ms is not None or bool(actuator.interlock_targets)


def action_helper_prefix(device: DeviceConfig, actuator_index: int) -> str:
    """Use forced inlining only for wrappers that cannot call other wrappers."""
    if actuator_needs_generated_wrapper(device, actuator_index):
        return INLINE_PREFIX
    return ALWAYS_INLINE_PREFIX


def render_toggle_call(
    device: DeviceConfig,
    actuator_index: int,
    *,
    cached_time: bool,
) -> str:
    """Render one generated timestamp-aware actuator toggle call."""
    function_name = actuator_action_function_name(device, actuator_index)
    if cached_time:
        return f"{function_name}Toggle(actionNow)"
    return f"{function_name}Toggle()"


def render_set_state_call(
    device: DeviceConfig,
    actuator_index: int,
    state: str,
    *,
    cached_time: bool,
) -> str:
    """Render one generated timestamp-aware actuator set-state call."""
    function_name = actuator_action_function_name(device, actuator_index)
    if cached_time:
        return f"{function_name}Set({state}, actionNow)"
    return f"{function_name}Set({state})"


def render_action_helper_declarations(device: DeviceConfig) -> list[str]:
    """Render forward declarations for mutually interlocked actuator wrappers."""
    lines: list[str] = []
    for actuator_index in range(len(device.actuators)):
        function_name = actuator_action_function_name(device, actuator_index)
        prefix = action_helper_prefix(device, actuator_index)
        lines.extend(
            [
                (f"{prefix} {function_name}Set(bool state) noexcept -> bool;"),
                (
                    f"{prefix} {function_name}Set("
                    "bool state, uint32_t actionNow) noexcept -> bool;"
                ),
                f"{prefix} {function_name}Toggle() noexcept -> bool;",
                (
                    f"{prefix} {function_name}Toggle("
                    "uint32_t actionNow) noexcept -> bool;"
                ),
            ]
        )
    return lines


def render_pulse_storage(device: DeviceConfig) -> list[str]:
    """Render pulse countdown storage only for profiles that need it."""
    if not any(actuator.pulse_ms is not None for actuator in device.actuators):
        return []
    return [
        "static uint16_t pulseRemaining_ms[CONFIG_PULSE_STORAGE_CAPACITY] = {};",
        "static uint8_t activePulseActuators = 0U;",
    ]


def render_cached_time_for_helper() -> list[str]:
    """Render timestamp acquisition for wrapper overloads that need coordination."""
    return [
        "#if LSH_CORE_ACTUATOR_NEEDS_SWITCH_TIMESTAMP",
        "    const uint32_t actionNow = timeKeeper::getTime();",
        "#else",
        "    constexpr uint32_t actionNow = 0U;",
        "#endif",
    ]


def render_interlock_lines(device: DeviceConfig, actuator_index: int) -> list[str]:
    """Render interlock OFF calls guarded by the requested ON state."""
    indexes = interlock_indexes(device, actuator_index)
    if not indexes:
        return []
    lines = ["    if (state)", "    {"]
    for interlock_index in indexes:
        function_name = actuator_action_function_name(device, interlock_index)
        lines.append(
            f"        anyActuatorChangedState |= {function_name}Set(false, actionNow);"
        )
    lines.append("    }")
    return lines


def render_pulse_set_lines(
    device: DeviceConfig,
    actuator_index: int,
    pulse_index: int,
) -> list[str]:
    """Render ON/OFF behavior for one generated pulse actuator."""
    actuator = device.actuators[actuator_index]
    object_name = actuator_name_at(device, actuator_index)
    pulse_ms = actuator.pulse_ms if actuator.pulse_ms is not None else 0
    return [
        (
            f"    static_assert({u16(pulse_ms)} >= "
            "constants::timings::ACTUATOR_DEBOUNCE_TIME_MS, "
            '"Pulse duration must be greater than or equal to actuator debounce.");'
        ),
        "    if (state)",
        "    {",
        f"        const bool actuatorWasOn = {object_name}.getState();",
        (
            f"        const bool pulseWasActive = "
            f"pulseRemaining_ms[{u8(pulse_index)}] != 0U;"
        ),
        (
            f"        const bool pulseStarted = {object_name}"
            f".setStateStatic<{u8(actuator_index)}>(true, actionNow);"
        ),
        "        anyActuatorChangedState |= pulseStarted;",
        "        if (pulseStarted || actuatorWasOn)",
        "        {",
        "            if (!pulseWasActive)",
        "            {",
        "                ++activePulseActuators;",
        "            }",
        f"            pulseRemaining_ms[{u8(pulse_index)}] = {u16(pulse_ms)};",
        "        }",
        "    }",
        "    else",
        "    {",
        f"        if (pulseRemaining_ms[{u8(pulse_index)}] != 0U)",
        "        {",
        f"            pulseRemaining_ms[{u8(pulse_index)}] = 0U;",
        "            --activePulseActuators;",
        "        }",
        (
            f"        anyActuatorChangedState |= {object_name}"
            f".setStateStatic<{u8(actuator_index)}>(false, actionNow);"
        ),
        "    }",
    ]


def render_regular_set_line(device: DeviceConfig, actuator_index: int) -> str:
    """Render the final direct set call for a non-pulse actuator helper."""
    object_name = actuator_name_at(device, actuator_index)
    return (
        f"    anyActuatorChangedState |= {object_name}"
        f".setStateStatic<{u8(actuator_index)}>(state, actionNow);"
    )


def render_action_helper_definition(
    device: DeviceConfig,
    actuator_index: int,
    pulse_indexes: dict[int, int],
) -> list[str]:
    """Render the central generated wrapper for one actuator."""
    object_name = actuator_name_at(device, actuator_index)
    function_name = actuator_action_function_name(device, actuator_index)
    needs_wrapper = actuator_needs_generated_wrapper(device, actuator_index)
    prefix = action_helper_prefix(device, actuator_index)
    lines = [
        f"{prefix} {function_name}Set(bool state) noexcept -> bool",
        "{",
    ]
    if needs_wrapper:
        lines.extend(render_cached_time_for_helper())
        lines.append(f"    return {function_name}Set(state, actionNow);")
    else:
        lines.append(
            f"    return {object_name}.setStateStatic<{u8(actuator_index)}>(state);"
        )
    lines.extend(
        [
            "}",
            "",
            (
                f"{prefix} {function_name}Set("
                "bool state, uint32_t actionNow) noexcept -> bool"
            ),
            "{",
            "    bool anyActuatorChangedState = false;",
        ]
    )
    lines.extend(render_interlock_lines(device, actuator_index))
    pulse_index = pulse_indexes.get(actuator_index)
    if pulse_index is None:
        lines.append(render_regular_set_line(device, actuator_index))
    else:
        lines.extend(render_pulse_set_lines(device, actuator_index, pulse_index))
    lines.extend(
        [
            "    return anyActuatorChangedState;",
            "}",
            "",
            f"{prefix} {function_name}Toggle() noexcept -> bool",
            "{",
        ]
    )
    if needs_wrapper:
        lines.extend(render_cached_time_for_helper())
        lines.append(f"    return {function_name}Toggle(actionNow);")
    else:
        lines.append(
            f"    return {object_name}.toggleStateStatic<{u8(actuator_index)}>();"
        )
    lines.extend(
        [
            "}",
            "",
            (f"{prefix} {function_name}Toggle(uint32_t actionNow) noexcept -> bool"),
            "{",
            f"    return {function_name}Set(!{object_name}.getState(), actionNow);",
            "}",
        ]
    )
    return lines


def render_generated_actuator_action_helpers(device: DeviceConfig) -> list[str]:
    """Render central wrappers used by every generated actuator action."""
    if not device.actuators:
        return []
    pulse_indexes = pulse_slot_map(device)
    lines: list[str] = []
    lines.extend(render_pulse_storage(device))
    if lines:
        lines.append("")
    lines.extend(render_action_helper_declarations(device))
    for actuator_index in range(len(device.actuators)):
        lines.append("")
        lines.extend(
            render_action_helper_definition(device, actuator_index, pulse_indexes)
        )
    return lines
