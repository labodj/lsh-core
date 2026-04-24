/**
 * @file    bridge_sync.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Tracks whether `lsh-bridge` has completed the post-boot handshake.
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

#ifndef LSH_CORE_COMMUNICATION_BRIDGE_SYNC_HPP
#define LSH_CORE_COMMUNICATION_BRIDGE_SYNC_HPP

#include <stdint.h>

namespace BridgeSync
{
void begin();                                         // Start a fresh bridge handshake and send the first BOOT.
void restartFromBridgeBoot();                         // Re-open the handshake after a runtime BOOT request received from the bridge.
void tick(uint16_t elapsed_ms);                       // Advance the bridge-sync state machine using the elapsed housekeeping time.
void onRequestDetailsServed();                        // Record that bridge details were served in the current session.
void onRequestStateServed();                          // Record that bridge state was served in the current session.
[[nodiscard]] auto allowsStateRequests() -> bool;     // Return true when REQUEST_STATE can be served safely.
[[nodiscard]] auto allowsMutatingCommands() -> bool;  // Return true when inbound bridge commands may change state.
}  // namespace BridgeSync

#endif  // LSH_CORE_COMMUNICATION_BRIDGE_SYNC_HPP
