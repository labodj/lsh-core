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
#include "util/constants/wrong_config_strings.hpp"
#include "util/debug/debug.hpp"
#include "util/reset.hpp"

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
etl::vector<uint8_t, CONFIG_MAX_ACTUATORS> actuatorsWithAutoOffIndexes{};  //!< Indexes of actuators with auto off functionality active

namespace
{
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

#if CONFIG_USE_ACTUATOR_ID_LUT
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

    totalActuators++;
}

/**
 * @brief Get a single actuator.
 *
 * @param actuatorId actuator UUID.
 * @return Actuator* a single actuator.
 */
auto getActuator(uint8_t actuatorId) -> Actuator *
{
#if CONFIG_USE_ACTUATOR_ID_LUT
    return actuators[static_cast<uint8_t>(actuatorIndexById[actuatorId] - 1U)];
#else
    return actuators[actuatorsMap.find(actuatorId)->second];
#endif
}

/**
 * @brief Get a single actuator index (in device vector of actuators).
 *
 * @param actuatorId actuator UUID.
 * @return uint8_t a single actuator index (in device vector of actuators).
 */
auto getIndex(uint8_t actuatorId) -> uint8_t
{
#if CONFIG_USE_ACTUATOR_ID_LUT
    return static_cast<uint8_t>(actuatorIndexById[actuatorId] - 1U);
#else
    return actuatorsMap.find(actuatorId)->second;
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
 * @brief Performs an auto-off timers check for actuators.
 *
 * @return true if any actuator has been automatically switched off.
 * @return false otherwise.
 */
auto actuatorsAutoOffTimersCheck() -> bool
{
    auto *const local_actuators = actuators.data();  // Cache the pointer to the actuators array
    bool somethingSwitchedOff = false;
    for (const auto actuatorIndex : actuatorsWithAutoOffIndexes)
    {
        somethingSwitchedOff |= local_actuators[actuatorIndex]->checkAutoOffTimer();
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
 * @brief Populates actuatorsWithAutoOffIndexes.
 *
 */
void finalizeSetup()
{
    DP_CONTEXT();
    // If this function has been called actuatorsWithAutoOffIndexes is already full
    if (!actuatorsWithAutoOffIndexes.empty())
    {
        return;
    }

    auto *const actuatorBegin = actuators.data();
    auto *const actuatorEnd = actuatorBegin + totalActuators;
    uint8_t i = 0U;
    for (auto *currActuator = actuatorBegin; currActuator != actuatorEnd; ++currActuator, ++i)
    {
        if ((*currActuator)->hasAutoOff())
        {
            actuatorsWithAutoOffIndexes.push_back(i);
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
