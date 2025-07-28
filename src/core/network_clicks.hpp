/**
 * @file    network_clicks.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares functions and state for managing network-based click actions.
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

#ifndef LSHCORE_CORE_NETWORK_CLICKS_HPP
#define LSHCORE_CORE_NETWORK_CLICKS_HPP

#include <etl/map.h>
#include <stdint.h>

#include "internal/user_config_bridge.hpp"
#include "util/constants/clicktypes.hpp"
/**
 * @brief "static class" Used to store and check network clicks
 *
 */
namespace NetworkClicks
{
    extern etl::map<uint8_t, uint32_t, CONFIG_MAX_CLICKABLES> longClickedNetworkClickables;      //!< Map of long clicked network clickable (<Clickable index, stored time>)
    extern etl::map<uint8_t, uint32_t, CONFIG_MAX_CLICKABLES> superLongClickedNetworkClickables; //!< Map of super long clicked network clickable (<Clickable index, stored time>)

    // Network clicks
    void request(uint8_t clickableIndex, constants::ClickType clickType);                                                     // Initiates a network click action.
    [[nodiscard]] auto confirm(uint8_t clickableIndex, constants::ClickType clickType) -> bool;                               // Confirms a pending network click action after receiving an ACK
    void storeNetworkClickTime(uint8_t clickableIndex, constants::ClickType clickType);                                       // Store click time for a network attached clickable
    [[nodiscard]] auto thereAreActiveNetworkCLicks() -> bool;                                                                 // Returns if there are active stored network clicks
    void eraseNetworkClick(uint8_t clickableIndex, constants::ClickType clickType);                                           // Erase a stored network click
    [[nodiscard]] auto isNetworkClickExpired(uint8_t clickableIndex, constants::ClickType clickType) -> bool;                 // Returns true if the timer of the clickable has passed the threshold, false otherwise
    [[nodiscard]] auto checkNetworkClickTimer(uint8_t clickableIndex, constants::ClickType clickType, bool failover) -> bool; // Timeout checks for network clicked clickable, if the time it's over it performs local action and resets its timer
    [[nodiscard]] auto checkAllNetworkClicksTimers(bool failover) -> bool;                                                    // Timeout checks for all network clicked clickables, if the time it's over it performs local action and resets its timer
} // namespace NetworkClicks

#endif // LSHCORE_CORE_NETWORK_CLICKS_HPP
