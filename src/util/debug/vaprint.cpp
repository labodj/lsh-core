/**
 * @file    vaprint.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the variadic print utility.
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

#include "util/debug/vaprint.hpp"

#include "internal/user_config_bridge.hpp"

namespace VaPrint
{

    uint8_t base = 10U;
    uint8_t prec = 2U;

    void setBase(uint8_t b) { base = b; }
    void setPrec(uint8_t p) { prec = p; }

    void print(const String &str) { CONFIG_DEBUG_SERIAL->print(str); }
    void print(char c) { CONFIG_DEBUG_SERIAL->print(c); }
    void print(char *const str) { CONFIG_DEBUG_SERIAL->print(str); }
    void print(const char *const str) { CONFIG_DEBUG_SERIAL->print(str); }
    void print(const __FlashStringHelper *const str) { CONFIG_DEBUG_SERIAL->print(str); }
    void print(float f) { print(static_cast<double>(f)); }
    void print(double d) { CONFIG_DEBUG_SERIAL->print(d, prec); }
    void println() { CONFIG_DEBUG_SERIAL->println(); }
} // namespace VaPrint
