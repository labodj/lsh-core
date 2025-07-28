/**
 * @file    memory.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares functions to check for free memory on AVR devices.
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

#ifndef LSHCORE_UTIL_DEBUG_MEMORY_HPP
#define LSHCORE_UTIL_DEBUG_MEMORY_HPP

#if (ARDUINO >= 100)
#include <Arduino.h>
#else
#include <WProgram.h>
#endif // (ARDUINO >= 100)

// MemoryFree library based on code posted here:
// https://forum.arduino.cc/index.php?topic=27536.msg204024#msg204024
// Extended by Matthew Murdoch to include walking of the free list.

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

    auto freeMemory() -> size_t;

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // LSHCORE_UTIL_DEBUG_MEMORY_HPP
