/**
 * @file    clicktypes.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Defines enums for various click types and fallback behaviors.
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

#ifndef LSHCORE_UTIL_CONSTANTS_CLICK_TYPES_HPP
#define LSHCORE_UTIL_CONSTANTS_CLICK_TYPES_HPP

#include <stdint.h>

/**
 * @brief namespace for constants.
 */
namespace constants
{
    /**
     * @brief Clickable (like button) click types.
     *
     */
    enum class ClickType : uint8_t
    {
        NONE,      //!< Never use this, it's just a default parameter
        SHORT,     //!< Short click
        LONG,      //!< Long click
        SUPER_LONG //!< Super long click
    };

    /**
     * @brief Long click behaviors.
     *
     */
    enum class LongClickType : uint8_t
    {
        NONE,    //!< Never use this, it's just a default parameter
        NORMAL,  //!< Turns on or off secondary attached actuators based on how many are on or off
        ON_ONLY, //!< Turns on secondary attached actuators
        OFF_ONLY //!< Turns off secondary attached actuators
    };

    /**
     * @brief Super long click behaviors.
     *
     */
    enum class SuperLongClickType : uint8_t
    {
        NONE,     //!< Never use this, it's just a default parameter
        NORMAL,   //!< Turns off all controllino unprotected actuators
        SELECTIVE //!< Turns off a list of selected unprotected actuators
    };

    /**
     * @brief Fallback for a network click that expired or receives a failover.
     *
     */
    enum class NoNetworkClickType : uint8_t
    {
        NONE,           //!< Never use this, it's just a default parameter
        LOCAL_FALLBACK, //!< Apply local logic
        DO_NOTHING      //!< Do nothing
    };
} // namespace constants

#endif // LSHCORE_UTIL_CONSTANTS_CLICK_TYPES_HPP
