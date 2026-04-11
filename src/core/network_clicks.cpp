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
#include "internal/etl_array.hpp"
#include "internal/user_config_bridge.hpp"
#include "peripherals/input/clickable.hpp"
#include "util/constants/timing.hpp"
#include "util/debug/debug.hpp"
#include "util/timekeeper.hpp"

using namespace Debug;

namespace NetworkClicks
{
    namespace
    {
        etl::array<uint32_t, CONFIG_MAX_CLICKABLES> longClickedNetworkClickTimes{};      //!< Stored times for active long clicks, indexed by clickable index.
        etl::array<uint32_t, CONFIG_MAX_CLICKABLES> superLongClickedNetworkClickTimes{}; //!< Stored times for active super-long clicks, indexed by clickable index.
        etl::array<uint8_t, CONFIG_MAX_CLICKABLES> longClickedCorrelationIds{};          //!< Correlation IDs for active long clicks, indexed by clickable index.
        etl::array<uint8_t, CONFIG_MAX_CLICKABLES> superLongClickedCorrelationIds{};     //!< Correlation IDs for active super-long clicks, indexed by clickable index.
        uint8_t activeLongClickedNetworkClicks = 0U;                                     //!< Number of pending long-click transactions.
        uint8_t activeSuperLongClickedNetworkClicks = 0U;                                //!< Number of pending super-long-click transactions.
        uint8_t nextCorrelationId = 0U; //!< Monotonic 8-bit generator. 0 is reserved as "missing/invalid".

        auto getTimeStorage(constants::ClickType clickType) -> etl::array<uint32_t, CONFIG_MAX_CLICKABLES> *
        {
            using constants::ClickType;
            switch (clickType)
            {
            case ClickType::LONG:
                return &longClickedNetworkClickTimes;
            case ClickType::SUPER_LONG:
                return &superLongClickedNetworkClickTimes;
            default:
                return nullptr;
            }
        }

        auto getCorrelationStorage(constants::ClickType clickType) -> etl::array<uint8_t, CONFIG_MAX_CLICKABLES> *
        {
            using constants::ClickType;
            switch (clickType)
            {
            case ClickType::LONG:
                return &longClickedCorrelationIds;
            case ClickType::SUPER_LONG:
                return &superLongClickedCorrelationIds;
            default:
                return nullptr;
            }
        }

        auto getActiveCountStorage(constants::ClickType clickType) -> uint8_t *
        {
            using constants::ClickType;
            switch (clickType)
            {
            case ClickType::LONG:
                return &activeLongClickedNetworkClicks;
            case ClickType::SUPER_LONG:
                return &activeSuperLongClickedNetworkClicks;
            default:
                return nullptr;
            }
        }

        auto generateCorrelationId() -> uint8_t
        {
            ++nextCorrelationId;
            if (nextCorrelationId == 0U)
            {
                ++nextCorrelationId;
            }
            return nextCorrelationId;
        }

        void clearCorrelationId(uint8_t clickableIndex, constants::ClickType clickType)
        {
            auto *const correlationStorage = getCorrelationStorage(clickType);
            if (correlationStorage == nullptr)
            {
                return;
            }
            (*correlationStorage)[clickableIndex] = 0U;
        }

        auto getStoredCorrelationId(uint8_t clickableIndex, constants::ClickType clickType) -> uint8_t
        {
            auto *const correlationStorage = getCorrelationStorage(clickType);
            if (correlationStorage == nullptr)
            {
                return 0U;
            }
            return (*correlationStorage)[clickableIndex];
        }

        auto isNetworkClickActive(uint8_t clickableIndex, constants::ClickType clickType) -> bool
        {
            return getStoredCorrelationId(clickableIndex, clickType) != 0U;
        }

        /**
         * @brief Bumps the active counter only when a click slot becomes active.
         */
        void markNetworkClickActive(uint8_t clickableIndex, constants::ClickType clickType)
        {
            auto *const activeCount = getActiveCountStorage(clickType);
            if (activeCount == nullptr || isNetworkClickActive(clickableIndex, clickType))
            {
                return;
            }
            ++(*activeCount);
        }

        /**
         * @brief Drops the active counter only when a click slot was actually pending.
         */
        void markNetworkClickInactive(uint8_t clickableIndex, constants::ClickType clickType)
        {
            auto *const activeCount = getActiveCountStorage(clickType);
            if (activeCount == nullptr || !isNetworkClickActive(clickableIndex, clickType) || *activeCount == 0U)
            {
                return;
            }
            --(*activeCount);
        }

        /**
         * @brief Timeout checks for every pending click of the specified type.
         *
         * @details The storage stays array-based to minimise RAM. Full scans are
         * only performed while at least one click of the given type is pending,
         * which keeps the steady-state cost near zero for a feature that is used
         * very rarely in normal installations.
         * @param clickType The click type to sweep (LONG or SUPER_LONG).
         * @param now Cached current time for this sweep.
         * @param failover If true, forces the fallback action for all pending clicks of that type, ignoring their timers.
         * @return true if any timer expired (or was forced by failover) and at least one local fallback action was performed.
         * @return false otherwise.
         */
        auto checkAllNetworkClicksTimers(constants::ClickType clickType, uint32_t now, bool failover) -> bool
        {
            DP_CONTEXT();
            using constants::NoNetworkClickType;
            using constants::timings::LCNB_TIMEOUT_MS;
            bool localFallbackPerformed = false; // True if a network click expired and a local click has been performed

            auto *const timeStorage = getTimeStorage(clickType);
            auto *const activeCount = getActiveCountStorage(clickType);
            if (timeStorage == nullptr || activeCount == nullptr || *activeCount == 0U)
            {
                return false;
            }

            for (uint8_t clickableIndex = 0U; clickableIndex < Clickables::totalClickables; ++clickableIndex)
            {
                if (!isNetworkClickActive(clickableIndex, clickType))
                {
                    continue;
                }

                if (failover || now - (*timeStorage)[clickableIndex] > LCNB_TIMEOUT_MS) // If expired or failover
                {
                    DPL(FPSTR(dStr::EXPIRED), FPSTR(dStr::SPACE), FPSTR(dStr::CLICKABLE), FPSTR(dStr::SPACE), FPSTR(dStr::INDEX), FPSTR(dStr::COLON_SPACE), clickableIndex);
                    auto *const clickable = Clickables::clickables[clickableIndex];
                    if (clickable->getNetworkFallback(clickType) == NoNetworkClickType::LOCAL_FALLBACK)
                    {
                        localFallbackPerformed |= Clickables::click(clickable, clickType);
                    }
                    eraseNetworkClick(clickableIndex, clickType);
                    if (*activeCount == 0U)
                    {
                        break;
                    }
                }
            }
            return localFallbackPerformed;
        }
    } // namespace

    using constants::ClickType;

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
        storeNetworkClickTime(clickableIndex, clickType);
        Serializer::serializeNetworkClick(clickableIndex, clickType, false, getStoredCorrelationId(clickableIndex, clickType));
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
        const uint8_t correlationId = getStoredCorrelationId(clickableIndex, clickType);
        if (correlationId == 0U)
        {
            return thereAreActiveNetworkClicks();
        }

        Serializer::serializeNetworkClick(clickableIndex, clickType, true, correlationId);
        eraseNetworkClick(clickableIndex, clickType);
        return thereAreActiveNetworkClicks();
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

        auto *const timeStorage = getTimeStorage(clickType);
        if (timeStorage == nullptr)
        {
            return;
        }

        markNetworkClickActive(clickableIndex, clickType);
        (*timeStorage)[clickableIndex] = timeKeeper::getTime();
        auto *const correlationStorage = getCorrelationStorage(clickType);
        if (correlationStorage != nullptr)
        {
            (*correlationStorage)[clickableIndex] = generateCorrelationId();
        }
    }

    auto matchesCorrelationId(uint8_t clickableIndex, constants::ClickType clickType, uint8_t correlationId) -> bool
    {
        if (correlationId == 0U)
        {
            return false;
        }
        return getStoredCorrelationId(clickableIndex, clickType) == correlationId;
    }

    /**
     * @brief Get if there are active stored network clicks.
     *
     * @return true if active stored network clicks are found.
     * @return false there are no stored network click.
     */
    auto thereAreActiveNetworkClicks() -> bool
    {
        return (activeLongClickedNetworkClicks != 0U || activeSuperLongClickedNetworkClicks != 0U);
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
        auto *const timeStorage = getTimeStorage(clickType);
        if (timeStorage == nullptr)
        {
            return;
        }
        markNetworkClickInactive(clickableIndex, clickType);
        (*timeStorage)[clickableIndex] = 0U;
        clearCorrelationId(clickableIndex, clickType);
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
        auto *const timeStorage = getTimeStorage(clickType);
        if (timeStorage == nullptr)
        {
            return true; // Clickable type is not valid, so it's "expired"
        }

        if (!isNetworkClickActive(clickableIndex, clickType))
        {
            return true;
        }
        if (timeKeeper::getTime() - (*timeStorage)[clickableIndex] > LCNB_TIMEOUT_MS) // It's expired
        {
            eraseNetworkClick(clickableIndex, clickType);
            return true;
        }

        return false;
    }

    /**
     * @brief Checks a specific pending network click for expiration or forced failover.
     *
     * If the timer has expired, or if `failover` is true, this function triggers the
     * configured fallback action (if any) and removes the click from the pending storage.
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

        auto *const timeStorage = getTimeStorage(clickType);
        if (timeStorage == nullptr)
        {
            return false;
        }

        bool localFallbackPerformed = false; // True if a network click expired and a local click has been performed

        if (isNetworkClickActive(clickableIndex, clickType))
        {
            if (failover || timeKeeper::getTime() - (*timeStorage)[clickableIndex] > LCNB_TIMEOUT_MS) // expired
            {
                auto *const clickable = Clickables::clickables[clickableIndex];
                if (clickable->getNetworkFallback(clickType) == NoNetworkClickType::LOCAL_FALLBACK)
                {
                    localFallbackPerformed |= Clickables::click(clickable, clickType);
                }
                eraseNetworkClick(clickableIndex, clickType);
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
        const auto now = timeKeeper::getTime();

        if (activeLongClickedNetworkClicks != 0U)
        {
            DPL(FPSTR(dStr::LONG), " ", FPSTR(dStr::CLICKED), FPSTR(dStr::NET_BUTTONS_NOT_EMPTY));
            localFallbackPerformed |= checkAllNetworkClicksTimers(ClickType::LONG, now, failover);
        }
        if (activeSuperLongClickedNetworkClicks != 0U)
        {
            DPL(FPSTR(dStr::SUPER_LONG), " ", FPSTR(dStr::CLICKED), FPSTR(dStr::NET_BUTTONS_NOT_EMPTY));
            localFallbackPerformed |= checkAllNetworkClicksTimers(ClickType::SUPER_LONG, now, failover);
        }
        return localFallbackPerformed;
    }

} // namespace NetworkClicks
