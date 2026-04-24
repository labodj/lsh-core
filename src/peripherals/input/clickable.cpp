/**
 * @file    clickable.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic of the Clickable class, including its state machine.
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

#include "peripherals/input/clickable.hpp"

#include "config/static_config.hpp"
#include "device/actuator_manager.hpp"
#include "device/clickable_manager.hpp"
#include "peripherals/output/actuator.hpp"
#include "util/debug/debug.hpp"
#include "util/saturating_time.hpp"

using constants::ClickType;
using namespace Debug;

/**
 * @brief Construct one iterator over a clickable actuator-link slice.
 *
 * @param ownerIndex Clickable dense index that owns the generated link slice.
 * @param linkType Click type whose generated link slice is being visited.
 * @param relativeIndex Relative link index inside the generated static slice.
 */
ClickableActuatorLinksView::Iterator::Iterator(uint8_t ownerIndex, constants::ClickType linkType, uint8_t relativeIndex) noexcept :
    clickableIndex(ownerIndex), clickType(linkType), linkIndex(relativeIndex)
{}

/**
 * @brief Dereference the current compact actuator-link entry.
 *
 * @return uint8_t The dense actuator index stored in the current link.
 */
auto ClickableActuatorLinksView::Iterator::operator*() const -> uint8_t
{
    return lsh::core::static_config::getClickActuatorLink(this->clickType, this->clickableIndex, this->linkIndex);
}

/**
 * @brief Advance the iterator to the next compact actuator-link entry.
 *
 * @return Iterator& This iterator after the increment.
 */
auto ClickableActuatorLinksView::Iterator::operator++() -> Iterator &
{
    ++this->linkIndex;
    return *this;
}

/**
 * @brief Compare two compact actuator-link iterators for equality.
 *
 * @param other The iterator to compare with.
 * @return true if both iterators point to the same compact entry.
 * @return false otherwise.
 */
auto ClickableActuatorLinksView::Iterator::operator==(const Iterator &other) const -> bool
{
    return this->clickableIndex == other.clickableIndex && this->clickType == other.clickType && this->linkIndex == other.linkIndex;
}

/**
 * @brief Compare two compact actuator-link iterators for inequality.
 *
 * @param other The iterator to compare with.
 * @return true if the iterators point to different compact entries.
 * @return false otherwise.
 */
auto ClickableActuatorLinksView::Iterator::operator!=(const Iterator &other) const -> bool
{
    return !(*this == other);
}

/**
 * @brief Construct a read-only view over one compact clickable actuator-link slice.
 *
 * @param ownerIndex Clickable dense index that owns the generated link slice.
 * @param linkType Click type whose generated link slice is exposed.
 * @param linkCount Number of valid entries in the slice.
 */
ClickableActuatorLinksView::ClickableActuatorLinksView(uint8_t ownerIndex, constants::ClickType linkType, uint8_t linkCount) noexcept :
    clickableIndex(ownerIndex), clickType(linkType), count(linkCount)
{}

/**
 * @brief Return an iterator to the beginning of the compact actuator-link slice.
 *
 * @return Iterator Iterator that points to the first valid compact link entry.
 */
auto ClickableActuatorLinksView::begin() const -> Iterator
{
    return this->empty() ? Iterator() : Iterator(this->clickableIndex, this->clickType, 0U);
}

/**
 * @brief Return an iterator one past the end of the compact actuator-link slice.
 *
 * @return Iterator Iterator that points one element past the last valid entry.
 */
auto ClickableActuatorLinksView::end() const -> Iterator
{
    return this->empty() ? Iterator() : Iterator(this->clickableIndex, this->clickType, this->count);
}

/**
 * @brief Return whether the compact actuator-link slice contains no entries.
 *
 * @return true if the slice is empty.
 * @return false otherwise.
 */
auto ClickableActuatorLinksView::empty() const -> bool
{
    return this->count == 0U;
}

/**
 * @brief Return the number of compact actuator-link entries in the slice.
 *
 * @return uint8_t Total number of valid entries.
 */
auto ClickableActuatorLinksView::size() const -> uint8_t
{
    return this->count;
}

/**
 * @brief Store the Clickable index on Clickables namespace Array.
 *
 * @param indexToSet index to set.
 */
void Clickable::setIndex(uint8_t indexToSet)
{
    this->index = indexToSet;
}

void Clickable::refreshQuickClickability()
{
    this->configFlags.isQuickClickable =
        (this->configFlags.isShortClickable && !this->configFlags.isLongClickable && !this->configFlags.isSuperLongClickable);
}

/**
 * @brief Set clickable short clickability.
 *
 * @param shortClickable if the clickable is short clickable.
 * @return Clickable& the object instance.
 */
auto Clickable::setClickableShort(bool shortClickable) -> Clickable &
{
    this->configFlags.isShortClickable = shortClickable;
    this->refreshQuickClickability();
    return *this;
}

/**
 * @brief Set clickable long clickability locally and over network.
 *
 * @param longClickable if the clickable is long clickable.
 * @param networkClickable if the clickable is long clickable over network.
 * @param fallback the fallback type (if network isn't working).
 * @return Clickable& the object instance.
 */
auto Clickable::setClickableLong(bool longClickable, bool networkClickable, constants::NoNetworkClickType fallback) -> Clickable &
{
    this->configFlags.isLongClickable = longClickable;
    this->configFlags.isNetworkLongClickable = networkClickable;
    this->configFlags.longNetworkFallbackDoNothing = fallback != constants::NoNetworkClickType::LOCAL_FALLBACK;
    this->refreshQuickClickability();
    return *this;
}

/**
 * @brief Source-compatible long-click setter for older hand-written setup code.
 *
 * @param longClickable if the clickable is long clickable.
 * @param clickType ignored because static profiles generate click types at compile time.
 * @param networkClickable if the clickable is long clickable over network.
 * @param fallback the fallback type (if network isn't working).
 * @return Clickable& the object instance.
 */
auto Clickable::setClickableLong(bool longClickable,
                                 constants::LongClickType clickType,
                                 bool networkClickable,
                                 constants::NoNetworkClickType fallback) -> Clickable &
{
    static_cast<void>(clickType);
    return this->setClickableLong(longClickable, networkClickable, fallback);
}

/**
 * @brief Set clickable super long clickability locally and over network.
 *
 * @param superLongClickable if the clickable is super long clickable.
 * @param networkClickable if the clickable is super long clickable over network.
 * @param fallback the fallback type (if network isn't working).
 * @return Clickable& the object instance.
 */
auto Clickable::setClickableSuperLong(bool superLongClickable, bool networkClickable, constants::NoNetworkClickType fallback) -> Clickable &
{
    this->configFlags.isSuperLongClickable = superLongClickable;
    this->configFlags.isNetworkSuperLongClickable = networkClickable;
    this->configFlags.superLongNetworkFallbackDoNothing = fallback != constants::NoNetworkClickType::LOCAL_FALLBACK;
    this->refreshQuickClickability();
    return *this;
}

/**
 * @brief Source-compatible super-long-click setter for older hand-written setup code.
 *
 * @param superLongClickable if the clickable is super long clickable.
 * @param clickType ignored because static profiles generate click types at compile time.
 * @param networkClickable if the clickable is super long clickable over network.
 * @param fallback the fallback type (if network isn't working).
 * @return Clickable& the object instance.
 */
auto Clickable::setClickableSuperLong(bool superLongClickable,
                                      constants::SuperLongClickType clickType,
                                      bool networkClickable,
                                      constants::NoNetworkClickType fallback) -> Clickable &
{
    static_cast<void>(clickType);
    return this->setClickableSuperLong(superLongClickable, networkClickable, fallback);
}

/**
 * @brief Source-compatible actuator-link setter.
 * @details Static TOML profiles generate actuator links as compile-time
 *          accessors, so this method intentionally leaves no runtime state.
 *
 * @param actuatorIndex the actuator index to be attached.
 * @param actuatorType the type of the actuator.
 * @return Clickable& the object instance.
 */
auto Clickable::addActuator(uint8_t actuatorIndex, constants::ClickType actuatorType) -> Clickable &
{
    static_cast<void>(actuatorIndex);
    static_cast<void>(actuatorType);
    return *this;
}

/**
 * @brief Add a short click actuator.
 *
 * @param actuatorIndex actuator index to be attached.
 * @return Clickable& the object instance.
 */
auto Clickable::addActuatorShort(uint8_t actuatorIndex) -> Clickable &
{
    this->addActuator(actuatorIndex, ClickType::SHORT);
    return *this;
}

/**
 * @brief Add a long click actuator.
 *
 * @param actuatorIndex actuator index to be attached.
 * @return Clickable& the object instance.
 */
auto Clickable::addActuatorLong(uint8_t actuatorIndex) -> Clickable &
{
    this->addActuator(actuatorIndex, ClickType::LONG);
    return *this;
}

/**
 * @brief Add a super long click actuator.
 *
 * @param actuatorIndex actuator index to be attached.
 * @return Clickable& the object instance.
 */
auto Clickable::addActuatorSuperLong(uint8_t actuatorIndex) -> Clickable &
{
    this->addActuator(actuatorIndex, ClickType::SUPER_LONG);
    return *this;
}

/**
 * @brief Set long click time.
 * @details Static TOML profiles generate the timing accessor at compile time.
 *          This setter only mutates state in the legacy non-pool fallback path.
 *
 * @param timeToSet_ms long click time in ms.
 * @return Clickable& the object instance.
 */
auto Clickable::setLongClickTime(uint16_t timeToSet_ms) -> Clickable &
{
#if CONFIG_USE_CLICKABLE_TIMING_POOL
    static_cast<void>(timeToSet_ms);
#else
    this->longClick_ms = timeToSet_ms;
#endif
    return *this;
}

/**
 * @brief Set super long click time.
 * @details Static TOML profiles generate the timing accessor at compile time.
 *          This setter only mutates state in the legacy non-pool fallback path.
 *
 * @param timeToSet_ms super long click time in ms.
 * @return Clickable& the object instance.
 */
auto Clickable::setSuperLongClickTime(uint16_t timeToSet_ms) -> Clickable &
{
#if CONFIG_USE_CLICKABLE_TIMING_POOL
    static_cast<void>(timeToSet_ms);
#else
    this->superLongClick_ms = timeToSet_ms;
#endif
    return *this;
}

auto Clickable::getLongClickTime() const -> uint16_t
{
#if CONFIG_USE_CLICKABLE_TIMING_POOL
    return lsh::core::static_config::getLongClickTime(this->index);
#else
    return this->longClick_ms;
#endif
}

auto Clickable::getSuperLongClickTime() const -> uint16_t
{
#if CONFIG_USE_CLICKABLE_TIMING_POOL
    return lsh::core::static_config::getSuperLongClickTime(this->index);
#else
    return this->superLongClick_ms;
#endif
}

/**
 * @brief Get the Clickable index on Clickables namespace Array.
 *
 * @return uint8_t clickable index.
 */
auto Clickable::getIndex() const -> uint8_t
{
    return this->index;
}

/**
 * @brief Get the read-only view of attached actuators.
 * @details The returned view stores only the clickable index, click type and
 *          count; each dereference reads the generated static accessor.
 *
 * @param actuatorType the type of actuator vector to get.
 * @return ClickableActuatorLinksView the attached actuators compact view.
 */
auto Clickable::getActuators(constants::ClickType actuatorType) const -> ClickableActuatorLinksView
{
    switch (actuatorType)
    {
    case ClickType::SHORT:
        return ClickableActuatorLinksView(this->index, ClickType::SHORT, this->getTotalActuators(ClickType::SHORT));
    case ClickType::LONG:
        return ClickableActuatorLinksView(this->index, ClickType::LONG, this->getTotalActuators(ClickType::LONG));
    case ClickType::SUPER_LONG:
        return ClickableActuatorLinksView(this->index, ClickType::SUPER_LONG, this->getTotalActuators(ClickType::SUPER_LONG));
    default:
        return {};  // Actuator Type mismatch
    }
}

/**
 * @brief Get the number of attached actuators of one kind.
 *
 * @param actuatorType the type of the actuator.
 * @return uint8_t the number of attached actuators of one kind.
 */
auto Clickable::getTotalActuators(constants::ClickType actuatorType) const -> uint8_t
{
    return lsh::core::static_config::getClickActuatorLinkCount(actuatorType, this->index);
}

/**
 * @brief Get the type of long click.
 * @details The value belongs to the generated static profile, not to the
 *          Clickable runtime object.
 *
 * @return constants::LongClickType the type of long click.
 */
auto Clickable::getLongClickType() const -> constants::LongClickType
{
    return lsh::core::static_config::getLongClickType(this->index);
}

/**
 * @brief Get if the clickable performs network clicks.
 *
 * @param clickType for which click type you want to know the network clickability.
 * @return true if the clickable in network clickable.
 * @return false if the clickable isn't network clickable.
 */
auto Clickable::isNetworkClickable(constants::ClickType clickType) const -> bool
{
    switch (clickType)
    {
    case ClickType::LONG:
        return this->configFlags.isNetworkLongClickable;
    case ClickType::SUPER_LONG:
        return this->configFlags.isNetworkSuperLongClickable;
    default:
        return false;
    }
}

/**
 * @brief Get the fallback type for a network click type.
 *
 * @param clickType the click type.
 * @return constants::NoNetworkClickType the fallback type.
 */
auto Clickable::getNetworkFallback(constants::ClickType clickType) const -> constants::NoNetworkClickType
{
    using constants::NoNetworkClickType;
    switch (clickType)
    {
    case ClickType::LONG:
        if (!this->configFlags.isNetworkLongClickable)
        {
            return NoNetworkClickType::NONE;
        }
        return this->configFlags.longNetworkFallbackDoNothing ? NoNetworkClickType::DO_NOTHING : NoNetworkClickType::LOCAL_FALLBACK;
    case ClickType::SUPER_LONG:
        if (!this->configFlags.isNetworkSuperLongClickable)
        {
            return NoNetworkClickType::NONE;
        }
        return this->configFlags.superLongNetworkFallbackDoNothing ? NoNetworkClickType::DO_NOTHING : NoNetworkClickType::LOCAL_FALLBACK;
    default:
        return NoNetworkClickType::NONE;
    }
}

/**
 * @brief Get the type of super long click.
 * @details The value belongs to the generated static profile, not to the
 *          Clickable runtime object.
 *
 * @return constants::SuperLongClickType the type of super long click.
 */
auto Clickable::getSuperLongClickType() const -> constants::SuperLongClickType
{
    return lsh::core::static_config::getSuperLongClickType(this->index);
}

/**
 * @brief Validates the clickable's configuration.
 *
 * A clickable is considered valid if it's enabled for at least one click type
 * (short, long, or super-long) AND has at least one real action behind it.
 * A "real action" can be either:
 * - one or more attached local actuators, or
 * - a network-click path for long or super-long clicks.
 *
 * This keeps the cached validity flag aligned with the real runtime behaviour,
 * where some installations intentionally use long/super-long clicks only over
 * the bridge without any local actuator attached to that same clickable.
 *
 * @return true if the clickable has a valid and actionable configuration, false otherwise.
 */
auto Clickable::check() -> bool
{
    this->refreshQuickClickability();

    const uint8_t clickableIndex = this->index;
    const bool hasLocalAction = (lsh::core::static_config::getShortClickActuatorLinkCount(clickableIndex) != 0U ||
                                 lsh::core::static_config::getLongClickActuatorLinkCount(clickableIndex) != 0U ||
                                 lsh::core::static_config::getSuperLongClickActuatorLinkCount(clickableIndex) != 0U);
    const bool hasNetworkAction = ((this->configFlags.isLongClickable && this->configFlags.isNetworkLongClickable) ||
                                   (this->configFlags.isSuperLongClickable && this->configFlags.isNetworkSuperLongClickable));

    return ((this->configFlags.isShortClickable || this->configFlags.isLongClickable || this->configFlags.isSuperLongClickable) &&
            (hasLocalAction || hasNetworkAction));
}

/**
 * @brief Perform a short click action.
 *
 * It toggles the state of the actuator, if it was ON it turns it OFF and vice versa.
 *
 * @return true if any actuator changed state.
 * @return false otherwise.
 */
auto Clickable::shortClick() const -> bool
{
    if (!this->configFlags.isShortClickable)
    {
        return false;
    }

    return lsh::core::static_config::runShortClick(this->index);
}

/**
 * @brief Perform a long click action.
 *
 * The action depends on the generated long-click type configuration.
 * If NORMAL -> Switch ON if more than half of attached long actuators are OFF, switch OFF otherwise.
 * If ON_ONLY -> Switch ON attached long actuators.
 * If OFF_ONLY -> Switch OFF attached long actuators.
 *
 * @return true if any actuator changed state.
 * @return false otherwise.
 */
auto Clickable::longClick() const -> bool
{
    if (!this->configFlags.isLongClickable)
    {
        return false;
    }

    return lsh::core::static_config::runLongClick(this->index);
}

/**
 * @brief Perform a super long click selective action.
 *
 * Turns off super long unprotected actuators.
 *
 * @return true if any actuator changed state.
 * @return false otherwise.
 */
auto Clickable::superLongClickSelective() const -> bool
{
    using constants::SuperLongClickType;
    if (!this->configFlags.isSuperLongClickable || this->getSuperLongClickType() != SuperLongClickType::SELECTIVE)
    {
        return false;
    }

    return lsh::core::static_config::runSuperLongClick(this->index);
}

/**
 * @brief Perform the clickable state detection.
 * @details The main loop computes the elapsed scan time once and passes it here
 *          for every clickable. The clickable then keeps only a 16-bit elapsed
 *          age for the current state instead of re-running 32-bit timestamp
 *          arithmetic on every scan. This keeps the AVR hot path smaller while
 *          preserving the same debounce and long-press semantics.
 *
 * @param elapsed_ms Milliseconds elapsed since the previous clickable scan.
 * @return constants::ClickResult the type of the click.
 */
auto Clickable::clickDetection(uint16_t elapsed_ms) -> constants::ClickResult
{
    using constants::ClickResult;

    // Read pin state once per call for consistency.
    const bool isPressed = this->getState();
    if (this->currentState != State::IDLE)
    {
        this->stateAge_ms = timeUtils::addElapsedTimeSaturated(this->stateAge_ms, elapsed_ms);
    }

    switch (this->currentState)
    {
    case State::IDLE:
        if (isPressed)
        {
            this->currentState = State::DEBOUNCING;
            this->stateAge_ms = 0U;
        }
        break;

    case State::DEBOUNCING:
        if (this->stateAge_ms >= constants::timings::CLICKABLE_DEBOUNCE_TIME_MS)
        {
            if (isPressed)
            {
                // Press confirmed. Transition to PRESSED state.
                this->currentState = State::PRESSED;
                this->stateAge_ms = 0U;  // This is the official start time of the press.
                this->lastActionFired = ActionFired::NONE;

                // If it's a "quick click" button, fire the action on press.
                const ClickableConfigFlags localFlags = this->configFlags;
                if (localFlags.isQuickClickable)
                {
                    return ClickResult::SHORT_CLICK_QUICK;
                }
            }
            else
            {
                // It was just a bounce/noise. Return to IDLE.
                this->currentState = State::IDLE;
            }
        }
        break;

    case State::PRESSED:
        if (isPressed)
        {
            const ClickableConfigFlags localFlags = this->configFlags;
            // Long must be emitted before super-long even if a slow scan jumps
            // directly beyond both thresholds. This lets one held button send
            // the long network click first and the super-long network click on
            // the following scan, preserving the intended action sequence.
            if (localFlags.isLongClickable && this->lastActionFired < ActionFired::LONG && this->stateAge_ms >= this->getLongClickTime())
            {
                this->lastActionFired = ActionFired::LONG;
                return ClickResult::LONG_CLICK;
            }

            if (localFlags.isSuperLongClickable && this->lastActionFired < ActionFired::SUPER_LONG &&
                this->stateAge_ms >= this->getSuperLongClickTime())
            {
                this->lastActionFired = ActionFired::SUPER_LONG;
                return ClickResult::SUPER_LONG_CLICK;
            }

            return ClickResult::NO_CLICK_KEEPING_CLICKED;
        }

        // If we are here, it means the button was released.
        // Set the next state and then fall through to process the release immediately.
        this->currentState = State::RELEASED;
        [[fallthrough]];

    case State::RELEASED:
    {
        const ClickableConfigFlags localFlags = this->configFlags;
        // This state is entered immediately after a release.
        // The FSM is now reset for the next cycle.
        this->currentState = State::IDLE;
        this->stateAge_ms = 0U;

        // Ignore the release if a quick click action was already fired on press.
        if (localFlags.isQuickClickable)
        {
            return ClickResult::NO_CLICK;
        }

        // If no timed action was fired during the press, it's a short click.
        if (this->lastActionFired == ActionFired::NONE)
        {
            return localFlags.isShortClickable ? ClickResult::SHORT_CLICK : ClickResult::NO_CLICK_NOT_SHORT_CLICKABLE;
        }

        // A timed action was already fired, so the release event itself doesn't trigger a new action.
        return ClickResult::NO_CLICK;
    }
    }
    return ClickResult::NO_CLICK;
}

namespace Clickables
{
/**
 * @brief Compatibility hook after user configuration.
 * @details Static profiles generate all clickable links at compile time, so
 *          there is no runtime pool to finalize.
 */
void finalizeActuatorLinkStorage()
{}
}  // namespace Clickables
