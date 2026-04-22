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

#include "internal/pin_tag.hpp"

namespace lsh
{
namespace core
{
namespace avr
{
/**
 * @brief Resolved fast-input binding for one Arduino pin.
 *
 * This is the small runtime payload that survives after the pin lookup path
 * has finished. The binding may come from the traditional PROGMEM Arduino
 * tables or from the constexpr Mega mapping used by `PinTag`.
 */
struct FastInputPinBinding
{
    uint8_t mask = 0U;                          //!< Final bit mask for the pin inside the AVR port.
    volatile const uint8_t *pinPort = nullptr;  //!< Final AVR input register used to sample the pin state.
};

/**
 * @brief Resolved fast-output binding for one Arduino pin.
 *
 * Output peripherals need both the target port and the DDR register used to
 * switch the pin direction to OUTPUT during construction.
 */
struct FastOutputPinBinding
{
    uint8_t mask = 0U;                     //!< Final bit mask for the pin inside the AVR port.
    volatile uint8_t *pinPort = nullptr;   //!< Final AVR output register used to drive the pin.
    volatile uint8_t *modePort = nullptr;  //!< Final AVR DDR register used to configure the pin direction.
};

/**
 * @brief Compile-time descriptor for one fully resolved AVR pin binding.
 *
 * The addresses are stored as raw MMIO addresses so the descriptor remains a
 * trivially constexpr object on AVR, then converted to typed pointers only at
 * the last moment when a binding is materialized.
 */
struct StaticPinDescriptor
{
    uint8_t mask = 0U;            //!< Final bit mask for the pin inside the port.
    uint16_t inputAddress = 0U;   //!< Raw MMIO address of the input register.
    uint16_t outputAddress = 0U;  //!< Raw MMIO address of the output register.
    uint16_t modeAddress = 0U;    //!< Raw MMIO address of the DDR register.
};

namespace detail
{
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
#define LSH_CORE_MEGA_IO_PIN_DESC(input_io_addr, output_io_addr, mode_io_addr, bit_index)                                \
    StaticPinDescriptor                                                                                                  \
    {                                                                                                                    \
        static_cast<uint8_t>(_BV(bit_index)), static_cast<uint16_t>((input_io_addr) + __SFR_OFFSET),                     \
            static_cast<uint16_t>((output_io_addr) + __SFR_OFFSET), static_cast<uint16_t>((mode_io_addr) + __SFR_OFFSET) \
    }

#define LSH_CORE_MEGA_MEM_PIN_DESC(input_mem_addr, output_mem_addr, mode_mem_addr, bit_index)                                \
    StaticPinDescriptor                                                                                                      \
    {                                                                                                                        \
        static_cast<uint8_t>(_BV(bit_index)), static_cast<uint16_t>(input_mem_addr), static_cast<uint16_t>(output_mem_addr), \
            static_cast<uint16_t>(mode_mem_addr)                                                                             \
    }

/**
 * @brief Canonical Arduino Mega 1280/2560 digital and analog pin map.
 *
 * The table mirrors the standard Arduino core mapping for pins `0..69`, so a
 * `PinTag<P>` can resolve directly to its AVR registers without consulting the
 * Arduino PROGMEM lookup tables. Ports in the low AVR I/O space (`A..G`) are
 * stored as memory-mapped addresses (`io + __SFR_OFFSET`), while extended MMIO
 * ports (`H..L`) already use their final memory address.
 */
inline constexpr StaticPinDescriptor kMegaPinDescriptors[] = {
    LSH_CORE_MEGA_IO_PIN_DESC(0x0CU, 0x0EU, 0x0DU, 0),      // 0  -> PORTE0
    LSH_CORE_MEGA_IO_PIN_DESC(0x0CU, 0x0EU, 0x0DU, 1),      // 1  -> PORTE1
    LSH_CORE_MEGA_IO_PIN_DESC(0x0CU, 0x0EU, 0x0DU, 4),      // 2  -> PORTE4
    LSH_CORE_MEGA_IO_PIN_DESC(0x0CU, 0x0EU, 0x0DU, 5),      // 3  -> PORTE5
    LSH_CORE_MEGA_IO_PIN_DESC(0x12U, 0x14U, 0x13U, 5),      // 4  -> PORTG5
    LSH_CORE_MEGA_IO_PIN_DESC(0x0CU, 0x0EU, 0x0DU, 3),      // 5  -> PORTE3
    LSH_CORE_MEGA_MEM_PIN_DESC(0x100U, 0x102U, 0x101U, 3),  // 6  -> PORTH3
    LSH_CORE_MEGA_MEM_PIN_DESC(0x100U, 0x102U, 0x101U, 4),  // 7  -> PORTH4
    LSH_CORE_MEGA_MEM_PIN_DESC(0x100U, 0x102U, 0x101U, 5),  // 8  -> PORTH5
    LSH_CORE_MEGA_MEM_PIN_DESC(0x100U, 0x102U, 0x101U, 6),  // 9  -> PORTH6
    LSH_CORE_MEGA_IO_PIN_DESC(0x03U, 0x05U, 0x04U, 4),      // 10 -> PORTB4
    LSH_CORE_MEGA_IO_PIN_DESC(0x03U, 0x05U, 0x04U, 5),      // 11 -> PORTB5
    LSH_CORE_MEGA_IO_PIN_DESC(0x03U, 0x05U, 0x04U, 6),      // 12 -> PORTB6
    LSH_CORE_MEGA_IO_PIN_DESC(0x03U, 0x05U, 0x04U, 7),      // 13 -> PORTB7
    LSH_CORE_MEGA_MEM_PIN_DESC(0x103U, 0x105U, 0x104U, 1),  // 14 -> PORTJ1
    LSH_CORE_MEGA_MEM_PIN_DESC(0x103U, 0x105U, 0x104U, 0),  // 15 -> PORTJ0
    LSH_CORE_MEGA_MEM_PIN_DESC(0x100U, 0x102U, 0x101U, 1),  // 16 -> PORTH1
    LSH_CORE_MEGA_MEM_PIN_DESC(0x100U, 0x102U, 0x101U, 0),  // 17 -> PORTH0
    LSH_CORE_MEGA_IO_PIN_DESC(0x09U, 0x0BU, 0x0AU, 3),      // 18 -> PORTD3
    LSH_CORE_MEGA_IO_PIN_DESC(0x09U, 0x0BU, 0x0AU, 2),      // 19 -> PORTD2
    LSH_CORE_MEGA_IO_PIN_DESC(0x09U, 0x0BU, 0x0AU, 1),      // 20 -> PORTD1
    LSH_CORE_MEGA_IO_PIN_DESC(0x09U, 0x0BU, 0x0AU, 0),      // 21 -> PORTD0
    LSH_CORE_MEGA_IO_PIN_DESC(0x00U, 0x02U, 0x01U, 0),      // 22 -> PORTA0
    LSH_CORE_MEGA_IO_PIN_DESC(0x00U, 0x02U, 0x01U, 1),      // 23 -> PORTA1
    LSH_CORE_MEGA_IO_PIN_DESC(0x00U, 0x02U, 0x01U, 2),      // 24 -> PORTA2
    LSH_CORE_MEGA_IO_PIN_DESC(0x00U, 0x02U, 0x01U, 3),      // 25 -> PORTA3
    LSH_CORE_MEGA_IO_PIN_DESC(0x00U, 0x02U, 0x01U, 4),      // 26 -> PORTA4
    LSH_CORE_MEGA_IO_PIN_DESC(0x00U, 0x02U, 0x01U, 5),      // 27 -> PORTA5
    LSH_CORE_MEGA_IO_PIN_DESC(0x00U, 0x02U, 0x01U, 6),      // 28 -> PORTA6
    LSH_CORE_MEGA_IO_PIN_DESC(0x00U, 0x02U, 0x01U, 7),      // 29 -> PORTA7
    LSH_CORE_MEGA_IO_PIN_DESC(0x06U, 0x08U, 0x07U, 7),      // 30 -> PORTC7
    LSH_CORE_MEGA_IO_PIN_DESC(0x06U, 0x08U, 0x07U, 6),      // 31 -> PORTC6
    LSH_CORE_MEGA_IO_PIN_DESC(0x06U, 0x08U, 0x07U, 5),      // 32 -> PORTC5
    LSH_CORE_MEGA_IO_PIN_DESC(0x06U, 0x08U, 0x07U, 4),      // 33 -> PORTC4
    LSH_CORE_MEGA_IO_PIN_DESC(0x06U, 0x08U, 0x07U, 3),      // 34 -> PORTC3
    LSH_CORE_MEGA_IO_PIN_DESC(0x06U, 0x08U, 0x07U, 2),      // 35 -> PORTC2
    LSH_CORE_MEGA_IO_PIN_DESC(0x06U, 0x08U, 0x07U, 1),      // 36 -> PORTC1
    LSH_CORE_MEGA_IO_PIN_DESC(0x06U, 0x08U, 0x07U, 0),      // 37 -> PORTC0
    LSH_CORE_MEGA_IO_PIN_DESC(0x09U, 0x0BU, 0x0AU, 7),      // 38 -> PORTD7
    LSH_CORE_MEGA_IO_PIN_DESC(0x12U, 0x14U, 0x13U, 2),      // 39 -> PORTG2
    LSH_CORE_MEGA_IO_PIN_DESC(0x12U, 0x14U, 0x13U, 1),      // 40 -> PORTG1
    LSH_CORE_MEGA_IO_PIN_DESC(0x12U, 0x14U, 0x13U, 0),      // 41 -> PORTG0
    LSH_CORE_MEGA_MEM_PIN_DESC(0x109U, 0x10BU, 0x10AU, 7),  // 42 -> PORTL7
    LSH_CORE_MEGA_MEM_PIN_DESC(0x109U, 0x10BU, 0x10AU, 6),  // 43 -> PORTL6
    LSH_CORE_MEGA_MEM_PIN_DESC(0x109U, 0x10BU, 0x10AU, 5),  // 44 -> PORTL5
    LSH_CORE_MEGA_MEM_PIN_DESC(0x109U, 0x10BU, 0x10AU, 4),  // 45 -> PORTL4
    LSH_CORE_MEGA_MEM_PIN_DESC(0x109U, 0x10BU, 0x10AU, 3),  // 46 -> PORTL3
    LSH_CORE_MEGA_MEM_PIN_DESC(0x109U, 0x10BU, 0x10AU, 2),  // 47 -> PORTL2
    LSH_CORE_MEGA_MEM_PIN_DESC(0x109U, 0x10BU, 0x10AU, 1),  // 48 -> PORTL1
    LSH_CORE_MEGA_MEM_PIN_DESC(0x109U, 0x10BU, 0x10AU, 0),  // 49 -> PORTL0
    LSH_CORE_MEGA_IO_PIN_DESC(0x03U, 0x05U, 0x04U, 3),      // 50 -> PORTB3
    LSH_CORE_MEGA_IO_PIN_DESC(0x03U, 0x05U, 0x04U, 2),      // 51 -> PORTB2
    LSH_CORE_MEGA_IO_PIN_DESC(0x03U, 0x05U, 0x04U, 1),      // 52 -> PORTB1
    LSH_CORE_MEGA_IO_PIN_DESC(0x03U, 0x05U, 0x04U, 0),      // 53 -> PORTB0
    LSH_CORE_MEGA_IO_PIN_DESC(0x0FU, 0x11U, 0x10U, 0),      // 54 -> PORTF0
    LSH_CORE_MEGA_IO_PIN_DESC(0x0FU, 0x11U, 0x10U, 1),      // 55 -> PORTF1
    LSH_CORE_MEGA_IO_PIN_DESC(0x0FU, 0x11U, 0x10U, 2),      // 56 -> PORTF2
    LSH_CORE_MEGA_IO_PIN_DESC(0x0FU, 0x11U, 0x10U, 3),      // 57 -> PORTF3
    LSH_CORE_MEGA_IO_PIN_DESC(0x0FU, 0x11U, 0x10U, 4),      // 58 -> PORTF4
    LSH_CORE_MEGA_IO_PIN_DESC(0x0FU, 0x11U, 0x10U, 5),      // 59 -> PORTF5
    LSH_CORE_MEGA_IO_PIN_DESC(0x0FU, 0x11U, 0x10U, 6),      // 60 -> PORTF6
    LSH_CORE_MEGA_IO_PIN_DESC(0x0FU, 0x11U, 0x10U, 7),      // 61 -> PORTF7
    LSH_CORE_MEGA_MEM_PIN_DESC(0x106U, 0x108U, 0x107U, 0),  // 62 -> PORTK0
    LSH_CORE_MEGA_MEM_PIN_DESC(0x106U, 0x108U, 0x107U, 1),  // 63 -> PORTK1
    LSH_CORE_MEGA_MEM_PIN_DESC(0x106U, 0x108U, 0x107U, 2),  // 64 -> PORTK2
    LSH_CORE_MEGA_MEM_PIN_DESC(0x106U, 0x108U, 0x107U, 3),  // 65 -> PORTK3
    LSH_CORE_MEGA_MEM_PIN_DESC(0x106U, 0x108U, 0x107U, 4),  // 66 -> PORTK4
    LSH_CORE_MEGA_MEM_PIN_DESC(0x106U, 0x108U, 0x107U, 5),  // 67 -> PORTK5
    LSH_CORE_MEGA_MEM_PIN_DESC(0x106U, 0x108U, 0x107U, 6),  // 68 -> PORTK6
    LSH_CORE_MEGA_MEM_PIN_DESC(0x106U, 0x108U, 0x107U, 7),  // 69 -> PORTK7
};

#undef LSH_CORE_MEGA_IO_PIN_DESC
#undef LSH_CORE_MEGA_MEM_PIN_DESC

/**
 * @brief Returns the constexpr Mega descriptor for one Arduino pin number.
 *
 * Out-of-range pins deliberately collapse to the zero descriptor so the
 * caller can transparently fall back to the traditional Arduino table lookup
 * path without duplicating range checks.
 */
[[nodiscard]] constexpr auto megaPinDescriptor(uint8_t pin) noexcept -> StaticPinDescriptor
{
    return (pin < (sizeof(kMegaPinDescriptors) / sizeof(kMegaPinDescriptors[0]))) ? kMegaPinDescriptors[pin] : StaticPinDescriptor{};
}

/**
 * @brief Tells whether the constexpr Mega mapping knows the given Arduino pin.
 */
[[nodiscard]] constexpr auto hasConstexprMegaBinding(uint8_t pin) noexcept -> bool
{
    return megaPinDescriptor(pin).mask != 0U;
}
#endif  // defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)

/**
 * @brief Rebuilds a read-only AVR register pointer from a raw MMIO address.
 */
[[nodiscard]] inline auto inputRegisterFromAddress(uint16_t address) noexcept -> volatile const uint8_t *
{
    // NOLINTNEXTLINE(performance-no-int-to-ptr): AVR descriptors intentionally store raw MMIO addresses.
    return reinterpret_cast<volatile const uint8_t *>(address);
}

/**
 * @brief Rebuilds a writable AVR register pointer from a raw MMIO address.
 */
[[nodiscard]] inline auto outputRegisterFromAddress(uint16_t address) noexcept -> volatile uint8_t *
{
    // NOLINTNEXTLINE(performance-no-int-to-ptr): AVR descriptors intentionally store raw MMIO addresses.
    return reinterpret_cast<volatile uint8_t *>(address);
}

/**
 * @brief Rebuilds a writable AVR DDR register pointer from a raw MMIO address.
 */
[[nodiscard]] inline auto modeRegisterFromAddress(uint16_t address) noexcept -> volatile uint8_t *
{
    // NOLINTNEXTLINE(performance-no-int-to-ptr): AVR descriptors intentionally store raw MMIO addresses.
    return reinterpret_cast<volatile uint8_t *>(address);
}
}  // namespace detail

/**
 * @brief Reads the fast-I/O bit mask for one runtime Arduino pin.
 */
[[nodiscard]] inline auto readPinBitMask(uint8_t pin) noexcept -> uint8_t
{
    return pgm_read_byte(digital_pin_to_bit_mask_PGM + pin);
}

/**
 * @brief Reads the Arduino core port index for one runtime pin.
 */
[[nodiscard]] inline auto readPinPortIndex(uint8_t pin) noexcept -> uint8_t
{
    return pgm_read_byte(digital_pin_to_port_PGM + pin);
}

/**
 * @brief Converts one Arduino core port index to the AVR input register.
 */
[[nodiscard]] inline auto inputRegister(uint8_t port) noexcept -> volatile const uint8_t *
{
    // NOLINTNEXTLINE(performance-no-int-to-ptr): AVR port lookup tables store raw MMIO addresses in PROGMEM.
    return reinterpret_cast<volatile const uint8_t *>(pgm_read_word(port_to_input_PGM + port));
}

/**
 * @brief Converts one Arduino core port index to the AVR output register.
 */
[[nodiscard]] inline auto outputRegister(uint8_t port) noexcept -> volatile uint8_t *
{
    // NOLINTNEXTLINE(performance-no-int-to-ptr): AVR port lookup tables store raw MMIO addresses in PROGMEM.
    return reinterpret_cast<volatile uint8_t *>(pgm_read_word(port_to_output_PGM + port));
}

/**
 * @brief Converts one Arduino core port index to the AVR DDR register.
 */
[[nodiscard]] inline auto modeRegister(uint8_t port) noexcept -> volatile uint8_t *
{
    // NOLINTNEXTLINE(performance-no-int-to-ptr): AVR port lookup tables store raw MMIO addresses in PROGMEM.
    return reinterpret_cast<volatile uint8_t *>(pgm_read_word(port_to_mode_PGM + port));
}

/**
 * @brief Resolves the AVR input register for one runtime Arduino pin.
 */
[[nodiscard]] inline auto inputRegisterForPin(uint8_t pin) noexcept -> volatile const uint8_t *
{
    return inputRegister(readPinPortIndex(pin));
}

/**
 * @brief Resolves the AVR output register for one runtime Arduino pin.
 */
[[nodiscard]] inline auto outputRegisterForPin(uint8_t pin) noexcept -> volatile uint8_t *
{
    return outputRegister(readPinPortIndex(pin));
}

/**
 * @brief Resolves the AVR DDR register for one runtime Arduino pin.
 */
[[nodiscard]] inline auto modeRegisterForPin(uint8_t pin) noexcept -> volatile uint8_t *
{
    return modeRegister(readPinPortIndex(pin));
}

/**
 * @brief Builds the cached fast-input binding for one runtime pin.
 */
[[nodiscard]] inline auto makeFastInputPinBinding(uint8_t pin) noexcept -> FastInputPinBinding
{
    return FastInputPinBinding{readPinBitMask(pin), inputRegisterForPin(pin)};
}

/**
 * @brief Builds the cached fast-output binding for one runtime pin.
 */
[[nodiscard]] inline auto makeFastOutputPinBinding(uint8_t pin) noexcept -> FastOutputPinBinding
{
    return FastOutputPinBinding{readPinBitMask(pin), outputRegisterForPin(pin), modeRegisterForPin(pin)};
}

/**
 * @brief Returns the bit mask for one compile-time pin tag.
 *
 * When the target board and pin are known by the constexpr Mega mapping, this
 * avoids the Arduino PROGMEM lookup tables entirely. Unsupported pins fall
 * back to the traditional runtime path.
 */
template <uint8_t Pin> [[nodiscard]] inline auto readPinBitMask(::lsh::core::PinTag<Pin>) noexcept -> uint8_t
{
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    if constexpr (detail::hasConstexprMegaBinding(Pin))
    {
        constexpr StaticPinDescriptor descriptor = detail::megaPinDescriptor(Pin);
        return descriptor.mask;
    }
#endif
    return readPinBitMask(static_cast<uint8_t>(Pin));
}

/**
 * @brief Resolves the AVR input register for one compile-time pin tag.
 */
template <uint8_t Pin> [[nodiscard]] inline auto inputRegisterForPin(::lsh::core::PinTag<Pin>) noexcept -> volatile const uint8_t *
{
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    if constexpr (detail::hasConstexprMegaBinding(Pin))
    {
        constexpr StaticPinDescriptor descriptor = detail::megaPinDescriptor(Pin);
        return detail::inputRegisterFromAddress(descriptor.inputAddress);
    }
#endif
    return inputRegisterForPin(static_cast<uint8_t>(Pin));
}

/**
 * @brief Resolves the AVR output register for one compile-time pin tag.
 */
template <uint8_t Pin> [[nodiscard]] inline auto outputRegisterForPin(::lsh::core::PinTag<Pin>) noexcept -> volatile uint8_t *
{
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    if constexpr (detail::hasConstexprMegaBinding(Pin))
    {
        constexpr StaticPinDescriptor descriptor = detail::megaPinDescriptor(Pin);
        return detail::outputRegisterFromAddress(descriptor.outputAddress);
    }
#endif
    return outputRegisterForPin(static_cast<uint8_t>(Pin));
}

/**
 * @brief Resolves the AVR DDR register for one compile-time pin tag.
 */
template <uint8_t Pin> [[nodiscard]] inline auto modeRegisterForPin(::lsh::core::PinTag<Pin>) noexcept -> volatile uint8_t *
{
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    if constexpr (detail::hasConstexprMegaBinding(Pin))
    {
        constexpr StaticPinDescriptor descriptor = detail::megaPinDescriptor(Pin);
        return detail::modeRegisterFromAddress(descriptor.modeAddress);
    }
#endif
    return modeRegisterForPin(static_cast<uint8_t>(Pin));
}

/**
 * @brief Builds the cached fast-input binding for one compile-time pin tag.
 */
template <uint8_t Pin> [[nodiscard]] inline auto makeFastInputPinBinding(::lsh::core::PinTag<Pin>) noexcept -> FastInputPinBinding
{
    // Materialize the constexpr descriptor into the same compact binding used
    // by the runtime path so peripherals keep one direct-register hot path.
    return FastInputPinBinding{readPinBitMask(::lsh::core::PinTag<Pin>{}), inputRegisterForPin(::lsh::core::PinTag<Pin>{})};
}

/**
 * @brief Builds the cached fast-output binding for one compile-time pin tag.
 */
template <uint8_t Pin> [[nodiscard]] inline auto makeFastOutputPinBinding(::lsh::core::PinTag<Pin>) noexcept -> FastOutputPinBinding
{
    // The compile-time path resolves addresses here, but the peripheral still
    // stores only the minimal mask/register pair needed by the hot write path.
    return FastOutputPinBinding{readPinBitMask(::lsh::core::PinTag<Pin>{}), outputRegisterForPin(::lsh::core::PinTag<Pin>{}),
                                modeRegisterForPin(::lsh::core::PinTag<Pin>{})};
}
}  // namespace avr
}  // namespace core
}  // namespace lsh

#endif  // LSH_CORE_INTERNAL_AVR_FAST_IO_HPP
