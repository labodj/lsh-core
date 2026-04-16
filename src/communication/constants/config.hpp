/**
 * @file    config.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Defines build-time configurable parameters for serial communication.
 *
 * Copyright 2025 Jacopo Labardi
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

#ifndef LSHCORE_COMMUNICATION_CONSTANTS_CONFIGS_HPP
#define LSHCORE_COMMUNICATION_CONSTANTS_CONFIGS_HPP

#include <ArduinoJson.h>
#include <stdint.h>

#include "internal/etl_bit.hpp"
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

#ifndef CONFIG_COM_SERIAL_BAUD
                static constexpr const uint32_t COM_SERIAL_BAUD = 250000U; //!< Default Serial connected with ESP speed in bauds
#else
                static constexpr const uint32_t COM_SERIAL_BAUD = CONFIG_COM_SERIAL_BAUD; //!< Serial connected with ESP speed in bauds
#endif // CONFIG_COM_SERIAL_BAUD

#ifndef CONFIG_COM_SERIAL_TIMEOUT_MS
                static constexpr const uint8_t COM_SERIAL_TIMEOUT_MS = 5U; //!< Default Serial connected with ESP timeout
#else
                static constexpr const uint8_t COM_SERIAL_TIMEOUT_MS = CONFIG_COM_SERIAL_TIMEOUT_MS; //!< Serial connected with ESP timeout
#endif // CONFIG_COM_SERIAL_TIMEOUT_MS

#ifndef CONFIG_COM_SERIAL_FLUSH_AFTER_SEND
                static constexpr const bool COM_SERIAL_FLUSH_AFTER_SEND = true; //!< Conservative default: flush after every payload send.
#else
                static constexpr const bool COM_SERIAL_FLUSH_AFTER_SEND = (CONFIG_COM_SERIAL_FLUSH_AFTER_SEND != 0);
#endif // CONFIG_COM_SERIAL_FLUSH_AFTER_SEND

                constexpr uint16_t PACKED_STATE_BYTES = static_cast<uint16_t>((CONFIG_MAX_ACTUATORS + 7U) / 8U); //!< Number of bytes required to represent all actuator states on wire.

                /*
                Received Json Document Size, the size is computed here https://arduinojson.org/v6/assistant/
                Processor: AVR, Mode: Deserialize, Input Type: Stream
                {"p":10} -> min: 10, recommended: 24
                              -> (JSON_OBJECT_SIZE(1) + number of strings + string characters number)
                              -> (8 + 2 + 6 = 16)
                {"p":12,"s":[90,3]} -> min: JSON_ARRAY_SIZE(ceil(CONFIG_MAX_ACTUATORS / 8)) + JSON_OBJECT_SIZE(2) + 4
                                      because `SET_STATE` uses packed state bytes, not one JSON element per actuator.
                {"p":17,"t":2,"i":255,"c":255} -> min: JSON_OBJECT_SIZE(4)
                {"p":13,"i":5,"s":0} -> min: 30, recommended: 48
                                                     -> (JSON_OBJECT_SIZE(3) + number of strings + string characters number)
                                                     -> (24 + 3 + 3 = 30)
                We can use 48 as base minimum size
                */
                constexpr uint16_t RECEIVED_DOC_MIN_SIZE = etl::bit_ceil(JSON_ARRAY_SIZE(PACKED_STATE_BYTES) + JSON_OBJECT_SIZE(2) + 4U); //!< Calculated minimum size for the JSON document received from the bridge.
                constexpr uint16_t RECEIVED_DOC_SIZE = RECEIVED_DOC_MIN_SIZE <= 48U ? 48U : RECEIVED_DOC_MIN_SIZE;                          //!< Final allocated size for the received JSON document, ensuring a minimum of 48 bytes.
                static_assert(RECEIVED_DOC_SIZE >= JSON_OBJECT_SIZE(4), "RECEIVED_DOC_SIZE must fit network click responses with correlation ID.");

                /**
                 * @brief Defines the size of the temporary on-stack buffer for reading raw serial messages.
                 */
                constexpr uint16_t RAW_INPUT_BUFFER_MIN_SIZE = etl::bit_ceil(31U); //!< Minimum buffer size for the longest fixed JSON command plus null terminator: {"p":17,"t":2,"i":255,"c":255}\0

                /**
                 * @brief Calculated size for the longest variable-length JSON command (packed `SET_STATE`).
                 */
                constexpr uint16_t RAW_INPUT_BUFFER_VARIABLE_CMD_SIZE = etl::bit_ceil(17U + (4U * PACKED_STATE_BYTES)); //!< Conservative worst case for {"p":12,"s":[255,...]}\n\0

                constexpr uint16_t RAW_INPUT_BUFFER_SIZE = RAW_INPUT_BUFFER_VARIABLE_CMD_SIZE <= RAW_INPUT_BUFFER_MIN_SIZE ? RAW_INPUT_BUFFER_MIN_SIZE : RAW_INPUT_BUFFER_VARIABLE_CMD_SIZE; //!< Final allocated size for the raw serial input buffer.
                static_assert(RAW_INPUT_BUFFER_SIZE >= 31U, "RAW_INPUT_BUFFER_SIZE must fit fixed-size click/failover commands plus null terminator.");

                /*
                Sent details Json Document size, the size is computed here https://arduinojson.org/v6/assistant/
                IMPORTANT: We are assuming that all keys strings and values strings are const char *
                {"p":1,"v":3,"n":"c1","a":[1,2,...],"b":[1,3,...]} -> JSON_ARRAY_SIZE(CONFIG_MAX_ACTUATORS) + JSON_ARRAY_SIZE(CONFIG_MAX_CLICKABLES) + JSON_OBJECT_SIZE(5)
                                                                    -> 8*CONFIG_MAX_ACTUATORS + 8*CONFIG_MAX_CLICKABLES + 40
                The bare minimum is 40 with no actuators nor clickables
                */
                constexpr uint16_t SENT_DOC_DETAILS_SIZE = JSON_ARRAY_SIZE(CONFIG_MAX_ACTUATORS) + JSON_ARRAY_SIZE(CONFIG_MAX_CLICKABLES) + JSON_OBJECT_SIZE(5); //!< Calculated size for the JSON document sent with device details.

                /*
                Sent state Json Document size, the size is computed here https://arduinojson.org/v6/assistant/
                IMPORTANT: We are assuming that all keys strings are const char * but not values
                {"p":2,"s":[90,3]} -> JSON_ARRAY_SIZE(ceil(CONFIG_MAX_ACTUATORS / 8)) + JSON_OBJECT_SIZE(2)
                because `ACTUATORS_STATE` is serialized as packed bytes.
                The bare minimum is 16 with no actuators.
                */
                constexpr uint16_t SENT_DOC_STATE_SIZE = JSON_ARRAY_SIZE(PACKED_STATE_BYTES) + JSON_OBJECT_SIZE(2); //!< Calculated size for the JSON document sent with packed actuator states.

                /*
                Sent network click Json Document, the size is computed here https://arduinojson.org/v6/assistant/
                IMPORTANT: We are assuming that all keys strings and values strings are const char *
                {"p":3,"t":1,"i":1,"c":42} -> JSON_OBJECT_SIZE(4) -> 32
                */
                constexpr uint16_t SENT_DOC_NETWORK_CLICK_SIZE = JSON_OBJECT_SIZE(4); //!< Calculated size for the JSON document sent for network clicks.

                constexpr uint16_t SENT_DOC_MAX_SIZE = SENT_DOC_DETAILS_SIZE; //!< The maximum possible size for any JSON document sent by the device.
        } // namespace espComConfigs
} // namespace constants

#endif // LSHCORE_COMMUNICATION_CONSTANTS_CONFIGS_HPP
