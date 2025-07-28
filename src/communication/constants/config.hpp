/**
 * @file    config.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Defines build-time configurable parameters for serial communication.
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

#ifndef LSHCORE_COMMUNICATION_CONSTANTS_CONFIGS_HPP
#define LSHCORE_COMMUNICATION_CONSTANTS_CONFIGS_HPP

#include <ArduinoJson.h>
#include <etl/bit.h>
#include <stdint.h>

#include "internal/user_config_bridge.hpp"
/**
 * @brief namespace for constants.
 */
namespace constants
{
        /**
         * @brief namespace for build configurable ESP communication constants.
         */
        namespace espComConfigs
        {
                // Ping timers
#ifndef CONFIG_PING_INTERVAL_MS
                static constexpr const uint16_t PING_INTERVAL_MS = 10000U; //!< Default Ping interval time in ms
#else
                static constexpr const uint16_t PING_INTERVAL_MS = CONFIG_PING_INTERVAL_MS; //!< Ping interval time in ms
#endif // CONFIG_PING_INTERVAL_MS

#ifndef CONFIG_CONNECTION_TIMEOUT_MS
                static constexpr const uint16_t CONNECTION_TIMEOUT_MS = PING_INTERVAL_MS + 200U; //!< Default Time for a connection timeout in ms
#else
                static constexpr const uint16_t CONNECTION_TIMEOUT_MS = CONFIG_CONNECTION_TIMEOUT_MS; //!< Time for a connection timeout in ms
#endif // CONFIG_CONNECTION_TIMEOUT_MS

#ifndef CONFIG_COM_SERIAL_SPEED_BAUD
                static constexpr const uint32_t COM_SERIAL_SPEED_BAUD = 250000U; //!< Default Serial connected with ESP speed in bauds
#else
                static constexpr const uint32_t COM_SERIAL_SPEED_BAUD = CONFIG_COM_SERIAL_SPEED_BAUD; //!< Serial connected with ESP speed in bauds
#endif // CONFIG_COM_SERIAL_SPEED_BAUD

#ifndef CONFIG_COM_SERIAL_TIMEOUT_MS
                static constexpr const uint8_t COM_SERIAL_TIMEOUT_MS = 5U; //!< Default Serial connected with ESP timeout
#else
                static constexpr const uint8_t COM_SERIAL_TIMEOUT_MS = CONFIG_COM_SERIAL_TIMEOUT_MS; //!< Serial connected with ESP timeout
#endif // CONFIG_COM_SERIAL_TIMEOUT_MS

                /*
                Received Json Document Size, the size is computed here https://arduinojson.org/v6/assistant/
                Processor: AVR, Mode: Deserialize, Input Type: Stream
                {"p":"10} -> min: 10, recommended: 24
                              -> (JSON_OBJECT_SIZE(1) + number of strings + string characters number)
                              -> (8 + 2 + 6 = 16)
                {"p":12,"s":[0,1,0,1,0,...]} -> min: JSON_ARRAY_SIZE(N) + JSON_OBJECT_SIZE(2) + number of strings + string characters number
                                                    -> (N actuator) (JSON_ARRAY_SIZE(N) + JSON_OBJECT_SIZE(2) + 2 + 2)
                                                    -> 0 actuators: 20 | 10 actuators: 100
                {"p":13,"i":5,"s":0} -> min: 30, recommended: 48
                                                     -> (JSON_OBJECT_SIZE(3) + number of strings + string characters number)
                                                     -> (24 + 3 + 3 = 30)
                We can use 48 as base minimum size
                */
                constexpr uint16_t RECEIVED_DOC_MIN_SIZE = etl::bit_ceil(JSON_ARRAY_SIZE(CONFIG_MAX_ACTUATORS) + JSON_OBJECT_SIZE(2) + 4U);
                constexpr uint16_t RECEIVED_DOC_SIZE = RECEIVED_DOC_MIN_SIZE <= 48U ? 48U : RECEIVED_DOC_MIN_SIZE;

                /**
                 * @brief Defines the size of the temporary on-stack buffer for reading raw serial messages.
                 */
                constexpr uint16_t RAW_INPUT_BUFFER_MIN_SIZE = etl::bit_ceil(22U); // {"p":16,"t":1,"i":1} -> 20 char +1 for '\n' +1 for '\0'.

                // Calculated size for the longest variable-length command ({"p":12,"s":[0,1,0,1,0,...]})
                constexpr uint16_t RAW_INPUT_BUFFER_VARIABLE_CMD_SIZE = (CONFIG_MAX_ACTUATORS > 0U)
                                                                            ? etl::bit_ceil(16U + (2U * CONFIG_MAX_ACTUATORS)) // {"p":12,"s":[0,1,0,1,0,...]} -> 16 + CONFIG_MAX_ACTUATORS *2
                                                                            : etl::bit_ceil(17U);                              // Special case for 0 actuators. ({"p":12,"s":[]} -> 15 +1 for '\n' +1 for '\0' )

                constexpr uint16_t RAW_INPUT_BUFFER_SIZE = RAW_INPUT_BUFFER_VARIABLE_CMD_SIZE <= RAW_INPUT_BUFFER_MIN_SIZE ? RAW_INPUT_BUFFER_MIN_SIZE : RAW_INPUT_BUFFER_VARIABLE_CMD_SIZE;
                /*
                Sent details Json Document size, the size is computed here https://arduinojson.org/v6/assistant/
                IMPORTANT: We are assuming that all keys strings and values strings are const char *
                {"p":1,"n":"c1","a":[1,2,...],"b":[1,3,...]} -> JSON_ARRAY_SIZE(CONFIG_MAX_ACTUATORS) + JSON_ARRAY_SIZE(CONFIG_MAX_CLICKABLES) + JSON_OBJECT_SIZE(4)
                                                             -> 8*CONFIG_MAX_ACTUATORS + 8*CONFIG_MAX_CLICKABLES + 32
                The bare minimum is 32 with no actuators nor clickables
                */
                constexpr uint16_t SENT_DOC_DETAILS_SIZE = JSON_ARRAY_SIZE(CONFIG_MAX_ACTUATORS) + JSON_ARRAY_SIZE(CONFIG_MAX_CLICKABLES) + JSON_OBJECT_SIZE(4);

                /*
                Sent state Json Document size, the size is computed here https://arduinojson.org/v6/assistant/
                IMPORTANT: We are assuming that all keys strings are const char * but not values
                {"p":2,"s":[0,1,0,1,...]}-> JSON_ARRAY_SIZE(CONFIG_MAX_ACTUATORS) + JSON_OBJECT_SIZE(2)
                The bare minimum is 16 with no actuators
                */
                constexpr uint16_t SENT_DOC_STATE_SIZE = JSON_ARRAY_SIZE(CONFIG_MAX_ACTUATORS) + JSON_OBJECT_SIZE(2);

                /*
                Sent network click Json Document, the size is computed here https://arduinojson.org/v6/assistant/
                IMPORTANT: We are assuming that all keys strings and values strings are const char *
                {"p":3,"t":1,"i":1,"c":0} -> JSON_OBJECT_SIZE(4) -> 32
                */
                constexpr uint16_t SENT_DOC_NETWORK_CLICK_SIZE = JSON_OBJECT_SIZE(4);

                constexpr uint16_t SENT_DOC_MAX_SIZE = SENT_DOC_DETAILS_SIZE;

#define SERIAL_RX_BUFFER_SIZE = 2048
        } // namespace espComConfigs
} // namespace constants

#endif // LSHCORE_COMMUNICATION_CONSTANTS_CONFIGS_HPP
