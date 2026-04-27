"""Filesystem emission for generated headers."""

from __future__ import annotations

import sys
from typing import TYPE_CHECKING

from .headers import generated_files

if TYPE_CHECKING:
    from collections.abc import Sequence
    from pathlib import Path

    from .models import ProjectConfig


def write_if_changed(path: Path, content: str, *, check: bool) -> bool:
    """Write one generated file, or report staleness in check mode."""
    existing = path.read_text(encoding="utf-8") if path.exists() else None
    if existing == content:
        return False
    if check:
        sys.stderr.write(f"stale: {path}\n")
        return True
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    return True


def generate(
    project: ProjectConfig,
    selected_devices: Sequence[str],
    *,
    check: bool = False,
) -> int:
    """Generate headers for the selected devices, optionally as a stale check."""
    changed = write_if_changed(
        project.settings.id_lock_file,
        project.id_lock_content,
        check=check,
    )
    for path, content in generated_files(project, selected_devices).items():
        changed |= write_if_changed(path, content, check=check)
    return 1 if check and changed else 0
