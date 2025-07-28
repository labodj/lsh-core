/**
 * @file    indicator.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic of the Indicator class.
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

#include "peripherals/output/indicator.hpp"

#include "device/actuator_manager.hpp"
#include "peripherals/output/actuator.hpp"
/**
 * @brief Set the state of the indicator.
 *
 * @param stateToSet the state to set true=ON, false=OFF.
 */
void inline Indicator::setState(bool stateToSet)
{
#ifdef CONFIG_USE_FAST_INDICATORS
    if (!stateToSet)
    {
        *this->pinPort &= ~this->pinMask;
    }
    else
    {
        *this->pinPort |= this->pinMask;
    }
#else
    digitalWrite(this->pinNumber, static_cast<uint8_t>(stateToSet));
#endif
}

/**
 * @brief Set the indicator index on Indicators namespace Array.
 *
 * @param indexToSet index to set.
 */
void Indicator::setIndex(uint8_t indexToSet)
{
    this->index = indexToSet;
}

/**
 * @brief Add one actuator to controlled controlledActuators vector.
 *
 * @param actuatorIndex index of actuator to be added.
 * @return Indicator& the object instance.
 */
auto Indicator::addActuator(uint8_t actuatorIndex) -> Indicator &
{
    this->controlledActuators.push_back(actuatorIndex);
    return *this;
}

/**
 * @brief Set the mode of the indicator.
 *
 * @param indicatorMode the mode to set.
 * @return Indicator& the object instance.
 */
auto Indicator::setMode(constants::IndicatorMode indicatorMode) -> Indicator &
{
    using constants::IndicatorMode;
    switch (indicatorMode)
    {
    case IndicatorMode::ANY:
    case IndicatorMode::ALL:
    case IndicatorMode::MAJORITY:
        this->mode = indicatorMode;
        break;
    default:
        break; // Not valid mode
    }
    return *this;
}

/**
 * @brief Switch the indicator based on controlled actuators status.
 *
 * The behavior depends on mode setting:
 *
 * If this->mode = ANY -> If any controlled actuator is ON switch ON the indicator, OFF otherwise.
 * If this->mode = ALL -> If all controlled actuators are ON switch ON the indicator, OFF otherwise.
 * If this->mode = MAJORITY -> If the majority of controlled actuators are ON switch ON the indicator, OFF otherwise.
 *
 */
void Indicator::check()
{
    using constants::IndicatorMode;

    // Cache the pointer to the global actuators array
    auto *const actuators = Actuators::actuators.data();

    // Evaluate new state
    bool newState;
    switch (this->mode)
    {
    case IndicatorMode::ANY:
    {
        newState = false;
        for (const auto i : this->controlledActuators)
        {
            newState |= actuators[i]->getState();
            if (newState)
            {
                break;
            }
        }
    }
    break;
    case IndicatorMode::ALL:
    {
        newState = true;
        for (const auto i : this->controlledActuators)
        {
            newState &= actuators[i]->getState();
            if (!newState)
            {
                break;
            }
        }
    }
    break;
    case IndicatorMode::MAJORITY:
    {
        uint8_t totalControlledActuatorsOn = 0U;
        for (const auto i : this->controlledActuators)
        {
            totalControlledActuatorsOn += static_cast<uint8_t>(actuators[i]->getState());
        }

        /*
         * The formula is (Total Actuators ON > Controlled actuators / 2)
         * To avoid float arithmetics and to speed things up use shift operator and swap the division with a multiplication
         * (Total actuators On x 2 > controlled actuators)
         */
        newState = (totalControlledActuatorsOn << 1U) > static_cast<uint8_t>(this->controlledActuators.size());
    }
    break;
    default:
        return; // Wrong IndicatorMode config
    }

    // If the state is the same as old one exit
    if (newState == this->actualState)
    {
        return; // Same state, do nothing
    }
    this->actualState = newState; // Store new state
    this->setState(newState);     // Apply new state
}

/**
 * @brief Resize controlled actuators vector to its actual size.
 *
 */
void Indicator::resizeVectors()
{
    this->controlledActuators.resize(this->controlledActuators.size());
}

/**
 * @brief Get the indicator index on Indicators namespace Array.
 *
 * @return uint8_t indicator index.
 */
auto Indicator::getIndex() const -> uint8_t
{
    return this->index;
}