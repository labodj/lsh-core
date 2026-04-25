/**
 * @file    deserializer.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic for deserializing and dispatching commands.
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

#include "communication/deserializer.hpp"

#include "communication/bridge_sync.hpp"
#include "communication/constants/protocol.hpp"
#include "communication/serializer.hpp"
#include "config/static_config.hpp"
#include "core/network_clicks.hpp"
#include "device/actuator_manager.hpp"
#include "device/clickable_manager.hpp"
#include "peripherals/output/actuator.hpp"
#include "util/debug/debug.hpp"

namespace Deserializer
{
using namespace Debug;
using namespace lsh::core::protocol;
namespace
{
/**
 * @brief Validate one JSON scalar as an exact uint8_t.
 *
 * @param value JSON value to validate.
 * @param out Output byte written only when validation succeeds.
 * @return true if the value is an integer in the `[0, 255]` range.
 * @return false otherwise.
 */
[[nodiscard]] auto tryGetUint8Scalar(const JsonVariantConst &value, uint8_t &out) -> bool
{
    if (!value.is<uint8_t>())
    {
        return false;
    }

    out = value.as<uint8_t>();
    return true;
}

/**
 * @brief Validate one JSON scalar as a packed actuator-state byte.
 *
 * @param value JSON value to validate.
 * @param out Output byte written only when validation succeeds.
 * @return true if the value is an exact integer in the `[0, 255]` range.
 * @return false otherwise.
 */
[[nodiscard]] auto tryGetPackedStateByte(const JsonVariantConst &value, uint8_t &out) -> bool
{
    return tryGetUint8Scalar(value, out);
}

/**
 * @brief Validate ignored tail bits in the final packed state byte.
 *
 * Static profiles know the exact actuator count, so the last byte can reject
 * bits that do not map to a real actuator. For topologies with a multiple of
 * eight actuators the compiler elides the check entirely.
 *
 * @param byteIndex Current packed state byte index.
 * @param packedByte Byte received from the bridge.
 * @return true if the byte cannot address non-existing actuators.
 * @return false if unused tail bits are set.
 */
[[nodiscard]] constexpr auto packedStateByteHasValidTail(uint8_t byteIndex, uint8_t packedByte) -> bool
{
    constexpr uint8_t tailBits = static_cast<uint8_t>(CONFIG_MAX_ACTUATORS & 0x07U);
    if constexpr (tailBits == 0U)
    {
        static_cast<void>(byteIndex);
        static_cast<void>(packedByte);
        return true;
    }
    else
    {
        constexpr uint8_t lastByteIndex = static_cast<uint8_t>(CONFIG_PACKED_ACTUATOR_STATE_BYTES - 1U);
        constexpr uint8_t validTailMask = static_cast<uint8_t>((1U << tailBits) - 1U);
        return byteIndex != lastByteIndex || (packedByte & static_cast<uint8_t>(~validTailMask)) == 0U;
    }
}

template <uint8_t ByteIndex, uint8_t ByteCount> struct PackedStateApplier
{
    [[nodiscard]] static auto apply(const JsonArrayConst &statesArray, bool &anyStateChanged) -> bool
    {
        uint8_t packedByte = 0U;
        if (!tryGetPackedStateByte(statesArray[ByteIndex], packedByte))
        {
            return false;
        }
        if (!packedStateByteHasValidTail(ByteIndex, packedByte))
        {
            return false;
        }
        anyStateChanged |= lsh::core::static_config::applyPackedActuatorStateByte(ByteIndex, packedByte);
        return PackedStateApplier<static_cast<uint8_t>(ByteIndex + 1U), ByteCount>::apply(statesArray, anyStateChanged);
    }
};

template <uint8_t ByteCount> struct PackedStateApplier<ByteCount, ByteCount>
{
    [[nodiscard]] static auto apply(const JsonArrayConst &statesArray, bool &anyStateChanged) -> bool
    {
        static_cast<void>(statesArray);
        static_cast<void>(anyStateChanged);
        return true;
    }
};

/**
 * @brief Validate one JSON scalar as a binary actuator state.
 *
 * @param value JSON value to validate.
 * @param out Output boolean written only when validation succeeds.
 * @return true if the value is exactly `0` or `1`.
 * @return false otherwise.
 */
[[nodiscard]] auto tryGetBinaryState(const JsonVariantConst &value, bool &out) -> bool
{
    uint8_t rawState = 0U;
    if (!tryGetPackedStateByte(value, rawState) || rawState > 1U)
    {
        return false;
    }

    out = rawState == 1U;
    return true;
}

#if CONFIG_USE_NETWORK_CLICKS
/**
 * @brief Processes payloads for network click acknowledgements and failovers.
 * @details This helper function handles the shared logic for both
 *          `NETWORK_CLICK_ACK` and `FAILOVER_CLICK`. Every scalar is validated
 *          explicitly so malformed bridge payloads are rejected at the final
 *          semantic boundary instead of being coerced into integer defaults.
 *
 * @param doc The const JsonDocument containing the payload.
 * @param cmd The specific command (either NETWORK_CLICK_ACK or FAILOVER_CLICK).
 * @param result A reference to the DispatchResult struct to be modified with the outcome.
 */
void processNetworkClickResponse(const JsonDocument &doc, lsh::core::protocol::Command cmd, DispatchResult &result)
{
    uint8_t jsonClickType = 0U;
    uint8_t clickableId = 0U;
    uint8_t correlationId = 0U;
    if (!tryGetUint8Scalar(doc[KEY_TYPE], jsonClickType) || !tryGetUint8Scalar(doc[KEY_ID], clickableId) ||
        !tryGetUint8Scalar(doc[KEY_CORRELATION_ID], correlationId))
    {
        return;
    }

    constants::ClickType clickType = constants::ClickType::NONE;
    switch (static_cast<lsh::core::protocol::ProtocolClickType>(jsonClickType))
    {
    case lsh::core::protocol::ProtocolClickType::LONG:
        clickType = constants::ClickType::LONG;
        break;
    case lsh::core::protocol::ProtocolClickType::SUPER_LONG:
        clickType = constants::ClickType::SUPER_LONG;
        break;
    default:
        DPL(jsonClickType);
        return;  // Invalid click type (was 0 or other value)
    }

    const uint8_t clickableIndex = lsh::core::static_config::getClickableIndexById(clickableId);
    if (clickableIndex >= CONFIG_MAX_CLICKABLES)
    {
        return;
    }
    if (!NetworkClicks::matchesCorrelationId(clickableIndex, clickType, correlationId))
    {
        DPL("Ignoring stale or mismatched network click response for clickable ID ", clickableId, " with correlation ID ", correlationId,
            ".");
        return;
    }

    if (cmd == Command::FAILOVER_CLICK)
    {
        result.stateChanged = NetworkClicks::checkNetworkClickTimer(clickableIndex, clickType, true);
    }
    else if (cmd == Command::NETWORK_CLICK_ACK)
    {
        if (!NetworkClicks::isNetworkClickExpired(clickableIndex, clickType))
        {
            result.networkClickHandled = NetworkClicks::confirm(clickableIndex, clickType);
        }
    }
}
#endif
}  // namespace

/**
 * @brief Main entry point for command processing. Consumes one decoded bridge
 * document and immediately dispatches the corresponding action.
 * @details This function acts as a command dispatcher. It reads the command ID
 * from the 'p' key and uses a switch statement for O(1) dispatching.
 * It directly calls functions in other modules (Serializer, Actuators, NetworkClicks)
 * to execute the command. This avoids intermediate state storage (like ResultsHolder)
 * and multiple switch statements in the main loop, maximizing performance.
 * The bridge runtime is intentionally semi-transparent and may raw-forward payloads it
 * does not need to optimize locally, so this dispatcher is also the final semantic
 * validation boundary for inbound LSH commands.
 * Every scalar that matters semantically is validated explicitly so malformed
 * bridge payloads are rejected here instead of being silently coerced by
 * ArduinoJson conversions.
 *
 * @param doc Parsed ArduinoJson document from BridgeSerial, regardless of the
 *            active wire codec.
 * @return DispatchResult A struct indicating the side-effects of the command,
 * telling the main loop if a general state update needs to be sent or if
 * network click timers need to be checked.
 */
auto deserializeAndDispatch(const JsonDocument &doc) -> DispatchResult
{
    DispatchResult result;  // Default: { false, false }

    uint8_t rawCommand = 0U;
    if (!tryGetUint8Scalar(doc[KEY_PAYLOAD], rawCommand))
    {
        return result;
    }

    const auto cmd = static_cast<Command>(rawCommand);

#if CONFIG_USE_NETWORK_CLICKS
    if (cmd == Command::NETWORK_CLICK_ACK || cmd == Command::FAILOVER_CLICK)
    {
        if (BridgeSync::allowsMutatingCommands())
        {
            processNetworkClickResponse(doc, cmd, result);
        }
        return result;
    }
#else
    if (cmd == Command::NETWORK_CLICK_ACK || cmd == Command::FAILOVER_CLICK)
    {
        return result;
    }
#endif

    switch (cmd)
    {
    case Command::SET_SINGLE_ACTUATOR:
        if (!BridgeSync::allowsMutatingCommands())
        {
            break;
        }
        // Get values from Json
        {
            bool state = false;
            if (!tryGetBinaryState(doc[KEY_STATE], state))
            {
                break;  // Wrong or missing jsonState
            }

            uint8_t id = 0U;
            if (!tryGetUint8Scalar(doc[KEY_ID], id))
            {
                break;
            }
            result.stateChanged = lsh::core::static_config::setActuatorStateById(id, state);
            break;
        }
    case Command::SET_STATE:
    {
        if (!BridgeSync::allowsMutatingCommands())
        {
            break;
        }

        const JsonArrayConst statesArray = doc[KEY_STATE];
        if (statesArray.isNull())
        {
            break;
        }

        constexpr uint8_t expectedBytes = CONFIG_PACKED_ACTUATOR_STATE_BYTES;
        if (statesArray.size() != expectedBytes)
        {
            break;
        }

        bool anyStateChanged = false;
        if (!PackedStateApplier<0U, expectedBytes>::apply(statesArray, anyStateChanged))
        {
            return result;
        }
        result.stateChanged = anyStateChanged;
        break;
    }

    case Command::FAILOVER:
#if CONFIG_USE_NETWORK_CLICKS
        if (!BridgeSync::allowsMutatingCommands())
        {
            break;
        }
        result.stateChanged = NetworkClicks::checkAllNetworkClicksTimers(true);
#else
        // Failover has no local meaning if the whole network-click subsystem was removed.
#endif
        break;

    case Command::REQUEST_STATE:
        if (!BridgeSync::allowsStateRequests())
        {
            break;
        }
        if (Serializer::serializeActuatorsState())
        {
            BridgeSync::onRequestStateServed();
            result.stateChanged = false;
        }
        break;

    case Command::REQUEST_DETAILS:
        if (Serializer::serializeDetails())
        {
            BridgeSync::onRequestDetailsServed();
            result.stateChanged = false;
        }
        break;

    case Command::BOOT:
        // BOOT is the only supported topology resync trigger. The controller topology is
        // static between reboots, so this always means "send fresh details and full state".
        // Re-open the bridge-sync gate first so this resync is reflected by the same
        // state machine that guards later mutating commands.
        BridgeSync::restartFromBridgeBoot();
        if (Serializer::serializeDetails())
        {
            BridgeSync::onRequestDetailsServed();
            if (Serializer::serializeActuatorsState())
            {
                BridgeSync::onRequestStateServed();
                result.stateChanged = false;
            }
        }
        break;

    case Command::PING_:
        break;

    default:
        DPL("Unknown or missing command ID: ", static_cast<uint8_t>(cmd));
        break;
    }
    return result;
}
}  // namespace Deserializer
