#!/usr/bin/env python3
"""Command-line entry point for the TOML static configuration generator."""

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
    ScaffoldOptions,
    StaticProfileData,
    default_vscode_schema_path,
    define_needs_escaped_build_flag,
    diagnose_project,
    format_config_file,
    generate,
    generated_files,
    infer_device_from_env,
    main,
    merged_defines,
    parse_args,
    parse_project,
    project_schema,
    project_schema_json,
    raw_build_flags,
    render_device_config,
    render_diagnostics,
    render_escaped_build_flag_define,
    render_scaffold,
    render_static_config,
    render_user_config,
    resolve_device_key,
    schema,
    schema_json,
    write_if_changed,
    write_project_schema,
    write_scaffold,
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
    "ScaffoldOptions",
    "StaticProfileData",
    "default_vscode_schema_path",
    "define_needs_escaped_build_flag",
    "diagnose_project",
    "format_config_file",
    "generate",
    "generated_files",
    "infer_device_from_env",
    "main",
    "merged_defines",
    "parse_args",
    "parse_project",
    "project_schema",
    "project_schema_json",
    "raw_build_flags",
    "render_device_config",
    "render_diagnostics",
    "render_escaped_build_flag_define",
    "render_scaffold",
    "render_static_config",
    "render_user_config",
    "resolve_device_key",
    "schema",
    "schema_json",
    "write_if_changed",
    "write_project_schema",
    "write_scaffold",
]

if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
