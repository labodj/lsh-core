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

#include "communication/constants/config.hpp"
#include "communication/constants/protocol.hpp"
#include "communication/bridge_serial.hpp"
#include "communication/payload_utils.hpp"
#include "device/actuator_manager.hpp"
#include "device/clickable_manager.hpp"
#include "internal/user_config_bridge.hpp"
#include "peripherals/input/clickable.hpp"
#include "peripherals/output/actuator.hpp"
#include "util/debug/debug.hpp"

namespace
{
/**
 * @brief A static JsonDocument used as a reusable buffer for all serialization tasks.
 * @details By declaring this document as static within an anonymous namespace, we ensure
 *          it is allocated only once in the .bss segment, not on the stack in every function call.
 *          This significantly improves performance by avoiding repeated construction/destruction
 *          and reduces the risk of stack overflow. The document is cleared before each use.
 *          Its size is determined by the largest possible payload to accommodate all message types.
 */
static StaticJsonDocument<constants::bridgeSerial::SENT_DOC_MAX_SIZE> serializationDoc;
}  // namespace

namespace Serializer
{
using namespace Debug;
using lsh::core::protocol::Command;
using lsh::core::protocol::KEY_PAYLOAD;

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

#ifdef CONFIG_MSG_PACK
    constexpr bool useMsgPack = true;
#else
    constexpr bool useMsgPack = false;
#endif

    const auto payloadToSend = utils::payloads::getSerial<useMsgPack>(payloadType);

    if (!payloadToSend.empty())
    {
        const size_t writtenBytes = CONFIG_COM_SERIAL->write(payloadToSend.data(), payloadToSend.size());
        if (writtenBytes != payloadToSend.size())
        {
            return false;
        }
        if constexpr (constants::bridgeSerial::COM_SERIAL_FLUSH_AFTER_SEND)
        {
            CONFIG_COM_SERIAL->flush();
        }
        BridgeSerial::updateLastSentTime();
        return true;
    }

    return false;
}

/**
 * @brief Prepare and send json details payload (eg: {"p":1,"v":3,"n":"c1","a":[1,2,...],"b":[1,3,...]}).
 */
auto serializeDetails() -> bool
{
    DP_CONTEXT();
    using namespace lsh::core::protocol;

    serializationDoc.clear();

    serializationDoc[KEY_PAYLOAD] = static_cast<uint8_t>(Command::DEVICE_DETAILS);  // "p":"1" (Payload: Device Details)
    serializationDoc[KEY_PROTOCOL_MAJOR] = WIRE_PROTOCOL_MAJOR;                     // "v":3   (Handshake-only protocol major)
    serializationDoc[KEY_NAME] = CONFIG_DEVICE_NAME;                                // "n":"c1" (Device Name: c1)

    JsonArray jsonActuators = serializationDoc.createNestedArray(KEY_ACTUATORS_ARRAY);  // "a": (Actuators IDs: ...)
    auto *const actuatorBegin = Actuators::actuators.data();
    auto *const actuatorEnd = actuatorBegin + Actuators::totalActuators;
    for (auto *currActuator = actuatorBegin; currActuator != actuatorEnd; ++currActuator)
    {
        jsonActuators.add((*currActuator)->getId());
    }

    JsonArray jsonClickables = serializationDoc.createNestedArray(KEY_BUTTONS_ARRAY);  // "b": (Buttons IDs: ...)
    auto *const clickableBegin = Clickables::clickables.data();
    auto *const clickableEnd = clickableBegin + Clickables::totalClickables;
    for (auto *currentClickable = clickableBegin; currentClickable != clickableEnd; ++currentClickable)
    {
        jsonClickables.add((*currentClickable)->getId());
    }

    // Send the Json
    return BridgeSerial::sendJson(serializationDoc);
}

/**
 * @brief Prepare and send a JSON actuators state payload with bitpacked byte array.
 * @details AVR-optimized packing using:
 *          - LUT for O(1) bit mask lookup (essential, AVR has no barrel shifter)
 *          - Bit shift `>>3` instead of `/8` (~1 cycle vs ~20-50 cycles)
 *          - Per-byte loop structure (fewer outer iterations)
 *          - Incremental index instead of multiplication (avoids `*8`)
 *
 *          Output format: {"p":2,"s":[byte0,byte1,...]}
 *          Each byte contains 8 actuator states (bit 0 = first actuator in byte).
 *          Example: 12 actuators → {"p":2,"s":[90,15]} (2 bytes vs 12 array elements)
 */
auto serializeActuatorsState() -> bool
{
    DP_CONTEXT();
    using namespace lsh::core::protocol;

    serializationDoc.clear();

    // The actuator manager already maintains a canonical packed shadow in wire
    // order, so serialization only has to append the ready-made bytes.
    const uint8_t numBytes = Actuators::getPackedStateByteCount();

    // Create the JSON structure
    serializationDoc[KEY_PAYLOAD] = static_cast<uint8_t>(Command::ACTUATORS_STATE);  // "p":2
    JsonArray stateArray = serializationDoc.createNestedArray(KEY_STATE);            // "s":[]

    for (uint8_t byteIndex = 0U; byteIndex < numBytes; ++byteIndex)
    {
        stateArray.add(Actuators::getPackedStateByte(byteIndex));
    }

    // Send the Json
    return BridgeSerial::sendJson(serializationDoc);
}

/**
 * @brief Prepares and sends a JSON network click payload.
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
    using namespace lsh::core::protocol;

    serializationDoc.clear();

    // Select command based on confirm flag
    const Command cmd = confirm ? Command::NETWORK_CLICK_CONFIRM : Command::NETWORK_CLICK_REQUEST;
    serializationDoc[KEY_PAYLOAD] = static_cast<uint8_t>(cmd);

    switch (clickType)
    {
    case constants::ClickType::LONG:
        serializationDoc[KEY_TYPE] = static_cast<uint8_t>(lsh::core::protocol::ProtocolClickType::LONG);  // "t":1  (Click Type: Long)
        break;
    case constants::ClickType::SUPER_LONG:
        serializationDoc[KEY_TYPE] =
            static_cast<uint8_t>(lsh::core::protocol::ProtocolClickType::SUPER_LONG);  // "t":2  (Click Type: Super Long)
        break;
    default:
        return false;  // Not valid click type
    }

    serializationDoc[KEY_ID] = Clickables::clickables[clickableIndex]->getId();  // "i":7 (Button ID: 7)
    serializationDoc[KEY_CORRELATION_ID] = correlationId;                        // "c":42 (Correlation ID)

    // Send the Json
    return BridgeSerial::sendJson(serializationDoc);
#endif
}

}  // namespace Serializer
