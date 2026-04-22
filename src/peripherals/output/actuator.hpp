/**
 * @file    actuator.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Defines the Actuator class, representing a physical relay or digital output.
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

#ifndef LSH_CORE_PERIPHERALS_OUTPUT_ACTUATOR_HPP
#define LSH_CORE_PERIPHERALS_OUTPUT_ACTUATOR_HPP

#include <stdint.h>

#include "internal/cpp_features.hpp"
#include "internal/pin_tag.hpp"
#include "internal/user_config_bridge.hpp"
#ifdef CONFIG_USE_FAST_ACTUATORS
#include "internal/avr_fast_io.hpp"
#endif

/**
 * @brief Represents an actuator (relay) attached to a digital pin.
 *
 */
class Actuator
{
private:
#ifndef CONFIG_USE_FAST_ACTUATORS
    const uint8_t pinNumber;  //!< The pin to which the actuator is connected to, for conventional IO
#else
    const uint8_t pinMask;            //!< Mask of the actuator, for fast IO
    volatile uint8_t *const pinPort;  //!< Port of the actuator, for fast IO

    /**
     * @brief Shared fast-I/O constructor fed by a fully resolved output binding.
     *
     * Both the runtime `uint8_t pin` path and the compile-time `PinTag` path
     * collapse here, so the initialization sequence is emitted only once.
     */
    explicit Actuator(lsh::core::avr::FastOutputPinBinding binding, uint8_t uniqueId, bool normalState) noexcept :
        pinMask(binding.mask), pinPort(binding.pinPort), defaultState(normalState), actualState(normalState), id(uniqueId)
    {
        // Keep the heavy init path in one non-template constructor so compile-time
        // pins do not clone the whole setup sequence for every instantiation.
        const uint8_t oldSREG = SREG;
        cli();
        *binding.modePort |= this->pinMask;
        SREG = oldSREG;

        // Apply the default state directly to the resolved AVR register so the
        // fast path starts from the exact same electrical state as the classic
        // `pinMode + digitalWrite` sequence, but without the Arduino helpers.
        if (!normalState)
        {
            *this->pinPort &= ~this->pinMask;
        }
        else
        {
            *this->pinPort |= this->pinMask;
        }
    }
#endif
    uint8_t index = UINT8_MAX;       //!< Actuator index on Actuators namespace array, or `UINT8_MAX` until registration succeeds.
    const bool defaultState;         //!< Default state of the actuator (false=OFF, true=ON)
    bool actualState = false;        //!< Actual state of the actuator (false=OFF, true=ON)
    uint32_t lastTimeSwitched = 0U;  //!< Last time the actuator performed a switch
    bool isProtected = false;        //!< True if it's protected against some turn ON/OFF behaviour
    uint8_t id;                      //!< Unique ID of the actuator (numeric)
    bool hasAutoOffTimer = false;    //!< True if auto-off timer is > 0
    uint32_t autoOffTimer_ms = 0U;   //!< Auto-off timer, 0:disabled, other value: actuator turned off after timer_ms

public:
#ifndef CONFIG_USE_FAST_ACTUATORS
    /**
     * @brief Construct a new Actuator object, conventional IO version.
     *
     * @param pin pin number
     * @param uniqueId the actuator unique id.
     * @param normalState the default state of the actuator.
     */
    explicit LSH_OPTIONAL_CONSTEXPR_CTOR Actuator(uint8_t pin, uint8_t uniqueId, bool normalState = false) noexcept :
        pinNumber(pin), defaultState(normalState), actualState(normalState), id(uniqueId)
    {
        pinMode(pin, OUTPUT);                                  // PinMode to Output
        digitalWrite(pin, static_cast<uint8_t>(normalState));  // Set the default state
    }

    /**
     * @brief Construct an actuator from a compile-time pin tag on the slow-I/O path.
     *
     * The tag keeps the public macro surface uniform even when fast I/O is
     * disabled or unavailable for the current build target.
     */
    template <uint8_t Pin>
    explicit LSH_OPTIONAL_CONSTEXPR_CTOR Actuator(lsh::core::PinTag<Pin>, uint8_t uniqueId, bool normalState = false) noexcept :
        Actuator(static_cast<uint8_t>(Pin), uniqueId, normalState)
    {}
#else
    /**
     * @brief Construct a new Actuator object, fast IO version.
     *
     * @param pin pin number
     * @param uniqueId the actuator unique id.
     * @param normalState the default state of the actuator.
     */
    explicit Actuator(uint8_t pin, uint8_t uniqueId, bool normalState = false) noexcept :
        Actuator(lsh::core::avr::makeFastOutputPinBinding(pin), uniqueId, normalState)
    {}

    /**
     * @brief Construct an actuator from a compile-time pin tag on the fast-I/O path.
     *
     * On supported AVR boards this resolves the port and mask without touching
     * the Arduino PROGMEM lookup tables in the translation unit that instantiates
     * the actuator.
     */
    template <uint8_t Pin>
    explicit Actuator(lsh::core::PinTag<Pin>, uint8_t uniqueId, bool normalState = false) noexcept :
        Actuator(lsh::core::avr::makeFastOutputPinBinding(lsh::core::PinTag<Pin>{}), uniqueId, normalState)
    {}
#endif

    /*  Workaround for https://stackoverflow.com/questions/28788353/clang-wweak-vtables-and-pure-abstract-class
        and https://stackoverflow.com/questions/28786473/clang-no-out-of-line-virtual-method-definitions-pure-abstract-c-class/40550578 */

#if LSH_USING_CPP17
    Actuator(const Actuator &) = delete;
    Actuator(Actuator &&) = delete;
    auto operator=(const Actuator &) -> Actuator & = delete;
    auto operator=(Actuator &&) -> Actuator & = delete;
#endif  // LSH_USING_CPP17

    // Setters
    [[nodiscard]] auto setState(bool state) -> bool;  // Sets the new state of the actuator, respecting debounce time.

    void setIndex(uint8_t indexToSet);                     // Set the actuator index on Actuators namespace Array
    auto setAutoOffTimer(uint32_t time_ms) -> Actuator &;  // Set "turn off" timer in ms
    auto setProtected(bool hasProtection)
        -> Actuator &;  // Set protection against global "turn-off" actions (e.g., a general super long click).

    // Getters
    [[nodiscard]] auto getIndex() const -> uint8_t;          // Get the actuator index on Actuators namespace Array
    [[nodiscard]] auto getId() const -> uint8_t;             // Return unique ID of the actuator
    [[nodiscard]] auto getState() const -> bool;             // Returns the state of the actuator (false=OFF, true=ON)
    [[nodiscard]] auto getDefaultState() const -> bool;      // Returns the default state of the actuator
    [[nodiscard]] auto hasAutoOff() const -> bool;           // Returns if the actuators has a timer set
    [[nodiscard]] auto getAutoOffTimer() const -> uint32_t;  // Returns the timer of the actuator
    [[nodiscard]] auto hasProtection() const -> bool;        // Returns true if the actuator is protected from global "turn-off" actions.

    // Utils
    [[nodiscard]] auto toggleState() -> bool;        // Switch the actuator
    [[nodiscard]] auto checkAutoOffTimer() -> bool;  // Checks if auto off timer is over, turn off if it's over
};

#endif  // LSH_CORE_PERIPHERALS_OUTPUT_ACTUATOR_HPP
