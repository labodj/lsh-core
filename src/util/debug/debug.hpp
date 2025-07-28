/**
 * @file    debug.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares debugging macros (DP, DPL) and helper functions.
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

#ifndef LSHCORE_UTIL_DEBUG_DEBUG_HPP
#define LSHCORE_UTIL_DEBUG_DEBUG_HPP

#include "communication/constants/config.hpp"
#include "internal/user_config_bridge.hpp"
#include "util/constants/debug.hpp"

#ifdef LSH_DEBUG
#include <ArduinoJson.h>
#include "util/debug/memory.hpp"
#include "util/debug/vaprint.hpp"
#endif // LSH_DEBUG

namespace Debug
{
    /**
     * @brief Helper to cast a PROGMEM string to const __FlashStringHelper *, needed to print PROGMEM strings.
     */
    inline auto FPSTR(const char *const progmemString) -> const __FlashStringHelper *
    {
        return reinterpret_cast<const __FlashStringHelper *>(progmemString);
    }

#ifdef LSH_DEBUG // If in debug mode

    /**
     * @brief Debug: Print
     *
     */
#define DP VaPrint::print

    /**
     * @brief Debug: Print Line
     *
     */
#define DPL VaPrint::println

    /**
     * @brief Debug: Serial Begin
     *
     */
    __attribute__((always_inline)) inline void DSB() { CONFIG_DEBUG_SERIAL->begin(constants::debugConfigs::DEBUG_SERIAL_BAUD); }

    /**
     * @brief Non-Debug Serial Begin, not needed when in debug mode
     *
     */
#define NDSB()

    /**
     * @brief Debug: Print Json
     *
     */
    template <typename T>
    __attribute__((always_inline)) constexpr inline void DPJ(const T &json)
    {
        serializeJson(json, Serial);
        DPL();
    }

    /**
     * @brief Debug: Print Free Memory
     *
     */
    inline void DFM()
    {
        DPL(FPSTR(dStr::FREE_MEMORY), FPSTR(dStr::COLON_SPACE), freeMemory());
    }

    /**
     * @brief Debug: Print Method/Function Context (auto-detects name).
     * @details Uses the compiler's __PRETTY_FUNCTION__ intrinsic to automatically
     *          print the full signature of the calling function.
     */
#define DP_CONTEXT() DPL(__PRETTY_FUNCTION__)

#else // Not in debug

    namespace details
    {
        extern bool serialIsActive;
    } // namespace details

#define DP(...)
#define DPL(...)
#define DSB()
#define DPJ(x)
#define DP_CONTEXT()
#define DFM()
    // Non-Debug Serial Begin, turn on Serial when not in debug mode
    void NDSB();
#endif // LSH_DEBUG

} // namespace Debug

#endif // LSHCORE_UTIL_DEBUG_DEBUG_HPP
