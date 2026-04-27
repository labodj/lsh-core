"""Advisory checks for public `lsh_devices.toml` profiles."""

from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING

from .constants import DEFAULT_LONG_CLICK_MS, DEFAULT_SUPER_LONG_CLICK_MS
from .platformio import merged_defines

if TYPE_CHECKING:
    from .models import ClickableConfig, DeviceConfig, ProjectConfig

SEMANTIC_DEFINE_HINTS = {
    "CONFIG_MSG_PACK": "features.codec",
    "CONFIG_USE_FAST_CLICKABLES": "features.fast_buttons",
    "CONFIG_USE_FAST_ACTUATORS": "features.fast_actuators",
    "CONFIG_USE_FAST_INDICATORS": "features.fast_indicators",
    "CONFIG_ACTUATOR_DEBOUNCE_TIME_MS": "timing.actuator_debounce",
    "CONFIG_CLICKABLE_DEBOUNCE_TIME_MS": "timing.button_debounce",
    "CONFIG_CLICKABLE_SCAN_INTERVAL_MS": "timing.scan_interval",
    "CONFIG_CLICKABLE_LONG_CLICK_TIME_MS": "timing.long_click",
    "CONFIG_CLICKABLE_SUPER_LONG_CLICK_TIME_MS": "timing.super_long_click",
    "CONFIG_LCNB_TIMEOUT_MS": "timing.network_click_timeout",
    "CONFIG_PING_INTERVAL_MS": "timing.ping_interval",
    "CONFIG_CONNECTION_TIMEOUT_MS": "timing.connection_timeout",
    "CONFIG_BRIDGE_BOOT_RETRY_INTERVAL_MS": "timing.bridge_boot_retry",
    "CONFIG_BRIDGE_AWAIT_STATE_TIMEOUT_MS": "timing.bridge_state_timeout",
    "CONFIG_DEBUG_SERIAL_BAUD": "serial.debug_baud",
    "CONFIG_COM_SERIAL_BAUD": "serial.bridge_baud",
    "CONFIG_COM_SERIAL_TIMEOUT_MS": "serial.timeout",
    "CONFIG_COM_SERIAL_MSGPACK_FRAME_IDLE_TIMEOUT_MS": (
        "serial.msgpack_frame_idle_timeout"
    ),
    "CONFIG_COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP": ("serial.max_rx_payloads_per_loop"),
    "CONFIG_COM_SERIAL_MAX_RX_BYTES_PER_LOOP": "serial.max_rx_bytes_per_loop",
    "CONFIG_COM_SERIAL_FLUSH_AFTER_SEND": "serial.flush_after_send",
    "CONFIG_DELAY_AFTER_RECEIVE_MS": "timing.post_receive_delay",
    "CONFIG_NETWORK_CLICK_CHECK_INTERVAL_MS": "timing.network_click_check_interval",
    "CONFIG_ACTUATORS_AUTO_OFF_CHECK_INTERVAL_MS": "timing.auto_off_check_interval",
    "CONFIG_LSH_BENCH": "features.bench",
    "CONFIG_BENCH_ITERATIONS": "features.bench_iterations",
}
MIN_RECOMMENDED_CLICK_THRESHOLD_GAP_MS = 250


@dataclass(frozen=True)
class Diagnostic:
    """One non-fatal configuration finding."""

    level: str
    path: str
    message: str


def diagnose_project(project: ProjectConfig) -> list[Diagnostic]:
    """Return non-fatal adoption and maintainability findings."""
    diagnostics: list[Diagnostic] = []
    _diagnose_lockfile(project, diagnostics)
    _diagnose_semantic_define_overlap(project, diagnostics)
    for device in project.devices.values():
        _diagnose_fast_io(project, device, diagnostics)
        _diagnose_clicks(device, diagnostics)
    return diagnostics


def render_diagnostics(diagnostics: list[Diagnostic]) -> str:
    """Render diagnostics in a stable, CLI-friendly format."""
    if not diagnostics:
        return "No doctor findings.\n"
    return "".join(
        f"{diagnostic.level}: {diagnostic.path}: {diagnostic.message}\n"
        for diagnostic in diagnostics
    )


def _diagnose_lockfile(
    project: ProjectConfig,
    diagnostics: list[Diagnostic],
) -> None:
    """Warn when a profile relies on automatic IDs."""
    if not project.auto_id_paths:
        return
    if not project.settings.id_lock_file.exists():
        diagnostics.append(
            Diagnostic(
                "warning",
                str(project.settings.id_lock_file),
                "automatic IDs are used; generate and commit this lockfile "
                "to keep public wire IDs stable.",
            )
        )
    diagnostics.extend(
        Diagnostic(
            "info",
            path,
            "ID is managed by lsh_devices.lock.toml.",
        )
        for path in project.auto_id_paths
    )


def _diagnose_semantic_define_overlap(
    project: ProjectConfig,
    diagnostics: list[Diagnostic],
) -> None:
    """Point raw expert defines back to the public semantic option."""
    for raw_define_path in project.raw_define_paths:
        define_name = raw_define_path.rsplit(".", 1)[-1]
        semantic_path = SEMANTIC_DEFINE_HINTS.get(define_name)
        if semantic_path is None:
            continue
        if raw_define_path.startswith("devices."):
            device_key = raw_define_path.split(".", 2)[1]
            target_path = f"devices.{device_key}.{semantic_path}"
        else:
            target_path = semantic_path
        diagnostics.append(
            Diagnostic(
                "info",
                raw_define_path,
                f"prefer {target_path} unless raw preprocessor text is needed.",
            )
        )


def _diagnose_fast_io(
    project: ProjectConfig,
    device: DeviceConfig,
    diagnostics: list[Diagnostic],
) -> None:
    """Warn when a Controllino profile disables fast I/O."""
    if "controllino" not in device.hardware_include.lower():
        return
    define_map = {
        define.name: define.value for define in merged_defines(project, device)
    }
    for define_name, resource_name in (
        ("CONFIG_USE_FAST_CLICKABLES", "buttons"),
        ("CONFIG_USE_FAST_ACTUATORS", "actuators"),
        ("CONFIG_USE_FAST_INDICATORS", "indicators"),
    ):
        if define_name not in define_map:
            diagnostics.append(
                Diagnostic(
                    "warning",
                    f"devices.{device.key}.features",
                    f"Controllino profile has fast I/O disabled for {resource_name}.",
                )
            )


def _diagnose_clicks(device: DeviceConfig, diagnostics: list[Diagnostic]) -> None:
    """Warn about network-click and threshold choices worth reviewing."""
    for clickable in device.clickables:
        _diagnose_network_click(device, clickable, diagnostics)
        _diagnose_threshold_gap(device, clickable, diagnostics)


def _diagnose_network_click(
    device: DeviceConfig,
    clickable: ClickableConfig,
    diagnostics: list[Diagnostic],
) -> None:
    """Warn when a network click deliberately has no local fallback."""
    for action_name, action in (
        ("long", clickable.long),
        ("super_long", clickable.super_long),
    ):
        if action.network and action.fallback == "DO_NOTHING":
            diagnostics.append(
                Diagnostic(
                    "info",
                    f"devices.{device.key}.buttons.{clickable.name}.{action_name}",
                    "network action has fallback = do_nothing; verify the "
                    "button may legitimately do nothing when offline.",
                )
            )


def _diagnose_threshold_gap(
    device: DeviceConfig,
    clickable: ClickableConfig,
    diagnostics: list[Diagnostic],
) -> None:
    """Warn when long and super-long thresholds are close but still valid."""
    if not (clickable.long.enabled and clickable.super_long.enabled):
        return
    long_ms = clickable.long.time_ms or DEFAULT_LONG_CLICK_MS
    super_long_ms = clickable.super_long.time_ms or DEFAULT_SUPER_LONG_CLICK_MS
    gap_ms = super_long_ms - long_ms
    if gap_ms < MIN_RECOMMENDED_CLICK_THRESHOLD_GAP_MS:
        diagnostics.append(
            Diagnostic(
                "info",
                f"devices.{device.key}.buttons.{clickable.name}.super_long",
                f"super-long threshold is only {gap_ms} ms after long-click; "
                "verify the UX intentionally feels that tight.",
            )
        )
