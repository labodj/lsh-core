/**
 * @file    actuator.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic of the Actuator class.
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

#include "peripherals/output/actuator.hpp"

#include "util/constants/timing.hpp"
#include "util/timekeeper.hpp"

/**
 * @brief Set the new actuator state if the new state can be set.
 *
 * @param state new state to set.
 * @return true if the state has been applied.
 * @return false otherwise.
 */
auto Actuator::setState(bool state) -> bool
{
    using constants::timings::ACTUATOR_DEBOUNCE_TIME_MS;
    // Apply new state only if it's different from the stored one
    if (this->actualState == state)
    {
        return false;
    }

    // Apply state if debounce is not active (elided at compile time) OR if it's active check if debounce time is elapsed
    if (ACTUATOR_DEBOUNCE_TIME_MS != 0U && (timeKeeper::getTime() - this->lastTimeSwitched < ACTUATOR_DEBOUNCE_TIME_MS))
    {
        return false;
    }
#ifdef CONFIG_USE_FAST_ACTUATORS
    if (!state)
    {
        *this->pinPort &= ~this->pinMask;
    }
    else
    {
        *this->pinPort |= this->pinMask;
    }
#else
    digitalWrite(this->pinNumber, static_cast<uint8_t>(state)); // Perform the switch
#endif
    this->actualState = state; // Store the new state
    this->lastTimeSwitched = timeKeeper::getTime();
    return true;
}

/**
 * @brief store the actuator index in Actuators namespace array.
 *
 * @param indexToSet index to set.
 */
void Actuator::setIndex(uint8_t indexToSet)
{
    this->index = indexToSet;
}

/**
 * @brief Set turn off timer, it represents the timer after that the actuator will be switched off.
 *
 * @param time_ms timer time in ms, if set to 0 disable timer.
 * @return Actuator& the object instance.
 */
auto Actuator::setAutoOffTimer(uint32_t time_ms) -> Actuator &
{
    this->hasAutoOffTimer = time_ms != 0U;
    this->autoOffTimer_ms = time_ms;
    return *this;
}

/**
 * @brief Set protection against some turn ON/OFF behaviour.
 *
 * @param hasProtection to set the property.
 * @return Actuator& the object instance.
 */
auto Actuator::setProtected(bool hasProtection) -> Actuator &
{
    this->isProtected = hasProtection;
    return *this;
}

/**
 * @brief Get the actuator index on Actuators namespace array.
 *
 * @return uint8_t actuator index.
 */
auto Actuator::getIndex() const -> uint8_t
{
    return this->index;
}

/**
 * @brief Get the unique ID of the actuator.
 *
 * @return uint8_t unique ID of the actuator.
 */
auto Actuator::getId() const -> uint8_t
{
    return this->id;
}

/**
 * @brief Get the state of the actuator.
 *
 * @return true if ON.
 * @return false if OFF.
 */
auto Actuator::getState() const -> bool
{
    return this->actualState;
}

/**
 * @brief Get the default state of the actuator.
 *
 * @return true if ON.
 * @return false if OFF.
 */
auto Actuator::getDefaultState() const -> bool
{
    return this->defaultState;
}

/**
 * @brief Get if the actuator has a timer set.
 *
 * @return true if has a timer.
 * @return false if it hasn't a timer.
 */
auto Actuator::hasAutoOff() const -> bool
{
    return this->hasAutoOffTimer;
}

/**
 * @brief Get the timer of the actuator in ms.
 *
 * @return uint32_t the timer of the actuator in ms.
 */
auto Actuator::getAutoOffTimer() const -> uint32_t
{
    return this->autoOffTimer_ms;
}

/**
 * @brief Get if the actuators is protected against some turn ON/OFF behaviour.
 *
 * @return true if it's protected.
 * @return false if it's not protected.
 */
auto Actuator::hasProtection() const -> bool
{
    return this->isProtected;
}

/**
 * @brief Switch the state of the actuator (if it was OFF is going to be ON and vice versa).
 *
 * @return true if the state has been changed.
 * @return false otherwise.
 */
auto Actuator::toggleState() -> bool
{
    return setState(!this->actualState);
}

/**
 * @brief Checks if auto off timer is over, switch OFF the actuator if it's over.
 *
 * @return true if the state has been changed.
 * @return false otherwise.
 */
auto Actuator::checkAutoOffTimer() -> bool
{
    if (this->actualState && this->hasAutoOff())
    {
        if (timeKeeper::getTime() - this->lastTimeSwitched >= this->getAutoOffTimer())
        {
            return this->setState(false);
        }
    }
    return false;
}
