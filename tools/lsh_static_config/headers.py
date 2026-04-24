"""Top-level generated header renderers."""

from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

from .configure import render_configure
from .cpp import header_guard, render_banner, str_literal
from .metrics import count_timing_overrides
from .payloads import render_static_payload_arrays, render_static_payload_writer_helper
from .profile import collect_static_profile_data
from .static_accessors import render_static_config_accessors

if TYPE_CHECKING:
    from collections.abc import Sequence

    from .models import ActuatorConfig, DeviceConfig, ProjectConfig


def render_user_config(project: ProjectConfig) -> str:
    """Render the device-router header included as lsh_user_config.hpp."""
    lines = render_banner(project.settings.user_config_header, project.source_path)
    guard = header_guard(project.settings.user_config_header)
    lines.extend([f"#ifndef {guard}", f"#define {guard}", ""])

    for index, device in enumerate(project.devices.values()):
        directive = "#if" if index == 0 else "#elif"
        lines.append(f"{directive} defined({device.build_macro})")
        lines.append(f"#include {str_literal(device.config_include)}")
    lines.append("#else")
    choices = ", ".join(device.build_macro for device in project.devices.values())
    lines.append(f"// Available profiles: {choices}.")
    lines.append(
        '#error "No lsh-core device profile selected. Define exactly one generated '
        'LSH_BUILD_* macro."'
    )
    lines.append("#endif")
    lines.extend(["", f"#endif  // {guard}", ""])
    return "\n".join(lines)


def render_device_config(device: DeviceConfig, project: ProjectConfig) -> str:
    """Render one device constants header."""
    lines = render_banner(device.config_include, project.source_path)
    guard = header_guard(device.config_include)
    lines.extend(
        [
            f"#ifndef {guard}",
            f"#define {guard}",
            "",
            f"#define LSH_HARDWARE_INCLUDE {device.hardware_include}",
            f"#define LSH_DEVICE_NAME {str_literal(device.device_name)}",
            "#define LSH_STATIC_CONFIG_INCLUDE "
            f"{str_literal(device.static_config_include)}",
            f"#define LSH_COM_SERIAL {device.com_serial}",
            f"#define LSH_DEBUG_SERIAL {device.debug_serial}",
            "",
            f"#endif  // {guard}",
            "",
        ],
    )
    return "\n".join(lines)


def render_object_declarations(device: DeviceConfig) -> list[str]:
    """Render static Actuator, Clickable and Indicator object declarations."""
    lines = ["namespace", "{"]
    lines.extend(render_static_payload_arrays(device))
    lines.append("")
    lines.extend(render_static_payload_writer_helper())
    if device.actuators or device.clickables or device.indicators:
        lines.append("")
    lines.extend(render_actuator_declaration(actuator) for actuator in device.actuators)
    if device.actuators and device.clickables:
        lines.append("")
    lines.extend(
        f"LSH_BUTTON({clickable.name}, {clickable.pin});"
        for clickable in device.clickables
    )
    if device.clickables and device.indicators:
        lines.append("")
    lines.extend(
        f"LSH_INDICATOR({indicator.name}, {indicator.pin});"
        for indicator in device.indicators
    )
    lines.append("}  // namespace")
    return lines


def render_actuator_declaration(actuator: ActuatorConfig) -> str:
    """Render one generated actuator object declaration."""
    if actuator.default_state:
        return (
            f"Actuator {actuator.name}"
            f"(::lsh::core::PinTag<({actuator.pin})>{{}}, true);"
        )
    return f"LSH_ACTUATOR({actuator.name}, {actuator.pin});"


def render_static_config(device: DeviceConfig, project: ProjectConfig) -> str:
    """Render the two-pass static profile consumed by lsh-core."""
    lines = render_banner(device.static_config_include, project.source_path)
    resource_guard = header_guard(device.static_config_include + "_RESOURCE")
    implementation_guard = header_guard(
        device.static_config_include + "_IMPLEMENTATION"
    )
    profile = collect_static_profile_data(device)

    lines.extend(
        [
            "#if defined(LSH_STATIC_CONFIG_RESOURCE_PASS)",
            "",
            f"#ifndef {resource_guard}",
            f"#define {resource_guard}",
            "",
        ]
    )
    resource_macros = {
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
        "LSH_STATIC_CONFIG_CLICKABLE_TIMING_OVERRIDES": count_timing_overrides(device),
        "LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS": len(profile.auto_off_indexes),
        "LSH_STATIC_CONFIG_ACTIVE_NETWORK_CLICKS": profile.active_network_clicks,
        "LSH_STATIC_CONFIG_DISABLE_NETWORK_CLICKS": 1
        if profile.active_network_clicks == 0
        else 0,
    }
    for macro, value in resource_macros.items():
        lines.append(f"#define {macro} {value}")
    lines.extend(
        [
            "",
            f"#endif  // {resource_guard}",
            "",
            "#elif defined(LSH_STATIC_CONFIG_IMPLEMENTATION_PASS)",
            "",
        ]
    )
    lines.extend(
        [f"#ifndef {implementation_guard}", f"#define {implementation_guard}", ""]
    )
    lines.extend(
        [
            '#include "device/actuator_manager.hpp"',
            '#include "device/clickable_manager.hpp"',
            '#include "device/indicator_manager.hpp"',
            '#include "lsh_user_macros.hpp"',
            '#include "util/constants/timing.hpp"',
            "",
            "#if defined(__AVR__)",
            "#include <avr/pgmspace.h>",
            "#define LSH_STATIC_CONFIG_PROGMEM PROGMEM",
            "#define LSH_STATIC_CONFIG_READ_BYTE(address) pgm_read_byte(address)",
            "#else",
            "#define LSH_STATIC_CONFIG_PROGMEM",
            "#define LSH_STATIC_CONFIG_READ_BYTE(address) (*(address))",
            "#endif",
            "",
        ],
    )
    lines.extend(render_object_declarations(device))
    lines.extend(["", "namespace lsh::core::static_config", "{"])
    lines.extend(render_static_config_accessors(device, profile))
    lines.extend(["}  // namespace lsh::core::static_config", ""])
    lines.extend(render_configure(device))
    lines.extend(
        [
            "",
            "#undef LSH_STATIC_CONFIG_READ_BYTE",
            "#undef LSH_STATIC_CONFIG_PROGMEM",
            "",
            f"#endif  // {implementation_guard}",
            "",
            "#else",
        ]
    )
    lines.append(
        f'#error "{Path(device.static_config_include).name} must be included '
        'through a static-config pass macro."'
    )
    lines.append("#endif")
    lines.append("")
    return "\n".join(lines)


def generated_files(
    project: ProjectConfig, selected_devices: Sequence[str]
) -> dict[Path, str]:
    """Render every generated file without touching the filesystem."""
    output_dir = project.settings.output_dir
    files: dict[Path, str] = {
        output_dir / project.settings.user_config_header: render_user_config(project),
    }
    for key in selected_devices:
        device = project.devices[key]
        files[output_dir / device.config_include] = render_device_config(
            device, project
        )
        files[output_dir / device.static_config_include] = render_static_config(
            device, project
        )
    return files
