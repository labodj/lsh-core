/**
 * @file    network_clicks.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic for handling network click requests, confirmations, and timeouts.
 *
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

#include "core/network_clicks.hpp"

#include "communication/serializer.hpp"
#include "device/clickable_manager.hpp"
#include "peripherals/input/clickable.hpp"
#include "util/constants/timing.hpp"
#include "util/debug/debug.hpp"
#include "util/timekeeper.hpp"

using namespace Debug;

namespace NetworkClicks
{
    namespace
    {
        /**
         * @brief Timeout checks for a specific type of network-pending clickables.
         *
         * @param map The map of pending clicks to check, e.g., long-clicked or super-long-clicked.
         * @param clickType The type of click this map represents (e.g., LONG, SUPER_LONG). This is crucial for retrieving the correct fallback behavior.
         * @param failover If true, forces the fallback action for all pending clicks in the map, ignoring their timers.
         * @return true if any timer expired (or was forced by failover) and at least one local fallback action was performed.
         * @return false otherwise.
         */
        auto checkAllNetworkClicksTimers(etl::imap<uint8_t, uint32_t> *const map, constants::ClickType clickType, bool failover) -> bool
        {
            DP_CONTEXT();
            using constants::NoNetworkClickType;
            using constants::timings::LCNB_TIMEOUT_MS;
            bool localFallbackPerformed = false; // True if a network click expired and a local click has been performed

            for (auto it = map->begin(); it != map->end();) // No increment (See: https://stackoverflow.com/questions/8234779/how-to-remove-from-a-map-while-iterating-it)
            {
                if (failover || timeKeeper::getTime() - it->second > LCNB_TIMEOUT_MS) // If expired or failover
                {
                    DPL(FPSTR(dStr::EXPIRED), FPSTR(dStr::SPACE), FPSTR(dStr::CLICKABLE), FPSTR(dStr::SPACE), FPSTR(dStr::INDEX), FPSTR(dStr::COLON_SPACE), it->first);
                    auto *const clickable = Clickables::clickables[it->first];
                    if (clickable->getNetworkFallback(clickType) == NoNetworkClickType::LOCAL_FALLBACK)
                    {
                        localFallbackPerformed |= Clickables::click(clickable, clickType);
                    }
                    map->erase(it++); // Erase and increment
                }
                else
                {
                    ++it; // Increment
                }
            }
            return localFallbackPerformed;
        }
    } // namespace

    using constants::ClickType;

    etl::map<uint8_t, uint32_t, CONFIG_MAX_CLICKABLES> longClickedNetworkClickables{};      //!< Map of long clicked network clickable (<Clickable index, stored time>)
    etl::map<uint8_t, uint32_t, CONFIG_MAX_CLICKABLES> superLongClickedNetworkClickables{}; //!< Map of super long clicked network clickable (<Clickable index, stored time>)

    /**
     * @brief Initiates a network click action.
     * @details Sends the initial network click request and starts the fallback timer.
     *          This function does not return a value; the caller is responsible
     *          for managing any necessary state changes.
     * @param clickableIndex The index of the clickable that was pressed.
     * @param clickType The type of click (LONG or SUPER_LONG).
     */
    void request(uint8_t clickableIndex, constants::ClickType clickType)
    {
        DP_CONTEXT();
        Serializer::serializeNetworkClick(clickableIndex, clickType, false);
        storeNetworkClickTime(clickableIndex, clickType);
    }

    /**
     * @brief Confirms a pending network click action after receiving an ACK.
     * @details Sends the final confirmation message and removes the click from the pending list.
     * @param clickableIndex The index of the clickable to confirm.
     * @param clickType The type of click to confirm.
     * @return true if there are still other active network clicks pending, false otherwise.
     */
    auto confirm(uint8_t clickableIndex, constants::ClickType clickType) -> bool
    {
        DP_CONTEXT();
        Serializer::serializeNetworkClick(clickableIndex, clickType, true);
        eraseNetworkClick(clickableIndex, clickType);
        return thereAreActiveNetworkCLicks();
    }

    /**
     * @brief Store click time for a network attached clickable.
     *
     * @param clickableIndex the index of the clickable.
     * @param clickType the type of the click (long or super long).
     */
    void storeNetworkClickTime(uint8_t clickableIndex, constants::ClickType clickType)
    {
        DP_CONTEXT();
        DPL(FPSTR(dStr::SPACE), FPSTR(dStr::DIVIDER), FPSTR(dStr::SPACE), FPSTR(dStr::CLICKABLE),
            FPSTR(dStr::SPACE), FPSTR(dStr::INDEX), FPSTR(dStr::COLON_SPACE),
            clickableIndex, FPSTR(dStr::SPACE), FPSTR(dStr::DIVIDER), FPSTR(dStr::SPACE),
            FPSTR(dStr::CLICK), FPSTR(dStr::SPACE), FPSTR(dStr::TYPE),
            FPSTR(dStr::COLON_SPACE), static_cast<int8_t>(clickType));

        etl::imap<uint8_t, uint32_t> *map = nullptr;
        switch (clickType)
        {
        case ClickType::LONG:
        {
            map = &longClickedNetworkClickables;
        }
        break;
        case ClickType::SUPER_LONG:
        {
            map = &superLongClickedNetworkClickables;
        }
        break;
        default:
            return;
        }

        (*map)[clickableIndex] = timeKeeper::getTime();
    }

    /**
     * @brief Get if there are active stored network clicks.
     *
     * @return true if active stored network clicks are found.
     * @return false there are no stored network click.
     */
    auto thereAreActiveNetworkCLicks() -> bool
    {
        return (!longClickedNetworkClickables.empty() || !superLongClickedNetworkClickables.empty());
    }

    /**
     * @brief Erase a stored network click.
     *
     * @param clickableIndex index of the clickable.
     * @param clickType type of the click.
     */
    void eraseNetworkClick(uint8_t clickableIndex, constants::ClickType clickType)
    {
        DP_CONTEXT();
        etl::imap<uint8_t, uint32_t> *map = nullptr;
        switch (clickType)
        {
        case ClickType::LONG:
            map = &longClickedNetworkClickables;
            break;

        case ClickType::SUPER_LONG:
            map = &superLongClickedNetworkClickables;
            break;

        default:
            return;
        }

        const auto it = map->find(clickableIndex);
        if (it != map->end())
        {
            map->erase(it);
        }
    }

    /**
     * @brief Checks if a pending network click has expired.
     *
     * If the click has expired, it is removed from the pending list as a side effect.
     *
     * @param clickableIndex The index of the clickable to check.
     * @param clickType The type of the pending click (long or super long).
     * @return true if the pending network click has expired, false otherwise.
     */
    auto isNetworkClickExpired(uint8_t clickableIndex, constants::ClickType clickType) -> bool
    {
        DP_CONTEXT();
        using constants::timings::LCNB_TIMEOUT_MS;
        etl::imap<uint8_t, uint32_t> *map = nullptr;
        switch (clickType)
        {
        case ClickType::LONG:
            map = &longClickedNetworkClickables;
            break;
        case ClickType::SUPER_LONG:
            map = &superLongClickedNetworkClickables;
            break;
        default:
            return true; // Clickable type is not valid, so it's "expired"
        }

        const auto it = map->find(clickableIndex);
        if (it == map->end())
        {
            return true;
        }
        if (timeKeeper::getTime() - it->second > LCNB_TIMEOUT_MS) // It's expired
        {
            map->erase(it); // Erase it for convenience
            return true;
        }

        return false;
    }

    /**
     * @brief Checks a specific pending network click for expiration or forced failover.
     *
     * If the timer has expired, or if `failover` is true, this function triggers the
     * configured fallback action (if any) and removes the click from the pending map.
     *
     * @param clickableIndex The index of the clickable to check.
     * @param clickType The type of the pending click.
     * @param failover If true, force the fallback action regardless of the timer's state.
     * @return true if a fallback action was performed, false otherwise.
     */
    auto checkNetworkClickTimer(uint8_t clickableIndex, constants::ClickType clickType, bool failover) -> bool
    {
        DP_CONTEXT();
        using constants::NoNetworkClickType;
        using constants::timings::LCNB_TIMEOUT_MS;

        etl::imap<uint8_t, uint32_t> *map = nullptr;

        switch (clickType)
        {
        case ClickType::LONG:
            map = &longClickedNetworkClickables;
            break;

        case ClickType::SUPER_LONG:
            map = &superLongClickedNetworkClickables;
            break;

        default:
            return false;
        }

        bool localFallbackPerformed = false; // True if a network click expired and a local click has been performed

        const auto it = map->find(clickableIndex);
        if (it != map->end())
        {
            if (failover || timeKeeper::getTime() - it->second > LCNB_TIMEOUT_MS) // expired
            {
                auto *const clickable = Clickables::clickables[it->first];
                if (clickable->getNetworkFallback(clickType) == NoNetworkClickType::LOCAL_FALLBACK)
                {
                    localFallbackPerformed |= Clickables::click(clickable, clickType);
                }
                map->erase(it);
            }
        }
        return localFallbackPerformed;
    }

    /**
     * @brief Timeout checks for all network clicked clickables.
     *
     * If the time is over it performs local action and resets the timer
     *
     * @param failover to perform local action regardless the state of the timer.
     * @return true if any timer is expired and at least one local fallback click has been performed.
     * @return false otherwise.
     */
    auto checkAllNetworkClicksTimers(bool failover) -> bool
    {
        // DP_CONTEXT(); pollutes serial
        bool localFallbackPerformed = false; // True if any network click expired and any local click has been performed
        if (!longClickedNetworkClickables.empty())
        {
            DPL(FPSTR(dStr::LONG), " ", FPSTR(dStr::CLICKED), FPSTR(dStr::NET_BUTTONS_NOT_EMPTY));
            localFallbackPerformed |= checkAllNetworkClicksTimers(&longClickedNetworkClickables, ClickType::LONG, failover);
        }
        if (!superLongClickedNetworkClickables.empty())
        {
            DPL(FPSTR(dStr::SUPER_LONG), " ", FPSTR(dStr::CLICKED), FPSTR(dStr::NET_BUTTONS_NOT_EMPTY));
            localFallbackPerformed |= checkAllNetworkClicksTimers(&superLongClickedNetworkClickables, ClickType::SUPER_LONG, failover);
        }
        return localFallbackPerformed;
    }

} // namespace NetworkClicks
