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
#include "device/indicator_manager.hpp"

/**
 * @brief Set the indicator index on Indicators namespace Array.
 *
 * @param indexToSet index to set.
 */
void Indicator::setIndex(uint8_t indexToSet)
{
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
    this->index = indexToSet;
#else
    // Generated release refresh code calls applyComputedState() directly with
    // compile-time topology, so indicators do not need an SRAM index byte.
    static_cast<void>(indexToSet);
#endif
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
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
    const bool newState = lsh::core::static_config::computeIndicatorState(this->index);
    this->applyComputedState(newState);
#endif
}

/**
 * @brief Get the indicator index on Indicators namespace Array.
 *
 * @return uint8_t indicator index.
 */
auto Indicator::getIndex() const -> uint8_t
{
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
    return this->index;
#else
    return UINT8_MAX;
#endif
}
