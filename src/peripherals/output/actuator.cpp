/**
 * @file    actuator.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic of the Actuator class.
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

#include "peripherals/output/actuator.hpp"

#include "device/actuator_manager.hpp"
#include "util/constants/timing.hpp"
#include "util/time_keeper.hpp"

#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES
static_assert(constants::timings::ACTUATOR_DEBOUNCE_TIME_MS == 0U,
              "Compact actuator switch-time storage requires CONFIG_ACTUATOR_DEBOUNCE_TIME_MS=0 to preserve exact debounce semantics.");
#endif

/**
 * @brief Set the new actuator state if the new state can be set.
 *
 * @param state new state to set.
 * @return true if the state has been applied.
 * @return false otherwise.
 */
auto Actuator::setState(bool state) -> bool
{
#if LSH_CORE_ACTUATOR_NEEDS_LOCAL_SWITCH_TIME
    using constants::timings::ACTUATOR_DEBOUNCE_TIME_MS;
#endif
    const uint8_t stateFlag = state ? ACTUATOR_FLAG_ACTUAL_STATE : 0U;
    // Apply new state only if it's different from the stored one
    if ((this->flags & ACTUATOR_FLAG_ACTUAL_STATE) == stateFlag)
    {
        return false;
    }

#if LSH_CORE_ACTUATOR_NEEDS_SWITCH_TIMESTAMP
    const uint32_t now = timeKeeper::getTime();
#endif

    // Apply state if debounce is not active (elided at compile time) OR if it's active check if debounce time is elapsed
#if LSH_CORE_ACTUATOR_NEEDS_LOCAL_SWITCH_TIME
    if (ACTUATOR_DEBOUNCE_TIME_MS != 0U && (now - this->lastTimeSwitched < ACTUATOR_DEBOUNCE_TIME_MS))
    {
        return false;
    }
#endif
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
    digitalWrite(this->pinNumber, static_cast<uint8_t>(state));  // Perform the switch
#endif
    if (state)
    {
        this->flags |= ACTUATOR_FLAG_ACTUAL_STATE;
    }
    else
    {
        this->flags &= static_cast<uint8_t>(~ACTUATOR_FLAG_ACTUAL_STATE);
    }
#if LSH_CORE_ACTUATOR_NEEDS_LOCAL_SWITCH_TIME
    this->lastTimeSwitched = now;
#endif
    // Keep the global packed shadow in sync so state serialization never has
    // to walk the actuator array just to rebuild protocol bytes. Static
    // profiles register every actuator before the runtime can switch it.
    Actuators::updatePackedState(this->index, state);
#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES && LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS > 0
    Actuators::recordSwitchTime(this->index, now);
#endif
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
 * @brief Validate the generated turn-off timer after actuator registration.
 *
 * @param time_ms timer time in ms; zero is valid only when the static profile has no timer.
 * @return Actuator& the object instance.
 */
auto Actuator::setAutoOffTimer(uint32_t time_ms) -> Actuator &
{
    Actuators::setAutoOffTimer(this->index, time_ms);
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
    if (hasProtection)
    {
        this->flags |= ACTUATOR_FLAG_PROTECTED;
    }
    else
    {
        this->flags &= static_cast<uint8_t>(~ACTUATOR_FLAG_PROTECTED);
    }
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
 * @brief Get the state of the actuator.
 *
 * @return true if ON.
 * @return false if OFF.
 */
auto Actuator::getState() const -> bool
{
    return (this->flags & ACTUATOR_FLAG_ACTUAL_STATE) != 0U;
}

/**
 * @brief Get the default state of the actuator.
 *
 * @return true if ON by default.
 * @return false if OFF by default.
 */
auto Actuator::getDefaultState() const -> bool
{
    return (this->flags & ACTUATOR_FLAG_DEFAULT_STATE) != 0U;
}

/**
 * @brief Get if the actuator has a timer set.
 *
 * @return true if has a timer.
 * @return false if it hasn't a timer.
 */
auto Actuator::hasAutoOff() const -> bool
{
    return Actuators::hasAutoOffTimer(this->index);
}

/**
 * @brief Get the timer of the actuator in ms.
 *
 * @return uint32_t the timer of the actuator in ms.
 */
auto Actuator::getAutoOffTimer() const -> uint32_t
{
    return Actuators::getAutoOffTimer(this->index);
}

/**
 * @brief Get if the actuators is protected against some turn ON/OFF behaviour.
 *
 * @return true if it's protected.
 * @return false if it's not protected.
 */
auto Actuator::hasProtection() const -> bool
{
    return (this->flags & ACTUATOR_FLAG_PROTECTED) != 0U;
}

/**
 * @brief Switch the state of the actuator (if it was OFF is going to be ON and vice versa).
 *
 * @return true if the state has been changed.
 * @return false otherwise.
 */
auto Actuator::toggleState() -> bool
{
    return this->setState((this->flags & ACTUATOR_FLAG_ACTUAL_STATE) == 0U);
}

/**
 * @brief Checks if auto off timer is over, switch OFF the actuator if it's over.
 *
 * @return true if the state has been changed.
 * @return false otherwise.
 */
auto Actuator::checkAutoOffTimer() -> bool
{
#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES
#if LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS > 0
    return Actuators::checkAutoOffTimer(this->index, this->getAutoOffTimer());
#else
    return false;
#endif
#else
    return this->checkAutoOffTimer(this->getAutoOffTimer());
#endif
}

/**
 * @brief Checks the provided auto-off timer, switch OFF the actuator if it's over.
 *
 * @param autoOffTimer_ms auto-off timer to test.
 * @return true if the state has been changed.
 * @return false otherwise.
 */
auto Actuator::checkAutoOffTimer(uint32_t autoOffTimer_ms) -> bool
{
#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES
#if LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS > 0
    return Actuators::checkAutoOffTimer(this->index, autoOffTimer_ms);
#else
    static_cast<void>(autoOffTimer_ms);
    return false;
#endif
#elif LSH_CORE_ACTUATOR_NEEDS_LOCAL_SWITCH_TIME
    if ((this->flags & ACTUATOR_FLAG_ACTUAL_STATE) != 0U && autoOffTimer_ms != 0U)
    {
        if (timeKeeper::getTime() - this->lastTimeSwitched >= autoOffTimer_ms)
        {
            return this->setState(false);
        }
    }
    return false;
#else
    static_cast<void>(autoOffTimer_ms);
    return false;
#endif
}
