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
#include "util/time_keeper.hpp"

namespace Actuators
{
using namespace Debug;
etl::array<Actuator *, CONFIG_MAX_ACTUATORS> actuators{};  //!< All device actuators (like relays)
PackedActuatorStateBytes packedActuatorStates{};  //!< Canonical packed actuator-state shadow kept in sync with `Actuator::setState()`.

namespace
{
#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES
etl::array<uint32_t, constants::config::AUTO_OFF_STORAGE_CAPACITY>
    autoOffLastSwitchTimes{};  //!< Dynamic switch timestamps for static auto-off actuator entries.
#endif

/**
 * @brief Abort setup when one actuator uses an invalid numeric ID.
 * @details Actuator ID zero is reserved as "missing" inside the wire protocol
 *          and in the bounded lookup tables, so configuration must reject it.
 */
void failWrongActuatorId()
{
    using namespace constants::wrongConfigStrings;
    NDSB();
    CONFIG_DEBUG_SERIAL->print(FPSTR(WRONG));
    CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
    CONFIG_DEBUG_SERIAL->print(FPSTR(ACTUATORS));
    CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
    CONFIG_DEBUG_SERIAL->println(FPSTR(ID));
    delay(10000);
    deviceReset();
}

/**
 * @brief Abort setup when an auto-off timer is attached to an unregistered actuator.
 */
void failWrongAutoOffActuator()
{
    using namespace constants::wrongConfigStrings;
    NDSB();
    CONFIG_DEBUG_SERIAL->print(FPSTR(WRONG));
    CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
    CONFIG_DEBUG_SERIAL->print(FPSTR(ACTUATORS));
    CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
    CONFIG_DEBUG_SERIAL->println(F("auto-off"));
    delay(10000);
    deviceReset();
}

/**
 * @brief Abort setup when the dense actuator prefix was corrupted.
 */
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

}  // namespace

/**
 * @brief Adds an actuator to the system.
 *
 * The actuator is stored in the main array slot selected by the generated static profile.
 * If the maximum number of actuators is exceeded, the device will reset to prevent undefined behavior.
 *
 * @param actuator A pointer to the Actuator object to add.
 * @param actuatorId Static wire ID of the actuator.
 * @param actuatorIndex Dense runtime index of the actuator.
 */
void addActuator(Actuator *const actuator, uint8_t actuatorId, uint8_t actuatorIndex)
{
    if (actuatorIndex >= CONFIG_MAX_ACTUATORS || actuators[actuatorIndex] != nullptr)
    {
        using namespace constants::wrongConfigStrings;
        NDSB();  // Begin serial if not in debug mode
        CONFIG_DEBUG_SERIAL->print(FPSTR(WRONG));
        CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
        CONFIG_DEBUG_SERIAL->print(FPSTR(ACTUATORS));
        CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
        CONFIG_DEBUG_SERIAL->println(FPSTR(NUMBER));
        delay(10000);
        deviceReset();
    }

    if (actuatorId == 0U)
    {
        failWrongActuatorId();
    }

    const uint8_t configuredIndex = lsh::core::static_config::getActuatorIndexById(actuatorId);
    if (configuredIndex != actuatorIndex)
    {
        failWrongActuatorId();
    }

    actuator->setIndex(actuatorIndex);    // Store current index inside the object, it can be useful
    actuators[actuatorIndex] = actuator;  // Insert in array of actuators

    DPL(FPSTR(dStr::ACTUATOR), FPSTR(dStr::SPACE), FPSTR(dStr::UUID), FPSTR(dStr::COLON_SPACE), actuatorId, FPSTR(dStr::SPACE),
        FPSTR(dStr::DIVIDER), FPSTR(dStr::SPACE), FPSTR(dStr::INDEX), FPSTR(dStr::COLON_SPACE), actuatorIndex);

    updatePackedState(actuatorIndex, actuator->getState());
}

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
    uint8_t actuatorIndex = UINT8_MAX;
    if (!tryGetIndex(actuatorId, actuatorIndex))
    {
        return nullptr;
    }
    return actuators[actuatorIndex];
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
    if (configuredIndex >= CONFIG_MAX_ACTUATORS || actuators[configuredIndex] == nullptr)
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
 * @brief Validate an auto-off timer against the generated static profile.
 *
 * @param actuatorIndex dense runtime actuator index.
 * @param time_ms expected timer duration; zero is valid only for actuators without auto-off.
 */
void setAutoOffTimer(uint8_t actuatorIndex, uint32_t time_ms)
{
    if (actuatorIndex >= CONFIG_MAX_ACTUATORS || actuators[actuatorIndex] == nullptr)
    {
        failWrongAutoOffActuator();
    }

    if (lsh::core::static_config::getAutoOffTimer(actuatorIndex) != time_ms)
    {
        failWrongAutoOffActuator();
    }
}

/**
 * @brief Return true when one actuator has an active auto-off timer.
 *
 * @param actuatorIndex dense runtime actuator index.
 * @return true if a non-zero timer is configured.
 * @return false otherwise.
 */
auto hasAutoOffTimer(uint8_t actuatorIndex) -> bool
{
    return lsh::core::static_config::getAutoOffTimer(actuatorIndex) != 0UL;
}

/**
 * @brief Return an actuator auto-off timer.
 *
 * @param actuatorIndex dense runtime actuator index.
 * @return uint32_t timer duration, or zero when disabled.
 */
auto getAutoOffTimer(uint8_t actuatorIndex) -> uint32_t
{
    return lsh::core::static_config::getAutoOffTimer(actuatorIndex);
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
#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES
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

/**
 * @brief Check one actuator auto-off deadline using manager-owned compact state.
 *
 * @param actuatorIndex dense runtime actuator index.
 * @param autoOffTimer_ms auto-off timeout to evaluate.
 * @return true if the actuator was switched off.
 * @return false otherwise.
 */
auto checkAutoOffTimer(uint8_t actuatorIndex, uint32_t autoOffTimer_ms) -> bool
{
    if (actuatorIndex >= CONFIG_MAX_ACTUATORS || actuators[actuatorIndex] == nullptr || autoOffTimer_ms == 0U)
    {
        return false;
    }

#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES
    const uint8_t entryIndex = lsh::core::static_config::getAutoOffIndexByActuatorIndex(actuatorIndex);
    if (entryIndex == UINT8_MAX)
    {
        return false;
    }

    auto *const actuator = actuators[actuatorIndex];
    if (actuator->getState() && (timeKeeper::getTime() - autoOffLastSwitchTimes[entryIndex] >= autoOffTimer_ms))
    {
        return actuator->setState(false);
    }
    return false;
#else
    return actuators[actuatorIndex]->checkAutoOffTimer(autoOffTimer_ms);
#endif
}

/**
 * @brief Performs an auto-off timers check for actuators.
 *
 * @return true if any actuator has been automatically switched off.
 * @return false otherwise.
 */
auto actuatorsAutoOffTimersCheck() -> bool
{
    auto *const local_actuators = actuators.data();  // Cache the pointer to the actuators array
    bool somethingSwitchedOff = false;
#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES
    const uint32_t now = timeKeeper::getTime();
#endif
    for (uint8_t entryIndex = 0U; entryIndex < constants::config::MAX_AUTO_OFF_ACTUATORS; ++entryIndex)
    {
        const uint8_t actuatorIndex = lsh::core::static_config::getAutoOffActuatorIndex(entryIndex);
        const uint32_t timer_ms = lsh::core::static_config::getAutoOffTimer(actuatorIndex);
#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES
        auto *const actuator = local_actuators[actuatorIndex];
        if (actuator->getState() && (now - autoOffLastSwitchTimes[entryIndex] >= timer_ms))
        {
            somethingSwitchedOff |= actuator->setState(false);
        }
#else
        somethingSwitchedOff |= local_actuators[actuatorIndex]->checkAutoOffTimer(timer_ms);
#endif
    }
    return somethingSwitchedOff;
}

/**
 * @brief Turns off all actuators.
 *
 * @return true if any actuator performed the switch.
 * @return false otherwise.
 */
auto turnOffAllActuators() -> bool
{
    return lsh::core::static_config::turnOffAllActuators();
}

/**
 * @brief Turns off all unprotected actuators.
 *
 * @return true if any actuator performed the switch.
 * @return false otherwise.
 */
auto turnOffUnprotectedActuators() -> bool
{
    return lsh::core::static_config::turnOffUnprotectedActuators();
}

/**
 * @brief Set the state for all actuators.
 *
 * @param states An array of boolean states to be set. The size must match the number of actuators.
 * @return true if any actuator performed the switch.
 * @return false otherwise.
 */
auto setAllActuatorsState(const etl::array<bool, CONFIG_MAX_ACTUATORS> &states) -> bool
{
    DP_CONTEXT();
    bool anySwitchPerformed = false;
    auto *const actuatorBegin = actuators.data();
    auto *const actuatorEnd = actuatorBegin + CONFIG_MAX_ACTUATORS;
    const bool *stateIt = states.data();
    for (auto *currActuator = actuatorBegin; currActuator != actuatorEnd; ++currActuator, ++stateIt)
    {
        anySwitchPerformed |= (*currActuator)->setState(*stateIt);
    }
    return anySwitchPerformed;
}

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
}

}  // namespace Actuators
