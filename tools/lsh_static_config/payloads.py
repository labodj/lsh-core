"""Serial-ready DEVICE_DETAILS payload generation."""

from __future__ import annotations

import json
from pathlib import Path
from typing import TYPE_CHECKING

from .constants import (
    MSGPACK_FIXARRAY_MAX_LENGTH,
    MSGPACK_FIXSTR_MAX_LENGTH,
    MSGPACK_FRAME_END,
    MSGPACK_FRAME_ESCAPE,
    MSGPACK_FRAME_ESCAPED_END,
    MSGPACK_FRAME_ESCAPED_ESCAPE,
    MSGPACK_POSITIVE_FIXINT_MAX,
    PROTOCOL_DEVICE_DETAILS,
    UINT8_MAX,
    UINT16_MAX,
    UINT32_MAX,
    WIRE_PROTOCOL_MAJOR_RE,
)
from .errors import fail

if TYPE_CHECKING:
    from collections.abc import Sequence

    from .models import DeviceConfig


def wire_protocol_major() -> int:
    """Read the generated wire-protocol major from the C++ protocol contract."""
    protocol_path = (
        Path(__file__).resolve().parents[2]
        / "src"
        / "communication"
        / "constants"
        / "protocol.hpp"
    )
    try:
        protocol_text = protocol_path.read_text(encoding="utf-8")
    except OSError as exc:
        fail(f"Cannot read {protocol_path}: {exc}")

    match = WIRE_PROTOCOL_MAJOR_RE.search(protocol_text)
    if match is None:
        fail(f"Cannot find WIRE_PROTOCOL_MAJOR in {protocol_path}.")
    return int(match.group(1))


def details_document(device: DeviceConfig) -> dict[str, object]:
    """Build the canonical DEVICE_DETAILS payload in protocol key order."""
    return {
        "p": PROTOCOL_DEVICE_DETAILS,
        "v": wire_protocol_major(),
        "n": device.device_name,
        "a": [actuator.actuator_id for actuator in device.actuators],
        "b": [clickable.clickable_id for clickable in device.clickables],
    }


def encode_json_details_payload(device: DeviceConfig) -> list[int]:
    """Return newline-delimited JSON DEVICE_DETAILS serial bytes."""
    payload = json.dumps(
        details_document(device),
        ensure_ascii=False,
        separators=(",", ":"),
    ).encode("utf-8")
    return [*payload, ord("\n")]


def msgpack_uint(value: int) -> list[int]:
    """Encode one non-negative integer using the shortest MsgPack form."""
    if value < 0 or value > UINT32_MAX:
        fail(f"MsgPack unsigned integer out of range: {value}.")
    if value <= MSGPACK_POSITIVE_FIXINT_MAX:
        return [value]
    if value <= UINT8_MAX:
        return [0xCC, value]
    if value <= UINT16_MAX:
        return [0xCD, (value >> 8) & UINT8_MAX, value & UINT8_MAX]
    return [
        0xCE,
        (value >> 24) & UINT8_MAX,
        (value >> 16) & UINT8_MAX,
        (value >> 8) & UINT8_MAX,
        value & UINT8_MAX,
    ]


def msgpack_string(value: str) -> list[int]:
    """Encode one UTF-8 string using the shortest MsgPack string header."""
    encoded = value.encode("utf-8")
    length = len(encoded)
    if length <= MSGPACK_FIXSTR_MAX_LENGTH:
        return [0xA0 | length, *encoded]
    if length <= UINT8_MAX:
        return [0xD9, length, *encoded]
    if length <= UINT16_MAX:
        return [0xDA, (length >> 8) & UINT8_MAX, length & UINT8_MAX, *encoded]
    fail("Device name is too long for the generated MsgPack details payload.")
    raise AssertionError


def msgpack_uint_array(values: Sequence[int]) -> list[int]:
    """Encode one MsgPack array of unsigned integers."""
    length = len(values)
    if length <= MSGPACK_FIXARRAY_MAX_LENGTH:
        encoded = [0x90 | length]
    elif length <= UINT16_MAX:
        encoded = [0xDC, (length >> 8) & UINT8_MAX, length & UINT8_MAX]
    else:
        fail("Generated MsgPack array is too long.")

    for value in values:
        encoded.extend(msgpack_uint(value))
    return encoded


def encode_msgpack_details_payload(device: DeviceConfig) -> list[int]:
    """Return raw MsgPack DEVICE_DETAILS bytes without serial framing."""
    payload: list[int] = [0x85]
    payload.extend(msgpack_string("p"))
    payload.extend(msgpack_uint(PROTOCOL_DEVICE_DETAILS))
    payload.extend(msgpack_string("v"))
    payload.extend(msgpack_uint(wire_protocol_major()))
    payload.extend(msgpack_string("n"))
    payload.extend(msgpack_string(device.device_name))
    payload.extend(msgpack_string("a"))
    payload.extend(
        msgpack_uint_array([actuator.actuator_id for actuator in device.actuators])
    )
    payload.extend(msgpack_string("b"))
    payload.extend(
        msgpack_uint_array([clickable.clickable_id for clickable in device.clickables])
    )
    return payload


def frame_msgpack_payload(payload: Sequence[int]) -> list[int]:
    """Wrap raw MsgPack bytes in the serial delimiter-and-escape transport."""
    framed = [MSGPACK_FRAME_END]
    for byte in payload:
        if byte == MSGPACK_FRAME_END:
            framed.extend([MSGPACK_FRAME_ESCAPE, MSGPACK_FRAME_ESCAPED_END])
        elif byte == MSGPACK_FRAME_ESCAPE:
            framed.extend([MSGPACK_FRAME_ESCAPE, MSGPACK_FRAME_ESCAPED_ESCAPE])
        else:
            framed.append(byte)
    framed.append(MSGPACK_FRAME_END)
    return framed


def encode_serial_msgpack_details_payload(device: DeviceConfig) -> list[int]:
    """Return serial-ready framed MsgPack DEVICE_DETAILS bytes."""
    return frame_msgpack_payload(encode_msgpack_details_payload(device))


def render_byte_array(name: str, values: Sequence[int]) -> list[str]:
    """Render one generated byte array with compact hex literals."""
    if len(values) > UINT16_MAX:
        fail(f"{name} cannot exceed {UINT16_MAX} bytes.")

    lines = [f"const uint8_t {name}[] LSH_STATIC_CONFIG_PROGMEM = {{"]
    for offset in range(0, len(values), 12):
        chunk = values[offset : offset + 12]
        suffix = "," if offset + len(chunk) < len(values) else ""
        rendered = ", ".join(f"0x{value:02X}U" for value in chunk)
        lines.append(f"    {rendered}{suffix}")
    lines.append("};")
    return lines


def render_static_payload_arrays(device: DeviceConfig) -> list[str]:
    """Render serial-ready DEVICE_DETAILS payloads for both supported codecs."""
    lines = ["// clang-format off"]
    lines.extend(
        render_byte_array(
            "DETAILS_JSON_PAYLOAD",
            encode_json_details_payload(device),
        )
    )
    lines.append("")
    lines.extend(
        render_byte_array(
            "DETAILS_MSGPACK_PAYLOAD",
            encode_serial_msgpack_details_payload(device),
        )
    )
    lines.append("// clang-format on")
    return lines


def render_static_payload_writer_helper() -> list[str]:
    """Render a direct PROGMEM/RAM byte writer for generated static payloads."""
    return [
        "constexpr uint16_t LSH_STATIC_CONFIG_UNROLLED_PAYLOAD_LIMIT = 128U;",
        "",
        (
            "template <uint16_t ByteIndex, uint16_t PayloadSize> "
            "struct GeneratedPayloadByteWriter"
        ),
        "{",
        "    static auto write(const uint8_t (&payload)[PayloadSize]) noexcept -> bool",
        "    {",
        (
            "        return CONFIG_COM_SERIAL->HardwareSerial::write("
            "LSH_STATIC_CONFIG_READ_BYTE(&payload[ByteIndex])) == 1U &&"
        ),
        (
            "               GeneratedPayloadByteWriter<static_cast<uint16_t>("
            "ByteIndex + 1U), PayloadSize>::write(payload);"
        ),
        "    }",
        "};",
        "",
        (
            "template <uint16_t PayloadSize> "
            "struct GeneratedPayloadByteWriter<PayloadSize, PayloadSize>"
        ),
        "{",
        "    static auto write(const uint8_t (&payload)[PayloadSize]) noexcept -> bool",
        "    {",
        "        static_cast<void>(payload);",
        "        return true;",
        "    }",
        "};",
        "",
        (
            "template <uint16_t PayloadSize> "
            "auto writeGeneratedPayloadLoop(const uint8_t (&payload)[PayloadSize]) "
            "noexcept -> bool"
        ),
        "{",
        "    for (uint16_t byteIndex = 0U; byteIndex < PayloadSize; ++byteIndex)",
        "    {",
        (
            "        if (CONFIG_COM_SERIAL->HardwareSerial::write("
            "LSH_STATIC_CONFIG_READ_BYTE(&payload[byteIndex])) != 1U)"
        ),
        "        {",
        "            return false;",
        "        }",
        "    }",
        "    return true;",
        "}",
        "",
        (
            "template <uint16_t PayloadSize> "
            "auto writeGeneratedPayload(const uint8_t (&payload)[PayloadSize]) "
            "noexcept -> bool"
        ),
        "{",
        (
            '    static_assert(PayloadSize > 0U, "Generated static payloads '
            'must not be empty.");'
        ),
        "    if constexpr (PayloadSize <= LSH_STATIC_CONFIG_UNROLLED_PAYLOAD_LIMIT)",
        "    {",
        "        return GeneratedPayloadByteWriter<0U, PayloadSize>::write(payload);",
        "    }",
        "    return writeGeneratedPayloadLoop(payload);",
        "}",
    ]
