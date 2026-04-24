/**
 * @file    serializer.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the data serialization logic for sending messages to `lsh-bridge`.
 *
 * Copyright 2026 Jacopo Labardi
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "communication/serializer.hpp"

#include <stddef.h>

#include "communication/constants/config.hpp"
#include "communication/constants/protocol.hpp"
#include "communication/bridge_serial.hpp"
#include "communication/msgpack_serial_framing.hpp"
#include "config/static_config.hpp"
#include "device/actuator_manager.hpp"
#include "device/clickable_manager.hpp"
#include "internal/user_config_bridge.hpp"
#include "peripherals/input/clickable.hpp"
#include "peripherals/output/actuator.hpp"
#include "util/debug/debug.hpp"

namespace
{
[[nodiscard]] auto writeSerialByte(uint8_t byte) -> bool
{
    return CONFIG_COM_SERIAL->HardwareSerial::write(byte) == 1U;
}

template <size_t Size> [[nodiscard]] auto writeLiteral(const char (&literal)[Size]) -> bool
{
    static_assert(Size > 0U, "String literal must include a null terminator.");
    for (size_t index = 0U; index < (Size - 1U); ++index)
    {
        if (!writeSerialByte(static_cast<uint8_t>(literal[index])))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] auto finishSuccessfulPayload() -> bool
{
    if constexpr (constants::bridgeSerial::COM_SERIAL_FLUSH_AFTER_SEND)
    {
        CONFIG_COM_SERIAL->HardwareSerial::flush();
    }
    BridgeSerial::updateLastSentTime();
    return true;
}

[[nodiscard]] auto writeStaticPayload(constants::payloads::StaticType payloadType) -> bool
{
    using constants::payloads::StaticType;
#ifdef CONFIG_MSG_PACK
    switch (payloadType)
    {
    case StaticType::BOOT:
        return writeSerialByte(0xC0U) && writeSerialByte(0x81U) && writeSerialByte(0xA1U) && writeSerialByte(0x70U) &&
               writeSerialByte(0x04U) && writeSerialByte(0xC0U);
    case StaticType::PING_:
        return writeSerialByte(0xC0U) && writeSerialByte(0x81U) && writeSerialByte(0xA1U) && writeSerialByte(0x70U) &&
               writeSerialByte(0x05U) && writeSerialByte(0xC0U);
    default:
        return false;
    }
#else
    switch (payloadType)
    {
    case StaticType::BOOT:
        return writeLiteral("{\"p\":4}\n");
    case StaticType::PING_:
        return writeLiteral("{\"p\":5}\n");
    default:
        return false;
    }
#endif
}

#ifdef CONFIG_MSG_PACK
[[nodiscard]] auto writeMsgPackFrameByte(uint8_t byte) -> bool
{
    using namespace lsh::core::transport;
    if (byte == MSGPACK_FRAME_END)
    {
        return writeSerialByte(MSGPACK_FRAME_ESCAPE) && writeSerialByte(MSGPACK_FRAME_ESCAPED_END);
    }

    if (byte == MSGPACK_FRAME_ESCAPE)
    {
        return writeSerialByte(MSGPACK_FRAME_ESCAPE) && writeSerialByte(MSGPACK_FRAME_ESCAPED_ESCAPE);
    }

    return writeSerialByte(byte);
}

[[nodiscard]] auto writeMsgPackUint(uint8_t value) -> bool
{
    if (value <= 0x7FU)
    {
        return writeMsgPackFrameByte(value);
    }
    return writeMsgPackFrameByte(0xCCU) && writeMsgPackFrameByte(value);
}

[[nodiscard]] auto writeMsgPackKey(char key) -> bool
{
    return writeMsgPackFrameByte(0xA1U) && writeMsgPackFrameByte(static_cast<uint8_t>(key));
}

[[nodiscard]] auto writeMsgPackArrayHeader(uint8_t elementCount) -> bool
{
    if (elementCount < 16U)
    {
        return writeMsgPackFrameByte(static_cast<uint8_t>(0x90U | elementCount));
    }
    return writeMsgPackFrameByte(0xDCU) && writeMsgPackFrameByte(0x00U) && writeMsgPackFrameByte(elementCount);
}

[[nodiscard]] auto beginMsgPackFrame() -> bool
{
    return writeSerialByte(lsh::core::transport::MSGPACK_FRAME_END);
}

[[nodiscard]] auto endMsgPackFrame() -> bool
{
    return writeSerialByte(lsh::core::transport::MSGPACK_FRAME_END);
}

[[nodiscard]] auto writeMsgPackActuatorsStatePayload() -> bool
{
    using lsh::core::protocol::Command;
    constexpr uint8_t byteCount = CONFIG_PACKED_ACTUATOR_STATE_BYTES;
    if (!beginMsgPackFrame() || !writeMsgPackFrameByte(0x82U) || !writeMsgPackKey('p') ||
        !writeMsgPackUint(static_cast<uint8_t>(Command::ACTUATORS_STATE)) || !writeMsgPackKey('s') || !writeMsgPackArrayHeader(byteCount))
    {
        return false;
    }

    for (uint8_t byteIndex = 0U; byteIndex < byteCount; ++byteIndex)
    {
        if (!writeMsgPackUint(Actuators::packedActuatorStates[byteIndex]))
        {
            return false;
        }
    }

    return endMsgPackFrame();
}

#if CONFIG_USE_NETWORK_CLICKS
[[nodiscard]] auto writeMsgPackNetworkClickPayload(uint8_t command, uint8_t protocolClickType, uint8_t clickableId, uint8_t correlationId)
    -> bool
{
    return beginMsgPackFrame() && writeMsgPackFrameByte(0x84U) && writeMsgPackKey('p') && writeMsgPackUint(command) &&
           writeMsgPackKey('t') && writeMsgPackUint(protocolClickType) && writeMsgPackKey('i') && writeMsgPackUint(clickableId) &&
           writeMsgPackKey('c') && writeMsgPackUint(correlationId) && endMsgPackFrame();
}
#endif
#else
[[nodiscard]] auto writeUint8Decimal(uint8_t value) -> bool
{
    if (value >= 100U)
    {
        const uint8_t hundreds = static_cast<uint8_t>(value / 100U);
        value = static_cast<uint8_t>(value - (hundreds * 100U));
        const uint8_t tens = static_cast<uint8_t>(value / 10U);
        return writeSerialByte(static_cast<uint8_t>('0' + hundreds)) && writeSerialByte(static_cast<uint8_t>('0' + tens)) &&
               writeSerialByte(static_cast<uint8_t>('0' + (value - (tens * 10U))));
    }

    if (value >= 10U)
    {
        const uint8_t tens = static_cast<uint8_t>(value / 10U);
        return writeSerialByte(static_cast<uint8_t>('0' + tens)) && writeSerialByte(static_cast<uint8_t>('0' + (value - (tens * 10U))));
    }

    return writeSerialByte(static_cast<uint8_t>('0' + value));
}

[[nodiscard]] auto writeJsonActuatorsStatePayload() -> bool
{
    if (!writeLiteral("{\"p\":2,\"s\":["))
    {
        return false;
    }

    constexpr uint8_t byteCount = CONFIG_PACKED_ACTUATOR_STATE_BYTES;
    for (uint8_t byteIndex = 0U; byteIndex < byteCount; ++byteIndex)
    {
        if (byteIndex != 0U && !writeSerialByte(static_cast<uint8_t>(',')))
        {
            return false;
        }
        if (!writeUint8Decimal(Actuators::packedActuatorStates[byteIndex]))
        {
            return false;
        }
    }

    return writeLiteral("]}\n");
}

#if CONFIG_USE_NETWORK_CLICKS
[[nodiscard]] auto writeJsonNetworkClickPayload(uint8_t command, uint8_t protocolClickType, uint8_t clickableId, uint8_t correlationId)
    -> bool
{
    return writeLiteral("{\"p\":") && writeUint8Decimal(command) && writeLiteral(",\"t\":") && writeUint8Decimal(protocolClickType) &&
           writeLiteral(",\"i\":") && writeUint8Decimal(clickableId) && writeLiteral(",\"c\":") && writeUint8Decimal(correlationId) &&
           writeLiteral("}\n");
}
#endif
#endif
}  // namespace

namespace Serializer
{
using namespace Debug;
using lsh::core::protocol::Command;

/**
 * @brief Send one compile-time pre-serialized static control payload.
 *
 * @param payloadType type of the payload.
 */
auto serializeStaticJson(constants::payloads::StaticType payloadType) -> bool
{
    using constants::payloads::StaticType;
    if (payloadType == StaticType::PING_ && !BridgeSerial::canPing())
    {
        return false;
    }

    if (writeStaticPayload(payloadType))
    {
        return finishSuccessfulPayload();
    }

    return false;
}

/**
 * @brief Send the generated device-details payload using the active serial codec.
 */
auto serializeDetails() -> bool
{
    DP_CONTEXT();
    if (!lsh::core::static_config::writeDetailsPayload())
    {
        return false;
    }

    return finishSuccessfulPayload();
}

/**
 * @brief Send an actuator-state payload with a bitpacked byte array.
 * @details The actuator manager keeps a packed state shadow in protocol order,
 *          so this path only appends already-built bytes to the outbound payload.
 *          JSON output format: {"p":2,"s":[byte0,byte1,...]}.
 */
auto serializeActuatorsState() -> bool
{
    DP_CONTEXT();
#ifdef CONFIG_MSG_PACK
    if (!writeMsgPackActuatorsStatePayload())
#else
    if (!writeJsonActuatorsStatePayload())
#endif
    {
        return false;
    }

    return finishSuccessfulPayload();
}

/**
 * @brief Send a network click request/confirmation payload using the active serial codec.
 * @details Uses NETWORK_CLICK_REQUEST (p:3) for network click requests.
 *          Example request:  {"p":3,"t":1,"i":7,"c":42}
 *
 * @param clickableIndex The index of the clickable.
 * @param clickType The type of the click (long, super long).
 * @param confirm Set to true to confirm the action after an ACK has been received.
 * @param correlationId The correlation ID that ties request, ack, failover and confirm together.
 */
auto serializeNetworkClick(uint8_t clickableIndex, constants::ClickType clickType, bool confirm, uint8_t correlationId) -> bool
{
    DP_CONTEXT();
#if !CONFIG_USE_NETWORK_CLICKS
    // Keep the call site compileable even when a consumer strips the feature.
    // The whole payload is intentionally dropped at compile time in that case.
    static_cast<void>(clickableIndex);
    static_cast<void>(clickType);
    static_cast<void>(confirm);
    static_cast<void>(correlationId);
    return false;
#else
    // Select command based on confirm flag
    const Command cmd = confirm ? Command::NETWORK_CLICK_CONFIRM : Command::NETWORK_CLICK_REQUEST;
    uint8_t protocolClickType = 0U;

    switch (clickType)
    {
    case constants::ClickType::LONG:
        protocolClickType = static_cast<uint8_t>(lsh::core::protocol::ProtocolClickType::LONG);
        break;
    case constants::ClickType::SUPER_LONG:
        protocolClickType = static_cast<uint8_t>(lsh::core::protocol::ProtocolClickType::SUPER_LONG);
        break;
    default:
        return false;  // Not valid click type
    }

    const uint8_t command = static_cast<uint8_t>(cmd);
    const uint8_t clickableId = Clickables::getId(clickableIndex);

#ifdef CONFIG_MSG_PACK
    if (!writeMsgPackNetworkClickPayload(command, protocolClickType, clickableId, correlationId))
#else
    if (!writeJsonNetworkClickPayload(command, protocolClickType, clickableId, correlationId))
#endif
    {
        return false;
    }
    return finishSuccessfulPayload();
#endif
}

}  // namespace Serializer
