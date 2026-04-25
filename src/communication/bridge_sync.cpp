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
#include "util/saturating_time.hpp"

namespace BridgeSync
{
namespace
{
/**
 * @brief Internal handshake phases of the controller-to-bridge synchronization state machine.
 */
enum class State : uint8_t
{
    AwaitBridgeDetails = 0U,
    AwaitBridgeState = 1U,
    Synced = 2U,
};

constexpr uint16_t TIMER_READY = UINT16_MAX;  //!< Saturated age used when a retry should be allowed immediately.

/** @brief Current controller-side handshake phase with the bridge runtime. */
State syncState = State::AwaitBridgeDetails;
/** @brief Saturated age since the last BOOT payload accepted by the UART. */
uint16_t bootRetryAge_ms = TIMER_READY;
/** @brief Saturated age spent waiting for `REQUEST_STATE` after details were served. */
uint16_t awaitingStateAge_ms = 0U;

/**
 * @brief Emit one `BOOT` payload and remember when it was sent.
 *
 */
auto sendBoot() -> bool
{
    if (!Serializer::serializeStaticPayload(constants::payloads::StaticType::BOOT))
    {
        return false;
    }

    bootRetryAge_ms = 0U;
    return true;
}
}  // namespace

/**
 * @brief Mark the bridge as out of sync and send the initial `BOOT`.
 * @details This is called whenever the controller starts up. From that moment
 *          on the bridge must ask for `REQUEST_DETAILS` and `REQUEST_STATE`
 *          before controller-side mutating commands are trusted.
 *
 */
void begin()
{
    syncState = State::AwaitBridgeDetails;
    awaitingStateAge_ms = 0U;
    bootRetryAge_ms = TIMER_READY;
    (void)sendBoot();
}

/**
 * @brief Re-open the handshake after the bridge explicitly requested a runtime resync.
 * @details Unlike `begin()`, this helper must not emit another `BOOT` back to the
 *          bridge. It only resets the controller-side trust gate so the caller can
 *          immediately serve fresh details and state in response to the bridge BOOT.
 *
 */
void restartFromBridgeBoot()
{
    syncState = State::AwaitBridgeDetails;
    awaitingStateAge_ms = 0U;
    bootRetryAge_ms = 0U;
}

/**
 * @brief Advance the bridge synchronization state machine.
 * @details This hot-path helper emits periodic `PING_` payloads once the bridge
 *          is synchronized, retries `BOOT` while waiting for `REQUEST_DETAILS`,
 *          and times out back to the boot phase if the bridge stalls after
 *          requesting details but before requesting state.
 *
 * @param elapsed_ms Milliseconds elapsed since the previous bridge housekeeping pass.
 */
void tick(uint16_t elapsed_ms)
{
    using constants::bridgeSerial::BRIDGE_AWAIT_STATE_TIMEOUT_MS;
    using constants::bridgeSerial::BRIDGE_BOOT_RETRY_INTERVAL_MS;

    if (elapsed_ms != 0U)
    {
        bootRetryAge_ms = timeUtils::addElapsedTimeSaturated(bootRetryAge_ms, elapsed_ms);
        awaitingStateAge_ms = timeUtils::addElapsedTimeSaturated(awaitingStateAge_ms, elapsed_ms);
    }

    switch (syncState)
    {
    case State::Synced:
        if (!Serializer::serializeStaticPayload(constants::payloads::StaticType::PING_))
        {
            // Either the heartbeat is still throttled or the UART rejected the
            // frame. In both cases the next tick will re-evaluate normally.
        }
        break;

    case State::AwaitBridgeDetails:
        if (bootRetryAge_ms >= BRIDGE_BOOT_RETRY_INTERVAL_MS)
        {
            (void)sendBoot();
        }
        break;

    case State::AwaitBridgeState:
        if (awaitingStateAge_ms >= BRIDGE_AWAIT_STATE_TIMEOUT_MS)
        {
            syncState = State::AwaitBridgeDetails;
            awaitingStateAge_ms = 0U;
            bootRetryAge_ms = TIMER_READY;
            (void)sendBoot();
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
 */
void onRequestDetailsServed()
{
    if (syncState == State::Synced)
    {
        return;
    }

    if (syncState == State::AwaitBridgeDetails)
    {
        syncState = State::AwaitBridgeState;
        awaitingStateAge_ms = 0U;
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
    awaitingStateAge_ms = 0U;
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
