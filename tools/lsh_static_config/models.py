"""Normalized TOML model objects used by parsing and rendering."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import TYPE_CHECKING, TypeAlias

if TYPE_CHECKING:
    from pathlib import Path

TomlValue: TypeAlias = object
TomlTable: TypeAlias = dict[str, TomlValue]
TomlArray: TypeAlias = list[TomlValue]
DefineMap: TypeAlias = dict[str, TomlValue]


@dataclass(frozen=True)
class DefineValue:
    """One preprocessor define passed to PlatformIO."""

    name: str
    value: str | None = None


@dataclass
class ClickAction:
    """Normalized short/long/super-long action for one clickable."""

    enabled: bool = False
    targets: list[str] = field(default_factory=list)
    click_type: str = "NORMAL"
    network: bool = False
    fallback: str = "LOCAL_FALLBACK"
    time_ms: int | None = None


@dataclass
class ActuatorConfig:
    """Normalized actuator declaration from TOML."""

    name: str
    actuator_id: int
    pin: str
    default_state: bool = False
    protected: bool = False
    auto_off_ms: int | None = None


@dataclass
class ClickableConfig:
    """Normalized clickable declaration with all resolved click actions."""

    name: str
    clickable_id: int
    pin: str
    short_targets: list[str] = field(default_factory=list)
    short_enabled: bool = False
    long: ClickAction = field(default_factory=ClickAction)
    super_long: ClickAction = field(default_factory=ClickAction)


@dataclass
class IndicatorConfig:
    """Normalized indicator declaration from TOML."""

    name: str
    pin: str
    targets: list[str] = field(default_factory=list)
    mode: str = "ANY"


@dataclass
class DeviceConfig:
    """Complete generated profile for a single firmware environment."""

    key: str
    device_name: str
    build_macro: str
    hardware_include: str
    com_serial: str
    debug_serial: str
    config_include: str
    static_config_include: str
    disable_rtc: bool = False
    disable_eth: bool = False
    defines: DefineMap = field(default_factory=dict)
    raw_build_flags: list[str] = field(default_factory=list)
    actuators: list[ActuatorConfig] = field(default_factory=list)
    clickables: list[ClickableConfig] = field(default_factory=list)
    indicators: list[IndicatorConfig] = field(default_factory=list)


@dataclass
class GeneratorSettings:
    """Generator-level output paths and naming conventions."""

    output_dir: Path
    config_dir: str = "lsh_configs"
    user_config_header: str = "lsh_user_config.hpp"


@dataclass
class ProjectConfig:
    """Parsed TOML project containing all device profiles."""

    source_path: Path
    settings: GeneratorSettings
    common_defines: DefineMap
    common_raw_build_flags: list[str]
    devices: dict[str, DeviceConfig]


@dataclass(frozen=True)
class StaticProfileData:
    """Precomputed generated-code data for one static device profile."""

    actuator_ids: list[int]
    clickable_ids: list[int]
    actuator_indexes: dict[str, int]
    auto_off_indexes: list[int]
    long_types: list[str]
    super_long_types: list[str]
    long_times: list[int]
    super_long_times: list[int]
    short_link_sets: list[list[int]]
    long_link_sets: list[list[int]]
    super_long_link_sets: list[list[int]]
    indicator_link_sets: list[list[int]]
    short_link_counts: list[int]
    long_link_counts: list[int]
    super_long_link_counts: list[int]
    indicator_link_counts: list[int]
    indicator_modes: list[str]
    short_links: int
    long_links: int
    super_long_links: int
    indicator_links: int
    active_network_clicks: int
