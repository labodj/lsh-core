/**
 * @file    serializer.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the data serialization logic for sending messages to the ESP bridge.
 *
 * Copyright 2025 Jacopo Labardi
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
#include "communication/esp_com.hpp"
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
    static StaticJsonDocument<constants::espComConfigs::SENT_DOC_MAX_SIZE> serializationDoc;
} // namespace

namespace Serializer
{
    using namespace Debug;
    using LSH::protocol::Command;
    using LSH::protocol::KEY_PAYLOAD;

    /**
     * @brief Send a static json payload.
     *
     * @param payloadType type of the payload.
     */
    void serializeStaticJson(constants::payloads::StaticType payloadType)
    {
        using constants::payloads::StaticType;
        if (payloadType == StaticType::PING_ && !EspCom::canPing())
        {
            return;
        }

#ifdef CONFIG_MSG_PACK
        constexpr bool useMsgPack = true;
#else
        constexpr bool useMsgPack = false;
#endif

        const auto payloadToSend = utils::payloads::get<useMsgPack>(payloadType);

        if (!payloadToSend.empty())
        {
            CONFIG_COM_SERIAL->write(payloadToSend.data(), payloadToSend.size());
            EspCom::updateLastSentTime();
        }
    }

    /**
     * @brief Prepare and send json details payload (eg: {"p":1,"v":3,"n":"c1","a":[1,2,...],"b":[1,3,...]}).
     */
    void serializeDetails()
    {
        DP_CONTEXT();
        using namespace LSH::protocol;

        serializationDoc.clear();

        serializationDoc[KEY_PAYLOAD] = static_cast<uint8_t>(Command::DEVICE_DETAILS); // "p":"1" (Payload: Device Details)
        serializationDoc[KEY_PROTOCOL_MAJOR] = WIRE_PROTOCOL_MAJOR;                     // "v":3   (Handshake-only protocol major)
        serializationDoc[KEY_NAME] = CONFIG_DEVICE_NAME;                               // "n":"c1" (Device Name: c1)

        JsonArray jsonActuators = serializationDoc.createNestedArray(KEY_ACTUATORS_ARRAY); // "a": (Actuators IDs: ...)
        auto *const actuatorBegin = Actuators::actuators.data();
        auto *const actuatorEnd = actuatorBegin + Actuators::totalActuators;
        for (auto *currActuator = actuatorBegin; currActuator != actuatorEnd; ++currActuator)
        {
            jsonActuators.add((*currActuator)->getId());
        }

        JsonArray jsonClickables = serializationDoc.createNestedArray(KEY_BUTTONS_ARRAY); // "b": (Buttons IDs: ...)
        auto *const clickableBegin = Clickables::clickables.data();
        auto *const clickableEnd = clickableBegin + Clickables::totalClickables;
        for (auto *currClickable = clickableBegin; currClickable != clickableEnd; ++currClickable)
        {
            jsonClickables.add((*currClickable)->getId());
        }

        // Send the Json
        EspCom::sendJson(serializationDoc);
    }

    namespace
    {
        /**
         * @brief Lookup table for bit masks (8-bit).
         * @details ESSENTIAL for AVR: AVR has no barrel shifter, so `1 << i` becomes
         *          an O(i) loop (~i cycles). LUT provides O(1) lookup (~2 cycles).
         *          For 12 actuators: ~66 cycles saved per serialization.
         */
        constexpr uint8_t BIT_MASK_8[8] = {
            0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
    } // anonymous namespace

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
    void serializeActuatorsState()
    {
        DP_CONTEXT();
        using namespace LSH::protocol;

        serializationDoc.clear();

        // Calculate how many bytes we need: ceil(totalActuators / 8)
        // Using >>3 instead of /8 (bit shift is MUCH faster on AVR, no division unit)
        const uint8_t numBytes = (Actuators::totalActuators + 7U) >> 3U;

        // Create the JSON structure
        serializationDoc[KEY_PAYLOAD] = static_cast<uint8_t>(Command::ACTUATORS_STATE); // "p":2
        JsonArray stateArray = serializationDoc.createNestedArray(KEY_STATE);           // "s":[]

        // Pack actuator states into bytes using optimized loop
        // Using <<3 instead of *8 (bit shift is single cycle on AVR)
        uint8_t actuatorIndex = 0U;
        for (uint8_t byteIndex = 0U; byteIndex < numBytes; ++byteIndex)
        {
            uint8_t packedByte = 0U;

            // Pack up to 8 actuators into this byte
            for (uint8_t bitIndex = 0U; bitIndex < 8U && actuatorIndex < Actuators::totalActuators; ++bitIndex)
            {
                if (Actuators::actuators[actuatorIndex]->getState())
                {
                    packedByte |= BIT_MASK_8[bitIndex]; // LUT lookup: O(1), ~2 cycles
                }
                ++actuatorIndex;
            }
            stateArray.add(packedByte);
        }

        // Send the Json
        EspCom::sendJson(serializationDoc);
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
    void serializeNetworkClick(uint8_t clickableIndex, constants::ClickType clickType, bool confirm, uint8_t correlationId)
    {
        DP_CONTEXT();
        using namespace LSH::protocol;

        serializationDoc.clear();

        // Select command based on confirm flag
        const Command cmd = confirm ? Command::NETWORK_CLICK_CONFIRM : Command::NETWORK_CLICK_REQUEST;
        serializationDoc[KEY_PAYLOAD] = static_cast<uint8_t>(cmd);

        switch (clickType)
        {
        case constants::ClickType::LONG:
            serializationDoc[KEY_TYPE] = static_cast<uint8_t>(LSH::protocol::ProtocolClickType::LONG); // "t":1  (Click Type: Long)
            break;
        case constants::ClickType::SUPER_LONG:
            serializationDoc[KEY_TYPE] = static_cast<uint8_t>(LSH::protocol::ProtocolClickType::SUPER_LONG); // "t":2  (Click Type: Super Long)
            break;
        default:
            return; // Not valid click type
        }

        serializationDoc[KEY_ID] = Clickables::clickables[clickableIndex]->getId(); // "i":7 (Button ID: 7)
        serializationDoc[KEY_CORRELATION_ID] = correlationId;                        // "c":42 (Correlation ID)

        // Send the Json
        EspCom::sendJson(serializationDoc);
    }

} // namespace Serializer
