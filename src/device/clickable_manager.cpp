/**
 * @file    clickable_manager.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic for managing and operating on all system Clickables.
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

#include "device/clickable_manager.hpp"

#include "device/actuator_manager.hpp"
#include "internal/user_config_bridge.hpp"
#include "peripherals/input/clickable.hpp"
#include "util/constants/config.hpp"
#include "util/constants/wrong_config_strings.hpp"
#include "util/debug/debug.hpp"
#include "util/reset.hpp"

using namespace Debug;

namespace Clickables
{
uint8_t totalClickables = 0U;                                 //!< Device real total Clickables
etl::array<Clickable *, CONFIG_MAX_CLICKABLES> clickables{};  //!< Device clickables
#if CONFIG_USE_CLICKABLE_ID_LUT
etl::array<uint8_t, CONFIG_MAX_CLICKABLE_ID + 1U> clickableIndexById{};  //!< Device clickables lookup (UUID -> clickable index + 1)
static_assert(CONFIG_MAX_CLICKABLE_ID > 0U, "CONFIG_MAX_CLICKABLE_ID must be greater than zero when the clickable ID LUT is enabled.");
#else
etl::map<uint8_t, uint8_t, CONFIG_MAX_CLICKABLES> clickablesMap{};  //!< Device clickables map (UUID -> clickables index)
#endif

namespace
{
/**
 * @brief Abort setup when one clickable uses an invalid numeric ID.
 * @details Clickable ID zero is reserved as "missing" in the wire protocol
 *          and in the bounded lookup tables, so configuration must reject it.
 */
void failWrongClickableId()
{
    using namespace constants::wrongConfigStrings;
    NDSB();
    CONFIG_DEBUG_SERIAL->print(FPSTR(WRONG));
    CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
    CONFIG_DEBUG_SERIAL->print(FPSTR(CLICKABLES));
    CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
    CONFIG_DEBUG_SERIAL->println(FPSTR(ID));
    delay(10000);
    deviceReset();
}

#if CONFIG_USE_CLICKABLE_ID_LUT
/**
 * @brief Abort setup when two clickables reuse the same numeric ID.
 * @details The bounded LUT requires a one-to-one mapping between wire ID and
 *          dense runtime index. Duplicate IDs would make later lookups ambiguous.
 */
void failDuplicateClickableId()
{
    using namespace constants::wrongConfigStrings;
    NDSB();
    CONFIG_DEBUG_SERIAL->print(FPSTR(DUPLICATE));
    CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
    CONFIG_DEBUG_SERIAL->print(FPSTR(CLICKABLES));
    CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
    CONFIG_DEBUG_SERIAL->println(FPSTR(ID));
    delay(10000);
    deviceReset();
}
#endif

/**
 * @brief Abort setup when a clickable was registered but has no actionable configuration.
 * @details After the compact pools are finalized, every clickable must map to at
 *          least one real action. Otherwise the installation contains dead inputs
 *          that can only confuse future maintenance.
 */
void failInvalidClickableConfiguration()
{
    NDSB();
    CONFIG_DEBUG_SERIAL->println(F("Wrong clickable configuration"));
    delay(10000);
    deviceReset();
}

/**
 * @brief Abort setup when the dense clickable prefix was corrupted.
 */
void failNonCompactClickableStorage()
{
    NDSB();
    CONFIG_DEBUG_SERIAL->println(F("Wrong clickable storage"));
    delay(10000);
    deviceReset();
}

#if CONFIG_USE_NETWORK_CLICKS
/**
 * @brief Abort setup when one held button can exceed the active network-click pool.
 */
void failNetworkClickPoolTooSmall()
{
    NDSB();
    CONFIG_DEBUG_SERIAL->println(F("Wrong active network clicks number"));
    delay(10000);
    deviceReset();
}
#endif
}  // namespace

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
        NDSB();  // Begin serial if not in debug mode
        CONFIG_DEBUG_SERIAL->print(FPSTR(WRONG));
        CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
        CONFIG_DEBUG_SERIAL->print(FPSTR(CLICKABLES));
        CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
        CONFIG_DEBUG_SERIAL->println(FPSTR(NUMBER));
        delay(10000);
        deviceReset();
    }

    if (clickable->getId() == 0U)
    {
        failWrongClickableId();
    }

#if CONFIG_USE_CLICKABLE_ID_LUT
    if (clickable->getId() > CONFIG_MAX_CLICKABLE_ID)
    {
        failWrongClickableId();
    }
    if (clickableIndexById[clickable->getId()] != 0U)
    {
        failDuplicateClickableId();
    }
#endif

    clickable->setIndex(currentIndex);     // Store current index inside the object, it can be useful
    clickables[currentIndex] = clickable;  // Insert in array of clickables
#if CONFIG_USE_CLICKABLE_ID_LUT
    clickableIndexById[clickable->getId()] = static_cast<uint8_t>(currentIndex + 1U);
#else
    clickablesMap[clickable->getId()] = currentIndex;  // Insert in map of clickables Map(UUID (integer) -> index in Vector)
#endif

    DPL(FPSTR(dStr::CLICKABLE), FPSTR(dStr::SPACE), FPSTR(dStr::UUID), FPSTR(dStr::COLON_SPACE), clickable->getId(), FPSTR(dStr::SPACE),
        FPSTR(dStr::DIVIDER), FPSTR(dStr::SPACE), FPSTR(dStr::INDEX), FPSTR(dStr::COLON_SPACE), currentIndex);

    totalClickables++;
}

/**
 * @brief Get a single clickable.
 *
 * @param clickableId clickable UUID.
 * @return Clickable* A single clickable when the ID exists.
 * @return nullptr When the ID is unknown.
 */
auto getClickable(uint8_t clickableId) -> Clickable *
{
#if CONFIG_USE_CLICKABLE_ID_LUT
    if (clickableId > CONFIG_MAX_CLICKABLE_ID)
    {
        return nullptr;
    }

    const uint8_t encodedIndex = clickableIndexById[clickableId];
    if (encodedIndex == 0U)
    {
        return nullptr;
    }
    return clickables[static_cast<uint8_t>(encodedIndex - 1U)];
#else
    const auto it = clickablesMap.find(clickableId);
    if (it == clickablesMap.end())
    {
        return nullptr;
    }
    return clickables[it->second];
#endif
}

/**
 * @brief Get a single clickable index (in device vector of clickable).
 *
 * @param clickableId clickable UUID.
 * @return uint8_t A single clickable index (in device vector of clickables).
 * @return UINT8_MAX When the ID is unknown.
 */
auto getIndex(uint8_t clickableId) -> uint8_t
{
#if CONFIG_USE_CLICKABLE_ID_LUT
    if (clickableId > CONFIG_MAX_CLICKABLE_ID)
    {
        return UINT8_MAX;
    }

    const uint8_t encodedIndex = clickableIndexById[clickableId];
    if (encodedIndex == 0U)
    {
        return UINT8_MAX;
    }
    return static_cast<uint8_t>(encodedIndex - 1U);
#else
    const auto it = clickablesMap.find(clickableId);
    if (it == clickablesMap.end())
    {
        return UINT8_MAX;
    }
    return it->second;
#endif
}

/**
 * @brief Resolves a clickable ID to its dense runtime index with a single map lookup.
 *
 * @param clickableId Clickable UUID.
 * @param clickableIndex Output runtime index when the ID exists.
 * @return true if the clickable exists.
 * @return false otherwise.
 */
auto tryGetIndex(uint8_t clickableId, uint8_t &clickableIndex) -> bool
{
#if CONFIG_USE_CLICKABLE_ID_LUT
    if (clickableId > CONFIG_MAX_CLICKABLE_ID)
    {
        return false;
    }

    const uint8_t encodedIndex = clickableIndexById[clickableId];
    if (encodedIndex == 0U)
    {
        return false;
    }
    clickableIndex = static_cast<uint8_t>(encodedIndex - 1U);
#else
    const auto it = clickablesMap.find(clickableId);
    if (it == clickablesMap.end())
    {
        return false;
    }
    clickableIndex = it->second;
#endif
    return true;
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
#if CONFIG_USE_CLICKABLE_ID_LUT
    return (clickableId <= CONFIG_MAX_CLICKABLE_ID && clickableIndexById[clickableId] != 0U);
#else
    return (clickablesMap.find(clickableId) != clickablesMap.end());
#endif
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

/** @brief Perform a click action.
     *
     * This helper function centralizes all click-related logic...
     *
     * @param clickableIndex The index of the clickable to click.
     * @param clickType  The click type to perform.
     * @return true if any actuator state was changed.
     * @return false otherwise.
     */
auto click(uint8_t clickableIndex, constants::ClickType clickType) -> bool
{
    if (clickableIndex >= totalClickables || clickables[clickableIndex] == nullptr)
    {
        return false;
    }
    return click(clickables[clickableIndex], clickType);
}

/**
 * @brief Finalize shared storage and validate the configured clickables.
 * @details The shared pools must be compacted before `Clickable::check()` runs,
 *          because validation and runtime both rely on the final `offset + count`
 *          view of each link list.
 *
 */
void finalizeSetup()
{
    DP_CONTEXT();
    finalizeActuatorLinkStorage();
    for (uint8_t clickableIndex = 0U; clickableIndex < totalClickables; ++clickableIndex)
    {
        auto *const currentClickable = clickables[clickableIndex];
        if (currentClickable == nullptr || currentClickable->getIndex() != clickableIndex)
        {
            failNonCompactClickableStorage();
        }

#if CONFIG_USE_NETWORK_CLICKS
        uint8_t networkSlotsNeededForOneHold = 0U;
        if (currentClickable->isNetworkClickable(constants::ClickType::LONG))
        {
            ++networkSlotsNeededForOneHold;
        }
        if (currentClickable->isNetworkClickable(constants::ClickType::SUPER_LONG))
        {
            ++networkSlotsNeededForOneHold;
        }
        if (networkSlotsNeededForOneHold > constants::config::MAX_ACTIVE_NETWORK_CLICKS)
        {
            failNetworkClickPoolTooSmall();
        }
#endif

        if (!currentClickable->check())
        {
            failInvalidClickableConfiguration();
        }
    }
    for (uint8_t clickableIndex = totalClickables; clickableIndex < CONFIG_MAX_CLICKABLES; ++clickableIndex)
    {
        if (clickables[clickableIndex] != nullptr)
        {
            failNonCompactClickableStorage();
        }
    }
#if !CONFIG_USE_CLICKABLE_ID_LUT
    if (clickablesMap.size() != totalClickables)
    {
        using namespace constants::wrongConfigStrings;
        NDSB();  // Begin serial if not in debug mode
        CONFIG_DEBUG_SERIAL->print(FPSTR(DUPLICATE));
        CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
        CONFIG_DEBUG_SERIAL->print(FPSTR(CLICKABLES));
        CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
        CONFIG_DEBUG_SERIAL->println(FPSTR(ID));
        delay(10000);
        deviceReset();
    }
#endif
}

}  // namespace Clickables
