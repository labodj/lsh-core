/**
 * @file    j2_config.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Compile-time constants for the J2 profile in the multi-device lsh-core example.
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

#ifndef J2_CONFIG_HPP
#define J2_CONFIG_HPP

// --- 1. CONTRACT: Specify which hardware library to use ---
#define LSH_HARDWARE_INCLUDE <Controllino.h>

// --- 2. CONTRACT: Define the constants for this build ---
#define LSH_DEVICE_NAME "j2"
#define LSH_MAX_CLICKABLES 10
#define LSH_MAX_ACTUATORS 8
#define LSH_MAX_INDICATORS 3
#define LSH_MAX_CLICKABLE_ID 12
#define LSH_MAX_ACTUATOR_ID 10
// These totals are derived from the real configure() body for this profile.
// Keeping them explicit matters on AVR because the fallback reserves the full
// worst-case pool budget in static RAM. These numbers instead describe the
// real link counts that `configure()` appends for this device.
#define LSH_MAX_SHORT_CLICK_ACTUATOR_LINKS 10
#define LSH_MAX_LONG_CLICK_ACTUATOR_LINKS 12
// Zero is intentional here: this profile configures no local super-long links,
// so the firmware keeps only the clamped minimal storage slot for that pool.
#define LSH_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS 0
#define LSH_MAX_INDICATOR_ACTUATOR_LINKS 3
#define LSH_MAX_CLICKABLE_TIMING_OVERRIDES 0
#define LSH_MAX_AUTO_OFF_ACTUATORS 2
#define LSH_MAX_ACTIVE_NETWORK_CLICKS 2
#define LSH_COM_SERIAL &Serial2
#define LSH_DEBUG_SERIAL &Serial

#endif  // J2_CONFIG_HPP
