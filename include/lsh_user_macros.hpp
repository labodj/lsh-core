/**
 * @file    lsh_user_macros.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Provides the user-facing macros (LSH_ACTUATOR, LSH_BUTTON, LSH_INDICATOR) for device configuration.
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

#ifndef LSH_CORE_LSH_USER_MACROS_HPP
#define LSH_CORE_LSH_USER_MACROS_HPP

// This file provides the generated-profile object macros. Each `pin` argument
// is expected to be a compile-time constant so it can flow through `PinTag` and
// unlock the constexpr AVR mapping path when the target board supports it.

#include "config/configurator.hpp"
#include "internal/pin_tag.hpp"
#include "internal/user_config_bridge.hpp"
#include "peripherals/input/clickable.hpp"
#include "peripherals/output/actuator.hpp"
#include "peripherals/output/indicator.hpp"
#include "util/debug/debug.hpp"

/**
 * @brief Defines an Actuator object bound to one compile-time pin.
 * @param var_name The name of the variable to declare (e.g., rel0).
 * @param pin The hardware pin for the actuator.
 */
#define LSH_ACTUATOR(var_name, pin) Actuator var_name(::lsh::core::PinTag<(pin)>{})

/**
 * @brief Defines a Clickable object bound to one compile-time pin.
 * @param var_name The name of the variable to declare (e.g., btn0).
 * @param pin The hardware pin for the clickable.
 */
#define LSH_BUTTON(var_name, pin) Clickable var_name(::lsh::core::PinTag<(pin)>{})

/**
 * @brief Defines an IndicatorLight. Does not require an ID.
 * @param var_name The name of the variable to declare (e.g., light0).
 * @param pin The hardware pin for the indicator.
 */
#define LSH_INDICATOR(var_name, pin) Indicator var_name(::lsh::core::PinTag<(pin)>{})

#endif  // LSH_CORE_LSH_USER_MACROS_HPP
