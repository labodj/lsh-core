/**
 * @file    avr_fast_io.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Typed helpers for AVR fast I/O access without Arduino macro casts.
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

#ifndef LSH_CORE_INTERNAL_AVR_FAST_IO_HPP
#define LSH_CORE_INTERNAL_AVR_FAST_IO_HPP

#if (ARDUINO >= 100)
#include <Arduino.h>
#else
#include <WProgram.h>
#endif  // (ARDUINO >= 100)

#include <stdint.h>

namespace lsh
{
namespace core
{
namespace avr
{
[[nodiscard]] inline auto readPinBitMask(uint8_t pin) noexcept -> uint8_t
{
    return pgm_read_byte(digital_pin_to_bit_mask_PGM + pin);
}

[[nodiscard]] inline auto readPinPortIndex(uint8_t pin) noexcept -> uint8_t
{
    return pgm_read_byte(digital_pin_to_port_PGM + pin);
}

[[nodiscard]] inline auto inputRegister(uint8_t port) noexcept -> volatile const uint8_t *
{
    // NOLINTNEXTLINE(performance-no-int-to-ptr): AVR port lookup tables store raw MMIO addresses in PROGMEM.
    return reinterpret_cast<volatile const uint8_t *>(pgm_read_word(port_to_input_PGM + port));
}

[[nodiscard]] inline auto outputRegister(uint8_t port) noexcept -> volatile uint8_t *
{
    // NOLINTNEXTLINE(performance-no-int-to-ptr): AVR port lookup tables store raw MMIO addresses in PROGMEM.
    return reinterpret_cast<volatile uint8_t *>(pgm_read_word(port_to_output_PGM + port));
}

[[nodiscard]] inline auto modeRegister(uint8_t port) noexcept -> volatile uint8_t *
{
    // NOLINTNEXTLINE(performance-no-int-to-ptr): AVR port lookup tables store raw MMIO addresses in PROGMEM.
    return reinterpret_cast<volatile uint8_t *>(pgm_read_word(port_to_mode_PGM + port));
}

[[nodiscard]] inline auto inputRegisterForPin(uint8_t pin) noexcept -> volatile const uint8_t *
{
    return inputRegister(readPinPortIndex(pin));
}

[[nodiscard]] inline auto outputRegisterForPin(uint8_t pin) noexcept -> volatile uint8_t *
{
    return outputRegister(readPinPortIndex(pin));
}

[[nodiscard]] inline auto modeRegisterForPin(uint8_t pin) noexcept -> volatile uint8_t *
{
    return modeRegister(readPinPortIndex(pin));
}
}  // namespace avr
}  // namespace core
}  // namespace lsh

#endif  // LSH_CORE_INTERNAL_AVR_FAST_IO_HPP
