/**
 * @file    clickable_manager.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic for managing and operating on all system Clickables.
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

#include "device/clickable_manager.hpp"

#include "device/actuator_manager.hpp"
#include "internal/user_config_bridge.hpp"
#include "peripherals/input/clickable.hpp"
#include "util/constants/wrong_config_strings.hpp"
#include "util/debug/debug.hpp"
#include "util/reset.hpp"

using namespace Debug;

namespace Clickables
{
    uint8_t totalClickables = 0U;                                      //!< Device real total Clickables
    etl::array<Clickable *, CONFIG_MAX_CLICKABLES> clickables{};       //!< Device clickables
    etl::map<uint8_t, uint8_t, CONFIG_MAX_CLICKABLES> clickablesMap{}; //!< Device clickables map (UUID -> clickables index)

    /**
     * @brief Adds a clickable to the system.
     *
     * The clickable is stored in the main array and its ID is mapped to its index for fast lookups.
     * If the maximum number of clickables is exceeded, the device will reset to prevent undefined behavior.
     *
     * @param clickable A pointer to the Clickable object to add.
     */
    void addClickable(Clickable *const clickable)
    {
        const uint8_t currentIndex = totalClickables;
        if (currentIndex >= CONFIG_MAX_CLICKABLES)
        {
            using namespace constants::wrongConfigStrings;
            NDSB(); // Begin serial if not in debug mode
            CONFIG_DEBUG_SERIAL->print(FPSTR(WRONG));
            CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
            CONFIG_DEBUG_SERIAL->print(FPSTR(CLICKABLES));
            CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
            CONFIG_DEBUG_SERIAL->println(FPSTR(NUMBER));
            delay(10000);
            deviceReset();
        }
        clickable->setIndex(currentIndex);                // Store current index inside the object, it can be useful
        clickables[currentIndex] = clickable;             // Insert in array of clickables
        clickablesMap[clickable->getId()] = currentIndex; // Insert in map of clickables Map(UUID (integer) -> index in Vector)

        DPL(FPSTR(dStr::CLICKABLE), FPSTR(dStr::SPACE), FPSTR(dStr::UUID), FPSTR(dStr::COLON_SPACE),
            clickable->getId(), FPSTR(dStr::SPACE), FPSTR(dStr::DIVIDER), FPSTR(dStr::SPACE),
            FPSTR(dStr::INDEX), FPSTR(dStr::COLON_SPACE), currentIndex);

        totalClickables++;
    }

    /**
     * @brief Get a single clickable.
     *
     * @param clickableId clickable UUID.
     * @return Clickable* a single clickable.
     */
    auto getClickable(uint8_t clickableId) -> Clickable *
    {
        return clickables[clickablesMap.find(clickableId)->second];
    }

    /**
     * @brief Get a single clickable index (in device vector of clickable).
     *
     * @param clickableId clickable UUID.
     * @return uint8_t a single clickable index (in device vector of clickable).
     */
    auto getIndex(uint8_t clickableId) -> uint8_t
    {
        return clickablesMap.find(clickableId)->second;
    }

    /**
     * @brief Get if the clickable actually exists.
     *
     * @param clickableId Unique ID of the clickable.
     * @return true if clickable exists.
     * @return false if clickable doesn't exist.
     */
    auto clickableExists(uint8_t clickableId) -> bool
    {
        return (clickablesMap.find(clickableId) != clickablesMap.end());
    }

    /**
     * @brief Perform a click.
     *
     * Method for all types of clicks, since not all click can be done within clickable class.
     *
     * @param clickable  the clickable to click.
     * @param clickType  the click type to perform.
     * @return true if any actuator state has been changed.
     * @return false otherwise.
     */
    auto click(const Clickable *const clickable, constants::ClickType clickType) -> bool
    {
        DP_CONTEXT();
        using namespace constants;
        switch (clickType)
        {
        case ClickType::SHORT:
            return clickable->shortClick();
        case ClickType::LONG:
            return clickable->longClick();
        case ClickType::SUPER_LONG:
            switch (clickable->getSuperLongClickType())
            {
            case SuperLongClickType::NORMAL:
                return Actuators::turnOffUnprotectedActuators();
            case SuperLongClickType::SELECTIVE:
                return clickable->superLongClickSelective();
            default:
                return false;
            }
        default:
            return false;
        }
    }

    /**
     * @brief Perform a click action.
     *
     * This helper function centralizes all click-related logic. It's necessary because some actions,
     * like a super long click that turns off all unprotected actuators, require access to global
     * collections (Actuators) and cannot be handled entirely within the Clickable class itself.
     *
     * @param clickable  The clickable to click.
     * @param clickType  The click type to perform.
     * @return true if any actuator state was changed.
     * @return false otherwise.
     */
    auto click(uint8_t clickableIndex, constants::ClickType clickType) -> bool
    {
        return click(clickables[clickableIndex], clickType);
    }

    /**
     * @brief Resize vectors of all clickables to the actual needed size.
     *
     */
    void finalizeSetup()
    {
        DP_CONTEXT();
        for (auto *const clickable : clickables) // Resize vectors inside clickable
        {
            clickable->resizeVectors();
            clickable->check();
        }
        if (clickablesMap.size() != totalClickables)
        {
            using namespace constants::wrongConfigStrings;
            NDSB(); // Begin serial if not in debug mode
            CONFIG_DEBUG_SERIAL->print(FPSTR(DUPLICATE));
            CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
            CONFIG_DEBUG_SERIAL->print(FPSTR(CLICKABLES));
            CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
            CONFIG_DEBUG_SERIAL->println(FPSTR(ID));
            delay(10000);
            deviceReset();
        }
    }

} // namespace Clickables
