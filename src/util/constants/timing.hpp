/**
 * @file    timing.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Centralizes all build-time configurable timing constants for the framework.
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

#ifndef LSHCORE_UTIL_CONSTANTS_TIMINGS_HPP
#define LSHCORE_UTIL_CONSTANTS_TIMINGS_HPP

#include <stdint.h>

namespace constants::timings
{
#ifndef CONFIG_ACTUATOR_DEBOUNCE_TIME_MS
    static constexpr const uint8_t ACTUATOR_DEBOUNCE_TIME_MS = 100U; //!< Default Actuator debounce (min time between switches)
#else
    static constexpr const uint8_t ACTUATOR_DEBOUNCE_TIME_MS = CONFIG_ACTUATOR_DEBOUNCE_TIME_MS; //!< Actuator debounce (min time between switches)
#endif // CONFIG_ACTUATOR_DEBOUNCE_TIME_MS

#ifndef CONFIG_CLICKABLE_DEBOUNCE_TIME_MS
    static constexpr const uint8_t CLICKABLE_DEBOUNCE_TIME_MS = 20U; //!< Default Clickable (button) debounce
#else
    static constexpr const uint8_t CLICKABLE_DEBOUNCE_TIME_MS = CONFIG_CLICKABLE_DEBOUNCE_TIME_MS; //!< Clickable (button) debounce
#endif // CONFIG_CLICKABLE_DEBOUNCE_TIME_MS

#ifndef CONFIG_CLICKABLE_LONG_CLICK_TIME_MS
    static constexpr const uint16_t CLICKABLE_LONG_CLICK_TIME_MS = 400U; //!< Default Clickable (button) long click time
#else
    static constexpr const uint16_t CLICKABLE_LONG_CLICK_TIME_MS = CONFIG_CLICKABLE_LONG_CLICK_TIME_MS; //!< Clickable (button) long click time
#endif // CONFIG_CLICKABLE_LONG_CLICK_TIME_MS

#ifndef CONFIG_CLICKABLE_SUPER_LONG_CLICK_TIME_MS
    static constexpr const uint16_t CLICKABLE_SUPER_LONG_CLICK_TIME_MS = 1000U; //!< Default Clickable (button) long click time
#else
    static constexpr const uint16_t CLICKABLE_SUPER_LONG_CLICK_TIME_MS = CONFIG_CLICKABLE_SUPER_LONG_CLICK_TIME_MS; //!< Clickable (button) long click time
#endif // CONFIG_CLICKABLE_SUPER_LONG_CLICK_TIME_MS

    /**
     * @brief Time in ms to wait before sending state to ESP after a json has been received.
     *
     * Needed to avoid double "send state" after a burst of "apply single actuator state" has been received.
     *
     */
#ifndef CONFIG_DELAY_AFTER_RECEIVE_MS
    static constexpr const uint8_t DELAY_AFTER_RECEIVE_MS = 50U;
#else
    static constexpr const uint8_t DELAY_AFTER_RECEIVE_MS = CONFIG_DELAY_AFTER_RECEIVE_MS;
#endif // CONFIG_DELAY_AFTER_RECEIVE_MS

/**
 * @brief Time between two consecutive ESP connection checks
 *
 */
#ifndef CONFIG_CONNECTION_CHECK_INTERVAL_MS
    static constexpr const uint16_t CONNECTION_CHECK_INTERVAL_MS = 1000U;
#else
    static constexpr const uint16_t CONNECTION_CHECK_INTERVAL_MS = CONFIG_CONNECTION_CHECK_INTERVAL_MS;
#endif // CONFIG_CONNECTION_CHECK_INTERVAL_MS

#ifndef CONFIG_NETWORK_CLICK_CHECK_INTERVAL_MS
    static constexpr const uint8_t NETWORK_CLICK_CHECK_INTERVAL_MS = 50U; //!< Default Network click check interval, in ms.
#else
    static constexpr const uint8_t NETWORK_CLICK_CHECK_INTERVAL_MS = CONFIG_NETWORK_CLICK_CHECK_INTERVAL_MS; //!< Network click check interval, in ms.
#endif // CONFIG_NETWORK_CLICK_CHECK_INTERVAL_MS

#ifndef CONFIG_ACTUATORS_AUTO_OFF_CHECK_INTERVAL_MS
    static constexpr const uint16_t ACTUATORS_AUTO_OFF_CHECK_INTERVAL_MS = 1000U; //!< Default Actuator auto off timer check interval, in ms.
#else
    static constexpr const uint16_t ACTUATORS_AUTO_OFF_CHECK_INTERVAL_MS = CONFIG_ACTUATORS_AUTO_OFF_CHECK_INTERVAL_MS; //!< Actuator auto off timer check interval, in ms.
#endif // CONFIG_ACTUATORS_AUTO_OFF_CHECK_INTERVAL_MS

#ifndef CONFIG_LCNB_TIMEOUT_MS
    static constexpr const uint16_t LCNB_TIMEOUT_MS = 1000U; //!< Default Long clicked network clickable (button) timeout
#else
    static constexpr const uint16_t LCNB_TIMEOUT_MS = CONFIG_LCNB_TIMEOUT_MS; //!< Long clicked network clickable (button) timeout
#endif // CONFIG_LCNB_TIMEOUT_MS

#ifdef CONFIG_LSH_BENCH
#ifndef CONFIG_BENCH_ITERATIONS
    static constexpr const uint32_t BENCH_ITERATIONS = 1000000U; //!< Default Benchmark loop count
#else
    static constexpr const uint32_t BENCH_ITERATIONS = CONFIG_BENCH_ITERATIONS; //!< Benchmark loop count
#endif // CONFIG_BENCH_ITERATIONS
#endif // CONFIG_LSH_BENCH
} // namespace constants::timings

#endif // LSHCORE_UTIL_CONSTANTS_TIMINGS_HPP