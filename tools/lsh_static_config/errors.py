"""User-facing generator errors."""

from __future__ import annotations

from typing import NoReturn


class ConfigError(RuntimeError):
    """Raised when the TOML profile cannot produce a safe static build."""


def fail(message: str) -> NoReturn:
    """Raise a user-facing configuration error."""
    raise ConfigError(message)
