/**
 * @file    bridge_serial.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares the low-level serial communication helpers used to talk with `lsh-bridge`.
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

#ifndef LSH_CORE_COMMUNICATION_BRIDGE_SERIAL_HPP
#define LSH_CORE_COMMUNICATION_BRIDGE_SERIAL_HPP

#include <ArduinoJson.h>
#include <stdint.h>

#include "communication/constants/config.hpp"
#include "communication/deserializer.hpp"
#include "internal/user_config_bridge.hpp"

/**
 * @brief Controller-side serial link used to exchange payloads with `lsh-bridge`.
 */
namespace BridgeSerial
{
extern uint16_t sendIdleAge_ms;              //!< Elapsed idle time since the last payload transmitted to the bridge, saturated at 65535 ms.
extern uint32_t lastReceivedPayloadTime_ms;  //!< Timestamp of the last valid payload received from the bridge.
extern bool firstValidPayloadReceived;       //!< True after the first valid bridge payload has been received.
#ifndef CONFIG_MSG_PACK
extern char rawInputBuffer[constants::bridgeSerial::RAW_INPUT_BUFFER_SIZE];  //!< Raw byte buffer used by the newline-delimited JSON codec.
extern size_t bufferedBytesCount;  //!< Number of meaningful bytes currently stored in `rawInputBuffer`.
extern bool discardUntilNewline;   //!< True while the JSON codec is discarding an overflowed frame tail.
#endif

void init();                                                // Initialize the hardware serial link used by the bridge.
void sendJson(const JsonDocument &documentToSend);          // Send one payload to the bridge using the active codec.
auto receiveAndDispatch() -> Deserializer::DispatchResult;  // Receive and dispatch one bridge payload, if available.
void tickSendIdleTimer(uint16_t elapsed_ms);                // Advance the ping idle timer using the elapsed loop time.
[[nodiscard]] auto canPing() -> bool;                       // Return true when another heartbeat may be emitted.
void updateLastSentTime();                                  // Reset the ping idle timer after a payload transmission.
[[nodiscard]] auto isConnected() -> bool;                   // Return true when the bridge is still considered online.
}  // namespace BridgeSerial

#endif  // LSH_CORE_COMMUNICATION_BRIDGE_SERIAL_HPP
