/**
 * @file    user_config_bridge.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Internal bridge that imports static profile resources into the library's scope.
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

#ifndef LSH_STATIC_CONFIG_INCLUDE
#error "LSH_STATIC_CONFIG_INCLUDE must point to the generated static profile header."
#endif

#define LSH_STATIC_CONFIG_RESOURCE_PASS 1
#include LSH_STATIC_CONFIG_INCLUDE
#undef LSH_STATIC_CONFIG_RESOURCE_PASS

#define CONFIG_HAS_STATIC_CONFIG 1

#ifndef LSH_STATIC_CONFIG_CLICKABLES
#error "LSH_STATIC_CONFIG_CLICKABLES must be defined by the static profile."
#endif
#ifndef LSH_STATIC_CONFIG_ACTUATORS
#error "LSH_STATIC_CONFIG_ACTUATORS must be defined by the static profile."
#endif
#ifndef LSH_STATIC_CONFIG_INDICATORS
#error "LSH_STATIC_CONFIG_INDICATORS must be defined by the static profile."
#endif
#ifndef LSH_STATIC_CONFIG_MAX_CLICKABLE_ID
#error "LSH_STATIC_CONFIG_MAX_CLICKABLE_ID must be defined by the static profile."
#endif
#ifndef LSH_STATIC_CONFIG_MAX_ACTUATOR_ID
#error "LSH_STATIC_CONFIG_MAX_ACTUATOR_ID must be defined by the static profile."
#endif
#ifndef LSH_STATIC_CONFIG_SHORT_CLICK_ACTUATOR_LINKS
#error "LSH_STATIC_CONFIG_SHORT_CLICK_ACTUATOR_LINKS must be defined by the static profile."
#endif
#ifndef LSH_STATIC_CONFIG_LONG_CLICK_ACTUATOR_LINKS
#error "LSH_STATIC_CONFIG_LONG_CLICK_ACTUATOR_LINKS must be defined by the static profile."
#endif
#ifndef LSH_STATIC_CONFIG_SUPER_LONG_CLICK_ACTUATOR_LINKS
#error "LSH_STATIC_CONFIG_SUPER_LONG_CLICK_ACTUATOR_LINKS must be defined by the static profile."
#endif
#ifndef LSH_STATIC_CONFIG_INDICATOR_ACTUATOR_LINKS
#error "LSH_STATIC_CONFIG_INDICATOR_ACTUATOR_LINKS must be defined by the static profile."
#endif
#ifndef LSH_STATIC_CONFIG_CLICKABLE_TIMING_OVERRIDES
#error "LSH_STATIC_CONFIG_CLICKABLE_TIMING_OVERRIDES must be defined by the static profile."
#endif
#ifndef LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS
#error "LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS must be defined by the static profile."
#endif
#ifndef LSH_STATIC_CONFIG_ACTIVE_NETWORK_CLICKS
#error "LSH_STATIC_CONFIG_ACTIVE_NETWORK_CLICKS must be defined by the static profile."
#endif
#ifndef LSH_STATIC_CONFIG_DISABLE_NETWORK_CLICKS
#error "LSH_STATIC_CONFIG_DISABLE_NETWORK_CLICKS must be defined by the static profile."
#endif

static_assert(LSH_STATIC_CONFIG_CLICKABLES >= 0, "LSH_STATIC_CONFIG_CLICKABLES must be non-negative.");
static_assert(LSH_STATIC_CONFIG_CLICKABLES <= UINT8_MAX, "LSH_STATIC_CONFIG_CLICKABLES must fit in uint8_t.");
static_assert(LSH_STATIC_CONFIG_ACTUATORS >= 0, "LSH_STATIC_CONFIG_ACTUATORS must be non-negative.");
static_assert(LSH_STATIC_CONFIG_ACTUATORS <= UINT8_MAX, "LSH_STATIC_CONFIG_ACTUATORS must fit in uint8_t.");
static_assert(LSH_STATIC_CONFIG_INDICATORS >= 0, "LSH_STATIC_CONFIG_INDICATORS must be non-negative.");
static_assert(LSH_STATIC_CONFIG_INDICATORS <= UINT8_MAX, "LSH_STATIC_CONFIG_INDICATORS must fit in uint8_t.");
static_assert(LSH_STATIC_CONFIG_MAX_CLICKABLE_ID > 0, "LSH_STATIC_CONFIG_MAX_CLICKABLE_ID must be greater than zero.");
static_assert(LSH_STATIC_CONFIG_MAX_CLICKABLE_ID <= UINT8_MAX, "LSH_STATIC_CONFIG_MAX_CLICKABLE_ID must fit in uint8_t.");
static_assert(LSH_STATIC_CONFIG_MAX_ACTUATOR_ID > 0, "LSH_STATIC_CONFIG_MAX_ACTUATOR_ID must be greater than zero.");
static_assert(LSH_STATIC_CONFIG_MAX_ACTUATOR_ID <= UINT8_MAX, "LSH_STATIC_CONFIG_MAX_ACTUATOR_ID must fit in uint8_t.");
static_assert(LSH_STATIC_CONFIG_DISABLE_NETWORK_CLICKS == 0U || LSH_STATIC_CONFIG_DISABLE_NETWORK_CLICKS == 1U,
              "LSH_STATIC_CONFIG_DISABLE_NETWORK_CLICKS must be 0 or 1.");

static constexpr const char *CONFIG_DEVICE_NAME = LSH_DEVICE_NAME;  //!< Stable device name exposed on the wire and in logs.
static constexpr uint8_t CONFIG_MAX_CLICKABLES =
    LSH_STATIC_CONFIG_CLICKABLES;  //!< Exact number of clickables declared by the generated static profile.
static constexpr uint8_t CONFIG_MAX_ACTUATORS =
    LSH_STATIC_CONFIG_ACTUATORS;  //!< Exact number of actuators declared by the generated static profile.
static constexpr uint8_t CONFIG_MAX_INDICATORS =
    LSH_STATIC_CONFIG_INDICATORS;  //!< Exact number of indicators declared by the generated static profile.
static constexpr uint8_t CONFIG_PACKED_ACTUATOR_STATE_BYTES =
    static_cast<uint8_t>((CONFIG_MAX_ACTUATORS + 7U) >> 3U);  //!< Packed bytes required by the actuator-state wire payload.
static constexpr uint8_t CONFIG_PACKED_ACTUATOR_STATE_STORAGE_CAPACITY =
    (CONFIG_PACKED_ACTUATOR_STATE_BYTES == 0U)
        ? 1U
        : CONFIG_PACKED_ACTUATOR_STATE_BYTES;  //!< Physical storage capacity for the packed actuator-state shadow.

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
static_assert(CONFIG_CLICKABLE_ACTUATOR_LINKS_WORST_CASE <= UINT16_MAX,
              "Worst-case clickable actuator-link budget exceeds 16-bit storage.");
static_assert(CONFIG_INDICATOR_ACTUATOR_LINKS_WORST_CASE <= UINT16_MAX,
              "Worst-case indicator actuator-link budget exceeds 16-bit storage.");

#if LSH_STATIC_CONFIG_DISABLE_NETWORK_CLICKS
#define CONFIG_USE_NETWORK_CLICKS 0
#else
#define CONFIG_USE_NETWORK_CLICKS 1
#endif

#if defined(LSH_NETWORK_CLICKS)
#error "LSH_NETWORK_CLICKS was removed; network-click support is derived from network=true click actions."
#endif

static_assert(LSH_STATIC_CONFIG_ACTIVE_NETWORK_CLICKS >= 0, "LSH_STATIC_CONFIG_ACTIVE_NETWORK_CLICKS must be non-negative.");
static_assert(LSH_STATIC_CONFIG_ACTIVE_NETWORK_CLICKS <= UINT8_MAX, "LSH_STATIC_CONFIG_ACTIVE_NETWORK_CLICKS must fit in uint8_t.");
static_assert(LSH_STATIC_CONFIG_ACTIVE_NETWORK_CLICKS <= CONFIG_NETWORK_CLICK_TRANSACTIONS_WORST_CASE,
              "LSH_STATIC_CONFIG_ACTIVE_NETWORK_CLICKS cannot exceed two network-click transactions per clickable.");
static_assert((CONFIG_USE_NETWORK_CLICKS && LSH_STATIC_CONFIG_ACTIVE_NETWORK_CLICKS > 0U) ||
                  (!CONFIG_USE_NETWORK_CLICKS && LSH_STATIC_CONFIG_ACTIVE_NETWORK_CLICKS == 0U),
              "Static network-click profile must use a positive pool only when network clicks are enabled.");
static constexpr uint8_t CONFIG_MAX_ACTIVE_NETWORK_CLICKS =
    LSH_STATIC_CONFIG_ACTIVE_NETWORK_CLICKS;  //!< Maximum number of concurrent bridge-assisted click transactions.
static constexpr uint8_t CONFIG_ACTIVE_NETWORK_CLICK_STORAGE_CAPACITY =
    (CONFIG_MAX_ACTIVE_NETWORK_CLICKS == 0U) ? 1U : CONFIG_MAX_ACTIVE_NETWORK_CLICKS;

// Static profiles resolve click timings through generated accessors. Keeping
// this switch enabled removes the per-clickable 16-bit timing fields from RAM;
// the legacy storage-capacity constants below remain for diagnostics/API shape.
#define CONFIG_USE_CLICKABLE_TIMING_POOL 1
static_assert(LSH_STATIC_CONFIG_CLICKABLE_TIMING_OVERRIDES >= 0, "LSH_STATIC_CONFIG_CLICKABLE_TIMING_OVERRIDES must be non-negative.");
static_assert(LSH_STATIC_CONFIG_CLICKABLE_TIMING_OVERRIDES <= UINT8_MAX,
              "LSH_STATIC_CONFIG_CLICKABLE_TIMING_OVERRIDES must fit in uint8_t.");
static constexpr uint16_t CONFIG_MAX_CLICKABLE_TIMING_OVERRIDES_WIDE = static_cast<uint16_t>(LSH_STATIC_CONFIG_CLICKABLE_TIMING_OVERRIDES);
static_assert(CONFIG_MAX_CLICKABLE_TIMING_OVERRIDES_WIDE <= static_cast<uint16_t>(CONFIG_MAX_CLICKABLES),
              "LSH_STATIC_CONFIG_CLICKABLE_TIMING_OVERRIDES cannot exceed LSH_STATIC_CONFIG_CLICKABLES.");
static constexpr uint8_t CONFIG_MAX_CLICKABLE_TIMING_OVERRIDES =
    static_cast<uint8_t>(CONFIG_MAX_CLICKABLE_TIMING_OVERRIDES_WIDE);  //!< Number of clickables that override default timings.
static constexpr uint8_t CONFIG_CLICKABLE_TIMING_STORAGE_CAPACITY =
    (CONFIG_MAX_CLICKABLE_TIMING_OVERRIDES == 0U) ? 1U : CONFIG_MAX_CLICKABLE_TIMING_OVERRIDES;

static_assert(LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS >= 0, "LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS must be non-negative.");
static_assert(LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS <= UINT8_MAX, "LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS must fit in uint8_t.");
static constexpr uint16_t CONFIG_MAX_AUTO_OFF_ACTUATORS_WIDE = static_cast<uint16_t>(LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS);
static_assert(CONFIG_MAX_AUTO_OFF_ACTUATORS_WIDE <= static_cast<uint16_t>(CONFIG_MAX_ACTUATORS),
              "LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS cannot exceed LSH_STATIC_CONFIG_ACTUATORS.");
static constexpr uint8_t CONFIG_MAX_AUTO_OFF_ACTUATORS =
    static_cast<uint8_t>(CONFIG_MAX_AUTO_OFF_ACTUATORS_WIDE);  //!< Number of actuators with an auto-off timer in the whole device.
static constexpr uint8_t CONFIG_AUTO_OFF_STORAGE_CAPACITY = (CONFIG_MAX_AUTO_OFF_ACTUATORS == 0U) ? 1U : CONFIG_MAX_AUTO_OFF_ACTUATORS;

#if defined(LSH_COMPACT_ACTUATOR_SWITCH_TIMES)
#error "LSH_COMPACT_ACTUATOR_SWITCH_TIMES was removed; set CONFIG_ACTUATOR_DEBOUNCE_TIME_MS=0 for automatic compact storage."
#endif

#if (LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS > 0) && defined(CONFIG_ACTUATOR_DEBOUNCE_TIME_MS) && (CONFIG_ACTUATOR_DEBOUNCE_TIME_MS == 0)
#define CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES 1
#else
#define CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES 0
#endif

static constexpr uint8_t CONFIG_MAX_CLICKABLE_ID =
    LSH_STATIC_CONFIG_MAX_CLICKABLE_ID;  //!< Highest clickable ID accepted by the generated clickable mapping.

static constexpr uint8_t CONFIG_MAX_ACTUATOR_ID =
    LSH_STATIC_CONFIG_MAX_ACTUATOR_ID;  //!< Highest actuator ID accepted by the generated actuator mapping.

static_assert(LSH_STATIC_CONFIG_SHORT_CLICK_ACTUATOR_LINKS >= 0, "LSH_STATIC_CONFIG_SHORT_CLICK_ACTUATOR_LINKS must be non-negative.");
static_assert(LSH_STATIC_CONFIG_SHORT_CLICK_ACTUATOR_LINKS <= CONFIG_CLICKABLE_ACTUATOR_LINKS_WORST_CASE,
              "LSH_STATIC_CONFIG_SHORT_CLICK_ACTUATOR_LINKS exceeds the clickable*actuator worst case.");
static constexpr uint16_t CONFIG_MAX_SHORT_CLICK_ACTUATOR_LINKS =
    LSH_STATIC_CONFIG_SHORT_CLICK_ACTUATOR_LINKS;  //!< Logical maximum of short-click links in the whole device.

static_assert(LSH_STATIC_CONFIG_LONG_CLICK_ACTUATOR_LINKS >= 0, "LSH_STATIC_CONFIG_LONG_CLICK_ACTUATOR_LINKS must be non-negative.");
static_assert(LSH_STATIC_CONFIG_LONG_CLICK_ACTUATOR_LINKS <= CONFIG_CLICKABLE_ACTUATOR_LINKS_WORST_CASE,
              "LSH_STATIC_CONFIG_LONG_CLICK_ACTUATOR_LINKS exceeds the clickable*actuator worst case.");
static constexpr uint16_t CONFIG_MAX_LONG_CLICK_ACTUATOR_LINKS =
    LSH_STATIC_CONFIG_LONG_CLICK_ACTUATOR_LINKS;  //!< Logical maximum of long-click links in the whole device.

static_assert(LSH_STATIC_CONFIG_SUPER_LONG_CLICK_ACTUATOR_LINKS >= 0,
              "LSH_STATIC_CONFIG_SUPER_LONG_CLICK_ACTUATOR_LINKS must be non-negative.");
static_assert(LSH_STATIC_CONFIG_SUPER_LONG_CLICK_ACTUATOR_LINKS <= CONFIG_CLICKABLE_ACTUATOR_LINKS_WORST_CASE,
              "LSH_STATIC_CONFIG_SUPER_LONG_CLICK_ACTUATOR_LINKS exceeds the clickable*actuator worst case.");
static constexpr uint16_t CONFIG_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS =
    LSH_STATIC_CONFIG_SUPER_LONG_CLICK_ACTUATOR_LINKS;  //!< Logical maximum of super-long-click links in the whole device.

static_assert(LSH_STATIC_CONFIG_INDICATOR_ACTUATOR_LINKS >= 0, "LSH_STATIC_CONFIG_INDICATOR_ACTUATOR_LINKS must be non-negative.");
static_assert(LSH_STATIC_CONFIG_INDICATOR_ACTUATOR_LINKS <= CONFIG_INDICATOR_ACTUATOR_LINKS_WORST_CASE,
              "LSH_STATIC_CONFIG_INDICATOR_ACTUATOR_LINKS exceeds the indicator*actuator worst case.");
static constexpr uint16_t CONFIG_MAX_INDICATOR_ACTUATOR_LINKS =
    LSH_STATIC_CONFIG_INDICATOR_ACTUATOR_LINKS;  //!< Logical maximum of indicator-to-actuator links in the whole device.

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
