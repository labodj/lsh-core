/**
 * @file    user_config_bridge.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Internal bridge that imports user-defined macros into the library's scope.
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

#ifndef LSHCORE_INTERNAL_USER_CONFIG_BRIDGE_HPP
#define LSHCORE_INTERNAL_USER_CONFIG_BRIDGE_HPP

#include "lsh_user_config.hpp"

#include "internal/lsh_config_types.hpp"

static constexpr const char *CONFIG_DEVICE_NAME = LSH_DEVICE_NAME;
static constexpr uint8_t CONFIG_MAX_CLICKABLES = LSH_MAX_CLICKABLES;
static constexpr uint8_t CONFIG_MAX_ACTUATORS = LSH_MAX_ACTUATORS;
static constexpr uint8_t CONFIG_MAX_INDICATORS = LSH_MAX_INDICATORS;
static constexpr HardwareSerial *const CONFIG_COM_SERIAL = LSH_COM_SERIAL;     //!< Serial attached to ESP needed for EspCom
static constexpr HardwareSerial *const CONFIG_DEBUG_SERIAL = LSH_DEBUG_SERIAL; //!< Serial attached to PC needed for debugging

#endif // LSHCORE_INTERNAL_USER_CONFIG_BRIDGE_HPP