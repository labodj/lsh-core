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
#include "internal/user_config_bridge.hpp"
#include "peripherals/input/clickable.hpp"
#include "util/constants/config.hpp"
#include "util/debug/debug.hpp"
#include "util/reset.hpp"

using namespace Debug;

namespace Clickables
{
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
etl::array<Clickable *, CONFIG_MAX_CLICKABLES> clickables{};  //!< Device clickables
#endif

namespace
{
/**
 * @brief Abort setup when a clickable was registered but has no actionable configuration.
 * @details After the compact pools are finalized, every clickable must map to at
 *          least one real action. Otherwise the installation contains dead inputs
 *          that can only confuse future maintenance.
 */
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
void failInvalidClickableConfiguration()
{
    NDSB();
    CONFIG_DEBUG_SERIAL->println(F("Wrong clickable configuration"));
    delay(10000);
    deviceReset();
}
#endif

/**
 * @brief Abort setup when the dense clickable prefix was corrupted.
 */
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
void failNonCompactClickableStorage()
{
    NDSB();
    CONFIG_DEBUG_SERIAL->println(F("Wrong clickable storage"));
    delay(10000);
    deviceReset();
}
#endif

#if CONFIG_USE_NETWORK_CLICKS
/**
 * @brief Abort setup when one held button can exceed the active network-click pool.
 */
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
void failNetworkClickPoolTooSmall()
{
    NDSB();
    CONFIG_DEBUG_SERIAL->println(F("Wrong active network clicks number"));
    delay(10000);
    deviceReset();
}
#endif
#endif
}  // namespace

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
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
    uint8_t clickableIndex = UINT8_MAX;
    if (!tryGetIndex(clickableId, clickableIndex))
    {
        return nullptr;
    }
    return clickables[clickableIndex];
#else
    static_cast<void>(clickableId);
    return nullptr;
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
    if (configuredIndex >= CONFIG_MAX_CLICKABLES)
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
 * @brief Validate the configured clickable table before runtime checks start.
 * @details Click actions are generated as static dispatch code, so setup only
 *          has to validate the dense object table and ensure every clickable has
 *          at least one executable action.
 *
 */
void finalizeSetup()
{
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
    DP_CONTEXT();
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
        const uint8_t networkSlotsNeededForOneHold = lsh::core::static_config::getNetworkClickSlotCount(clickableIndex);
        if (networkSlotsNeededForOneHold > constants::config::MAX_ACTIVE_NETWORK_CLICKS)
        {
            failNetworkClickPoolTooSmall();
        }
#endif

        if (!lsh::core::static_config::isClickableConfigurationValid(clickableIndex))
        {
            failInvalidClickableConfiguration();
        }
    }
#endif
}

}  // namespace Clickables
