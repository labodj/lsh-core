/**
 * @file    deserializer.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic for deserializing and dispatching commands.
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

#include "communication/deserializer.hpp"

#include "communication/constants/protocol.hpp"
#include "communication/serializer.hpp"
#include "core/network_clicks.hpp"
#include "device/actuator_manager.hpp"
#include "device/clickable_manager.hpp"
#include "peripherals/output/actuator.hpp"
#include "util/debug/debug.hpp"

namespace Deserializer
{
    using namespace Debug;
    using namespace LSH::protocol;
    namespace
    {
        /**
         * @brief Processes payloads for network click acknowledgements and failovers.
         * @details This helper function handles the shared logic for both NETWORK_CLICK_ACK
         * and FAILOVER_CLICK commands. It extracts the button ID and click type from the
         * JSON document and calls the appropriate function in the NetworkClicks module.
         * It uses a "validation by convention" approach, where an ID of 0 is treated as
         * invalid, implicitly handling missing or null JSON keys.
         *
         * @param doc The const JsonDocument containing the payload.
         * @param cmd The specific command (either NETWORK_CLICK_ACK or FAILOVER_CLICK).
         * @param result A reference to the DispatchResult struct to be modified with the outcome.
         */
        void processNetworkClickResponse(const JsonDocument &doc, LSH::protocol::Command cmd, DispatchResult &result)
        {
            // Directly get numeric values..as<uint8_t>() returns 0 if key is missing / null.
            const auto jsonClickType = doc[KEY_TYPE].as<uint8_t>();
            const auto clickableId = doc[KEY_ID].as<uint8_t>();

            // A single check for existence handles missing keys (id=0) and invalid IDs.
            if (!Clickables::clickableExists(clickableId))
                return;

            constants::ClickType clickType = constants::ClickType::NONE;
            switch (static_cast<LSH::protocol::ProtocolClickType>(jsonClickType))
            {
            case LSH::protocol::ProtocolClickType::LONG:
                clickType = constants::ClickType::LONG;
                break;
            case LSH::protocol::ProtocolClickType::SUPER_LONG:
                clickType = constants::ClickType::SUPER_LONG;
                break;
            default:
                DPL(jsonClickType);
                return; // Invalid click type (was 0 or other value)
            }

            const uint8_t clickableIndex = Clickables::getIndex(clickableId);

            if (cmd == Command::FAILOVER_CLICK)
            {
                result.stateChanged = NetworkClicks::checkNetworkClickTimer(clickableIndex, clickType, true);
            }
            else if (cmd == Command::NETWORK_CLICK_ACK)
            {
                if (!NetworkClicks::isNetworkClickExpired(clickableIndex, clickType))
                {
                    result.stateChanged = NetworkClicks::confirm(clickableIndex, clickType);
                    result.networkClickHandled = result.stateChanged;
                }
            }
        }
    } // namespace

    /**
     * @brief Main entry point for command processing. Deserializes a JSON document
     * and immediately dispatches the corresponding action.
     * @details This function acts as a command dispatcher. It reads the command ID
     * from the 'p' key and uses a switch statement for O(1) dispatching.
     * It directly calls functions in other modules (Serializer, Actuators, NetworkClicks)
     * to execute the command. This avoids intermediate state storage (like ResultsHolder)
     * and multiple switch statements in the main loop, maximizing performance.
     * The validation relies on a "validation by convention" approach, where a value of 0
     * for IDs or commands is treated as invalid, eliminating the need for
     * `containsKey` checks.
     *
     * @param doc A const reference to the parsed JsonDocument from EspCom.
     * @return DispatchResult A struct indicating the side-effects of the command,
     * telling the main loop if a general state update needs to be sent or if
     * network click timers need to be checked.
     */
    auto deserializeAndDispatch(const JsonDocument &doc) -> DispatchResult
    {
        DispatchResult result; // Default: { false, false }

        // Directly cast. If "p" is missing, cmd will be 0, caught by switch's default.
        const auto cmd = static_cast<Command>(doc[KEY_PAYLOAD].as<uint8_t>());

        switch (cmd)
        {
        case Command::SET_SINGLE_ACTUATOR:
            // Get values from Json
            {
                const auto id = doc[KEY_ID].as<uint8_t>();
                // ActuatorExists handles id=0 (key missing) and non-existent IDs in one call.
                if (!Actuators::actuatorExists(id))
                {
                    break;
                }

                const JsonVariantConst jsonState = doc[KEY_STATE];
                if (jsonState.isNull() || !jsonState.is<uint8_t>())
                {
                    break; // Wrong or missing jsonState
                }
                const auto state = jsonState.as<uint8_t>() == 1;

                result.stateChanged = Actuators::actuators[Actuators::getIndex(id)]->setState(state);
                break;
            }
        case Command::SET_STATE:
        {
            const JsonArrayConst statesArray = doc[KEY_STATE];
            if (statesArray.size() != Actuators::totalActuators)
            {
                break;
            }
            bool anyStateChanged = false;
            for (uint8_t i = 0; i < Actuators::totalActuators; ++i)
            {
                anyStateChanged |= Actuators::actuators[i]->setState(statesArray[i].as<uint8_t>() == 1);
            }
            result.stateChanged = anyStateChanged;
            break;
        }

        case Command::NETWORK_CLICK_ACK:
        case Command::FAILOVER_CLICK:
            processNetworkClickResponse(doc, cmd, result);
            break;

        case Command::FAILOVER:
            result.stateChanged = NetworkClicks::checkAllNetworkClicksTimers(true);
            break;

        case Command::REQUEST_STATE:
            Serializer::serializeActuatorsState();
            result.stateChanged = false;
            break;

        case Command::REQUEST_DETAILS:
            Serializer::serializeDetails();
            result.stateChanged = false;
            break;

        case Command::BOOT:
            Serializer::serializeDetails();
            Serializer::serializeActuatorsState();
            result.stateChanged = false;
            break;

        case Command::PING_:
            break;

        default:
            DPL("Unknown or missing command ID: ", static_cast<uint8_t>(cmd));
            break;
        }
        return result;
    }
} // namespace Deserializer