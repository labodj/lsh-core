/**
 * @file    wrong_config_strings.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Defines PROGMEM strings for user configuration error messages.
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

#ifndef LSHCORE_UTIL_CONSTANTS_WRONG_CONFIG_STRINGS_HPP
#define LSHCORE_UTIL_CONSTANTS_WRONG_CONFIG_STRINGS_HPP

#include <avr/pgmspace.h>

/**
 * @brief namespace for constants.
 */
namespace constants
{
    /**
     * @brief namespace for wrong config static EEPROM strings.
     *
     */
    namespace wrongConfigStrings
    {
        constexpr const char SPACE[] PROGMEM = " ";               // NOLINT
        constexpr const char WRONG[] PROGMEM = "Wrong";           // NOLINT
        constexpr const char ACTUATORS[] PROGMEM = "actuators";   // NOLINT
        constexpr const char CLICKABLES[] PROGMEM = "clickables"; // NOLINT
        constexpr const char INDICATORS[] PROGMEM = "indicators"; // NOLINT
        constexpr const char NUMBER[] PROGMEM = "number";         // NOLINT
        constexpr const char DUPLICATE[] PROGMEM = "Duplicate";   // NOLINT
        constexpr const char ID[] PROGMEM = "ID";                 // NOLINT
    } // namespace wrongConfigStrings

} // namespace constants

#endif // LSHCORE_UTIL_CONSTANTS_WRONG_CONFIG_STRINGS_HPP
