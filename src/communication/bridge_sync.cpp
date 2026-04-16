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
    } // namespace

    void begin(uint32_t now)
    {
        syncState = State::AwaitBridgeDetails;
        awaitingStateSince_ms = 0U;
        sendBoot(now);
    }

    void tick(uint32_t now)
    {
        using constants::espComConfigs::BRIDGE_AWAIT_STATE_TIMEOUT_MS;
        using constants::espComConfigs::BRIDGE_BOOT_RETRY_INTERVAL_MS;

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

    void onRequestStateServed()
    {
        if (syncState != State::AwaitBridgeState)
        {
            return;
        }

        syncState = State::Synced;
        awaitingStateSince_ms = 0U;
    }

    auto allowsStateRequests() -> bool
    {
        return syncState != State::AwaitBridgeDetails;
    }

    auto allowsMutatingCommands() -> bool
    {
        return syncState == State::Synced;
    }
} // namespace BridgeSync
