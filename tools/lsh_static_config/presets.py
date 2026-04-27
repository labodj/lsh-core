"""Named board/profile presets for public schema v2."""

from __future__ import annotations

from dataclasses import dataclass

from .errors import fail


@dataclass(frozen=True)
class Preset:
    """Resolved public preset defaults."""

    name: str
    hardware_include: str
    debug_serial: str
    bridge_serial: str
    codec: str
    fast_io: bool
    controllino_pin_aliases: bool


PRESETS = {
    "controllino-maxi/fast-msgpack": Preset(
        name="controllino-maxi/fast-msgpack",
        hardware_include="Controllino.h",
        debug_serial="Serial",
        bridge_serial="Serial2",
        codec="msgpack",
        fast_io=True,
        controllino_pin_aliases=True,
    ),
    "controllino-maxi/fast-json": Preset(
        name="controllino-maxi/fast-json",
        hardware_include="Controllino.h",
        debug_serial="Serial",
        bridge_serial="Serial2",
        codec="json",
        fast_io=True,
        controllino_pin_aliases=True,
    ),
    "arduino-generic/msgpack": Preset(
        name="arduino-generic/msgpack",
        hardware_include="Arduino.h",
        debug_serial="Serial",
        bridge_serial="Serial",
        codec="msgpack",
        fast_io=False,
        controllino_pin_aliases=False,
    ),
    "arduino-generic/json": Preset(
        name="arduino-generic/json",
        hardware_include="Arduino.h",
        debug_serial="Serial",
        bridge_serial="Serial",
        codec="json",
        fast_io=False,
        controllino_pin_aliases=False,
    ),
}


def resolve_preset(name: str) -> Preset:
    """Resolve a public preset name or fail with the current choices."""
    preset = PRESETS.get(name.lower())
    if preset is None:
        choices = ", ".join(sorted(PRESETS))
        fail(f"preset must be one of: {choices}.")
    return preset
