/**
 * @file    actuator.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Defines the Actuator class, representing a physical relay or digital output.
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

#ifndef LSHCORE_PERIPHERALS_OUTPUT_ACTUATOR_HPP
#define LSHCORE_PERIPHERALS_OUTPUT_ACTUATOR_HPP

#include <stdint.h>

#include "internal/user_config_bridge.hpp"

/**
 * @brief Represents an actuator (relay) attached to a digital pin.
 *
 */
class Actuator
{
private:
#ifndef CONFIG_USE_FAST_ACTUATORS
    const uint8_t pinNumber; //!< The pin to which the actuator is connected to, for conventional IO
#else
    const uint8_t pinMask;           //!< Mask of the actuator, for fast IO
    volatile uint8_t *const pinPort; //!< Port of the actuator, for fast IO
#endif
    uint8_t index = 0U;             //!< Actuator index on Actuators namespace array
    const bool defaultState;        //!< Default state of the actuator (false=OFF, true=ON)
    bool actualState = false;       //!< Actual state of the actuator (false=OFF, true=ON)
    uint32_t lastTimeSwitched = 0U; //!< Last time the actuator performed a switch
    bool isProtected = false;       //!< True if it's protected against some turn ON/OFF behaviour
    uint8_t id;                     //!< Unique ID of the actuator (numeric)
    bool hasAutoOffTimer = false;   //!< True if auto-off timer is > 0
    uint32_t autoOffTimer_ms = 0U;  //!< Auto-off timer, 0:disabled, other value: actuator turned off after timer_ms

public:
#ifndef CONFIG_USE_FAST_ACTUATORS
    /**
     * @brief Construct a new Actuator object, conventional IO version.
     *
     * @param pin pin number
     * @param uniqueId the actuator unique id.
     * @param normalState the default state of the actuator.
     */
    explicit constexpr Actuator(uint8_t pin, uint8_t uniqueId, bool normalState = false) noexcept : pinNumber(pin), defaultState(normalState), id(uniqueId)
    {
        pinMode(pin, OUTPUT);                                 // PinMode to Output
        digitalWrite(pin, static_cast<uint8_t>(normalState)); // Set the default state
    }
#else
    /**
     * @brief Construct a new Actuator object, fast IO version.
     *
     * @param pin pin number
     * @param uniqueId the actuator unique id.
     * @param normalState the default state of the actuator.
     */
    explicit Actuator(uint8_t pin, uint8_t uniqueId, bool normalState = false) noexcept : pinMask(digitalPinToBitMask(pin)), pinPort(portOutputRegister(digitalPinToPort(pin))), defaultState(normalState), id(uniqueId)
    {
        // PinMode to OUTPUT
        uint8_t port = digitalPinToPort(pin);
        volatile uint8_t *const reg = portModeRegister(port);
        const uint8_t oldSREG = SREG;
        cli();
        *reg |= this->pinMask;
        SREG = oldSREG;

        // Set the default state
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

    /*  Workaround for https://stackoverflow.com/questions/28788353/clang-wweak-vtables-and-pure-abstract-class
        and https://stackoverflow.com/questions/28786473/clang-no-out-of-line-virtual-method-definitions-pure-abstract-c-class/40550578 */

#if (__cplusplus >= 201703L) && (__GNUC__ >= 7)
    Actuator(const Actuator &) = delete;
    Actuator(Actuator &&) = delete;
    auto operator=(const Actuator &) -> Actuator & = delete;
    auto operator=(Actuator &&) -> Actuator & = delete;
#endif // (__cplusplus >= 201703L) && (__GNUC__ >= 7)

    // Setters
    [[nodiscard]] auto setState(bool state) -> bool; // Sets the new state of the actuator, respecting debounce time.

    void setIndex(uint8_t indexToSet);                    // Set the actuator index on Actuators namespace Array
    auto setAutoOffTimer(uint32_t time_ms) -> Actuator &; // Set "turn off" timer in ms
    auto setProtected(bool hasProtection) -> Actuator &;  // Set protection against global "turn-off" actions (e.g., a general super long click).

    // Getters
    [[nodiscard]] auto getIndex() const -> uint8_t;         // Get the actuator index on Actuators namespace Array
    [[nodiscard]] auto getId() const -> uint8_t;            // Return unique ID of the actuator
    [[nodiscard]] auto getState() const -> bool;            // Returns the state of the actuator (false=OFF, true=ON)
    [[nodiscard]] auto getDefaultState() const -> bool;     // Returns the default state of the actuator
    [[nodiscard]] auto hasAutoOff() const -> bool;          // Returns if the actuators has a timer set
    [[nodiscard]] auto getAutoOffTimer() const -> uint32_t; // Returns the timer of the actuator
    [[nodiscard]] auto hasProtection() const -> bool;       // Returns true if the actuator is protected from global "turn-off" actions.

    // Utils
    [[nodiscard]] auto toggleState() -> bool;       // Switch the actuator
    [[nodiscard]] auto checkAutoOffTimer() -> bool; // Checks if auto off timer is over, turn off if it's over
};

#endif // LSHCORE_PERIPHERALS_OUTPUT_ACTUATOR_HPP