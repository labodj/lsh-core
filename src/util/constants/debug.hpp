/**
 * @file    debug.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Defines constants, strings, and configuration parameters used for debugging.
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

#ifndef LSHCORE_UTIL_CONSTANTS_DEBUG_HPP
#define LSHCORE_UTIL_CONSTANTS_DEBUG_HPP

#include <stdint.h>

/**
 * @brief namespace for constants
 */
namespace constants
{
    /**
     * @brief namespace for debug configurable parameters.
     *
     */
    namespace debugConfigs
    {
        /**
         * @brief Debug serial baud rate.
         *
         */
#ifndef CONFIG_DEBUG_SERIAL_BAUD
        static constexpr const uint32_t DEBUG_SERIAL_BAUD = 115200U;
#else
        static constexpr const uint32_t DEBUG_SERIAL_BAUD = CONFIG_DEBUG_SERIAL_BAUD;
#endif
    } // namespace debugConfigs

} // namespace constants

#ifdef LSH_DEBUG
#include <avr/pgmspace.h>
/**
 * @brief Static EEPROM debug strings.
 *
 */
namespace dStr
{
    constexpr const char SPACE[] PROGMEM = " ";        // NOLINT
    constexpr const char COLON_SPACE[] PROGMEM = ": "; // NOLINT
    constexpr const char POINT[] PROGMEM = ".";        // NOLINT
    constexpr const char DIVIDER[] PROGMEM = "||";     // NOLINT

    constexpr const char FUNCTION[] PROGMEM = "Function";                                            // NOLINT
    constexpr const char METHOD[] PROGMEM = "Method";                                                // NOLINT
    constexpr const char CALLED[] PROGMEM = "called";                                                // NOLINT
    constexpr const char FREE_MEMORY[] PROGMEM = "Free memory";                                      // NOLINT
    constexpr const char COMPILED_BY_GCC[] PROGMEM = "Compiled by GCC";                              // NOLINT
    constexpr const char EXEC_TIME[] PROGMEM = "Exec time";                                          // NOLINT
    constexpr const char IS_CONNECTED[] PROGMEM = "Is connected";                                    // NOLINT
    constexpr const char CLICKABLE[] PROGMEM = "Clickable";                                          // NOLINT
    constexpr const char SHORT[] PROGMEM = "short";                                                  // NOLINT
    constexpr const char LONG[] PROGMEM = "long";                                                    // NOLINT
    constexpr const char SUPER_LONG[] PROGMEM = "super long";                                        // NOLINT
    constexpr const char CLICKED[] PROGMEM = "clicked";                                              // NOLINT
    constexpr const char ESP_EXIT_CODE[] PROGMEM = "ESP exit code";                                  // NOLINT
    constexpr const char MESSAGE_RECEIVED_AT_TIME[] PROGMEM = "Message received at time";            // NOLINT
    constexpr const char ACTUATOR[] PROGMEM = "Actuator";                                            // NOLINT
    constexpr const char DOES_NOT_EXIST[] PROGMEM = "does not exist";                                // NOLINT
    constexpr const char JSON_DOC_IS_NULL[] PROGMEM = "JsonDoc is NULL";                             // NOLINT
    constexpr const char JSON_RECEIVED[] PROGMEM = "JSON received";                                  // NOLINT
    constexpr const char JSON_SENT[] PROGMEM = "JSON sent";                                          // NOLINT
    constexpr const char SENDING_PING_TO_ESP[] PROGMEM = "Sending ping to ESP";                      // NOLINT
    constexpr const char SENDING_BOOT_TO_ESP[] PROGMEM = "Sending boot to ESP";                      // NOLINT
    constexpr const char UUID[] PROGMEM = "UUID";                                                    // NOLINT
    constexpr const char INDEX[] PROGMEM = "Index";                                                  // NOLINT
    constexpr const char EXPIRED[] PROGMEM = "Expired";                                              // NOLINT
    constexpr const char NET_BUTTONS_NOT_EMPTY[] PROGMEM = "Network buttons not empty, checking..."; // NOLINT
    constexpr const char CLICK[] PROGMEM = "Click";                                                  // NOLINT
    constexpr const char TYPE[] PROGMEM = "type";                                                    // NOLINT
    constexpr const char FOR[] PROGMEM = "for";                                                      // NOLINT
    constexpr const char ITERATIONS[] PROGMEM = "iterations";                                        // NOLINT
} // namespace dStr

#endif // LSH_DEBUG

#endif // LSHCORE_UTIL_CONSTANTS_DEBUG_HPP
