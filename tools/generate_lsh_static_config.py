#!/usr/bin/env python3
"""Compatibility entry point for the TOML static configuration generator."""

from __future__ import annotations

import sys
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.lsh_static_config import (
    ActuatorConfig,
    ClickableConfig,
    ClickAction,
    ConfigError,
    DefineValue,
    DeviceConfig,
    GeneratorSettings,
    IndicatorConfig,
    ProjectConfig,
    StaticProfileData,
    generate,
    generated_files,
    infer_device_from_env,
    main,
    merged_defines,
    parse_args,
    parse_project,
    raw_build_flags,
    render_device_config,
    render_static_config,
    render_user_config,
    resolve_device_key,
    write_if_changed,
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

if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
