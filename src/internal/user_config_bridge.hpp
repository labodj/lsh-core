/**
 * @file    user_config_bridge.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Internal bridge that imports user-defined macros into the library's scope.
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

#ifndef LSH_CORE_INTERNAL_USER_CONFIG_BRIDGE_HPP
#define LSH_CORE_INTERNAL_USER_CONFIG_BRIDGE_HPP

#include "lsh_user_config.hpp"

#include "internal/lsh_config_types.hpp"

static constexpr const char *CONFIG_DEVICE_NAME = LSH_DEVICE_NAME;
static constexpr uint8_t CONFIG_MAX_CLICKABLES = LSH_MAX_CLICKABLES;
static constexpr uint8_t CONFIG_MAX_ACTUATORS = LSH_MAX_ACTUATORS;
static constexpr uint8_t CONFIG_MAX_INDICATORS = LSH_MAX_INDICATORS;

// Network clicks are optional at device level. Keeping this as a compile-time
// switch lets small installations remove all related state, timeout scans and
// serializer/deserializer branches when the feature is not used at all.
#ifdef LSH_DISABLE_NETWORK_CLICKS
#define CONFIG_USE_NETWORK_CLICKS 0
#else
#define CONFIG_USE_NETWORK_CLICKS 1
#endif

// Dense ID lookup can use a fixed ETL array when the consumer provides either:
// - the real maximum numeric ID, or
// - an explicit "IDs are dense" opt-in.
//
// The runtime stores `index + 1` in the LUT so that zero can still mean
// "missing entry" without extra flags.
#ifdef LSH_MAX_CLICKABLE_ID
#define CONFIG_USE_CLICKABLE_ID_LUT 1
static constexpr uint8_t CONFIG_MAX_CLICKABLE_ID = LSH_MAX_CLICKABLE_ID;
#elif defined(LSH_ASSUME_DENSE_CLICKABLE_IDS)
#define CONFIG_USE_CLICKABLE_ID_LUT 1
static constexpr uint8_t CONFIG_MAX_CLICKABLE_ID = CONFIG_MAX_CLICKABLES;
#else
#define CONFIG_USE_CLICKABLE_ID_LUT 0
#endif

#ifdef LSH_MAX_ACTUATOR_ID
#define CONFIG_USE_ACTUATOR_ID_LUT 1
static constexpr uint8_t CONFIG_MAX_ACTUATOR_ID = LSH_MAX_ACTUATOR_ID;
#elif defined(LSH_ASSUME_DENSE_ACTUATOR_IDS)
#define CONFIG_USE_ACTUATOR_ID_LUT 1
static constexpr uint8_t CONFIG_MAX_ACTUATOR_ID = CONFIG_MAX_ACTUATORS;
#else
#define CONFIG_USE_ACTUATOR_ID_LUT 0
#endif

// These totals size the compact shared pools used by Clickable and Indicator.
// They count real link entries, not devices.
//
// Example:
// - one button with three long-click actuators contributes 3 entries
// - one indicator following two relays contributes 2 entries
//
// When the consumer does not provide an explicit total, fall back to the old
// worst-case allocation strategy so existing projects stay compatible.
#ifdef LSH_MAX_SHORT_CLICK_ACTUATOR_LINKS
static constexpr uint16_t CONFIG_MAX_SHORT_CLICK_ACTUATOR_LINKS = LSH_MAX_SHORT_CLICK_ACTUATOR_LINKS;
#else
static constexpr uint16_t CONFIG_MAX_SHORT_CLICK_ACTUATOR_LINKS =
    static_cast<uint16_t>(CONFIG_MAX_CLICKABLES) * static_cast<uint16_t>(CONFIG_MAX_ACTUATORS);
#endif

#ifdef LSH_MAX_LONG_CLICK_ACTUATOR_LINKS
static constexpr uint16_t CONFIG_MAX_LONG_CLICK_ACTUATOR_LINKS = LSH_MAX_LONG_CLICK_ACTUATOR_LINKS;
#else
static constexpr uint16_t CONFIG_MAX_LONG_CLICK_ACTUATOR_LINKS =
    static_cast<uint16_t>(CONFIG_MAX_CLICKABLES) * static_cast<uint16_t>(CONFIG_MAX_ACTUATORS);
#endif

#ifdef LSH_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS
static constexpr uint16_t CONFIG_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS = LSH_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS;
#else
static constexpr uint16_t CONFIG_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS =
    static_cast<uint16_t>(CONFIG_MAX_CLICKABLES) * static_cast<uint16_t>(CONFIG_MAX_ACTUATORS);
#endif

#ifdef LSH_MAX_INDICATOR_ACTUATOR_LINKS
static constexpr uint16_t CONFIG_MAX_INDICATOR_ACTUATOR_LINKS = LSH_MAX_INDICATOR_ACTUATOR_LINKS;
#else
static constexpr uint16_t CONFIG_MAX_INDICATOR_ACTUATOR_LINKS =
    static_cast<uint16_t>(CONFIG_MAX_INDICATORS) * static_cast<uint16_t>(CONFIG_MAX_ACTUATORS);
#endif

// ETL arrays must have a strictly positive compile-time capacity.
// A device may legitimately configure zero links of one category, so the
// runtime keeps two numbers:
// - CONFIG_MAX_*_LINKS: the real logical maximum used for validation
// - CONFIG_*_STORAGE_CAPACITY: the physical ETL capacity, clamped to at least 1
//
// When the logical maximum is zero, the extra ETL slot is never used at runtime.
static constexpr uint16_t CONFIG_SHORT_CLICK_ACTUATOR_LINK_STORAGE_CAPACITY =
    (CONFIG_MAX_SHORT_CLICK_ACTUATOR_LINKS == 0U) ? 1U : CONFIG_MAX_SHORT_CLICK_ACTUATOR_LINKS;
static constexpr uint16_t CONFIG_LONG_CLICK_ACTUATOR_LINK_STORAGE_CAPACITY =
    (CONFIG_MAX_LONG_CLICK_ACTUATOR_LINKS == 0U) ? 1U : CONFIG_MAX_LONG_CLICK_ACTUATOR_LINKS;
static constexpr uint16_t CONFIG_SUPER_LONG_CLICK_ACTUATOR_LINK_STORAGE_CAPACITY =
    (CONFIG_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS == 0U) ? 1U : CONFIG_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS;
static constexpr uint16_t CONFIG_INDICATOR_ACTUATOR_LINK_STORAGE_CAPACITY =
    (CONFIG_MAX_INDICATOR_ACTUATOR_LINKS == 0U) ? 1U : CONFIG_MAX_INDICATOR_ACTUATOR_LINKS;

static constexpr HardwareSerial *const CONFIG_COM_SERIAL = LSH_COM_SERIAL;      //!< Serial port used for the controller-to-bridge link.
static constexpr HardwareSerial *const CONFIG_DEBUG_SERIAL = LSH_DEBUG_SERIAL;  //!< Serial port used for local debug output.

#endif  // LSH_CORE_INTERNAL_USER_CONFIG_BRIDGE_HPP
