"""Compatibility facade for generated action renderers.

The implementation is intentionally split by responsibility:

- `click_actions` owns the action bodies and cold dispatchers.
- `click_scan` owns the button polling hot path.
- `runtime_paths` assembles the generated helpers consumed by static_config.

Keeping this module as a facade avoids breaking external tooling that imported
`render_generated_action_accessors` while keeping the generator internals small.
"""

from __future__ import annotations

from .runtime_paths import render_generated_action_accessors

__all__ = ["render_generated_action_accessors"]
