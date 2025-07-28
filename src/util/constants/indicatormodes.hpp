/**
 * @file    indicatormodes.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Defines the IndicatorMode enum for indicator light behavior.
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

#ifndef LSHCORE_UTIL_CONSTANTS_INDICATOR_MODES_HPP
#define LSHCORE_UTIL_CONSTANTS_INDICATOR_MODES_HPP

#include <stdint.h>

/**
 * @brief namespace for constants.
 */
namespace constants
{
    /**
     * @brief Indicator mode for an indicator.
     *
     */
    enum class IndicatorMode : uint8_t
    {
        ANY,     //!< If any controlled actuators is ON turn on the indicator
        ALL,     //!< If all controlled actuators are ON turn on the indicator
        MAJORITY //!< If the majority of controlled actuators is ON turn ON the indicator
    };

} // namespace constants

#endif // LSHCORE_UTIL_CONSTANTS_INDICATOR_MODES_HPP