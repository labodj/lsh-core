/**
 * @file    saturating_time.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares tiny helpers for elapsed-time arithmetic that must never wrap.
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

#ifndef LSH_CORE_UTIL_SATURATING_TIME_HPP
#define LSH_CORE_UTIL_SATURATING_TIME_HPP

#include <stdint.h>

namespace timeUtils
{
/**
 * @brief Add one elapsed-time delta to a 16-bit age without wrapping.
 *
 * The firmware uses 16-bit "age" counters in several hot paths. Saturating the
 * addition keeps those counters monotonic even after very long loop stalls,
 * debugger pauses or temporary scheduling hiccups, while staying cheaper than
 * carrying 32-bit timestamps through every object.
 *
 * @param currentAge_ms Age already accumulated before the current loop pass.
 * @param elapsed_ms Additional milliseconds measured since the previous pass.
 * @return uint16_t Updated age, saturated at `UINT16_MAX`.
 */
[[nodiscard]] constexpr inline auto addElapsedTimeSaturated(uint16_t currentAge_ms, uint16_t elapsed_ms) -> uint16_t
{
    const uint16_t updatedAge_ms = static_cast<uint16_t>(currentAge_ms + elapsed_ms);
    if (updatedAge_ms < currentAge_ms)
    {
        return UINT16_MAX;
    }
    return updatedAge_ms;
}
}  // namespace timeUtils

#endif  // LSH_CORE_UTIL_SATURATING_TIME_HPP
