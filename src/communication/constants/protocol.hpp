/**
 * @file    protocol.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Defines the communication protocol contract (JSON keys, command IDs).
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

#ifndef LSHCORE_COMMUNICATION_CONSTANTS_PROTOCOL_HPP
#define LSHCORE_COMMUNICATION_CONSTANTS_PROTOCOL_HPP

#include <stdint.h>

namespace LSH
{
    namespace protocol
    {
        // === JSON KEYS ===
        constexpr const char *KEY_PAYLOAD = "p";         //!< Payload
        constexpr const char *KEY_NAME = "n";            //!< Device Name
        constexpr const char *KEY_ACTUATORS_ARRAY = "a"; //!< Actuators IDs
        constexpr const char *KEY_BUTTONS_ARRAY = "b";   //!< Buttons IDs
        constexpr const char *KEY_ID = "i";              //!< ID
        constexpr const char *KEY_STATE = "s";           //!< Actuators State
        constexpr const char *KEY_TYPE = "t";            //!< Click Type
        constexpr const char *KEY_CONFIRM = "c";         //!< Confirm

        /**
         * @brief Defines the valid command types for the 'p' (payload) key in JSON messages.
         */
        enum class Command : uint8_t
        {
            // Arduino -> ESP
            DEVICE_DETAILS = 1,
            ACTUATORS_STATE = 2,
            NETWORK_CLICK = 3,

            // Omnidirectional
            BOOT = 4,
            PING_ = 5,

            // ESP -> Arduino (or MQTT -> ESP)
            REQUEST_DETAILS = 10,
            REQUEST_STATE = 11,
            SET_STATE = 12,
            SET_SINGLE_ACTUATOR = 13,
            NETWORK_CLICK_ACK = 14,
            FAILOVER = 15,
            FAILOVER_CLICK = 16,

            // ESP System command (MQTT -> ESP)
            SYSTEM_REBOOT = 254,
            SYSTEM_RESET = 255
        };

        /**
         * @brief Defines the valid click types for the 't' (type) key in JSON messages.
         */
        enum class ProtocolClickType : uint8_t
        {
            LONG = 1,
            SUPER_LONG = 2
        };

    } // namespace protocol
} // namespace LSH

#endif // LSHCORE_COMMUNICATION_CONSTANTS_PROTOCOL_HPP