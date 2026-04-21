/**
 * @file    network_clicks.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares functions and state for managing network-based click actions.
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

#ifndef LSH_CORE_CORE_NETWORK_CLICKS_HPP
#define LSH_CORE_CORE_NETWORK_CLICKS_HPP

#include <stdint.h>

#include "util/constants/click_types.hpp"
/**
 * @brief "static class" Used to store and check network clicks
 *
 * @details Network clicks are intentionally rare in this system. To keep RAM
 * usage low on embedded targets, this module stores pending click metadata in
 * fixed arrays indexed by clickable index and only keeps small active counters.
 * Timeout sweeps scan the clickable prefix only while at least one click of the
 * corresponding type is pending. Each clickable/type pair can hold at most one
 * in-flight network transaction at a time. Callers must inspect the returned
 * `RequestResult`: only `TransportRejected` means the network path is
 * unavailable for this click, while `AlreadyPending` means the press was
 * intentionally ignored because the previous transaction is still open.
 */
namespace NetworkClicks
{
enum class RequestResult : uint8_t
{
    Accepted,          //!< The request frame has been accepted by the UART and the request-timeout window is now active.
    AlreadyPending,    //!< The same clickable/clickType pair already has one in-flight transaction.
    TransportRejected  //!< The UART rejected the outgoing request frame.
};

// Network clicks
[[nodiscard]] auto request(uint8_t clickableIndex, constants::ClickType clickType)
    -> RequestResult;  // Initiates a network click action and reports whether the transaction was accepted, already pending, or rejected.
[[nodiscard]] auto confirm(uint8_t clickableIndex, constants::ClickType clickType)
    -> bool;  // Confirms a pending network click action after receiving an ACK
void storeNetworkClickTime(uint8_t clickableIndex, constants::ClickType clickType);  // Store click time for a network attached clickable
[[nodiscard]] auto matchesCorrelationId(uint8_t clickableIndex, constants::ClickType clickType, uint8_t correlationId)
    -> bool;                                               // Returns true if the active click matches the given correlation ID.
[[nodiscard]] auto thereAreActiveNetworkClicks() -> bool;  // Returns if there are active stored network clicks
void eraseNetworkClick(uint8_t clickableIndex, constants::ClickType clickType);  // Erase a stored network click
[[nodiscard]] auto isNetworkClickExpired(uint8_t clickableIndex, constants::ClickType clickType)
    -> bool;  // Returns true if the timer of the clickable has passed the threshold, false otherwise
[[nodiscard]] auto checkNetworkClickTimer(uint8_t clickableIndex, constants::ClickType clickType, bool failover)
    -> bool;  // Timeout checks for network clicked clickable, if the time it's over it performs local action and resets its timer
[[nodiscard]] auto checkAllNetworkClicksTimers(bool failover)
    -> bool;  // Timeout checks for all network clicked clickables, if the time it's over it performs local action and resets its timer
}  // namespace NetworkClicks

#endif  // LSH_CORE_CORE_NETWORK_CLICKS_HPP
