/**
 * @file    network_clicks.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic for handling network click requests, confirmations, and timeouts.
 *
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

#include "core/network_clicks.hpp"

#include "communication/serializer.hpp"
#include "config/static_config.hpp"
#include "internal/etl_array.hpp"
#include "util/constants/config.hpp"
#include "util/constants/timing.hpp"
#include "util/debug/debug.hpp"
#include "util/saturating_time.hpp"
#include "util/time_keeper.hpp"

using namespace Debug;

namespace NetworkClicks
{
#if CONFIG_USE_NETWORK_CLICKS
namespace
{
constexpr uint8_t NETWORK_CLICK_FLAG_ACTIVE = 0x01U;
constexpr uint8_t NETWORK_CLICK_FLAG_ACKED = 0x02U;

struct ActiveNetworkClick
{
    uint16_t age_ms = 0U;
    uint8_t correlationId = 0U;
    uint8_t flags = 0U;
};

etl::array<ActiveNetworkClick, constants::config::ACTIVE_NETWORK_CLICK_STORAGE_CAPACITY>
    activeNetworkClicks{};              //!< Pending network-click transactions, dense and compact.
uint8_t activeNetworkClickCount = 0U;   //!< Number of active entries in `activeNetworkClicks`.
uint8_t nextCorrelationId = 0U;         //!< Monotonic 8-bit generator. 0 is reserved as "missing/invalid".
uint32_t lastTimersUpdateTime_ms = 0U;  //!< Last cached time used to advance active network-click ages.

auto isSupportedNetworkClickType(constants::ClickType clickType) -> bool
{
    using constants::ClickType;
    return clickType == ClickType::LONG || clickType == ClickType::SUPER_LONG;
}

auto isNetworkClickActive(const ActiveNetworkClick &entry) -> bool
{
    return (entry.flags & NETWORK_CLICK_FLAG_ACTIVE) != 0U;
}

auto isNetworkClickAcked(const ActiveNetworkClick &entry) -> bool
{
    return (entry.flags & NETWORK_CLICK_FLAG_ACKED) != 0U;
}

void markNetworkClickAcked(ActiveNetworkClick &entry)
{
    entry.flags |= NETWORK_CLICK_FLAG_ACKED;
}

auto getNetworkClickSlotIndex(uint8_t clickableIndex, constants::ClickType clickType) -> uint8_t
{
    if (!isSupportedNetworkClickType(clickType))
    {
        return UINT8_MAX;
    }
    return lsh::core::static_config::getNetworkClickSlotIndex(clickableIndex, clickType);
}

/**
 * @brief Allocate one non-zero correlation ID for a new network-click transaction.
 * @details Correlation ID zero is reserved as "missing" so the generator wraps
 *          around it when the 8-bit counter overflows.
 *
 * @return uint8_t Correlation ID to embed in the outgoing request payload.
 */
auto generateCorrelationId() -> uint8_t
{
    ++nextCorrelationId;
    if (nextCorrelationId == 0U)
    {
        ++nextCorrelationId;
    }
    return nextCorrelationId;
}

auto findActiveNetworkClickSlotIndex(uint8_t clickableIndex, constants::ClickType clickType) -> uint8_t
{
    const uint8_t slotIndex = getNetworkClickSlotIndex(clickableIndex, clickType);
    if (slotIndex >= constants::config::MAX_ACTIVE_NETWORK_CLICKS)
    {
        return UINT8_MAX;
    }
    return isNetworkClickActive(activeNetworkClicks[slotIndex]) ? slotIndex : UINT8_MAX;
}

auto findActiveNetworkClick(uint8_t clickableIndex, constants::ClickType clickType) -> ActiveNetworkClick *
{
    const uint8_t slotIndex = findActiveNetworkClickSlotIndex(clickableIndex, clickType);
    return slotIndex == UINT8_MAX ? nullptr : &activeNetworkClicks[slotIndex];
}

void clearActiveNetworkClickSlot(uint8_t slotIndex)
{
    auto &entry = activeNetworkClicks[slotIndex];
    if (isNetworkClickActive(entry) && activeNetworkClickCount != 0U)
    {
        --activeNetworkClickCount;
    }
    entry.age_ms = 0U;
    entry.correlationId = 0U;
    entry.flags = 0U;
}

void eraseActiveNetworkClickAt(uint8_t slotIndex)
{
    if (slotIndex >= constants::config::MAX_ACTIVE_NETWORK_CLICKS)
    {
        return;
    }
    clearActiveNetworkClickSlot(slotIndex);
}

auto appendActiveNetworkClick(uint8_t clickableIndex, constants::ClickType clickType) -> ActiveNetworkClick *
{
    if (activeNetworkClickCount >= constants::config::MAX_ACTIVE_NETWORK_CLICKS)
    {
        return nullptr;
    }

    const uint8_t slotIndex = getNetworkClickSlotIndex(clickableIndex, clickType);
    if (slotIndex >= constants::config::MAX_ACTIVE_NETWORK_CLICKS)
    {
        return nullptr;
    }

    auto &entry = activeNetworkClicks[slotIndex];
    if (isNetworkClickActive(entry))
    {
        return nullptr;
    }

    entry.age_ms = 0U;
    entry.correlationId = generateCorrelationId();
    entry.flags = NETWORK_CLICK_FLAG_ACTIVE;
    ++activeNetworkClickCount;
    return &entry;
}

template <uint8_t SlotIndex> void advanceActiveTimerSlots(uint16_t elapsed_ms)
{
    // The number of network-click slots is fully generated. Recursing over the
    // compile-time slot count lets AVR-GCC erase the loop counter and branch
    // bookkeeping for small profiles while preserving identical behavior.
    if constexpr (SlotIndex < constants::config::MAX_ACTIVE_NETWORK_CLICKS)
    {
        auto &entry = activeNetworkClicks[SlotIndex];
        if (isNetworkClickActive(entry))
        {
            entry.age_ms = timeUtils::addElapsedTimeSaturated(entry.age_ms, elapsed_ms);
        }
        advanceActiveTimerSlots<static_cast<uint8_t>(SlotIndex + 1U)>(elapsed_ms);
    }
}

/**
 * @brief Bring all active network-click timers up to the current cached time.
 * @details This keeps per-clickable storage at 16 bits while ensuring ACK
 *          handling and timeout sweeps both observe fresh timer values. Only
 *          one 32-bit delta is paid for the whole module.
 *
 * @param now Cached current time from the main loop or dispatcher.
 */
void advanceActiveTimersTo(uint32_t now)
{
    if (activeNetworkClickCount == 0U)
    {
        lastTimersUpdateTime_ms = now;
        return;
    }

    const uint32_t elapsedSinceLastUpdate_ms = now - lastTimersUpdateTime_ms;
    if (elapsedSinceLastUpdate_ms == 0U)
    {
        return;
    }

    lastTimersUpdateTime_ms = now;
    const uint16_t elapsed_ms = (elapsedSinceLastUpdate_ms > UINT16_MAX) ? UINT16_MAX : static_cast<uint16_t>(elapsedSinceLastUpdate_ms);
    advanceActiveTimerSlots<0U>(elapsed_ms);
}

template <uint8_t SlotIndex> auto checkAllNetworkClickTimerSlots(bool failover) -> bool
{
    // Keep this sweep generated-slot based instead of walking a dynamic vector:
    // each slot has a fixed clickable/type owner, so the compiler can fold the
    // slot-index dispatch used by the generated static_config accessors.
    if constexpr (SlotIndex >= constants::config::MAX_ACTIVE_NETWORK_CLICKS)
    {
        static_cast<void>(failover);
        return false;
    }

    using constants::timings::LCNB_TIMEOUT_MS;
    bool localFallbackPerformed = false;
    auto &entry = activeNetworkClicks[SlotIndex];
    if (isNetworkClickActive(entry))
    {
        const constants::ClickType clickType = lsh::core::static_config::getNetworkClickType(SlotIndex);
        const uint8_t clickableIndex = lsh::core::static_config::getNetworkClickClickableIndex(SlotIndex);

        if (isNetworkClickAcked(entry))
        {
            if (entry.age_ms > LCNB_TIMEOUT_MS)
            {
                DPL("Dropping acknowledged network click after confirm retry timeout.");
                eraseActiveNetworkClickAt(SlotIndex);
            }
            else if (entry.correlationId != 0U && Serializer::serializeNetworkClick(clickableIndex, clickType, true, entry.correlationId))
            {
                eraseActiveNetworkClickAt(SlotIndex);
            }
        }
        else if (failover || entry.age_ms > LCNB_TIMEOUT_MS)
        {
            DPL(FPSTR(dStr::EXPIRED), FPSTR(dStr::SPACE), FPSTR(dStr::CLICKABLE), FPSTR(dStr::SPACE), FPSTR(dStr::INDEX),
                FPSTR(dStr::COLON_SPACE), clickableIndex);
            localFallbackPerformed = lsh::core::static_config::runNetworkClickFallback(clickableIndex, clickType);
            eraseActiveNetworkClickAt(SlotIndex);
        }
    }

    const bool laterFallbackPerformed = checkAllNetworkClickTimerSlots<static_cast<uint8_t>(SlotIndex + 1U)>(failover);
    return localFallbackPerformed || laterFallbackPerformed;
}
}  // namespace

using constants::ClickType;

/**
 * @brief Initiates a network click action.
 * @details Sends the initial network click request and keeps the pending
 *          timeout active only if the frame is accepted by the UART. If the
 *          serial transport rejects the request, the pending click is removed
 *          immediately so the caller can decide whether to execute a local
 *          fallback action instead of silently losing the press.
 * @param clickableIndex The index of the clickable that was pressed.
 * @param clickType The type of click (LONG or SUPER_LONG).
 * @return RequestResult::Accepted if the request frame has been accepted by
 *         the UART and the pending timeout should remain active.
 * @return RequestResult::AlreadyPending if the same clickable/type pair still
 *         has one unresolved transaction and this press should be ignored.
 * @return RequestResult::TransportRejected if the UART rejected the request
 *         and the caller may decide to execute a local fallback instead.
 */
auto request(uint8_t clickableIndex, constants::ClickType clickType) -> RequestResult
{
    DP_CONTEXT();
    if (!isSupportedNetworkClickType(clickType))
    {
        return RequestResult::TransportRejected;
    }
    if (findActiveNetworkClick(clickableIndex, clickType) != nullptr)
    {
        DPL("Rejecting new network click because the same clickable and click type still have one pending transaction.");
        return RequestResult::AlreadyPending;
    }

    advanceActiveTimersTo(timeKeeper::getTime());
    auto *const entry = appendActiveNetworkClick(clickableIndex, clickType);
    if (entry == nullptr)
    {
        DPL("Rejecting new network click because the active pool is full.");
        return RequestResult::TransportRejected;
    }

    if (!Serializer::serializeNetworkClick(clickableIndex, clickType, false, entry->correlationId))
    {
        eraseNetworkClick(clickableIndex, clickType);
        return RequestResult::TransportRejected;
    }
    return RequestResult::Accepted;
}

/**
 * @brief Confirms a pending network click action after receiving an ACK.
 * @details Once the ACK has been accepted from the bridge, the local timeout
 *          must stop immediately or the click could later trigger a spurious
 *          local fallback even though the network path already succeeded. The
 *          slot therefore stays active but moves into an "ACKed" sub-state:
 *          no local fallback is allowed anymore, and the loop retries
 *          `NETWORK_CLICK_CONFIRM` until the UART accepts it or a bounded
 *          cleanup timeout expires.
 * @param clickableIndex The index of the clickable to confirm.
 * @param clickType The type of click to confirm.
 * @return true if there are still other active network clicks pending, false otherwise.
 */
auto confirm(uint8_t clickableIndex, constants::ClickType clickType) -> bool
{
    DP_CONTEXT();
    auto *const entry = findActiveNetworkClick(clickableIndex, clickType);
    if (entry == nullptr || entry->correlationId == 0U)
    {
        return thereAreActiveNetworkClicks();
    }

    markNetworkClickAcked(*entry);
    // Once the ACK has arrived, the confirm retry budget must start from the
    // ACK moment, not from the original request. Otherwise a late ACK could
    // leave no real time for the bridge to receive NETWORK_CLICK_CONFIRM.
    entry->age_ms = 0U;
    const uint8_t correlationId = entry->correlationId;
    if (Serializer::serializeNetworkClick(clickableIndex, clickType, true, correlationId))
    {
        eraseNetworkClick(clickableIndex, clickType);
    }
    else
    {
        DPL("Keeping acknowledged network click pending until NETWORK_CLICK_CONFIRM is accepted by the UART.");
    }
    return thereAreActiveNetworkClicks();
}

/**
 * @brief Check whether a pending click slot still carries the expected correlation ID.
 *
 * @param clickableIndex Dense clickable index that owns the pending transaction.
 * @param clickType Network click type to inspect.
 * @param correlationId Correlation ID received from the bridge payload.
 * @return true if the slot is active and still matches the provided correlation ID.
 * @return false otherwise.
 */
auto matchesCorrelationId(uint8_t clickableIndex, constants::ClickType clickType, uint8_t correlationId) -> bool
{
    if (correlationId == 0U)
    {
        return false;
    }
    const auto *const entry = findActiveNetworkClick(clickableIndex, clickType);
    return entry != nullptr && entry->correlationId == correlationId;
}

/**
 * @brief Get if there are active stored network clicks.
 *
 * @return true if active stored network clicks are found.
 * @return false there are no stored network click.
 */
auto thereAreActiveNetworkClicks() -> bool
{
    return activeNetworkClickCount != 0U;
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
    eraseActiveNetworkClickAt(findActiveNetworkClickSlotIndex(clickableIndex, clickType));
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

    auto *const entry = findActiveNetworkClick(clickableIndex, clickType);
    if (entry == nullptr)
    {
        return true;
    }

    advanceActiveTimersTo(timeKeeper::getTime());
    if (entry->age_ms > LCNB_TIMEOUT_MS)  // It's expired
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
    using constants::timings::LCNB_TIMEOUT_MS;

    auto *const entry = findActiveNetworkClick(clickableIndex, clickType);
    if (entry == nullptr)
    {
        return false;
    }

    bool localFallbackPerformed = false;  // True if a network click expired and a local click has been performed

    advanceActiveTimersTo(timeKeeper::getTime());
    if (isNetworkClickAcked(*entry))
    {
        if (entry->age_ms > LCNB_TIMEOUT_MS)
        {
            eraseNetworkClick(clickableIndex, clickType);
            return false;
        }

        if (Serializer::serializeNetworkClick(clickableIndex, clickType, true, entry->correlationId))
        {
            eraseNetworkClick(clickableIndex, clickType);
        }
        return false;
    }

    if (failover || entry->age_ms > LCNB_TIMEOUT_MS)  // expired
    {
        localFallbackPerformed |= lsh::core::static_config::runNetworkClickFallback(clickableIndex, clickType);
        eraseNetworkClick(clickableIndex, clickType);
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
    const auto now = timeKeeper::getTime();
    advanceActiveTimersTo(now);
    if (activeNetworkClickCount == 0U)
    {
        return false;
    }
    return checkAllNetworkClickTimerSlots<0U>(failover);
}

#else
/**
 * @brief Stubbed request path used when network clicks are compiled out.
 * @details Keeping the symbol available preserves the public API while making
 *          the generated firmware pay effectively zero runtime state for a
 *          feature the consumer does not use.
 */
auto request(uint8_t clickableIndex, constants::ClickType clickType) -> RequestResult
{
    static_cast<void>(clickableIndex);
    static_cast<void>(clickType);
    return RequestResult::TransportRejected;
}

/**
 * @brief Stubbed confirmation path used when network clicks are compiled out.
 */
auto confirm(uint8_t clickableIndex, constants::ClickType clickType) -> bool
{
    static_cast<void>(clickableIndex);
    static_cast<void>(clickType);
    return false;
}

/**
 * @brief Stubbed correlation matcher used when network clicks are compiled out.
 */
auto matchesCorrelationId(uint8_t clickableIndex, constants::ClickType clickType, uint8_t correlationId) -> bool
{
    static_cast<void>(clickableIndex);
    static_cast<void>(clickType);
    static_cast<void>(correlationId);
    return false;
}

/**
 * @brief Reports no pending network clicks when the feature is compiled out.
 */
auto thereAreActiveNetworkClicks() -> bool
{
    return false;
}

/**
 * @brief Stubbed erase path used when network clicks are compiled out.
 */
void eraseNetworkClick(uint8_t clickableIndex, constants::ClickType clickType)
{
    static_cast<void>(clickableIndex);
    static_cast<void>(clickType);
}

/**
 * @brief Reports every network click as expired when the feature is compiled out.
 */
auto isNetworkClickExpired(uint8_t clickableIndex, constants::ClickType clickType) -> bool
{
    static_cast<void>(clickableIndex);
    static_cast<void>(clickType);
    return true;
}

/**
 * @brief Stubbed single-timer checker used when network clicks are compiled out.
 */
auto checkNetworkClickTimer(uint8_t clickableIndex, constants::ClickType clickType, bool failover) -> bool
{
    static_cast<void>(clickableIndex);
    static_cast<void>(clickType);
    static_cast<void>(failover);
    return false;
}

/**
 * @brief Stubbed timeout sweep used when network clicks are compiled out.
 */
auto checkAllNetworkClicksTimers(bool failover) -> bool
{
    static_cast<void>(failover);
    return false;
}
#endif
}  // namespace NetworkClicks
