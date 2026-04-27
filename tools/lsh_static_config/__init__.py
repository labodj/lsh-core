"""Public API for the lsh-core TOML static configuration generator."""

from __future__ import annotations

from .cli import main, parse_args
from .doctor import diagnose_project, render_diagnostics
from .emitter import generate, write_if_changed
from .errors import ConfigError
from .formatter import format_config_file
from .headers import (
    generated_files,
    render_device_config,
    render_static_config,
    render_user_config,
)
from .json_schema import (
    default_vscode_schema_path,
    project_schema,
    project_schema_json,
    schema,
    schema_json,
    write_project_schema,
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
    define_needs_escaped_build_flag,
    infer_device_from_env,
    merged_defines,
    raw_build_flags,
    render_escaped_build_flag_define,
    resolve_device_key,
)
from .scaffold import ScaffoldOptions, render_scaffold, write_scaffold

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
