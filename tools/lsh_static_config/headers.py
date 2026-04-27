"""Top-level generated header renderers."""

from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

from .configure import render_configure
from .cpp import header_guard, render_banner, str_literal
from .payloads import render_static_payload_arrays, render_static_payload_writer_helper
from .profile import collect_static_profile_data
from .resource_macros import render_static_resource_macros
from .static_accessors import render_static_config_accessors
from .topology import (
    actuator_object_name,
    clickable_object_name,
    indicator_object_name,
)

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


def render_static_config_router(project: ProjectConfig) -> str:
    """Render the pass router that selects one generated static implementation."""
    lines = render_banner(
        project.settings.static_config_router_header, project.source_path
    )
    lines.extend(
        [
            "// Intentionally no include guard: lsh-core includes this router twice",
            "// with different pass macros. A conventional guard would make the",
            "// implementation pass disappear after the resource pass.",
            "",
        ]
    )
    for index, device in enumerate(project.devices.values()):
        directive = "#if" if index == 0 else "#elif"
        lines.append(f"{directive} defined({device.build_macro})")
        lines.append(f"#include {str_literal(device.static_config_include)}")
    lines.append("#else")
    choices = ", ".join(device.build_macro for device in project.devices.values())
    lines.append(f"// Available profiles: {choices}.")
    lines.append(
        '#error "No lsh-core static profile selected. Define exactly one generated '
        'LSH_BUILD_* macro."'
    )
    lines.append("#endif")
    lines.append("")
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
            "#include LSH_HARDWARE_INCLUDE",
            "",
            "static constexpr auto LSH_DEVICE_NAME() -> const char *",
            "{",
            f"    return {str_literal(device.device_name)};",
            "}",
            "",
            "static constexpr auto LSH_COM_SERIAL() -> HardwareSerial *",
            "{",
            f"    return {device.com_serial};",
            "}",
            "",
            "static constexpr auto LSH_DEBUG_SERIAL() -> HardwareSerial *",
            "{",
            f"    return {device.debug_serial};",
            "}",
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
    lines.extend(
        render_actuator_declaration(index, actuator)
        for index, actuator in enumerate(device.actuators)
    )
    if device.actuators and device.clickables:
        lines.append("")
    lines.extend(
        f"LSH_BUTTON({clickable_object_name(index, clickable)}, {clickable.pin});"
        for index, clickable in enumerate(device.clickables)
    )
    if device.clickables and device.indicators:
        lines.append("")
    lines.extend(
        f"LSH_INDICATOR({indicator_object_name(index, indicator)}, {indicator.pin});"
        for index, indicator in enumerate(device.indicators)
    )
    lines.append("}  // namespace")
    return lines


def render_actuator_declaration(index: int, actuator: ActuatorConfig) -> str:
    """Render one generated actuator object declaration."""
    object_name = actuator_object_name(index, actuator)
    if actuator.default_state:
        return (
            f"Actuator {object_name}(::lsh::core::PinTag<({actuator.pin})>{{}}, true);"
        )
    return f"LSH_ACTUATOR({object_name}, {actuator.pin});"


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
    lines.extend(render_static_resource_macros(device, profile))
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
            '#include "communication/bridge_serial.hpp"',
            '#include "config/static_config.hpp"',
            '#include "core/network_clicks.hpp"',
            '#include "device/actuator_manager.hpp"',
            '#include "device/clickable_manager.hpp"',
            '#include "device/indicator_manager.hpp"',
            '#include "lsh_user_macros.hpp"',
            '#include "util/constants/click_detection.hpp"',
            '#include "util/constants/click_results.hpp"',
            '#include "util/constants/click_types.hpp"',
            '#include "util/constants/timing.hpp"',
            '#include "util/debug/debug.hpp"',
            '#include "util/time_keeper.hpp"',
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
        output_dir
        / project.settings.static_config_router_header: render_static_config_router(
            project
        ),
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
