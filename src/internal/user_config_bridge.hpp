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

#include <stdint.h>

#include "lsh_user_config.hpp"

#include "internal/lsh_config_types.hpp"

static_assert(LSH_MAX_CLICKABLES >= 0, "LSH_MAX_CLICKABLES must be non-negative.");
static_assert(LSH_MAX_CLICKABLES <= UINT8_MAX, "LSH_MAX_CLICKABLES must fit in uint8_t.");
static_assert(LSH_MAX_ACTUATORS >= 0, "LSH_MAX_ACTUATORS must be non-negative.");
static_assert(LSH_MAX_ACTUATORS <= UINT8_MAX, "LSH_MAX_ACTUATORS must fit in uint8_t.");
static_assert(LSH_MAX_INDICATORS >= 0, "LSH_MAX_INDICATORS must be non-negative.");
static_assert(LSH_MAX_INDICATORS <= UINT8_MAX, "LSH_MAX_INDICATORS must fit in uint8_t.");

static constexpr const char *CONFIG_DEVICE_NAME = LSH_DEVICE_NAME;    //!< Stable device name exposed on the wire and in logs.
static constexpr uint8_t CONFIG_MAX_CLICKABLES = LSH_MAX_CLICKABLES;  //!< Maximum number of clickables declared by the consumer profile.
static constexpr uint8_t CONFIG_MAX_ACTUATORS = LSH_MAX_ACTUATORS;    //!< Maximum number of actuators declared by the consumer profile.
static constexpr uint8_t CONFIG_MAX_INDICATORS = LSH_MAX_INDICATORS;  //!< Maximum number of indicators declared by the consumer profile.

// Compute worst-case compact-pool budgets in 32 bits first, then downcast only
// after a static check. Today the device counts are 8-bit, so the product still
// fits in 16 bits, but keeping the wide intermediate makes the intent explicit
// and protects future maintenance if the count types ever grow.
static constexpr uint32_t CONFIG_CLICKABLE_ACTUATOR_LINKS_WORST_CASE =
    static_cast<uint32_t>(CONFIG_MAX_CLICKABLES) * static_cast<uint32_t>(CONFIG_MAX_ACTUATORS);
static constexpr uint32_t CONFIG_INDICATOR_ACTUATOR_LINKS_WORST_CASE =
    static_cast<uint32_t>(CONFIG_MAX_INDICATORS) * static_cast<uint32_t>(CONFIG_MAX_ACTUATORS);
static constexpr uint16_t CONFIG_NETWORK_CLICK_TRANSACTIONS_WORST_CASE =
    static_cast<uint16_t>(static_cast<uint16_t>(CONFIG_MAX_CLICKABLES) * 2U);
static constexpr uint8_t CONFIG_DEFAULT_MAX_ACTIVE_NETWORK_CLICKS =
    (CONFIG_NETWORK_CLICK_TRANSACTIONS_WORST_CASE > UINT8_MAX) ? UINT8_MAX
                                                               : static_cast<uint8_t>(CONFIG_NETWORK_CLICK_TRANSACTIONS_WORST_CASE);
static_assert(CONFIG_CLICKABLE_ACTUATOR_LINKS_WORST_CASE <= UINT16_MAX,
              "Worst-case clickable actuator-link budget exceeds 16-bit storage.");
static_assert(CONFIG_INDICATOR_ACTUATOR_LINKS_WORST_CASE <= UINT16_MAX,
              "Worst-case indicator actuator-link budget exceeds 16-bit storage.");

// Network clicks are optional at device level. Keeping this as a compile-time
// switch lets small installations remove all related state, timeout scans and
// serializer/deserializer branches when the feature is not used at all.
#ifdef LSH_DISABLE_NETWORK_CLICKS
#define CONFIG_USE_NETWORK_CLICKS 0
#else
#define CONFIG_USE_NETWORK_CLICKS 1
#endif

#ifdef LSH_MAX_ACTIVE_NETWORK_CLICKS
static_assert(LSH_MAX_ACTIVE_NETWORK_CLICKS >= 0, "LSH_MAX_ACTIVE_NETWORK_CLICKS must be non-negative.");
static_assert(LSH_MAX_ACTIVE_NETWORK_CLICKS <= UINT8_MAX, "LSH_MAX_ACTIVE_NETWORK_CLICKS must fit in uint8_t.");
static_assert(LSH_MAX_ACTIVE_NETWORK_CLICKS <= CONFIG_NETWORK_CLICK_TRANSACTIONS_WORST_CASE,
              "LSH_MAX_ACTIVE_NETWORK_CLICKS cannot exceed two network-click transactions per clickable.");
static constexpr uint8_t CONFIG_MAX_ACTIVE_NETWORK_CLICKS =
    LSH_MAX_ACTIVE_NETWORK_CLICKS;  //!< Maximum number of concurrent bridge-assisted click transactions.
#else
static constexpr uint8_t CONFIG_MAX_ACTIVE_NETWORK_CLICKS =
    CONFIG_DEFAULT_MAX_ACTIVE_NETWORK_CLICKS;  //!< Compatibility default for pending network-click transactions, byte-count clamped.
#endif
static_assert(!CONFIG_USE_NETWORK_CLICKS || CONFIG_MAX_ACTIVE_NETWORK_CLICKS > 0U,
              "Network clicks need at least one active transaction slot; use LSH_DISABLE_NETWORK_CLICKS to remove the feature.");
static constexpr uint8_t CONFIG_ACTIVE_NETWORK_CLICK_STORAGE_CAPACITY =
    (CONFIG_MAX_ACTIVE_NETWORK_CLICKS == 0U) ? 1U : CONFIG_MAX_ACTIVE_NETWORK_CLICKS;

#ifdef LSH_MAX_CLICKABLE_TIMING_OVERRIDES
#define CONFIG_USE_CLICKABLE_TIMING_POOL 1
static_assert(LSH_MAX_CLICKABLE_TIMING_OVERRIDES >= 0, "LSH_MAX_CLICKABLE_TIMING_OVERRIDES must be non-negative.");
static_assert(LSH_MAX_CLICKABLE_TIMING_OVERRIDES <= UINT8_MAX, "LSH_MAX_CLICKABLE_TIMING_OVERRIDES must fit in uint8_t.");
static constexpr uint16_t CONFIG_MAX_CLICKABLE_TIMING_OVERRIDES_WIDE = static_cast<uint16_t>(LSH_MAX_CLICKABLE_TIMING_OVERRIDES);
static_assert(CONFIG_MAX_CLICKABLE_TIMING_OVERRIDES_WIDE <= static_cast<uint16_t>(CONFIG_MAX_CLICKABLES),
              "LSH_MAX_CLICKABLE_TIMING_OVERRIDES cannot exceed LSH_MAX_CLICKABLES.");
static constexpr uint8_t CONFIG_MAX_CLICKABLE_TIMING_OVERRIDES = static_cast<uint8_t>(
    CONFIG_MAX_CLICKABLE_TIMING_OVERRIDES_WIDE);  //!< Number of clickables that may override default long/super-long timings.
#else
#define CONFIG_USE_CLICKABLE_TIMING_POOL 0
#endif

#if CONFIG_USE_CLICKABLE_TIMING_POOL
static constexpr uint8_t CONFIG_CLICKABLE_TIMING_STORAGE_CAPACITY =
    (CONFIG_MAX_CLICKABLE_TIMING_OVERRIDES == 0U) ? 1U : CONFIG_MAX_CLICKABLE_TIMING_OVERRIDES;
#endif

#ifdef LSH_MAX_AUTO_OFF_ACTUATORS
static_assert(LSH_MAX_AUTO_OFF_ACTUATORS >= 0, "LSH_MAX_AUTO_OFF_ACTUATORS must be non-negative.");
static_assert(LSH_MAX_AUTO_OFF_ACTUATORS <= UINT8_MAX, "LSH_MAX_AUTO_OFF_ACTUATORS must fit in uint8_t.");
static constexpr uint16_t CONFIG_MAX_AUTO_OFF_ACTUATORS_WIDE = static_cast<uint16_t>(LSH_MAX_AUTO_OFF_ACTUATORS);
static_assert(CONFIG_MAX_AUTO_OFF_ACTUATORS_WIDE <= static_cast<uint16_t>(CONFIG_MAX_ACTUATORS),
              "LSH_MAX_AUTO_OFF_ACTUATORS cannot exceed LSH_MAX_ACTUATORS.");
static constexpr uint8_t CONFIG_MAX_AUTO_OFF_ACTUATORS =
    static_cast<uint8_t>(CONFIG_MAX_AUTO_OFF_ACTUATORS_WIDE);  //!< Number of actuators with an auto-off timer in the whole device.
#else
static constexpr uint8_t CONFIG_MAX_AUTO_OFF_ACTUATORS = CONFIG_MAX_ACTUATORS;
#endif
static constexpr uint8_t CONFIG_AUTO_OFF_STORAGE_CAPACITY = (CONFIG_MAX_AUTO_OFF_ACTUATORS == 0U) ? 1U : CONFIG_MAX_AUTO_OFF_ACTUATORS;

#ifdef LSH_COMPACT_ACTUATOR_SWITCH_TIMES
#define CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES 1
#else
#define CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES 0
#endif

// Dense ID lookup can use a fixed ETL array when the consumer provides either:
// - the real maximum numeric ID, or
// - an explicit "IDs are dense" opt-in.
//
// The runtime stores `index + 1` in the LUT so that zero can still mean
// "missing entry" without extra flags.
#ifdef LSH_MAX_CLICKABLE_ID
#define CONFIG_USE_CLICKABLE_ID_LUT 1
static_assert(LSH_MAX_CLICKABLE_ID > 0, "LSH_MAX_CLICKABLE_ID must be greater than zero.");
static_assert(LSH_MAX_CLICKABLE_ID <= UINT8_MAX, "LSH_MAX_CLICKABLE_ID must fit in uint8_t.");
static constexpr uint8_t CONFIG_MAX_CLICKABLE_ID =
    LSH_MAX_CLICKABLE_ID;  //!< Highest clickable ID accepted by the bounded clickable lookup table.
#elif defined(LSH_ASSUME_DENSE_CLICKABLE_IDS)
#define CONFIG_USE_CLICKABLE_ID_LUT 1
static constexpr uint8_t CONFIG_MAX_CLICKABLE_ID =
    CONFIG_MAX_CLICKABLES;  //!< Highest clickable ID accepted when clickable IDs are declared dense.
#else
#define CONFIG_USE_CLICKABLE_ID_LUT 0
#endif

#ifdef LSH_MAX_ACTUATOR_ID
#define CONFIG_USE_ACTUATOR_ID_LUT 1
static_assert(LSH_MAX_ACTUATOR_ID > 0, "LSH_MAX_ACTUATOR_ID must be greater than zero.");
static_assert(LSH_MAX_ACTUATOR_ID <= UINT8_MAX, "LSH_MAX_ACTUATOR_ID must fit in uint8_t.");
static constexpr uint8_t CONFIG_MAX_ACTUATOR_ID =
    LSH_MAX_ACTUATOR_ID;  //!< Highest actuator ID accepted by the bounded actuator lookup table.
#elif defined(LSH_ASSUME_DENSE_ACTUATOR_IDS)
#define CONFIG_USE_ACTUATOR_ID_LUT 1
static constexpr uint8_t CONFIG_MAX_ACTUATOR_ID =
    CONFIG_MAX_ACTUATORS;  //!< Highest actuator ID accepted when actuator IDs are declared dense.
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
// When the consumer does not provide an explicit total, fall back to the
// compatibility worst-case allocation strategy so existing projects keep
// building unchanged.
//
// On AVR this fallback can waste a surprising amount of static RAM because the
// compact pools are allocated in `.bss`, not lazily at runtime. Emit a build
// warning so a maintainer sees the tradeoff immediately instead of discovering
// it later from a tight memory report.
#if defined(__AVR__)
#ifndef LSH_MAX_SHORT_CLICK_ACTUATOR_LINKS
#warning "lsh-core AVR build: LSH_MAX_SHORT_CLICK_ACTUATOR_LINKS not defined; worst-case clickable*actuator storage will be reserved."
#endif
#ifndef LSH_MAX_LONG_CLICK_ACTUATOR_LINKS
#warning "lsh-core AVR build: LSH_MAX_LONG_CLICK_ACTUATOR_LINKS not defined; worst-case clickable*actuator storage will be reserved."
#endif
#ifndef LSH_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS
#warning "lsh-core AVR build: LSH_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS not defined; worst-case clickable*actuator storage will be reserved."
#endif
#ifndef LSH_MAX_INDICATOR_ACTUATOR_LINKS
#warning "lsh-core AVR build: LSH_MAX_INDICATOR_ACTUATOR_LINKS not defined; worst-case indicator*actuator storage will be reserved."
#endif
#endif

#ifdef LSH_MAX_SHORT_CLICK_ACTUATOR_LINKS
static_assert(LSH_MAX_SHORT_CLICK_ACTUATOR_LINKS >= 0, "LSH_MAX_SHORT_CLICK_ACTUATOR_LINKS must be non-negative.");
static_assert(LSH_MAX_SHORT_CLICK_ACTUATOR_LINKS <= CONFIG_CLICKABLE_ACTUATOR_LINKS_WORST_CASE,
              "LSH_MAX_SHORT_CLICK_ACTUATOR_LINKS exceeds the clickable*actuator worst case.");
static constexpr uint16_t CONFIG_MAX_SHORT_CLICK_ACTUATOR_LINKS =
    LSH_MAX_SHORT_CLICK_ACTUATOR_LINKS;  //!< Logical maximum of short-click links in the whole device.
#else
static constexpr uint16_t CONFIG_MAX_SHORT_CLICK_ACTUATOR_LINKS = static_cast<uint16_t>(
    CONFIG_CLICKABLE_ACTUATOR_LINKS_WORST_CASE);  //!< Worst-case short-click link budget when the consumer does not provide a tighter total.
#endif

#ifdef LSH_MAX_LONG_CLICK_ACTUATOR_LINKS
static_assert(LSH_MAX_LONG_CLICK_ACTUATOR_LINKS >= 0, "LSH_MAX_LONG_CLICK_ACTUATOR_LINKS must be non-negative.");
static_assert(LSH_MAX_LONG_CLICK_ACTUATOR_LINKS <= CONFIG_CLICKABLE_ACTUATOR_LINKS_WORST_CASE,
              "LSH_MAX_LONG_CLICK_ACTUATOR_LINKS exceeds the clickable*actuator worst case.");
static constexpr uint16_t CONFIG_MAX_LONG_CLICK_ACTUATOR_LINKS =
    LSH_MAX_LONG_CLICK_ACTUATOR_LINKS;  //!< Logical maximum of long-click links in the whole device.
#else
static constexpr uint16_t CONFIG_MAX_LONG_CLICK_ACTUATOR_LINKS = static_cast<uint16_t>(
    CONFIG_CLICKABLE_ACTUATOR_LINKS_WORST_CASE);  //!< Worst-case long-click link budget when the consumer does not provide a tighter total.
#endif

#ifdef LSH_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS
static_assert(LSH_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS >= 0, "LSH_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS must be non-negative.");
static_assert(LSH_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS <= CONFIG_CLICKABLE_ACTUATOR_LINKS_WORST_CASE,
              "LSH_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS exceeds the clickable*actuator worst case.");
static constexpr uint16_t CONFIG_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS =
    LSH_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS;  //!< Logical maximum of super-long-click links in the whole device.
#else
static constexpr uint16_t CONFIG_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS = static_cast<uint16_t>(
    CONFIG_CLICKABLE_ACTUATOR_LINKS_WORST_CASE);  //!< Worst-case super-long-click link budget when the consumer does not provide a tighter total.
#endif

#ifdef LSH_MAX_INDICATOR_ACTUATOR_LINKS
static_assert(LSH_MAX_INDICATOR_ACTUATOR_LINKS >= 0, "LSH_MAX_INDICATOR_ACTUATOR_LINKS must be non-negative.");
static_assert(LSH_MAX_INDICATOR_ACTUATOR_LINKS <= CONFIG_INDICATOR_ACTUATOR_LINKS_WORST_CASE,
              "LSH_MAX_INDICATOR_ACTUATOR_LINKS exceeds the indicator*actuator worst case.");
static constexpr uint16_t CONFIG_MAX_INDICATOR_ACTUATOR_LINKS =
    LSH_MAX_INDICATOR_ACTUATOR_LINKS;  //!< Logical maximum of indicator-to-actuator links in the whole device.
#else
static constexpr uint16_t CONFIG_MAX_INDICATOR_ACTUATOR_LINKS = static_cast<uint16_t>(
    CONFIG_INDICATOR_ACTUATOR_LINKS_WORST_CASE);  //!< Worst-case indicator link budget when the consumer does not provide a tighter total.
#endif

// ETL arrays must have a strictly positive compile-time capacity.
// A device may legitimately configure zero links of one category, so the
// runtime keeps two numbers:
// - CONFIG_MAX_*_LINKS: the real logical maximum used for validation
// - CONFIG_*_STORAGE_CAPACITY: the physical ETL capacity, clamped to at least 1
//
// When the logical maximum is zero, the extra ETL slot is never used at runtime.
static constexpr uint16_t CONFIG_SHORT_CLICK_ACTUATOR_LINK_STORAGE_CAPACITY =
    (CONFIG_MAX_SHORT_CLICK_ACTUATOR_LINKS == 0U)
        ? 1U
        : CONFIG_MAX_SHORT_CLICK_ACTUATOR_LINKS;  //!< Physical ETL storage capacity for short-click links, clamped to at least one slot.
static constexpr uint16_t CONFIG_LONG_CLICK_ACTUATOR_LINK_STORAGE_CAPACITY =
    (CONFIG_MAX_LONG_CLICK_ACTUATOR_LINKS == 0U)
        ? 1U
        : CONFIG_MAX_LONG_CLICK_ACTUATOR_LINKS;  //!< Physical ETL storage capacity for long-click links, clamped to at least one slot.
static constexpr uint16_t CONFIG_SUPER_LONG_CLICK_ACTUATOR_LINK_STORAGE_CAPACITY =
    (CONFIG_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS == 0U)
        ? 1U
        : CONFIG_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS;  //!< Physical ETL storage capacity for super-long-click links, clamped to at least one slot.
static constexpr uint16_t CONFIG_INDICATOR_ACTUATOR_LINK_STORAGE_CAPACITY =
    (CONFIG_MAX_INDICATOR_ACTUATOR_LINKS == 0U)
        ? 1U
        : CONFIG_MAX_INDICATOR_ACTUATOR_LINKS;  //!< Physical ETL storage capacity for indicator links, clamped to at least one slot.

static constexpr HardwareSerial *const CONFIG_COM_SERIAL = LSH_COM_SERIAL;      //!< Serial port used for the controller-to-bridge link.
static constexpr HardwareSerial *const CONFIG_DEBUG_SERIAL = LSH_DEBUG_SERIAL;  //!< Serial port used for local debug output.

#endif  // LSH_CORE_INTERNAL_USER_CONFIG_BRIDGE_HPP
