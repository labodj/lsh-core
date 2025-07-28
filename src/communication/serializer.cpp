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
#if (SENT_DOC_DETAILS_SIZE > 1024U)
        DynamicJsonDocument doc(constants::espComConfigs::SENT_DOC_DETAILS_SIZE);
#else
        StaticJsonDocument<constants::espComConfigs::SENT_DOC_DETAILS_SIZE> doc;
#endif
        using namespace LSH::protocol;

        doc[KEY_PAYLOAD] = static_cast<uint8_t>(Command::DEVICE_DETAILS); // "p":"1" (Payload: Device Details)
        doc[KEY_NAME] = CONFIG_DEVICE_NAME;                               // "n":"c1" (Device Name: c1)

        JsonArray jsonActuators = doc.createNestedArray(KEY_ACTUATORS_ARRAY); // "a": (Actuators IDs: ...)
        for (const auto *const actuator : Actuators::actuators)
        {
            jsonActuators.add(actuator->getId());
        }

        JsonArray jsonClickables = doc.createNestedArray(KEY_BUTTONS_ARRAY); // "b": (Buttons IDs: ...)
        for (const auto *const clickable : Clickables::clickables)
        {
            jsonClickables.add(clickable->getId());
        }

        // Send the Json
        EspCom::sendJson(doc);
    }

    /**
     * @brief Prepare and send a json actuators state payload (eg: {"p":2,"s":[0,1,0,1,...]}).
     */
    void serializeActuatorsState()
    {
        DP_CONTEXT();
#if (SENT_DOC_STATE_SIZE > 1024U)
        DynamicJsonDocument doc(constants::espComConfigs::SENT_DOC_STATE_SIZE);
#else
        StaticJsonDocument<constants::espComConfigs::SENT_DOC_STATE_SIZE> doc;
#endif
        using namespace LSH::protocol;
        // Write the Json
        doc[KEY_PAYLOAD] = static_cast<uint8_t>(Command::ACTUATORS_STATE); // "p":2   (Payload: Actuators State)
        JsonArray jsonActuators = doc.createNestedArray(KEY_STATE);        // "s": (State: ...)

        for (const auto *const actuator : Actuators::actuators)
        {
            jsonActuators.add(static_cast<uint8_t>(actuator->getState())); // Convert bool to uint
        }

        // Send the Json
        EspCom::sendJson(doc);
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
#if (SENT_DOC_NETWORK_CLICK_SIZE > 1024U)
        DynamicJsonDocument doc(constants::espComConfigs::SENT_DOC_NETWORK_CLICK_SIZE);
#else
        StaticJsonDocument<constants::espComConfigs::SENT_DOC_NETWORK_CLICK_SIZE> doc;
#endif

        // Write the Json
        using namespace LSH::protocol;
        doc[KEY_PAYLOAD] = static_cast<uint8_t>(Command::NETWORK_CLICK); // "p":3   (Payload: Network CLick)

        switch (clickType)
        {
        case constants::ClickType::LONG:
            doc[KEY_TYPE] = static_cast<uint8_t>(LSH::protocol::ProtocolClickType::LONG); // "t":1  (Click Type: Long)
            break;
        case constants::ClickType::SUPER_LONG:
            doc[KEY_TYPE] = static_cast<uint8_t>(LSH::protocol::ProtocolClickType::SUPER_LONG); // "t":2  (Click Type: Super Long)
            break;
        default:
            return; // Not valid click type
        }

        doc[KEY_ID] = Clickables::clickables[clickableIndex]->getId(); // "i":7 (Button ID: 7)
        doc[KEY_CONFIRM] = static_cast<uint8_t>(confirm);              // "c":0 (Confirm: false)

        // Send the Json
        EspCom::sendJson(doc);
    }

} // namespace Serializer
