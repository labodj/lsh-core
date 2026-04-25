/**
 * @file    actuator_manager.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic for managing and operating on all system Actuators.
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

#include "device/actuator_manager.hpp"

#include "config/static_config.hpp"
#include "internal/user_config_bridge.hpp"
#include "peripherals/output/actuator.hpp"
#include "util/constants/config.hpp"
#include "util/constants/wrong_config_strings.hpp"
#include "util/debug/debug.hpp"
#include "util/reset.hpp"

namespace Actuators
{
using namespace Debug;
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
etl::array<Actuator *, CONFIG_MAX_ACTUATORS> actuators{};  //!< All device actuators (like relays)
#endif
PackedActuatorStateBytes packedActuatorStates{};  //!< Canonical packed actuator-state shadow kept in sync with `Actuator::setState()`.

namespace
{
#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES
etl::array<uint32_t, constants::config::AUTO_OFF_STORAGE_CAPACITY>
    autoOffLastSwitchTimes{};  //!< Dynamic switch timestamps for static auto-off actuator entries.
#endif

/**
 * @brief Abort setup when the dense actuator prefix was corrupted.
 */
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
void failNonCompactActuatorStorage()
{
    using namespace constants::wrongConfigStrings;
    NDSB();
    CONFIG_DEBUG_SERIAL->print(FPSTR(WRONG));
    CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
    CONFIG_DEBUG_SERIAL->print(FPSTR(ACTUATORS));
    CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
    CONFIG_DEBUG_SERIAL->println(F("storage"));
    delay(10000);
    deviceReset();
}
#endif

}  // namespace

/**
 * @brief Return the static wire ID for one registered actuator index.
 *
 * @param actuatorIndex dense runtime actuator index.
 * @return uint8_t actuator ID, or zero when the index is outside the static profile.
 */
auto getId(uint8_t actuatorIndex) -> uint8_t
{
    return lsh::core::static_config::getActuatorId(actuatorIndex);
}

/**
 * @brief Get a single actuator.
 *
 * @param actuatorId actuator UUID.
 * @return Actuator* A single actuator when the ID exists.
 * @return nullptr When the ID is unknown.
 */
auto getActuator(uint8_t actuatorId) -> Actuator *
{
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
    uint8_t actuatorIndex = UINT8_MAX;
    if (!tryGetIndex(actuatorId, actuatorIndex))
    {
        return nullptr;
    }
    return actuators[actuatorIndex];
#else
    static_cast<void>(actuatorId);
    return nullptr;
#endif
}

/**
 * @brief Get a single actuator index (in device vector of actuators).
 *
 * @param actuatorId actuator UUID.
 * @return uint8_t A single actuator index (in device vector of actuators).
 * @return UINT8_MAX When the ID is unknown.
 */
auto getIndex(uint8_t actuatorId) -> uint8_t
{
    uint8_t actuatorIndex = UINT8_MAX;
    if (!tryGetIndex(actuatorId, actuatorIndex))
    {
        return UINT8_MAX;
    }
    return actuatorIndex;
}

/**
 * @brief Resolves an actuator ID to its dense runtime index with a single map lookup.
 *
 * @param actuatorId Actuator UUID.
 * @param actuatorIndex Output runtime index when the ID exists.
 * @return true if the actuator exists.
 * @return false otherwise.
 */
auto tryGetIndex(uint8_t actuatorId, uint8_t &actuatorIndex) -> bool
{
    const uint8_t configuredIndex = lsh::core::static_config::getActuatorIndexById(actuatorId);
    if (configuredIndex >= CONFIG_MAX_ACTUATORS)
    {
        return false;
    }
    actuatorIndex = configuredIndex;
    return true;
}

/**
 * @brief Get if the actuator actually exists.
 *
 * @param actuatorId Unique ID of the actuator.
 * @return true if actuator exists.
 * @return false if actuator doesn't exist.
 */
auto actuatorExists(uint8_t actuatorId) -> bool
{
    uint8_t actuatorIndex = UINT8_MAX;
    return tryGetIndex(actuatorId, actuatorIndex);
}

/**
 * @brief Records the latest switch time for an actuator when compact actuator
 *        timer storage is enabled.
 *
 * @param actuatorIndex dense runtime actuator index.
 * @param now_ms cached switch timestamp.
 */
void recordSwitchTime(uint8_t actuatorIndex, uint32_t now_ms)
{
#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES && LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS > 0
    const uint8_t entryIndex = lsh::core::static_config::getAutoOffIndexByActuatorIndex(actuatorIndex);
    if (entryIndex != UINT8_MAX)
    {
        autoOffLastSwitchTimes[entryIndex] = now_ms;
    }
#else
    static_cast<void>(actuatorIndex);
    static_cast<void>(now_ms);
#endif
}

#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES && LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS > 0
/**
 * @brief Check one generated compact auto-off entry.
 * @details The generated profile owns the actuator/timer pairing, while this
 *          manager keeps the compact timestamp pool private. This helper is
 *          therefore the only bridge between generated direct code and the
 *          timestamp storage.
 *
 * @param autoOffIndex Dense index inside the compact auto-off timestamp pool.
 * @param actuatorIndex Dense static-profile actuator index used for packed-state updates.
 * @param actuator Actuator controlled by the generated entry.
 * @param now_ms Cached current time.
 * @param timer_ms Auto-off timeout for the actuator.
 * @return true if the actuator was switched off.
 */
auto checkCompactAutoOffTimer(uint8_t autoOffIndex, uint8_t actuatorIndex, Actuator &actuator, uint32_t now_ms, uint32_t timer_ms) -> bool
{
    if (autoOffIndex >= constants::config::MAX_AUTO_OFF_ACTUATORS)
    {
        return false;
    }
    if (actuator.getState() && (now_ms - autoOffLastSwitchTimes[autoOffIndex] >= timer_ms))
    {
        return actuator.setStateForIndex(actuatorIndex, false, now_ms);
    }
    return false;
}
#endif

/**
 * @brief Keep the compact actuator-state shadow aligned with one runtime state change.
 *
 * @param actuatorIndex dense runtime actuator index.
 * @param state new actuator state.
 */
void updatePackedState(uint8_t actuatorIndex, bool state)
{
    if (actuatorIndex >= CONFIG_MAX_ACTUATORS)
    {
        return;
    }

    // This shadow intentionally tracks dense runtime index, not logical ID:
    // the controller protocol serializes actuator state in compact runtime order.
    const uint8_t byteIndex = static_cast<uint8_t>(actuatorIndex >> 3U);
    const uint8_t bitMask = static_cast<uint8_t>(1U << (actuatorIndex & 0x07U));
    if (state)
    {
        packedActuatorStates[byteIndex] |= bitMask;
    }
    else
    {
        packedActuatorStates[byteIndex] &= static_cast<uint8_t>(~bitMask);
    }
}

/**
 * @brief Return the number of packed bytes required by the current topology.
 */
auto getPackedStateByteCount() -> uint8_t
{
    return CONFIG_PACKED_ACTUATOR_STATE_BYTES;
}

/**
 * @brief Return one packed state byte in protocol wire order.
 *
 * @param byteIndex packed byte index.
 * @return uint8_t packed actuator-state byte, or zero when out of range.
 */
auto getPackedStateByte(uint8_t byteIndex) -> uint8_t
{
    if (byteIndex >= CONFIG_PACKED_ACTUATOR_STATE_BYTES)
    {
        return 0U;
    }

    return packedActuatorStates[byteIndex];
}

/**
 * @brief Final validation after actuator registration.
 *
 */
void finalizeSetup()
{
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
    DP_CONTEXT();
    for (uint8_t actuatorIndex = 0U; actuatorIndex < CONFIG_MAX_ACTUATORS; ++actuatorIndex)
    {
        const uint8_t actuatorId = getId(actuatorIndex);
        if (actuators[actuatorIndex] == nullptr || actuators[actuatorIndex]->getIndex() != actuatorIndex || actuatorId == 0U ||
            lsh::core::static_config::getActuatorIndexById(actuatorId) != actuatorIndex)
        {
            failNonCompactActuatorStorage();
        }
    }
    for (uint8_t entryIndex = 0U; entryIndex < constants::config::MAX_AUTO_OFF_ACTUATORS; ++entryIndex)
    {
        const uint8_t actuatorIndex = lsh::core::static_config::getAutoOffActuatorIndex(entryIndex);
        if (actuatorIndex >= CONFIG_MAX_ACTUATORS || actuators[actuatorIndex] == nullptr ||
            lsh::core::static_config::getAutoOffTimer(actuatorIndex) == 0UL)
        {
            failNonCompactActuatorStorage();
        }
    }
#endif
}

}  // namespace Actuators
