"""Command-line interface for the static configuration generator."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import TYPE_CHECKING, cast

from .doctor import diagnose_project, render_diagnostics
from .emitter import generate
from .errors import ConfigError, fail
from .formatter import format_config_file
from .json_schema import project_schema_json, schema_json, write_project_schema
from .parser import parse_project
from .platformio import merged_defines, raw_build_flags, resolve_device_key
from .presets import PRESETS
from .scaffold import ScaffoldOptions, write_scaffold

if TYPE_CHECKING:
    from collections.abc import Sequence

    from .models import ProjectConfig


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    """Parse command-line arguments for the generator CLI."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "config",
        nargs="?",
        type=Path,
        help="path to the lsh-core TOML device configuration",
    )
    parser.add_argument(
        "--device",
        action="append",
        help="device key to generate; defaults to all devices",
    )
    parser.add_argument(
        "--check", action="store_true", help="fail if generated headers are stale"
    )
    parser.add_argument(
        "--list-devices",
        action="store_true",
        help="print configured device keys and exit",
    )
    parser.add_argument(
        "--print-platformio-defines",
        metavar="DEVICE",
        help="print CPP defines for one device",
    )
    parser.add_argument(
        "--print-json-schema",
        action="store_true",
        help="print the schema v2 JSON Schema and exit",
    )
    parser.add_argument(
        "--print-project-json-schema",
        action="store_true",
        help="print a JSON Schema specialized with names from this config",
    )
    parser.add_argument(
        "--write-vscode-schema",
        nargs="?",
        const=True,
        metavar="PATH",
        help=(
            "write a project-specific schema; defaults to "
            "<config-dir>/.vscode/lsh_devices.schema.json"
        ),
    )
    parser.add_argument(
        "--init-config",
        metavar="PATH",
        type=Path,
        help="write a guided starter TOML config and exit",
    )
    parser.add_argument(
        "--preset",
        choices=sorted(PRESETS),
        default="controllino-maxi/fast-msgpack",
        help="preset used by --init-config",
    )
    parser.add_argument(
        "--device-key",
        default="my_device",
        help="device key used by --init-config",
    )
    parser.add_argument(
        "--device-name",
        help="runtime device name used by --init-config; defaults to --device-key",
    )
    parser.add_argument(
        "--relays",
        type=int,
        default=4,
        help="number of relay actuators created by --init-config",
    )
    parser.add_argument(
        "--buttons",
        type=int,
        default=4,
        help="number of buttons created by --init-config",
    )
    parser.add_argument(
        "--indicators",
        type=int,
        default=1,
        help="number of indicators created by --init-config",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="allow --init-config to overwrite an existing TOML file",
    )
    parser.add_argument(
        "--no-vscode-schema",
        action="store_true",
        help="skip .vscode schema creation during --init-config",
    )
    parser.add_argument(
        "--doctor",
        action="store_true",
        help="print non-fatal configuration advice and exit",
    )
    parser.add_argument(
        "--format-config",
        action="store_true",
        help="rewrite the TOML file in canonical schema v2 order",
    )
    parser.add_argument(
        "--check-format",
        action="store_true",
        help="fail if the TOML file is not in canonical schema v2 order",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str]) -> int:
    """Run the generator CLI."""
    try:
        args = parse_args(argv)
        standalone_status = _handle_standalone_command(args)
        if standalone_status is not None:
            return standalone_status
        config_path = _required_config_path(args)
        format_status = _handle_format(args, config_path)
        if format_status is not None:
            return format_status
        project = parse_project(config_path)
        schema_status = _handle_project_schema_command(args, config_path)
        if schema_status is not None:
            return schema_status
        return _dispatch_project_command(args, project)
    except ConfigError as exc:
        sys.stderr.write(f"lsh static config error: {exc}\n")
        return 2


def _handle_standalone_command(args: argparse.Namespace) -> int | None:
    """Run commands that do not need an existing project TOML."""
    if args.init_config is not None:
        write_scaffold(
            cast("Path", args.init_config),
            ScaffoldOptions(
                preset=cast("str", args.preset),
                device_key=cast("str", args.device_key),
                device_name=cast("str | None", args.device_name),
                relays=cast("int", args.relays),
                buttons=cast("int", args.buttons),
                indicators=cast("int", args.indicators),
                force=cast("bool", args.force),
                write_schema=not cast("bool", args.no_vscode_schema),
            ),
        )
        return 0
    if args.print_json_schema:
        sys.stdout.write(schema_json())
        return 0
    return None


def _required_config_path(args: argparse.Namespace) -> Path:
    """Return the required config path for commands that operate on a project."""
    if args.config is None:
        msg = (
            "config path is required unless --print-json-schema or --init-config "
            "is used."
        )
        fail(msg)
    return cast("Path", args.config).resolve()


def _schema_output_path(raw_path: object) -> Path | None:
    """Resolve the optional --write-vscode-schema path."""
    if raw_path is True:
        return None
    if not isinstance(raw_path, str):
        fail("--write-vscode-schema expects an optional path.")
    return Path(raw_path).resolve()


def _handle_project_schema_command(
    args: argparse.Namespace,
    config_path: Path,
) -> int | None:
    """Run schema commands that require a valid existing TOML project."""
    if args.print_project_json_schema:
        sys.stdout.write(project_schema_json(config_path))
        return 0
    if args.write_vscode_schema is not None:
        schema_path = _schema_output_path(args.write_vscode_schema)
        write_project_schema(config_path, schema_path)
        return 0
    return None


def _handle_format(args: argparse.Namespace, config_path: Path) -> int | None:
    """Run the formatter subcommand when requested."""
    if not (args.format_config or args.check_format):
        return None
    stale = format_config_file(config_path, check=args.check_format)
    if args.check_format and stale:
        sys.stderr.write(f"stale-format: {config_path}\n")
        return 1
    if args.format_config:
        return 0
    return None


def _list_devices(project: ProjectConfig) -> int:
    """Print device keys in TOML order."""
    for key in project.devices:
        sys.stdout.write(f"{key}\n")
    return 0


def _dispatch_project_command(
    args: argparse.Namespace,
    project: ProjectConfig,
) -> int:
    """Run the selected project-level command."""
    if args.list_devices:
        return _list_devices(project)
    if args.print_platformio_defines:
        return _print_platformio_defines(project, args.print_platformio_defines)
    if args.doctor:
        sys.stdout.write(render_diagnostics(diagnose_project(project)))
        return 0
    selected = _selected_devices(project, args.device)
    return generate(project, selected, check=args.check)


def _print_platformio_defines(project: ProjectConfig, requested_device: str) -> int:
    """Print PlatformIO defines and raw flags for one device."""
    device = project.devices[resolve_device_key(project, requested_device)]
    for define in merged_defines(project, device):
        rendered_define = (
            define.name if define.value is None else f"{define.name}={define.value}"
        )
        sys.stdout.write(f"{rendered_define}\n")
    for flag in raw_build_flags(project, device):
        sys.stdout.write(f"{flag}\n")
    return 0


def _selected_devices(
    project: ProjectConfig,
    requested_devices: Sequence[str] | None,
) -> list[str]:
    """Resolve CLI device selectors."""
    if requested_devices is None:
        return list(project.devices)
    return [resolve_device_key(project, device) for device in requested_devices]
