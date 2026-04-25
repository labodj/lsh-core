/**
 * @file    click_detection.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Bit flags consumed by the generated clickable scan path.
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

#ifndef LSH_CORE_UTIL_CONSTANTS_CLICK_DETECTION_HPP
#define LSH_CORE_UTIL_CONSTANTS_CLICK_DETECTION_HPP

#include <stdint.h>

namespace constants
{
namespace clickDetection
{
static constexpr uint8_t SHORT_ENABLED = 0x01U;       //!< A release without timed action may emit a short click.
static constexpr uint8_t LONG_ENABLED = 0x02U;        //!< A held press may emit a long click.
static constexpr uint8_t SUPER_LONG_ENABLED = 0x04U;  //!< A held press may emit a super-long click.
static constexpr uint8_t QUICK_SHORT = 0x08U;         //!< The short click fires on press because no timed action exists.

[[nodiscard]] constexpr inline auto hasFlag(uint8_t flags, uint8_t flag) noexcept -> bool
{
    return (flags & flag) != 0U;
}

[[nodiscard]] constexpr inline auto makeFlags(bool shortEnabled, bool longEnabled, bool superLongEnabled) noexcept -> uint8_t
{
    return static_cast<uint8_t>((shortEnabled ? SHORT_ENABLED : 0U) | (longEnabled ? LONG_ENABLED : 0U) |
                                (superLongEnabled ? SUPER_LONG_ENABLED : 0U) |
                                ((shortEnabled && !longEnabled && !superLongEnabled) ? QUICK_SHORT : 0U));
}
}  // namespace clickDetection
}  // namespace constants

#endif  // LSH_CORE_UTIL_CONSTANTS_CLICK_DETECTION_HPP
