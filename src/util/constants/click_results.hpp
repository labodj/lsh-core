/**
 * @file    click_results.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Defines the ClickResult enum for the clickable state machine.
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

#ifndef LSH_CORE_UTIL_CONSTANTS_CLICK_RESULTS_HPP
#define LSH_CORE_UTIL_CONSTANTS_CLICK_RESULTS_HPP

#include <stdint.h>

/**
 * @brief Namespace that groups firmware constants.
 */
namespace constants
{
/**
 * @brief Result of clickable click detection via clickDetection().
 *
 */
enum class ClickResult : uint8_t
{
    NO_CLICK,                     //!< No click detected.
    SHORT_CLICK,                  //!< Short click detected.
    SHORT_CLICK_QUICK,            //!< Short click detected on press for quick-only buttons.
    LONG_CLICK,                   //!< Long click detected.
    SUPER_LONG_CLICK,             //!< Super long click detected.
    NO_CLICK_KEEPING_CLICKED,     //!< The clickable is kept pressed after a timed action.
    NO_CLICK_NOT_SHORT_CLICKABLE  //!< A short press was detected on a non-short-clickable input.
};
}  // namespace constants

#endif  // LSH_CORE_UTIL_CONSTANTS_CLICK_RESULTS_HPP
