/**
 * @file    j1_config.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Compile-time constants for the J1 profile in the multi-device lsh-core example.
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

#ifndef J1_CONFIG_HPP
#define J1_CONFIG_HPP

// --- 1. CONTRACT: Specify which hardware library to use ---
#define LSH_HARDWARE_INCLUDE <Controllino.h>

// --- 2. CONTRACT: Define the constants for this build ---
#define LSH_DEVICE_NAME "j1"
#define LSH_MAX_CLICKABLES 10
#define LSH_MAX_ACTUATORS 9
#define LSH_MAX_INDICATORS 1
#define LSH_COM_SERIAL &Serial2
#define LSH_DEBUG_SERIAL &Serial

#endif  // J1_CONFIG_HPP
