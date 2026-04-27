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
    // Keep the public single-actuator path lazy: if the state is already right,
    // do not pay the 32-bit cached-time read just to reject a no-op.
    if (!this->wouldChangeState(state))
    {
        return false;
    }
#if LSH_CORE_ACTUATOR_NEEDS_SWITCH_TIMESTAMP
    return this->applyStateChange(state, timeKeeper::getTime(), this->runtimeIndex());
#else
    return this->applyStateChange(state, 0U, this->runtimeIndex());
#endif
}

/**
 * @brief Set the new actuator state if the new state can be set.
 *
 * @param state new state to set.
 * @param now_ms caller-cached timestamp used when debounce or auto-off storage needs it.
 * @return true if the state has been applied.
 * @return false otherwise.
 */
auto Actuator::setState(bool state, uint32_t now_ms) -> bool
{
    // The generated multi-actuator path may already have paid for `now_ms`.
    // Still reject no-op writes before debounce and pin work.
    if (!this->wouldChangeState(state))
    {
        return false;
    }
    return this->applyStateChange(state, now_ms, this->runtimeIndex());
}

/**
 * @brief Set the new actuator state with a generated dense runtime index.
 *
 * @param actuatorIndex Dense static-profile actuator index.
 * @param state new state to set.
 * @return true if the state has been applied.
 * @return false otherwise.
 */
auto Actuator::setStateForIndex(uint8_t actuatorIndex, bool state) -> bool
{
    if (!this->wouldChangeState(state))
    {
        return false;
    }
#if LSH_CORE_ACTUATOR_NEEDS_SWITCH_TIMESTAMP
    return this->applyStateChange(state, timeKeeper::getTime(), actuatorIndex);
#else
    return this->applyStateChange(state, 0U, actuatorIndex);
#endif
}

/**
 * @brief Set the new actuator state with generated index and cached timestamp.
 *
 * @param actuatorIndex Dense static-profile actuator index.
 * @param state new state to set.
 * @param now_ms caller-cached timestamp used when debounce or auto-off storage needs it.
 * @return true if the state has been applied.
 * @return false otherwise.
 */
auto Actuator::setStateForIndex(uint8_t actuatorIndex, bool state, uint32_t now_ms) -> bool
{
    if (!this->wouldChangeState(state))
    {
        return false;
    }
    return this->applyStateChange(state, now_ms, actuatorIndex);
}

/**
 * @brief Apply a known state transition after the caller rejected no-op writes.
 *
 * @param state new state to set.
 * @param now_ms caller-cached timestamp used when debounce or auto-off storage needs it.
 * @return true if the state has been applied.
 * @return false otherwise.
 */
auto Actuator::applyStateChange(bool state, uint32_t now_ms, uint8_t actuatorIndex) -> bool
{
    if (!this->debounceAllowsSwitch(now_ms))
    {
        return false;
    }
    this->writePinState(state);
    this->updateCachedStateFlag(state);
#if LSH_CORE_ACTUATOR_NEEDS_LOCAL_SWITCH_TIME
    this->lastTimeSwitched = now_ms;
#endif
    // Keep the global packed shadow in sync so state serialization never has
    // to walk the actuator array just to rebuild protocol bytes. Static
    // profiles register every actuator before the runtime can switch it.
    Actuators::updatePackedState(actuatorIndex, state);
#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES && LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS > 0
    Actuators::recordSwitchTime(actuatorIndex, now_ms);
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
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
    this->index = indexToSet;
#else
    static_cast<void>(indexToSet);
#endif
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
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
    return this->index;
#else
    return UINT8_MAX;
#endif
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
 * @brief Switch the state of the actuator using a caller-cached timestamp.
 *
 * @param now_ms cached timestamp shared by a generated action body.
 * @return true if the state has been changed.
 * @return false otherwise.
 */
auto Actuator::toggleState(uint32_t now_ms) -> bool
{
    return this->setState((this->flags & ACTUATOR_FLAG_ACTUAL_STATE) == 0U, now_ms);
}

/**
 * @brief Toggle the actuator with a generated dense runtime index.
 *
 * @param actuatorIndex Dense static-profile actuator index.
 * @return true if the state has been changed.
 * @return false otherwise.
 */
auto Actuator::toggleStateForIndex(uint8_t actuatorIndex) -> bool
{
    return this->setStateForIndex(actuatorIndex, (this->flags & ACTUATOR_FLAG_ACTUAL_STATE) == 0U);
}

/**
 * @brief Toggle the actuator with generated index and caller-cached timestamp.
 *
 * @param actuatorIndex Dense static-profile actuator index.
 * @param now_ms cached timestamp shared by a generated action body.
 * @return true if the state has been changed.
 * @return false otherwise.
 */
auto Actuator::toggleStateForIndex(uint8_t actuatorIndex, uint32_t now_ms) -> bool
{
    return this->setStateForIndex(actuatorIndex, (this->flags & ACTUATOR_FLAG_ACTUAL_STATE) == 0U, now_ms);
}

/**
 * @brief Checks the provided auto-off timer, switch OFF the actuator if it's over.
 *
 * @param now_ms caller-cached current time in milliseconds.
 * @param autoOffTimer_ms auto-off timer to test.
 * @return true if the state has been changed.
 * @return false otherwise.
 */
auto Actuator::checkAutoOffTimer(uint32_t now_ms, uint32_t autoOffTimer_ms) -> bool
{
    return this->checkAutoOffTimerForIndex(this->runtimeIndex(), now_ms, autoOffTimer_ms);
}

/**
 * @brief Checks an auto-off timer with the generated dense actuator index.
 *
 * @param actuatorIndex Dense static-profile actuator index.
 * @param now_ms caller-cached current time in milliseconds.
 * @param autoOffTimer_ms auto-off timer to test.
 * @return true if the state has been changed.
 * @return false otherwise.
 */
auto Actuator::checkAutoOffTimerForIndex(uint8_t actuatorIndex, uint32_t now_ms, uint32_t autoOffTimer_ms) -> bool
{
#if !CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES && LSH_CORE_ACTUATOR_NEEDS_LOCAL_SWITCH_TIME
    if ((this->flags & ACTUATOR_FLAG_ACTUAL_STATE) != 0U && autoOffTimer_ms != 0U)
    {
        if (now_ms - this->lastTimeSwitched >= autoOffTimer_ms)
        {
            return this->setStateForIndex(actuatorIndex, false, now_ms);
        }
    }
    return false;
#else
    static_cast<void>(actuatorIndex);
    static_cast<void>(now_ms);
    static_cast<void>(autoOffTimer_ms);
    return false;
#endif
}
