/**
 * @file    lsh_user_macros.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Provides the user-facing macros (LSH_ACTUATOR, LSH_BUTTON, LSH_INDICATOR) for device configuration.
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

#ifndef LSHCORE_LSH_USER_MACROS_HPP
#define LSHCORE_LSH_USER_MACROS_HPP

// This file provides the user-facing macros. It needs the definitions of the
// classes it instantiates and the bridge to the user's configuration.

#include "config/configurator.hpp"
#include "internal/user_config_bridge.hpp"
#include "peripherals/input/clickable.hpp"
#include "peripherals/output/actuator.hpp"
#include "peripherals/output/indicator.hpp"
#include "util/debug/debug.hpp"

/**
 * @brief Defines an Actuator with a compile-time check to ensure its ID is not 0.
 * @param var_name The name of the variable to declare (e.g., rel0).
 * @param pin The hardware pin for the actuator.
 * @param id The unique numeric ID for the actuator. MUST be > 0.
 */
#define LSH_ACTUATOR(var_name, pin, id)                                                           \
    static_assert((id) > 0, "Actuator ID must be > 0. Please use positive IDs starting from 1."); \
    Actuator var_name((pin), (id))

/**
 * @brief Defines a Clickable (Button) with a compile-time check to ensure its ID is not 0.
 * @param var_name The name of the variable to declare (e.g., btn0).
 * @param pin The hardware pin for the clickable.
 * @param id The unique numeric ID for the clickable. MUST be > 0.
 */
#define LSH_BUTTON(var_name, pin, id)                                                                  \
    static_assert((id) > 0, "Button ID cannot must be > 0. Please use positive IDs starting from 1."); \
    Clickable var_name((pin), (id))

/**
 * @brief Defines an IndicatorLight. Does not require an ID.
 * @param var_name The name of the variable to declare (e.g., light0).
 * @param pin The hardware pin for the indicator.
 */
#define LSH_INDICATOR(var_name, pin) \
    Indicator var_name((pin))

#endif // LSHCORE_LSH_USER_MACROS_HPP
