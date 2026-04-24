"""Public API for the lsh-core TOML static configuration generator."""

from __future__ import annotations

from .cli import main, parse_args
from .emitter import generate, write_if_changed
from .errors import ConfigError
from .headers import (
    generated_files,
    render_device_config,
    render_static_config,
    render_user_config,
)
from .models import (
    ActuatorConfig,
    ClickableConfig,
    ClickAction,
    DefineValue,
    DeviceConfig,
    GeneratorSettings,
    IndicatorConfig,
    ProjectConfig,
    StaticProfileData,
)
from .parser import parse_project
from .platformio import (
    infer_device_from_env,
    merged_defines,
    raw_build_flags,
    resolve_device_key,
)

__all__ = [
    "ActuatorConfig",
    "ClickAction",
    "ClickableConfig",
    "ConfigError",
    "DefineValue",
    "DeviceConfig",
    "GeneratorSettings",
    "IndicatorConfig",
    "ProjectConfig",
    "StaticProfileData",
    "generate",
    "generated_files",
    "infer_device_from_env",
    "main",
    "merged_defines",
    "parse_args",
    "parse_project",
    "raw_build_flags",
    "render_device_config",
    "render_static_config",
    "render_user_config",
    "resolve_device_key",
    "write_if_changed",
]
