/**
 * @file    bridge_sync.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Tracks whether the ESP bridge has completed the post-boot handshake.
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

#ifndef LSHCORE_COMMUNICATION_BRIDGE_SYNC_HPP
#define LSHCORE_COMMUNICATION_BRIDGE_SYNC_HPP

#include <stdint.h>

namespace BridgeSync
{
    /**
     * @brief Marks the bridge as out-of-sync and sends the initial BOOT.
     * @details The bridge must subsequently ask for REQUEST_DETAILS and then
     *          REQUEST_STATE before controller commands are accepted again.
     */
    void begin(uint32_t now);

    /**
     * @brief Drives the bridge sync finite-state machine from the hot 1 ms loop branch.
     * @details - When synchronized, emits the normal hop-local PING cadence.
     *          - When waiting for REQUEST_DETAILS, periodically retries BOOT.
     *          - When waiting for REQUEST_STATE, times out back to BOOT retries.
     */
    void tick(uint32_t now);

    /**
     * @brief Records that REQUEST_DETAILS has just been served to the bridge.
     * @details Only affects the pending handshake states; when already synced,
     *          a runtime REQUEST_DETAILS remains a harmless read-only snapshot.
     */
    void onRequestDetailsServed(uint32_t now);

    /**
     * @brief Records that REQUEST_STATE has just been served to the bridge.
     * @details This closes the pending startup handshake only if the bridge had
     *          previously requested details in the same pending session.
     */
    void onRequestStateServed();

    /**
     * @brief Returns whether REQUEST_STATE may be served safely right now.
     * @details State requests are blocked while waiting for the bridge to ask
     *          for details first, preventing a stale bridge from interpreting a
     *          fresh state snapshot with an outdated topology.
     */
    [[nodiscard]] auto allowsStateRequests() -> bool;

    /**
     * @brief Returns whether inbound bridge commands may mutate controller state.
     * @details Mutating commands are ignored until the bridge has completed the
     *          REQUEST_DETAILS -> REQUEST_STATE handshake after a controller boot.
     */
    [[nodiscard]] auto allowsMutatingCommands() -> bool;
} // namespace BridgeSync

#endif // LSHCORE_COMMUNICATION_BRIDGE_SYNC_HPP
