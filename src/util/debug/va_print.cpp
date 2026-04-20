/**
 * @file    va_print.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the variadic print utility.
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

#include "util/debug/va_print.hpp"

#include "internal/user_config_bridge.hpp"

namespace VaPrint
{
/** @brief Numeric base passed to `HardwareSerial::print()` when printing integral debug values. */
uint8_t base = 10U;
/** @brief Decimal precision passed to `HardwareSerial::print()` when printing floating-point debug values. */
uint8_t prec = 2U;

/**
 * @brief Set the numeric base used for integral debug output.
 *
 * @param baseToSet New base passed to `HardwareSerial::print()` for integral values.
 */
void setBase(uint8_t baseToSet)
{
    base = baseToSet;
}

/**
 * @brief Set the decimal precision used for floating-point debug output.
 *
 * @param precisionToSet Number of decimals to print for floating-point values.
 */
void setPrec(uint8_t precisionToSet)
{
    prec = precisionToSet;
}

/**
 * @brief Print an Arduino `String` without appending a newline.
 *
 * @param text String to print.
 */
void print(const String &text)
{
    CONFIG_DEBUG_SERIAL->print(text);
}

/**
 * @brief Print a single character without appending a newline.
 *
 * @param character Character to print.
 */
void print(char character)
{
    CONFIG_DEBUG_SERIAL->print(character);
}

/**
 * @brief Print a mutable C string without appending a newline.
 *
 * @param text String to print.
 */
void print(char *const text)
{
    CONFIG_DEBUG_SERIAL->print(text);
}

/**
 * @brief Print an immutable C string without appending a newline.
 *
 * @param text String to print.
 */
void print(const char *const text)
{
    CONFIG_DEBUG_SERIAL->print(text);
}

/**
 * @brief Print a PROGMEM string without appending a newline.
 *
 * @param text PROGMEM string to print.
 */
void print(const __FlashStringHelper *const text)
{
    CONFIG_DEBUG_SERIAL->print(text);
}

/**
 * @brief Print a `float` without appending a newline.
 *
 * @param value Floating-point value to print.
 */
void print(float value)
{
    print(static_cast<double>(value));
}

/**
 * @brief Print a `double` without appending a newline.
 *
 * @param value Floating-point value to print.
 */
void print(double value)
{
    CONFIG_DEBUG_SERIAL->print(value, prec);
}

/**
 * @brief Print only a newline.
 */
void println()
{
    CONFIG_DEBUG_SERIAL->println();
}
}  // namespace VaPrint
