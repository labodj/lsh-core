/**
 * @file    config.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Exposes user-profile resource limits as named compile-time constants.
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

#ifndef LSH_CORE_UTIL_CONSTANTS_CONFIG_HPP
#define LSH_CORE_UTIL_CONSTANTS_CONFIG_HPP

#include <stdint.h>

#include "internal/user_config_bridge.hpp"

namespace constants
{
/**
 * @brief Resource limits derived from `lsh_user_config.hpp`.
 * @details `internal/user_config_bridge.hpp` keeps the legacy `CONFIG_*`
 *          symbols used by low-level allocation code. This namespace provides
 *          the same resource-limit values from the public constants area so
 *          documentation and new code do not need to depend on internal names.
 */
namespace config
{
static constexpr uint8_t MAX_ACTIVE_NETWORK_CLICKS = CONFIG_MAX_ACTIVE_NETWORK_CLICKS;
static constexpr uint8_t ACTIVE_NETWORK_CLICK_STORAGE_CAPACITY = CONFIG_ACTIVE_NETWORK_CLICK_STORAGE_CAPACITY;

#if CONFIG_USE_CLICKABLE_TIMING_POOL
static constexpr uint8_t MAX_CLICKABLE_TIMING_OVERRIDES = CONFIG_MAX_CLICKABLE_TIMING_OVERRIDES;
static constexpr uint8_t CLICKABLE_TIMING_STORAGE_CAPACITY = CONFIG_CLICKABLE_TIMING_STORAGE_CAPACITY;
#else
static constexpr uint8_t MAX_CLICKABLE_TIMING_OVERRIDES = 0U;
static constexpr uint8_t CLICKABLE_TIMING_STORAGE_CAPACITY = 0U;
#endif

static constexpr uint8_t MAX_AUTO_OFF_ACTUATORS = CONFIG_MAX_AUTO_OFF_ACTUATORS;
static constexpr uint8_t AUTO_OFF_STORAGE_CAPACITY = CONFIG_AUTO_OFF_STORAGE_CAPACITY;
static constexpr uint16_t MAX_SHORT_CLICK_ACTUATOR_LINKS = CONFIG_MAX_SHORT_CLICK_ACTUATOR_LINKS;
static constexpr uint16_t MAX_LONG_CLICK_ACTUATOR_LINKS = CONFIG_MAX_LONG_CLICK_ACTUATOR_LINKS;
static constexpr uint16_t MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS = CONFIG_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS;
static constexpr uint16_t MAX_INDICATOR_ACTUATOR_LINKS = CONFIG_MAX_INDICATOR_ACTUATOR_LINKS;
}  // namespace config
}  // namespace constants

#endif  // LSH_CORE_UTIL_CONSTANTS_CONFIG_HPP
