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
uint8_t totalActuators = 0U;                               //!< Device real total Actuators
etl::array<Actuator *, CONFIG_MAX_ACTUATORS> actuators{};  //!< All device actuators (like relays)
#if CONFIG_USE_ACTUATOR_ID_LUT
etl::array<uint8_t, CONFIG_MAX_ACTUATOR_ID + 1U> actuatorIndexById{};  //!< Device actuators lookup (UUID -> actuator index + 1)
static_assert(CONFIG_MAX_ACTUATOR_ID > 0U, "CONFIG_MAX_ACTUATOR_ID must be greater than zero when the actuator ID LUT is enabled.");
#else
etl::map<uint8_t, uint8_t, CONFIG_MAX_ACTUATORS> actuatorsMap{};  //!< Device actuators map (UUID -> actuator index)
#endif
PackedActuatorStateBitset packedActuatorStates{};  //!< Canonical packed actuator-state shadow kept in sync with `Actuator::setState()`.

namespace
{
struct AutoOffTimerEntry
{
    uint32_t timer_ms = 0U;
#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES
    uint32_t lastSwitchTime_ms = 0U;
#endif
    uint8_t actuatorIndex = 0U;
};

etl::array<AutoOffTimerEntry, constants::config::AUTO_OFF_STORAGE_CAPACITY>
    autoOffTimers{};              //!< Shared storage for actuators that actually have an auto-off timer.
uint8_t totalAutoOffTimers = 0U;  //!< Number of active entries inside `autoOffTimers`.

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
 * @brief Abort setup when the configured auto-off pool is too small.
 */
void failAutoOffTimerOverflow()
{
    using namespace constants::wrongConfigStrings;
    NDSB();
    CONFIG_DEBUG_SERIAL->print(FPSTR(WRONG));
    CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
    CONFIG_DEBUG_SERIAL->print(FPSTR(ACTUATORS));
    CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
    CONFIG_DEBUG_SERIAL->println(F("auto-off timers number"));
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

/**
 * @brief Find the shared auto-off timer entry for an actuator.
 *
 * @param actuatorIndex dense runtime actuator index.
 * @return uint8_t entry index, or UINT8_MAX when no timer is configured.
 */
auto findAutoOffTimerEntry(uint8_t actuatorIndex) -> uint8_t
{
    for (uint8_t entryIndex = 0U; entryIndex < totalAutoOffTimers; ++entryIndex)
    {
        if (autoOffTimers[entryIndex].actuatorIndex == actuatorIndex)
        {
            return entryIndex;
        }
    }
    return UINT8_MAX;
}

#if CONFIG_USE_ACTUATOR_ID_LUT
/**
 * @brief Abort setup when two actuators reuse the same numeric ID.
 * @details The bounded LUT requires a one-to-one mapping between wire ID and
 *          dense runtime index. Duplicate IDs would make later lookups ambiguous.
 */
void failDuplicateActuatorId()
{
    using namespace constants::wrongConfigStrings;
    NDSB();
    CONFIG_DEBUG_SERIAL->print(FPSTR(DUPLICATE));
    CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
    CONFIG_DEBUG_SERIAL->print(FPSTR(ACTUATORS));
    CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
    CONFIG_DEBUG_SERIAL->println(FPSTR(ID));
    delay(10000);
    deviceReset();
}
#endif
}  // namespace

/**
 * @brief Adds an actuator to the system.
 *
 * The actuator is stored in the main array and its ID is mapped to its index for fast lookups.
 * If the maximum number of actuators is exceeded, the device will reset to prevent undefined behavior.
 *
 * @param actuator A pointer to the Actuator object to add.
 */
void addActuator(Actuator *const actuator)
{
    const uint8_t currentIndex = totalActuators;
    if (currentIndex >= CONFIG_MAX_ACTUATORS)
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

    if (actuator->getId() == 0U)
    {
        failWrongActuatorId();
    }

#if CONFIG_USE_ACTUATOR_ID_LUT
    if (actuator->getId() > CONFIG_MAX_ACTUATOR_ID)
    {
        failWrongActuatorId();
    }
    if (actuatorIndexById[actuator->getId()] != 0U)
    {
        failDuplicateActuatorId();
    }
#endif

    actuator->setIndex(currentIndex);    // Store current index inside the object, it can be useful
    actuators[currentIndex] = actuator;  // Insert in array of actuators
#if CONFIG_USE_ACTUATOR_ID_LUT
    actuatorIndexById[actuator->getId()] = static_cast<uint8_t>(currentIndex + 1U);
#else
    actuatorsMap[actuator->getId()] = currentIndex;
#endif

    DPL(FPSTR(dStr::ACTUATOR), FPSTR(dStr::SPACE), FPSTR(dStr::UUID), FPSTR(dStr::COLON_SPACE), actuator->getId(), FPSTR(dStr::SPACE),
        FPSTR(dStr::DIVIDER), FPSTR(dStr::SPACE), FPSTR(dStr::INDEX), FPSTR(dStr::COLON_SPACE), currentIndex);

    updatePackedState(currentIndex, actuator->getState());
    totalActuators++;
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
#if CONFIG_USE_ACTUATOR_ID_LUT
    if (actuatorId > CONFIG_MAX_ACTUATOR_ID)
    {
        return nullptr;
    }

    const uint8_t encodedIndex = actuatorIndexById[actuatorId];
    if (encodedIndex == 0U)
    {
        return nullptr;
    }
    return actuators[static_cast<uint8_t>(encodedIndex - 1U)];
#else
    const auto it = actuatorsMap.find(actuatorId);
    if (it == actuatorsMap.end())
    {
        return nullptr;
    }
    return actuators[it->second];
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
#if CONFIG_USE_ACTUATOR_ID_LUT
    if (actuatorId > CONFIG_MAX_ACTUATOR_ID)
    {
        return UINT8_MAX;
    }

    const uint8_t encodedIndex = actuatorIndexById[actuatorId];
    if (encodedIndex == 0U)
    {
        return UINT8_MAX;
    }
    return static_cast<uint8_t>(encodedIndex - 1U);
#else
    const auto it = actuatorsMap.find(actuatorId);
    if (it == actuatorsMap.end())
    {
        return UINT8_MAX;
    }
    return it->second;
#endif
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
#if CONFIG_USE_ACTUATOR_ID_LUT
    if (actuatorId > CONFIG_MAX_ACTUATOR_ID)
    {
        return false;
    }

    const uint8_t encodedIndex = actuatorIndexById[actuatorId];
    if (encodedIndex == 0U)
    {
        return false;
    }
    actuatorIndex = static_cast<uint8_t>(encodedIndex - 1U);
#else
    const auto it = actuatorsMap.find(actuatorId);
    if (it == actuatorsMap.end())
    {
        return false;
    }
    actuatorIndex = it->second;
#endif
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
#if CONFIG_USE_ACTUATOR_ID_LUT
    return (actuatorId <= CONFIG_MAX_ACTUATOR_ID && actuatorIndexById[actuatorId] != 0U);
#else
    return (actuatorsMap.find(actuatorId) != actuatorsMap.end());
#endif
}

/**
 * @brief Store or remove an auto-off timer in the compact shared timer pool.
 *
 * @param actuatorIndex dense runtime actuator index.
 * @param time_ms timer duration; zero disables the timer.
 */
void setAutoOffTimer(uint8_t actuatorIndex, uint32_t time_ms)
{
    if (actuatorIndex >= totalActuators || actuators[actuatorIndex] == nullptr)
    {
        failWrongAutoOffActuator();
    }

    const uint8_t entryIndex = findAutoOffTimerEntry(actuatorIndex);
    if (time_ms == 0U)
    {
        if (entryIndex != UINT8_MAX)
        {
            --totalAutoOffTimers;
            autoOffTimers[entryIndex] = autoOffTimers[totalAutoOffTimers];
            autoOffTimers[totalAutoOffTimers].timer_ms = 0U;
#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES
            autoOffTimers[totalAutoOffTimers].lastSwitchTime_ms = 0U;
#endif
            autoOffTimers[totalAutoOffTimers].actuatorIndex = 0U;
        }
        return;
    }

    if (entryIndex != UINT8_MAX)
    {
        autoOffTimers[entryIndex].timer_ms = time_ms;
        return;
    }

    if (totalAutoOffTimers >= constants::config::MAX_AUTO_OFF_ACTUATORS)
    {
        failAutoOffTimerOverflow();
    }

    auto &entry = autoOffTimers[totalAutoOffTimers];
    entry.timer_ms = time_ms;
#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES
    entry.lastSwitchTime_ms = timeKeeper::getTime();
#endif
    entry.actuatorIndex = actuatorIndex;
    ++totalAutoOffTimers;
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
    return findAutoOffTimerEntry(actuatorIndex) != UINT8_MAX;
}

/**
 * @brief Return an actuator auto-off timer.
 *
 * @param actuatorIndex dense runtime actuator index.
 * @return uint32_t timer duration, or zero when disabled.
 */
auto getAutoOffTimer(uint8_t actuatorIndex) -> uint32_t
{
    const uint8_t entryIndex = findAutoOffTimerEntry(actuatorIndex);
    return entryIndex == UINT8_MAX ? 0U : autoOffTimers[entryIndex].timer_ms;
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
    const uint8_t entryIndex = findAutoOffTimerEntry(actuatorIndex);
    if (entryIndex != UINT8_MAX)
    {
        autoOffTimers[entryIndex].lastSwitchTime_ms = now_ms;
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
    if (actuatorIndex >= totalActuators || actuators[actuatorIndex] == nullptr || autoOffTimer_ms == 0U)
    {
        return false;
    }

#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES
    const uint8_t entryIndex = findAutoOffTimerEntry(actuatorIndex);
    if (entryIndex == UINT8_MAX)
    {
        return false;
    }

    auto *const actuator = actuators[actuatorIndex];
    if (actuator->getState() && (timeKeeper::getTime() - autoOffTimers[entryIndex].lastSwitchTime_ms >= autoOffTimer_ms))
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
    for (uint8_t entryIndex = 0U; entryIndex < totalAutoOffTimers; ++entryIndex)
    {
        const auto &entry = autoOffTimers[entryIndex];
#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES
        auto *const actuator = local_actuators[entry.actuatorIndex];
        if (actuator->getState() && (now - entry.lastSwitchTime_ms >= entry.timer_ms))
        {
            somethingSwitchedOff |= actuator->setState(false);
        }
#else
        somethingSwitchedOff |= local_actuators[entry.actuatorIndex]->checkAutoOffTimer(entry.timer_ms);
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
    bool anySwitchPerformed = false;
    for (uint8_t i = 0U; i < totalActuators; ++i)
    {
        anySwitchPerformed |= actuators[i]->setState(false);
    }
    return anySwitchPerformed;
}

/**
 * @brief Turns off all unprotected actuators.
 *
 * @return true if any actuator performed the switch.
 * @return false otherwise.
 */
auto turnOffUnprotectedActuators() -> bool
{
    bool anySwitchPerformed = false;
    for (uint8_t i = 0U; i < totalActuators; ++i)
    {
        auto *actuator = actuators[i];
        if (!actuator->hasProtection())
        {
            anySwitchPerformed |= actuator->setState(false);
        }
    }
    return anySwitchPerformed;
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
    auto *const actuatorEnd = actuatorBegin + totalActuators;
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
    packedActuatorStates.set(actuatorIndex, state);
}

/**
 * @brief Return the number of packed bytes required by the current topology.
 */
auto getPackedStateByteCount() -> uint8_t
{
    // Equivalent to ceil(totalActuators / 8) while staying cheap on AVR.
    return static_cast<uint8_t>((static_cast<uint16_t>(totalActuators) + 7U) >> 3U);
}

/**
 * @brief Return one packed state byte in protocol wire order.
 *
 * @param byteIndex packed byte index.
 * @return uint8_t packed actuator-state byte, or zero when out of range.
 */
auto getPackedStateByte(uint8_t byteIndex) -> uint8_t
{
    const uint8_t byteCount = getPackedStateByteCount();
    if (byteIndex >= byteCount)
    {
        return 0U;
    }

    const uint16_t bitOffset = static_cast<uint16_t>(byteIndex) * 8U;
    const uint16_t remainingBits = static_cast<uint16_t>(totalActuators) - bitOffset;
    const uint8_t extractedBits = remainingBits < 8U ? static_cast<uint8_t>(remainingBits) : 8U;
    // `extract()` lets the compact shadow stay authoritative for wire encoding:
    // byte 0 carries actuator bits 0..7, byte 1 carries 8..15, and so on.
    return packedActuatorStates.extract<uint8_t>(bitOffset, extractedBits);
}

/**
 * @brief Final validation after actuator registration.
 *
 */
void finalizeSetup()
{
    DP_CONTEXT();
    for (uint8_t actuatorIndex = 0U; actuatorIndex < totalActuators; ++actuatorIndex)
    {
        if (actuators[actuatorIndex] == nullptr || actuators[actuatorIndex]->getIndex() != actuatorIndex)
        {
            failNonCompactActuatorStorage();
        }
    }
    for (uint8_t actuatorIndex = totalActuators; actuatorIndex < CONFIG_MAX_ACTUATORS; ++actuatorIndex)
    {
        if (actuators[actuatorIndex] != nullptr)
        {
            failNonCompactActuatorStorage();
        }
    }
    for (uint8_t entryIndex = 0U; entryIndex < totalAutoOffTimers; ++entryIndex)
    {
        const uint8_t actuatorIndex = autoOffTimers[entryIndex].actuatorIndex;
        if (actuatorIndex >= totalActuators || actuators[actuatorIndex] == nullptr)
        {
            failNonCompactActuatorStorage();
        }
    }

#if !CONFIG_USE_ACTUATOR_ID_LUT
    if (actuatorsMap.size() != totalActuators)
    {
        using namespace constants::wrongConfigStrings;
        NDSB();  // Begin serial if not in debug mode
        CONFIG_DEBUG_SERIAL->print(FPSTR(DUPLICATE));
        CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
        CONFIG_DEBUG_SERIAL->print(FPSTR(ACTUATORS));
        CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
        CONFIG_DEBUG_SERIAL->println(FPSTR(ID));
        delay(10000);
        deviceReset();
    }
#endif
}

}  // namespace Actuators
