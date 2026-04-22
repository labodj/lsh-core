/**
 * @file    pin_tag.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Compile-time pin tag used to route peripheral construction through constant pin paths.
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

#ifndef LSH_CORE_INTERNAL_PIN_TAG_HPP
#define LSH_CORE_INTERNAL_PIN_TAG_HPP

#include <stdint.h>

namespace lsh
{
namespace core
{
/**
 * @brief Type-level wrapper around one Arduino pin number.
 *
 * The public `LSH_*` device macros instantiate peripherals from compile-time
 * pin constants. Carrying the pin as a type lets the fast-I/O layer choose a
 * constexpr mapping path on supported AVR boards while keeping the runtime
 * constructors available as a fallback for dynamic callers.
 *
 * @tparam PinNumber Arduino pin number encoded in the type.
 */
template <uint8_t PinNumber> struct PinTag
{
    static constexpr uint8_t value = PinNumber;  //!< Pin number encoded by the tag and propagated through overload resolution.
};
}  // namespace core
}  // namespace lsh

#endif  // LSH_CORE_INTERNAL_PIN_TAG_HPP
