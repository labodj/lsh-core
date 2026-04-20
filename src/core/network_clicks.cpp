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
#include "device/clickable_manager.hpp"
#include "internal/etl_array.hpp"
#include "internal/user_config_bridge.hpp"
#include "peripherals/input/clickable.hpp"
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
etl::array<uint16_t, CONFIG_MAX_CLICKABLES>
    longClickedNetworkClickAges_ms{};  //!< Elapsed ages for active long clicks, indexed by clickable index.
etl::array<uint16_t, CONFIG_MAX_CLICKABLES>
    superLongClickedNetworkClickAges_ms{};  //!< Elapsed ages for active super-long clicks, indexed by clickable index.
etl::array<uint8_t, CONFIG_MAX_CLICKABLES>
    longClickedCorrelationIds{};  //!< Correlation IDs for active long clicks, indexed by clickable index.
etl::array<uint8_t, CONFIG_MAX_CLICKABLES>
    superLongClickedCorrelationIds{};  //!< Correlation IDs for active super-long clicks, indexed by clickable index.
etl::array<uint8_t, CONFIG_MAX_CLICKABLES> longClickedAckedFlags{};  //!< Non-zero once the bridge ACK has been accepted for a long click.
etl::array<uint8_t, CONFIG_MAX_CLICKABLES>
    superLongClickedAckedFlags{};                  //!< Non-zero once the bridge ACK has been accepted for a super-long click.
uint8_t activeLongClickedNetworkClicks = 0U;       //!< Number of pending long-click transactions.
uint8_t activeSuperLongClickedNetworkClicks = 0U;  //!< Number of pending super-long-click transactions.
uint8_t nextCorrelationId = 0U;                    //!< Monotonic 8-bit generator. 0 is reserved as "missing/invalid".
uint32_t lastTimersUpdateTime_ms = 0U;             //!< Last cached time used to advance active network-click ages.

auto getAgeStorage(constants::ClickType clickType) -> etl::array<uint16_t, CONFIG_MAX_CLICKABLES> *
{
    using constants::ClickType;
    switch (clickType)
    {
    case ClickType::LONG:
        return &longClickedNetworkClickAges_ms;
    case ClickType::SUPER_LONG:
        return &superLongClickedNetworkClickAges_ms;
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

auto getAckedStorage(constants::ClickType clickType) -> etl::array<uint8_t, CONFIG_MAX_CLICKABLES> *
{
    using constants::ClickType;
    switch (clickType)
    {
    case ClickType::LONG:
        return &longClickedAckedFlags;
    case ClickType::SUPER_LONG:
        return &superLongClickedAckedFlags;
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

/**
 * @brief Clear the stored correlation ID for one pending click slot.
 *
 * @param clickableIndex Dense clickable index that owns the pending transaction.
 * @param clickType Network click type whose slot must be cleared.
 */
void clearCorrelationId(uint8_t clickableIndex, constants::ClickType clickType)
{
    auto *const correlationStorage = getCorrelationStorage(clickType);
    if (correlationStorage == nullptr)
    {
        return;
    }
    (*correlationStorage)[clickableIndex] = 0U;
}

/**
 * @brief Read the correlation ID currently stored for one pending click slot.
 *
 * @param clickableIndex Dense clickable index that owns the pending transaction.
 * @param clickType Network click type whose slot must be read.
 * @return uint8_t Stored correlation ID, or zero when the slot is invalid or empty.
 */
auto getStoredCorrelationId(uint8_t clickableIndex, constants::ClickType clickType) -> uint8_t
{
    auto *const correlationStorage = getCorrelationStorage(clickType);
    if (correlationStorage == nullptr)
    {
        return 0U;
    }
    return (*correlationStorage)[clickableIndex];
}

/**
 * @brief Return whether one clickable currently has an active network-click transaction of the requested type.
 *
 * @param clickableIndex Dense clickable index that owns the slot.
 * @param clickType Network click type to inspect.
 * @return true if the slot currently stores a non-zero correlation ID.
 * @return false otherwise.
 */
auto isNetworkClickActive(uint8_t clickableIndex, constants::ClickType clickType) -> bool
{
    return getStoredCorrelationId(clickableIndex, clickType) != 0U;
}

/**
 * @brief Return whether one pending slot has already accepted the remote ACK.
 */
auto isNetworkClickAcked(uint8_t clickableIndex, constants::ClickType clickType) -> bool
{
    auto *const ackedStorage = getAckedStorage(clickType);
    if (ackedStorage == nullptr)
    {
        return false;
    }
    return (*ackedStorage)[clickableIndex] != 0U;
}

/**
 * @brief Mark one pending slot as acknowledged by the bridge.
 */
void markNetworkClickAcked(uint8_t clickableIndex, constants::ClickType clickType)
{
    auto *const ackedStorage = getAckedStorage(clickType);
    if (ackedStorage == nullptr)
    {
        return;
    }
    (*ackedStorage)[clickableIndex] = 1U;
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
 * @brief Advance every active network-click timer of the specified type.
 * @details Full scans are acceptable here because network clicks are rare and
 *          this work only runs while at least one transaction is pending.
 *
 * @param clickType Pending click type to age.
 * @param elapsed_ms Milliseconds elapsed since the last aging pass.
 */
void ageActiveTimers(constants::ClickType clickType, uint16_t elapsed_ms)
{
    auto *const ageStorage = getAgeStorage(clickType);
    if (ageStorage == nullptr || elapsed_ms == 0U)
    {
        return;
    }

    for (uint8_t clickableIndex = 0U; clickableIndex < Clickables::totalClickables; ++clickableIndex)
    {
        if (!isNetworkClickActive(clickableIndex, clickType))
        {
            continue;
        }
        (*ageStorage)[clickableIndex] = timeUtils::addElapsedTimeSaturated((*ageStorage)[clickableIndex], elapsed_ms);
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
    if (activeLongClickedNetworkClicks == 0U && activeSuperLongClickedNetworkClicks == 0U)
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

    if (activeLongClickedNetworkClicks != 0U)
    {
        ageActiveTimers(constants::ClickType::LONG, elapsed_ms);
    }
    if (activeSuperLongClickedNetworkClicks != 0U)
    {
        ageActiveTimers(constants::ClickType::SUPER_LONG, elapsed_ms);
    }
}

/**
 * @brief Timeout checks for every pending click of the specified type.
 *
 * @details The storage stays array-based to minimise RAM. Full scans are
 * only performed while at least one click of the given type is pending,
 * which keeps the steady-state cost near zero for a feature that is used
 * very rarely in normal installations.
 * @param clickType The click type to sweep (LONG or SUPER_LONG).
 * @param failover If true, forces the fallback action for all pending clicks of that type, ignoring their timers.
 * @return true if any timer expired (or was forced by failover) and at least one local fallback action was performed.
 * @return false otherwise.
 */
auto checkAllNetworkClicksTimers(constants::ClickType clickType, bool failover) -> bool
{
    DP_CONTEXT();
    using constants::NoNetworkClickType;
    using constants::timings::LCNB_TIMEOUT_MS;
    bool localFallbackPerformed = false;  // True if a network click expired and a local click has been performed

    auto *const ageStorage = getAgeStorage(clickType);
    auto *const activeCount = getActiveCountStorage(clickType);
    if (ageStorage == nullptr || activeCount == nullptr || *activeCount == 0U)
    {
        return false;
    }

    for (uint8_t clickableIndex = 0U; clickableIndex < Clickables::totalClickables; ++clickableIndex)
    {
        if (!isNetworkClickActive(clickableIndex, clickType))
        {
            continue;
        }

        if (isNetworkClickAcked(clickableIndex, clickType))
        {
            if ((*ageStorage)[clickableIndex] > LCNB_TIMEOUT_MS)
            {
                DPL("Dropping acknowledged network click after confirm retry timeout.");
                eraseNetworkClick(clickableIndex, clickType);
                if (*activeCount == 0U)
                {
                    break;
                }
                continue;
            }

            const uint8_t correlationId = getStoredCorrelationId(clickableIndex, clickType);
            if (correlationId != 0U && Serializer::serializeNetworkClick(clickableIndex, clickType, true, correlationId))
            {
                eraseNetworkClick(clickableIndex, clickType);
                if (*activeCount == 0U)
                {
                    break;
                }
            }
            continue;
        }

        if (failover || (*ageStorage)[clickableIndex] > LCNB_TIMEOUT_MS)  // If expired or failover
        {
            DPL(FPSTR(dStr::EXPIRED), FPSTR(dStr::SPACE), FPSTR(dStr::CLICKABLE), FPSTR(dStr::SPACE), FPSTR(dStr::INDEX),
                FPSTR(dStr::COLON_SPACE), clickableIndex);
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
    if (isNetworkClickActive(clickableIndex, clickType))
    {
        DPL("Rejecting new network click because the same clickable and click type still have one pending transaction.");
        return RequestResult::AlreadyPending;
    }

    storeNetworkClickTime(clickableIndex, clickType);
    if (!Serializer::serializeNetworkClick(clickableIndex, clickType, false, getStoredCorrelationId(clickableIndex, clickType)))
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
    const uint8_t correlationId = getStoredCorrelationId(clickableIndex, clickType);
    if (correlationId == 0U)
    {
        return thereAreActiveNetworkClicks();
    }

    markNetworkClickAcked(clickableIndex, clickType);
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
 * @brief Store click time for a network attached clickable.
 *
 * @param clickableIndex the index of the clickable.
 * @param clickType the type of the click (long or super long).
 */
void storeNetworkClickTime(uint8_t clickableIndex, constants::ClickType clickType)
{
    DP_CONTEXT();
    DPL(FPSTR(dStr::SPACE), FPSTR(dStr::DIVIDER), FPSTR(dStr::SPACE), FPSTR(dStr::CLICKABLE), FPSTR(dStr::SPACE), FPSTR(dStr::INDEX),
        FPSTR(dStr::COLON_SPACE), clickableIndex, FPSTR(dStr::SPACE), FPSTR(dStr::DIVIDER), FPSTR(dStr::SPACE), FPSTR(dStr::CLICK),
        FPSTR(dStr::SPACE), FPSTR(dStr::TYPE), FPSTR(dStr::COLON_SPACE), static_cast<int8_t>(clickType));

    auto *const ageStorage = getAgeStorage(clickType);
    if (ageStorage == nullptr)
    {
        return;
    }

    advanceActiveTimersTo(timeKeeper::getTime());
    markNetworkClickActive(clickableIndex, clickType);
    (*ageStorage)[clickableIndex] = 0U;
    auto *const ackedStorage = getAckedStorage(clickType);
    if (ackedStorage != nullptr)
    {
        (*ackedStorage)[clickableIndex] = 0U;
    }
    auto *const correlationStorage = getCorrelationStorage(clickType);
    if (correlationStorage != nullptr)
    {
        (*correlationStorage)[clickableIndex] = generateCorrelationId();
    }
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
    auto *const ageStorage = getAgeStorage(clickType);
    if (ageStorage == nullptr)
    {
        return;
    }
    markNetworkClickInactive(clickableIndex, clickType);
    (*ageStorage)[clickableIndex] = 0U;
    auto *const ackedStorage = getAckedStorage(clickType);
    if (ackedStorage != nullptr)
    {
        (*ackedStorage)[clickableIndex] = 0U;
    }
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
    auto *const ageStorage = getAgeStorage(clickType);
    if (ageStorage == nullptr)
    {
        return true;  // Clickable type is not valid, so it's "expired"
    }

    if (!isNetworkClickActive(clickableIndex, clickType))
    {
        return true;
    }

    if (isNetworkClickAcked(clickableIndex, clickType))
    {
        advanceActiveTimersTo(timeKeeper::getTime());
        if ((*ageStorage)[clickableIndex] > LCNB_TIMEOUT_MS)
        {
            eraseNetworkClick(clickableIndex, clickType);
            return true;
        }
        return false;
    }

    advanceActiveTimersTo(timeKeeper::getTime());
    if ((*ageStorage)[clickableIndex] > LCNB_TIMEOUT_MS)  // It's expired
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

    auto *const ageStorage = getAgeStorage(clickType);
    if (ageStorage == nullptr)
    {
        return false;
    }

    bool localFallbackPerformed = false;  // True if a network click expired and a local click has been performed

    advanceActiveTimersTo(timeKeeper::getTime());
    if (isNetworkClickActive(clickableIndex, clickType))
    {
        if (isNetworkClickAcked(clickableIndex, clickType))
        {
            if ((*ageStorage)[clickableIndex] > LCNB_TIMEOUT_MS)
            {
                eraseNetworkClick(clickableIndex, clickType);
                return false;
            }

            const uint8_t correlationId = getStoredCorrelationId(clickableIndex, clickType);
            if (correlationId != 0U && Serializer::serializeNetworkClick(clickableIndex, clickType, true, correlationId))
            {
                eraseNetworkClick(clickableIndex, clickType);
            }
            return false;
        }

        if (failover || (*ageStorage)[clickableIndex] > LCNB_TIMEOUT_MS)  // expired
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
    bool localFallbackPerformed = false;  // True if any network click expired and any local click has been performed
    const auto now = timeKeeper::getTime();
    advanceActiveTimersTo(now);

    if (activeLongClickedNetworkClicks != 0U)
    {
        DPL(FPSTR(dStr::LONG), " ", FPSTR(dStr::CLICKED), FPSTR(dStr::NET_BUTTONS_NOT_EMPTY));
        localFallbackPerformed |= checkAllNetworkClicksTimers(ClickType::LONG, failover);
    }
    if (activeSuperLongClickedNetworkClicks != 0U)
    {
        DPL(FPSTR(dStr::SUPER_LONG), " ", FPSTR(dStr::CLICKED), FPSTR(dStr::NET_BUTTONS_NOT_EMPTY));
        localFallbackPerformed |= checkAllNetworkClicksTimers(ClickType::SUPER_LONG, failover);
    }
    return localFallbackPerformed;
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
 * @brief Stubbed timer-start path used when network clicks are compiled out.
 */
void storeNetworkClickTime(uint8_t clickableIndex, constants::ClickType clickType)
{
    static_cast<void>(clickableIndex);
    static_cast<void>(clickType);
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
