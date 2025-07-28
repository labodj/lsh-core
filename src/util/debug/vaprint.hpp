/**
 * @file    vaprint.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares a variadic print utility for streamlined debugging output.
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

#ifndef LSHCORE_UTIL_DEBUG_VAPRINT_HPP
#define LSHCORE_UTIL_DEBUG_VAPRINT_HPP

#include <stdint.h>

#include "internal/user_config_bridge.hpp"

namespace VaPrint
{

  extern uint8_t base;
  extern uint8_t prec;

  void setBase(uint8_t b);
  void setPrec(uint8_t p);

  void print(const String &str);
  void print(char c);
  void print(char *str);
  void print(const char *str);
  void print(const __FlashStringHelper *str);
  void print(float f);
  void print(double d);
  void println();

  template <typename T>
  static inline void print(T value)
  {
    CONFIG_DEBUG_SERIAL->print(value, base);
  }

  template <typename T, typename... Rest>
  static inline void print(T value, Rest... rest)
  {
    print(value);
    print(rest...);
  }

  template <typename T, typename... Rest>
  __attribute__((always_inline)) static inline void println(T value, Rest... rest)
  {
    print(value);
    println(rest...);
  }

} // namespace VaPrint

#endif // LSHCORE_UTIL_DEBUG_VAPRINT_HPP