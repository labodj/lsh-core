"""Shared constants and accepted TOML vocabulary for the generator."""

from __future__ import annotations

import re

UINT8_MAX = 255
UINT16_MAX = 65535
UINT32_MAX = 4294967295
DEFAULT_LONG_CLICK_MS = 400
DEFAULT_SUPER_LONG_CLICK_MS = 1000
MAX_INLINE_SUM_TERMS = 2
CLANG_FORMAT_COLUMN_LIMIT = 140
PROTOCOL_DEVICE_DETAILS = 1
MSGPACK_POSITIVE_FIXINT_MAX = 0x7F
MSGPACK_FIXSTR_MAX_LENGTH = 31
MSGPACK_FIXARRAY_MAX_LENGTH = 15

IDENTIFIER_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
MACRO_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
DURATION_RE = re.compile(r"^\s*(\d+)\s*(ms|s|m|h)?\s*$", re.IGNORECASE)
WIRE_PROTOCOL_MAJOR_RE = re.compile(r"WIRE_PROTOCOL_MAJOR\s*=\s*(\d+)U")
TRANSPORT_BYTE_RE = re.compile(
    r"(MSGPACK_FRAME_(?:END|ESCAPE|ESCAPED_END|ESCAPED_ESCAPE))\s*=\s*(0x[0-9A-Fa-f]+|\d+)U"
)
SAFE_CPP_EXPR_FORBIDDEN = set("{};#\"'\n\r")

LONG_CLICK_TYPES = {
    "normal": "NORMAL",
    "on_only": "ON_ONLY",
    "off_only": "OFF_ONLY",
}

SUPER_LONG_CLICK_TYPES = {
    "normal": "NORMAL",
    "selective": "SELECTIVE",
}

NETWORK_FALLBACKS = {
    "local": "LOCAL_FALLBACK",
    "do_nothing": "DO_NOTHING",
}

INDICATOR_MODES = {
    "any": "ANY",
    "all": "ALL",
    "majority": "MAJORITY",
}
