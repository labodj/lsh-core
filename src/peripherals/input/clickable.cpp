/**
 * @file    clickable.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic of the Clickable class, including its state machine.
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

#include "peripherals/input/clickable.hpp"

#include "device/actuator_manager.hpp"
#include "peripherals/output/actuator.hpp"
#include "util/timekeeper.hpp"

using constants::ClickType;

/**
 * @brief Get the state of the clickable if configured as INPUT with its external pulldown resistor (PIN -> BUTTON -> +12v/+5V).
 *
 * @return true if clicked.
 * @return false if not clicked.
 */
auto inline Clickable::getState() -> bool
{
#ifdef CONFIG_USE_FAST_CLICKABLES
    return (*this->pinPort & this->pinMask) != 0U;
#else
    return (static_cast<bool>(digitalRead(this->pinNumber)));
#endif
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

/**
 * @brief Set clickable short clickability.
 *
 * @param shortClickable if the clickable is short clickable.
 * @return Clickable& the object instance.
 */
auto Clickable::setClickableShort(bool shortClickable) -> Clickable &
{
    this->configFlags.isShortClickable = shortClickable;
    return *this;
}

/**
 * @brief Set clickable long clickability locally and over network.
 *
 * @param longClickable if the clickable is long clickable.
 * @param clickType to set the type of long click.
 * @param networkClickable if the clickable is long clickable over network.
 * @param fallback the fallback type (if network isn't working).
 * @return Clickable& the object instance.
 */
auto Clickable::setClickableLong(bool longClickable, constants::LongClickType clickType, bool networkClickable, constants::NoNetworkClickType fallback) -> Clickable &
{
    this->configFlags.isLongClickable = longClickable;
    this->longClickType = clickType;
    this->configFlags.isNetworkLongClickable = networkClickable;
    this->longClickFallback = fallback;
    return *this;
}

/**
 * @brief Set clickable super long clickability locally and over network.
 *
 * @param superLongClickable if the clickable is super long clickable.
 * @param clickType to set the type of super long click.
 * @param networkClickable if the clickable is super long clickable over network.
 * @param fallback the fallback type (if network isn't working).
 * @return Clickable& the object instance.
 */
auto Clickable::setClickableSuperLong(bool superLongClickable, constants::SuperLongClickType clickType, bool networkClickable, constants::NoNetworkClickType fallback) -> Clickable &
{
    this->configFlags.isSuperLongClickable = superLongClickable;
    this->superLongClickType = clickType;
    this->configFlags.isNetworkSuperLongClickable = networkClickable;
    this->superLongClickFallback = fallback;
    return *this;
}

/**
 * @brief Add an actuator to a list of actuators controlled by the clickable.
 *
 * @param actuatorIndex the actuator index to be attached.
 * @param actuatorType the type of the actuator.
 * @return Clickable& the object instance.
 */
auto Clickable::addActuator(uint8_t actuatorIndex, constants::ClickType actuatorType) -> Clickable &
{
    switch (actuatorType)
    {
    case ClickType::SHORT:
        this->actuatorsShort.push_back(actuatorIndex);
        break;
    case ClickType::LONG:
        this->actuatorsLong.push_back(actuatorIndex);
        break;
    case ClickType::SUPER_LONG:
        this->actuatorsSuperLong.push_back(actuatorIndex);
        break;
    default:
        break; // Actuator Type mismatch
    }
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
 * @brief Set debounce time.
 *
 * @param timeToSet_ms debounce time in ms.
 * @return Clickable& the object instance.
 */
auto Clickable::setDebounceTime(uint8_t timeToSet_ms) -> Clickable &
{
    this->debounce_ms = timeToSet_ms;
    return *this;
}

/**
 * @brief Set long click time.
 *
 * @param timeToSet_ms long click time in ms.
 * @return Clickable& the object instance.
 */
auto Clickable::setLongClickTime(uint16_t timeToSet_ms) -> Clickable &
{
    this->longClick_ms = timeToSet_ms;
    return *this;
}

/**
 * @brief Set super long click time.
 *
 * @param timeToSet_ms super long click time in ms.
 * @return Clickable& the object instance.
 */
auto Clickable::setSuperLongClickTime(uint16_t timeToSet_ms) -> Clickable &
{
    this->superLongClick_ms = timeToSet_ms;
    return *this;
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
 * @brief Get the unique ID of the clickable.
 *
 * @return uint8_t unique ID of the clickable.
 */
auto Clickable::getId() const -> uint8_t
{
    return this->id;
}

/**
 * @brief Get the const vector of attached actuators.
 *
 * @param actuatorType the type of actuator vector to get.
 * @return etl::ivector<uint8_t>* the attached actuators const vector pointer.
 */
auto Clickable::getActuators(constants::ClickType actuatorType) const -> const etl::ivector<uint8_t> *
{
    switch (actuatorType)
    {
    case ClickType::SHORT:
        return &this->actuatorsShort;
    case ClickType::LONG:
        return &this->actuatorsLong;
    case ClickType::SUPER_LONG:
        return &this->actuatorsSuperLong;
    default:
        return nullptr; // Actuator Type mismatch
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
    switch (actuatorType)
    {
    case ClickType::SHORT:
        return static_cast<uint8_t>(this->actuatorsShort.size());
    case ClickType::LONG:
        return static_cast<uint8_t>(this->actuatorsLong.size());
    case ClickType::SUPER_LONG:
        return static_cast<uint8_t>(this->actuatorsSuperLong.size());
    default:
        return 0U; // Actuator Type mismatch
    }
}

/**
 * @brief Get the type of long click.
 *
 * @return constants::LongClickType the type of long click.
 */
auto Clickable::getLongClickType() const -> constants::LongClickType
{
    return this->longClickType;
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
        return this->longClickFallback;
    case ClickType::SUPER_LONG:
        return this->superLongClickFallback;
    default:
        return NoNetworkClickType::NONE;
    }
}

/**
 * @brief Get the type of super long click.
 *
 * @return constants::SuperLongClickType the type of super long click.
 */
auto Clickable::getSuperLongClickType() const -> constants::SuperLongClickType
{
    return this->superLongClickType;
}

/**
 * @brief Validates the clickable's configuration.
 *
 * A clickable is considered valid if it's enabled for at least one click type
 * (short, long, or super-long) AND is configured to control at least one actuator.
 * This check also sets internal optimization flags.
 *
 * @return true if the clickable has a valid and actionable configuration, false otherwise.
 */
auto Clickable::check() -> bool
{
    this->configFlags.isChecked = true; // We have checked the clickable
    this->configFlags.isQuickClickable = (this->configFlags.isShortClickable && !this->configFlags.isLongClickable && !this->configFlags.isSuperLongClickable);
    if (this->configFlags.isShortClickable || this->configFlags.isLongClickable || this->configFlags.isSuperLongClickable)
    {
        // This check ensures a clickable is linked to at least one local actuator.
        // This check might be removed in the future to support "virtual clickables"
        // that only trigger network actions without controlling any local hardware.
        if (!this->actuatorsShort.empty() || !this->actuatorsLong.empty() || !this->actuatorsSuperLong.empty())
        {
            this->configFlags.isValid = true;
            return true;
        }
    }
    this->configFlags.isValid = false;
    return false;
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

    bool anyActuatorChangedState = false;
    for (const auto actuatorIndex : this->actuatorsShort)
    {
        anyActuatorChangedState |= Actuators::actuators[actuatorIndex]->toggleState();
    }

    return anyActuatorChangedState;
}

/**
 * @brief Perform a long click action.
 *
 * The action depends on the longClickType configuration.
 * If NORMAL -> Switch ON if less than half of attached long actuators are OFF, switch OFF otherwise.
 * If ON_ONLY -> Switch ON attached long actuators.
 * If OFF_ONLY -> Switch OFF attached long actuators.
 *
 * @return true if any actuator changed state.
 * @return false otherwise.
 */
auto Clickable::longClick() const -> bool
{
    using constants::LongClickType;

    if (!this->configFlags.isLongClickable)
    {
        return false;
    }

    bool stateToSet = false;      // The state to be set to long actuators
    uint8_t actuatorsLongOn = 0U; // Number of active long actuators

    switch (this->longClickType)
    {
    case LongClickType::NORMAL:
        // Check long actuators are ON or OFF (actuatorsLongOn==0: everything OFF, actuatorsLongOn==totalLongActuators: everything ON)
        for (const auto actuatorIndex : this->actuatorsLong)
        {
            actuatorsLongOn += static_cast<uint8_t>(Actuators::actuators[actuatorIndex]->getState());
        }
        /*
        Less than half of attached secondary actuators are OFF -> stateToSet = true
        More or equal than half of attached long actuators are OFF -> stateToSet = false
        The formula is(Long actuators ON < Total actuators / 2)
        To avoid float arithmetics and to speed things up use shift operator and swap the division with a multiplication
        (Long actuators ON x 2 < Total actuators)
        */
        stateToSet = ((actuatorsLongOn << 1U) < this->getTotalActuators(ClickType::LONG));
        break;
    case LongClickType::ON_ONLY:
        stateToSet = true;
        break;
    case LongClickType::OFF_ONLY:
        stateToSet = false;
        break;
    default:
        return false; // Not a valid longClickType
    }

    bool anyActuatorChangedState = false;
    // Set stateToSet to all actuators in the long actuators list
    for (const auto actuatorIndex : this->actuatorsLong)
    {
        anyActuatorChangedState |= Actuators::actuators[actuatorIndex]->setState(stateToSet);
    }
    return anyActuatorChangedState;
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
    if (!this->configFlags.isSuperLongClickable || this->superLongClickType != SuperLongClickType::SELECTIVE)
    {
        return false;
    }

    bool anyActuatorChangedState = false;
    for (const auto actuatorIndex : this->actuatorsSuperLong)
    {
        if (!Actuators::actuators[actuatorIndex]->hasProtection())
        {
            anyActuatorChangedState |= Actuators::actuators[actuatorIndex]->setState(false);
        }
    }
    return anyActuatorChangedState;
}

/**
 * @brief Perform the clickable state detection.
 *
 * @return constants::ClickResult the type of the click.
 */
auto Clickable::clickDetection() -> constants::ClickResult
{
    using constants::ClickResult;

    // --- Optimization: Cache the config flags ---
    // Read the entire bitfield from RAM into a local variable once.
    // The compiler will very likely keep this in a CPU register for the fastest possible access.
    const ClickableConfigFlags localFlags = this->configFlags;

    // Read pin state once per call for consistency.
    const bool isPressed = this->getState();
    const uint32_t now = timeKeeper::getTime();

    switch (this->currentState)
    {
    case State::IDLE:
        if (isPressed)
        {
            this->currentState = State::DEBOUNCING;
            this->stateChangeTime = now;
        }
        break;

    case State::DEBOUNCING:
        if (now - this->stateChangeTime >= this->debounce_ms)
        {
            if (isPressed)
            {
                // Press confirmed. Transition to PRESSED state.
                this->currentState = State::PRESSED;
                this->stateChangeTime = now; // This is the official start time of the press.
                this->lastActionFired = ActionFired::NONE;

                // If it's a "quick click" button, fire the action on press.
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
            // Button is still being held. Check for timed actions.
            const uint32_t pressDuration = now - this->stateChangeTime;

            // Check for super long press first (higher priority).
            if (localFlags.isSuperLongClickable && this->lastActionFired < ActionFired::SUPER_LONG && pressDuration >= this->superLongClick_ms)
            {
                this->lastActionFired = ActionFired::SUPER_LONG;
                return ClickResult::SUPER_LONG_CLICK;
            }

            // Then check for long press.
            if (localFlags.isLongClickable && this->lastActionFired < ActionFired::LONG && pressDuration >= this->longClick_ms)
            {
                this->lastActionFired = ActionFired::LONG;
                return ClickResult::LONG_CLICK;
            }

            return ClickResult::NO_CLICK_KEEPING_CLICKED;
        }

        // If we are here, it means the button was released.
        // Set the next state and then fall through to process the release immediately.
        this->currentState = State::RELEASED;
        [[fallthrough]];

    case State::RELEASED:
    {
        // This state is entered immediately after a release.
        // The FSM is now reset for the next cycle.
        this->currentState = State::IDLE;

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

/**
 * @brief Resize vectors to actual needed size
 *
 */
void Clickable::resizeVectors()
{
    this->actuatorsShort.resize(this->actuatorsShort.size());
    this->actuatorsLong.resize(this->actuatorsLong.size());
    this->actuatorsSuperLong.resize(this->actuatorsSuperLong.size());
}