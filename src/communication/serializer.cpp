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
     * @brief Prepare and send json details payload (eg: {"p":1,"n":"c1","a":[1,2,...],"b":[1,3,...]}).
     */
    void serializeDetails()
    {
        DP_CONTEXT();
        using namespace LSH::protocol;

        serializationDoc.clear();

        serializationDoc[KEY_PAYLOAD] = static_cast<uint8_t>(Command::DEVICE_DETAILS); // "p":"1" (Payload: Device Details)
        serializationDoc[KEY_NAME] = CONFIG_DEVICE_NAME;                               // "n":"c1" (Device Name: c1)

        JsonArray jsonActuators = serializationDoc.createNestedArray(KEY_ACTUATORS_ARRAY); // "a": (Actuators IDs: ...)
        for (const auto *const actuator : Actuators::actuators)
        {
            jsonActuators.add(actuator->getId());
        }

        JsonArray jsonClickables = serializationDoc.createNestedArray(KEY_BUTTONS_ARRAY); // "b": (Buttons IDs: ...)
        for (const auto *const clickable : Clickables::clickables)
        {
            jsonClickables.add(clickable->getId());
        }

        // Send the Json
        EspCom::sendJson(serializationDoc);
    }

    /**
     * @brief Prepare and send a json actuators state payload (eg: {"p":2,"s":[0,1,0,1,...]}).
     */
    void serializeActuatorsState()
    {
        DP_CONTEXT();
        using namespace LSH::protocol;

        serializationDoc.clear();

        // Write the Json
        serializationDoc[KEY_PAYLOAD] = static_cast<uint8_t>(Command::ACTUATORS_STATE); // "p":2   (Payload: Actuators State)
        JsonArray jsonActuators = serializationDoc.createNestedArray(KEY_STATE);        // "s": (State: ...)

        for (const auto *const actuator : Actuators::actuators)
        {
            jsonActuators.add(static_cast<uint8_t>(actuator->getState())); // Convert bool to uint
        }

        // Send the Json
        EspCom::sendJson(serializationDoc);
    }

    /**
     * @brief Prepares and sends a JSON network click payload (e.g., {"p":3,"t":1,"i":1,"c":0}).
     *
     * @param clickableIndex The index of the clickable.
     * @param clickType The type of the click (long, super long).
     * @param confirm Set to true to confirm the action after an ACK has been received.
     */
    void serializeNetworkClick(uint8_t clickableIndex, constants::ClickType clickType, bool confirm)
    {
        DP_CONTEXT();
        using namespace LSH::protocol;

        serializationDoc.clear();

        // Write the Json
        serializationDoc[KEY_PAYLOAD] = static_cast<uint8_t>(Command::NETWORK_CLICK); // "p":3   (Payload: Network CLick)

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
        serializationDoc[KEY_CONFIRM] = static_cast<uint8_t>(confirm);              // "c":0 (Confirm: false)

        // Send the Json
        EspCom::sendJson(serializationDoc);
    }

} // namespace Serializer
