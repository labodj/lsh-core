"""Command-line interface for the static configuration generator."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import TYPE_CHECKING

from .emitter import generate
from .errors import ConfigError
from .parser import parse_project
from .platformio import merged_defines, raw_build_flags, resolve_device_key

if TYPE_CHECKING:
    from collections.abc import Sequence


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    """Parse command-line arguments for the generator CLI."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "config", type=Path, help="path to the lsh-core TOML device configuration"
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
    return parser.parse_args(argv)


def main(argv: Sequence[str]) -> int:
    """Run the generator CLI."""
    try:
        args = parse_args(argv)
        project = parse_project(args.config.resolve())

        if args.list_devices:
            for key in project.devices:
                sys.stdout.write(f"{key}\n")
            return 0

        if args.print_platformio_defines:
            device = project.devices[
                resolve_device_key(project, args.print_platformio_defines)
            ]
            for define in merged_defines(project, device):
                rendered_define = (
                    define.name
                    if define.value is None
                    else f"{define.name}={define.value}"
                )
                sys.stdout.write(f"{rendered_define}\n")
            for flag in raw_build_flags(project, device):
                sys.stdout.write(f"{flag}\n")
            return 0

        selected = (
            [resolve_device_key(project, device) for device in args.device]
            if args.device
            else list(project.devices)
        )
        return generate(project, selected, check=args.check)
    except ConfigError as exc:
        sys.stderr.write(f"lsh static config error: {exc}\n")
        return 2
