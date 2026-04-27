"""One-shot converter from legacy lsh-core TOML profiles to schema v2."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import TYPE_CHECKING

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.lsh_static_config.errors import ConfigError, fail
from tools.lsh_static_config.legacy_migration import migrate_file

if TYPE_CHECKING:
    from collections.abc import Sequence


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    """Parse migration CLI arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("config", type=Path, help="legacy lsh_devices.toml path")
    parser.add_argument(
        "--output",
        type=Path,
        help="write the migrated schema v2 profile to this path",
    )
    parser.add_argument(
        "--write",
        action="store_true",
        help="replace the input file in place",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str]) -> int:
    """Run the one-shot migration tool."""
    try:
        args = parse_args(argv)
        if args.write and args.output is not None:
            fail("--write and --output are mutually exclusive.")
        migrated = migrate_file(args.config.resolve())
        return _emit_migration(args, migrated)
    except ConfigError as exc:
        sys.stderr.write(f"lsh migration error: {exc}\n")
        return 2


def _emit_migration(args: argparse.Namespace, migrated: str) -> int:
    """Write migrated TOML to the requested destination."""
    if args.write:
        args.config.write_text(migrated, encoding="utf-8")
    elif args.output is not None:
        args.output.write_text(migrated, encoding="utf-8")
    else:
        sys.stdout.write(migrated)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
