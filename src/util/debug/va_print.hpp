/**
 * @file    va_print.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares a variadic print utility for streamlined debugging output.
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

#ifndef LSH_CORE_UTIL_DEBUG_VA_PRINT_HPP
#define LSH_CORE_UTIL_DEBUG_VA_PRINT_HPP

#include <stdint.h>

#include "internal/user_config_bridge.hpp"

namespace VaPrint
{
extern uint8_t base;  //!< Numeric base used when printing integral values.
extern uint8_t prec;  //!< Decimal precision used when printing floating-point values.

void setBase(uint8_t baseToSet);              // Set the numeric base used for integral debug prints.
void setPrec(uint8_t precisionToSet);         // Set the decimal precision used for floating-point debug prints.
void print(const String &text);               // Print an Arduino `String` without appending a newline.
void print(char character);                   // Print a single character without appending a newline.
void print(char *text);                       // Print a mutable C string without appending a newline.
void print(const char *text);                 // Print an immutable C string without appending a newline.
void print(const __FlashStringHelper *text);  // Print a PROGMEM string without appending a newline.
void print(float value);                      // Print a `float` without appending a newline.
void print(double value);                     // Print a `double` without appending a newline.
void println();                               // Print only a newline.

/**
 * @brief Print any value supported by `HardwareSerial::print()`.
 *
 * Integral values use the global `base`, while other types follow the normal
 * Arduino `Print` semantics.
 */
template <typename T> static inline void print(T value)
{
    CONFIG_DEBUG_SERIAL->print(value, base);
}

/**
 * @brief Print multiple values in sequence without appending a final newline.
 */
template <typename T, typename... Rest> static inline void print(T value, Rest... rest)
{
    print(value);
    print(rest...);
}

/**
 * @brief Print multiple values and terminate the line once at the end.
 */
template <typename T, typename... Rest> __attribute__((always_inline)) static inline void println(T value, Rest... rest)
{
    print(value);
    println(rest...);
}

}  // namespace VaPrint

#endif  // LSH_CORE_UTIL_DEBUG_VA_PRINT_HPP
