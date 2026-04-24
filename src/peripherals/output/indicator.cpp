/**
 * @file    indicator.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic of the Indicator class.
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

#include "peripherals/output/indicator.hpp"

#include "config/static_config.hpp"
#include "device/actuator_manager.hpp"
#include "device/indicator_manager.hpp"
#include "peripherals/output/actuator.hpp"

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
 * @brief Source-compatible actuator-link setter.
 * @details Static TOML profiles generate indicator links as compile-time
 *          accessors, so this method intentionally leaves no runtime state.
 *
 * @param actuatorIndex index of actuator to be added.
 * @return Indicator& the object instance.
 */
auto Indicator::addActuator(uint8_t actuatorIndex) -> Indicator &
{
    static_cast<void>(actuatorIndex);
    return *this;
}

/**
 * @brief Set the mode of the indicator.
 * @details Static TOML profiles generate the mode accessor at compile time, so
 *          this method is kept only for source compatibility.
 *
 * @param indicatorMode the mode to set.
 * @return Indicator& the object instance.
 */
auto Indicator::setMode(constants::IndicatorMode indicatorMode) -> Indicator &
{
    static_cast<void>(indicatorMode);
    return *this;
}

/**
 * @brief Switch the indicator based on controlled actuators status.
 *
 * The behavior depends on mode setting:
 *
 * If mode = ANY -> If any controlled actuator is ON switch ON the indicator, OFF otherwise.
 * If mode = ALL -> If all controlled actuators are ON switch ON the indicator, OFF otherwise.
 * If mode = MAJORITY -> If the majority of controlled actuators are ON switch ON the indicator, OFF otherwise.
 *
 * Indicators with zero controlled actuators are treated as OFF here as a final
 * safety net, even though setup validation should already reject that config.
 *
 */
void Indicator::check()
{
    const bool newState = lsh::core::static_config::computeIndicatorState(this->index);

    // If the state did not change, avoid touching the output pin.
    if (newState == this->actualState)
    {
        return;  // Same state, do nothing
    }
    this->actualState = newState;  // Store new state
    this->setState(newState);      // Apply new state
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

/**
 * @brief Return whether this indicator controls at least one actuator.
 *
 * @return true if the indicator has at least one attached actuator.
 * @return false otherwise.
 */
auto Indicator::hasAttachedActuators() const -> bool
{
    return lsh::core::static_config::getIndicatorActuatorLinkCount(this->index) != 0U;
}

namespace Indicators
{
/**
 * @brief Compatibility hook after user configuration.
 * @details Static profiles generate all indicator links at compile time, so
 *          there is no runtime pool to finalize.
 */
void finalizeActuatorLinkStorage()
{}
}  // namespace Indicators
