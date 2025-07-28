/**
 * @file    clickresults.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Defines the ClickResult enum for the clickable state machine.
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

#ifndef LSHCORE_UTIL_CONSTANTS_CLICK_RESULTS_HPP
#define LSHCORE_UTIL_CONSTANTS_CLICK_RESULTS_HPP

#include <stdint.h>

/**
 * @brief namespace for constants.
 */
namespace constants
{
    /**
     * @brief Result of clickable click detection via clickDetection().
     *
     */
    enum class ClickResult : uint8_t
    {
        NOT_VALID,                                 //!< Clickable is not valid
        NO_CLICK,                                  //!< No click detected
        SHORT_CLICK,                               //!< Short click detected
        SHORT_CLICK_QUICK,                         //!< Short click detected, if it's not long and super long clickable
        LONG_CLICK,                                //!< Long click detected
        DOUBLE_CLICK,                              // TODO(labo): WIP
        SUPER_LONG_CLICK,                          //!< Super long click detected
        NO_CLICK_DEBOUNCE_NOT_ELAPSED,             //!< Debounce time not elapsed
        NO_CLICK_TOO_EARLY_FOR_A_LONG_CLICK,       //!< The clickable is released too early to be a long click
        NO_CLICK_TOO_EARLY_FOR_A_SUPER_LONG_CLICK, //!< The clickable is released too early to be a super long click
        NO_CLICK_TOO_LATE_FOR_A_SHORT_CLICK,       //!< The clickable is released too late to be a short click
        NO_CLICK_KEEPING_CLICKED,                  //!< The clickable is kept clicked after super long press
        NO_CLICK_NOT_SHORT_CLICKABLE,              //!< Detected a short click but the clickable is not short clickable
        NO_CLICK_NOT_LONG_CLICKABLE,               //!< Detected a long click but the clickable is not long clickable
        NO_CLICK_NOT_SUPER_LONG_CLICKABLE,         //!< Detected a super long click but the clickable is not super long clickable
        NO_CLICK_NOT_LONG_OR_SUPER_LONG_CLICKABLE  //!< Detected a long or super long click but the clickable is not long or super long clickable
    };
} // namespace constants

#endif // LSHCORE_UTIL_CONSTANTS_CLICK_RESULTS_HPP
