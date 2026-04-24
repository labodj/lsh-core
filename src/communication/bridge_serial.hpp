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
#include <stddef.h>
#include <stdint.h>

#include "communication/constants/config.hpp"
#include "communication/deserializer.hpp"
#include "internal/user_config_bridge.hpp"

/**
 * @brief Controller-side serial link used to exchange payloads with `lsh-bridge`.
 */
namespace BridgeSerial
{
struct ReceiveResult
{
    Deserializer::DispatchResult dispatch{};  //!< Effects produced by one successfully dispatched payload.
    uint16_t consumedBytes = 0U;              //!< Raw UART bytes consumed during this receive attempt.
    bool payloadDispatched = false;           //!< True only when one full payload was parsed and dispatched.
};

extern uint16_t sendIdleAge_ms;
extern uint16_t receiveIdleAge_ms;

void init();                                                              // Initialize the hardware serial link used by the bridge.
[[nodiscard]] auto sendJson(const JsonDocument &documentToSend) -> bool;  // Send one payload to the bridge using the active codec.
auto receiveAndDispatch(uint16_t maxBytesToConsume)
    -> ReceiveResult;                                  // Consume up to N bytes and dispatch at most one complete bridge payload.
void tickSendIdleTimer(uint16_t elapsed_ms);           // Advance the ping idle timer using the elapsed bridge-housekeeping time.
[[nodiscard]] auto canPing() -> bool;                  // Return true when another heartbeat may be emitted.
void updateLastSentTime();                             // Reset the ping idle timer after a payload transmission.
[[nodiscard]] auto isConnected() -> bool;              // Return true when the bridge is still considered online.
[[nodiscard]] auto isConnected(uint32_t now) -> bool;  // Return bridge liveness using a caller-provided timestamp.
}  // namespace BridgeSerial

#endif  // LSH_CORE_COMMUNICATION_BRIDGE_SERIAL_HPP
