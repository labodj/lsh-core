/**
 * @file    time_keeper.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares a utility for consistent time management within the main loop.
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

#ifndef LSH_CORE_UTIL_TIME_KEEPER_HPP
#define LSH_CORE_UTIL_TIME_KEEPER_HPP

#include <stdint.h>

#include "internal/user_config_bridge.hpp"

/**
 * @brief Utility namespace that exposes the controller time used by the main loop.
 */
namespace timeKeeper
{
extern uint32_t now;  //!< Cached value returned by the latest `millis()` refresh.

/**
 * @brief Gets the cached timestamp from the last `timeKeeper::update()` call.
 *
 * This avoids multiple calls to `millis()` within the same loop iteration, ensuring consistency.
 *
 * @return The cached time in milliseconds.
 */
inline auto getTime() -> uint32_t
{
    return now;
}

/**
 * @brief Refresh the cached timestamp from `millis()`.
 */
__attribute__((always_inline)) inline void update()
{
    now = millis();
}

/**
 * @brief Gets the current time directly by calling `millis()`.
 *
 * Use this when a fresh, non-cached timestamp is required.
 *
 * @return The current, real-time value from `millis()`.
 */
inline auto getRealTime() -> uint32_t
{
    return millis();
}
}  // namespace timeKeeper

#endif  // LSH_CORE_UTIL_TIME_KEEPER_HPP
