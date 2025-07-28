/**
 * @file    lsh_config_types.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Internal bridge to include the user-specified hardware library (e.g., Arduino.h).
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

#ifndef LSHCORE_INTERNAL_LSH_CONFIG_TYPES_HPP
#define LSHCORE_INTERNAL_LSH_CONFIG_TYPES_HPP

// This file acts as a bridge for hardware types.
// Its sole responsibility is to include the hardware library
// specified by the user via the LSH_HARDWARE_INCLUDE macro.
// This makes types like HardwareSerial, uint8_t, etc., available
// to all other headers in the LSH library.

#ifdef LSH_HARDWARE_INCLUDE
#include LSH_HARDWARE_INCLUDE
#else
// If the user forgets to define the macro, include Arduino.h
// as a safe fallback to prevent cryptic compilation errors.
#include <Arduino.h>
#endif

#endif // LSHCORE_INTERNAL_LSH_CONFIG_TYPES_HPP