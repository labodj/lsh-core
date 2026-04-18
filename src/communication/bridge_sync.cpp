/**
 * @file    bridge_sync.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the controller-side bridge handshake state machine.
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

#include "communication/bridge_sync.hpp"

#include "communication/constants/config.hpp"
#include "communication/serializer.hpp"

namespace BridgeSync
{
namespace
{
enum class State : uint8_t
{
    AwaitBridgeDetails = 0U,
    AwaitBridgeState = 1U,
    Synced = 2U,
};

State syncState = State::AwaitBridgeDetails;
uint32_t lastBootSentTime_ms = 0U;
uint32_t awaitingStateSince_ms = 0U;

void sendBoot(uint32_t now)
{
    Serializer::serializeStaticJson(constants::payloads::StaticType::BOOT);
    lastBootSentTime_ms = now;
}
}  // namespace

/**
 * @brief Mark the bridge as out of sync and send the initial `BOOT`.
 * @details This is called whenever the controller starts up. From that moment
 *          on the bridge must ask for `REQUEST_DETAILS` and `REQUEST_STATE`
 *          again before controller-side mutating commands are trusted.
 *
 * @param now Current loop timestamp.
 */
void begin(uint32_t now)
{
    syncState = State::AwaitBridgeDetails;
    awaitingStateSince_ms = 0U;
    sendBoot(now);
}

/**
 * @brief Re-open the handshake after the bridge explicitly requested a runtime resync.
 * @details Unlike `begin()`, this helper must not emit another `BOOT` back to the
 *          bridge. It only resets the controller-side trust gate so the caller can
 *          immediately serve fresh details and state in response to the bridge BOOT.
 *
 * @param now Current loop timestamp.
 */
void restartFromBridgeBoot(uint32_t now)
{
    syncState = State::AwaitBridgeDetails;
    awaitingStateSince_ms = 0U;
    lastBootSentTime_ms = now;
}

/**
 * @brief Advance the bridge synchronization state machine.
 * @details This hot-path helper emits periodic `PING_` payloads once the bridge
 *          is synchronized, retries `BOOT` while waiting for `REQUEST_DETAILS`,
 *          and times out back to the boot phase if the bridge stalls after
 *          requesting details but before requesting state.
 *
 * @param now Current loop timestamp.
 */
void tick(uint32_t now)
{
    using constants::bridgeSerial::BRIDGE_AWAIT_STATE_TIMEOUT_MS;
    using constants::bridgeSerial::BRIDGE_BOOT_RETRY_INTERVAL_MS;

    switch (syncState)
    {
    case State::Synced:
        Serializer::serializeStaticJson(constants::payloads::StaticType::PING_);
        break;

    case State::AwaitBridgeDetails:
        if ((now - lastBootSentTime_ms) >= BRIDGE_BOOT_RETRY_INTERVAL_MS)
        {
            sendBoot(now);
        }
        break;

    case State::AwaitBridgeState:
        if ((now - awaitingStateSince_ms) >= BRIDGE_AWAIT_STATE_TIMEOUT_MS)
        {
            syncState = State::AwaitBridgeDetails;
            awaitingStateSince_ms = 0U;
            sendBoot(now);
        }
        break;
    }
}

/**
 * @brief Record that `REQUEST_DETAILS` has just been served.
 * @details When the bridge is waiting for details this transitions the handshake
 *          into the phase where a subsequent `REQUEST_STATE` is expected.
 *          When already synchronized this remains a harmless read-only refresh.
 *
 * @param now Current loop timestamp.
 */
void onRequestDetailsServed(uint32_t now)
{
    if (syncState == State::Synced)
    {
        return;
    }

    if (syncState == State::AwaitBridgeDetails)
    {
        syncState = State::AwaitBridgeState;
        awaitingStateSince_ms = now;
    }
}

/**
 * @brief Record that `REQUEST_STATE` has just been served.
 * @details This closes the startup handshake only when the bridge was already
 *          in the `AwaitBridgeState` phase of the current synchronization session.
 */
void onRequestStateServed()
{
    if (syncState != State::AwaitBridgeState)
    {
        return;
    }

    syncState = State::Synced;
    awaitingStateSince_ms = 0U;
}

/**
 * @brief Return whether `REQUEST_STATE` may be served safely right now.
 * @details State snapshots are blocked while the bridge still has to request
 *          details first, otherwise it could interpret a fresh state using a
 *          stale topology model.
 *
 * @return true if state requests are currently safe to serve.
 * @return false if the bridge still needs details first.
 */
auto allowsStateRequests() -> bool
{
    return syncState != State::AwaitBridgeDetails;
}

/**
 * @brief Return whether inbound bridge commands may mutate controller state.
 * @details Mutating commands stay blocked until the bridge completed the
 *          `REQUEST_DETAILS -> REQUEST_STATE` handshake after the latest boot.
 *
 * @return true if mutating commands are currently trusted.
 * @return false if the handshake is still incomplete.
 */
auto allowsMutatingCommands() -> bool
{
    return syncState == State::Synced;
}
}  // namespace BridgeSync
