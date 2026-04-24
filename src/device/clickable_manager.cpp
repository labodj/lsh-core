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

#include "config/static_config.hpp"
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
etl::array<Clickable *, CONFIG_MAX_CLICKABLES> clickables{};  //!< Device clickables

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
 * The clickable is stored in the main array slot selected by the generated static profile.
 * If the maximum number of clickables is exceeded, the device will reset to prevent undefined behavior.
 *
 * @param clickable A pointer to the Clickable object to add.
 * @param clickableId Static wire ID of the clickable.
 * @param clickableIndex Dense runtime index of the clickable.
 */
void addClickable(Clickable *const clickable, uint8_t clickableId, uint8_t clickableIndex)
{
    if (clickableIndex >= CONFIG_MAX_CLICKABLES || clickables[clickableIndex] != nullptr)
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

    if (clickableId == 0U)
    {
        failWrongClickableId();
    }

    const uint8_t configuredIndex = lsh::core::static_config::getClickableIndexById(clickableId);
    if (configuredIndex != clickableIndex)
    {
        failWrongClickableId();
    }

    clickable->setIndex(clickableIndex);     // Store current index inside the object, it can be useful
    clickables[clickableIndex] = clickable;  // Insert in array of clickables

    DPL(FPSTR(dStr::CLICKABLE), FPSTR(dStr::SPACE), FPSTR(dStr::UUID), FPSTR(dStr::COLON_SPACE), clickableId, FPSTR(dStr::SPACE),
        FPSTR(dStr::DIVIDER), FPSTR(dStr::SPACE), FPSTR(dStr::INDEX), FPSTR(dStr::COLON_SPACE), clickableIndex);
}

/**
 * @brief Return the static wire ID for one registered clickable index.
 *
 * @param clickableIndex dense runtime clickable index.
 * @return uint8_t clickable ID, or zero when the index is outside the static profile.
 */
auto getId(uint8_t clickableIndex) -> uint8_t
{
    return lsh::core::static_config::getClickableId(clickableIndex);
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
    uint8_t clickableIndex = UINT8_MAX;
    if (!tryGetIndex(clickableId, clickableIndex))
    {
        return nullptr;
    }
    return clickables[clickableIndex];
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
    uint8_t clickableIndex = UINT8_MAX;
    if (!tryGetIndex(clickableId, clickableIndex))
    {
        return UINT8_MAX;
    }
    return clickableIndex;
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
    const uint8_t configuredIndex = lsh::core::static_config::getClickableIndexById(clickableId);
    if (configuredIndex >= CONFIG_MAX_CLICKABLES || clickables[configuredIndex] == nullptr)
    {
        return false;
    }
    clickableIndex = configuredIndex;
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
    uint8_t clickableIndex = UINT8_MAX;
    return tryGetIndex(clickableId, clickableIndex);
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
        return lsh::core::static_config::runSuperLongClick(clickable->getIndex());
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
    if (clickableIndex >= CONFIG_MAX_CLICKABLES || clickables[clickableIndex] == nullptr)
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
    for (uint8_t clickableIndex = 0U; clickableIndex < CONFIG_MAX_CLICKABLES; ++clickableIndex)
    {
        auto *const currentClickable = clickables[clickableIndex];
        const uint8_t clickableId = getId(clickableIndex);
        if (currentClickable == nullptr || currentClickable->getIndex() != clickableIndex || clickableId == 0U ||
            lsh::core::static_config::getClickableIndexById(clickableId) != clickableIndex)
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
}

}  // namespace Clickables
