/**
 * @file    actuator_manager.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic for managing and operating on all system Actuators.
 *
 * Copyright 2025 Jacopo Labardi
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
    uint8_t totalActuators = 0U;                                              //!< Device real total Actuators
    etl::array<Actuator *, CONFIG_MAX_ACTUATORS> actuators{};                 //!< All device actuators (like relays)
    etl::map<uint8_t, uint8_t, CONFIG_MAX_ACTUATORS> actuatorsMap{};          //!< Device actuators map (UUID -> actuator index)
    etl::vector<uint8_t, CONFIG_MAX_ACTUATORS> actuatorsWithAutoOffIndexes{}; //!< Indexes of actuators with auto off functionality active

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
            NDSB(); // Begin serial if not in debug mode
            CONFIG_DEBUG_SERIAL->print(FPSTR(WRONG));
            CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
            CONFIG_DEBUG_SERIAL->print(FPSTR(ACTUATORS));
            CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
            CONFIG_DEBUG_SERIAL->println(FPSTR(NUMBER));
            delay(10000);
            deviceReset();
        }
        actuator->setIndex(currentIndex);   // Store current index inside the object, it can be useful
        actuators[currentIndex] = actuator; // Insert in array of actuators
        actuatorsMap[actuator->getId()] = currentIndex;

        DPL(FPSTR(dStr::ACTUATOR), FPSTR(dStr::SPACE), FPSTR(dStr::UUID), FPSTR(dStr::COLON_SPACE),
            actuator->getId(), FPSTR(dStr::SPACE), FPSTR(dStr::DIVIDER), FPSTR(dStr::SPACE),
            FPSTR(dStr::INDEX), FPSTR(dStr::COLON_SPACE), currentIndex);

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
        return actuators[actuatorsMap.find(actuatorId)->second];
    }

    /**
     * @brief Get a single actuator index (in device vector of actuators).
     *
     * @param actuatorId actuator UUID.
     * @return uint8_t a single actuator index (in device vector of actuators).
     */
    auto getIndex(uint8_t actuatorId) -> uint8_t
    {
        return actuatorsMap.find(actuatorId)->second;
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
        return (actuatorsMap.find(actuatorId) != actuatorsMap.end());
    }

    /**
     * @brief Performs an auto-off timers check for actuators.
     *
     * @return true if any actuator has been automatically switched off.
     * @return false otherwise.
     */
    auto actuatorsAutoOffTimersCheck() -> bool
    {
        auto *const local_actuators = actuators.data(); // Cache the pointer to the actuators array
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
        for (uint8_t i = 0U; const bool state : states)
        {
            anySwitchPerformed |= actuators[i]->setState(state);
            i++;
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

        for (uint8_t i = 0U; const auto *const actuator : actuators)
        {
            if (actuator->hasAutoOff())
            {
                actuatorsWithAutoOffIndexes.push_back(i);
            }
            i++;
        }

        actuatorsWithAutoOffIndexes.resize(actuatorsWithAutoOffIndexes.size()); // Resize actuators with auto off vector

        if (actuatorsMap.size() != totalActuators)
        {
            using namespace constants::wrongConfigStrings;
            NDSB(); // Begin serial if not in debug mode
            CONFIG_DEBUG_SERIAL->print(FPSTR(DUPLICATE));
            CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
            CONFIG_DEBUG_SERIAL->print(FPSTR(ACTUATORS));
            CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
            CONFIG_DEBUG_SERIAL->println(FPSTR(ID));
            delay(10000);
            deviceReset();
        }
    }

} // namespace Actuators
