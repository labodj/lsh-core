/**
 * @file    indicator_manager.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares the manager for the global collection of Indicator objects.
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

#ifndef LSHCORE_DEVICE_INDICATOR_MANAGER_HPP
#define LSHCORE_DEVICE_INDICATOR_MANAGER_HPP

#include <stdint.h>

#include <etl/array.h>

#include "internal/user_config_bridge.hpp"

class Indicator; //!< Forward declaration

/**
 * @brief Globally stores all indicators and to operates over them.
 *
 */
namespace Indicators
{
    extern uint8_t totalIndicators;                                   //!< Device real total Indicators
    extern etl::array<Indicator *, CONFIG_MAX_INDICATORS> indicators; //!< Device indicators

    void addIndicator(Indicator *indicator); // Add one indicator to indicators vector and activate it
    void indicatorsCheck();                  // Performs an  indicator check for every indicator set
    void finalizeSetup();                    // Resize vectors of all indicators to the actual needed size
} // namespace Indicators

#endif // LSHCORE_DEVICE_INDICATOR_MANAGER_HPP
