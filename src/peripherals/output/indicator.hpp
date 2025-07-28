/**
 * @file    indicator.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Defines the Indicator class, representing an LED or status light.
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

#ifndef LSHCORE_PERIPHERALS_OUTPUT_INDICATOR_HPP
#define LSHCORE_PERIPHERALS_OUTPUT_INDICATOR_HPP

#include <etl/vector.h>
#include <stdint.h>

#include "internal/user_config_bridge.hpp"
#include "util/constants/indicatormodes.hpp"

/**
 * @brief Represents a state indicator for one or more attached actuators, indicators are normally connected to a digital out.
 *
 */
class Indicator
{
private:
#ifndef CONFIG_USE_FAST_INDICATORS
    const uint8_t pinNumber; //!< The pin to which the indicator is connected to, for conventional IO
#else
    const uint8_t pinMask;           //!< Mask of the indicator, for fast IO
    volatile uint8_t *const pinPort; //!< Port of the indicator, for fast IO
#endif
    uint8_t index = 0U;                                               //!< Indicator index on Indicators namespace Array
    constants::IndicatorMode mode = constants::IndicatorMode::ANY;    //!< Indicator mode
    etl::vector<uint8_t, CONFIG_MAX_ACTUATORS> controlledActuators{}; //!< Controlled actuators indexes
    bool actualState = false;                                         //!< Actual state of the indicator

public:
#ifndef CONFIG_USE_FAST_INDICATORS
    explicit constexpr Indicator(uint8_t pin) noexcept : pinNumber(pin)
    {
        pinMode(pin, OUTPUT); // PinMode to Output
    }
#else
    explicit Indicator(uint8_t pin) noexcept : pinMask(digitalPinToBitMask(pin)), pinPort(portOutputRegister(digitalPinToPort(pin)))
    {
        // PinMode to OUTPUT
        uint8_t port = digitalPinToPort(pin);
        volatile uint8_t *const reg = portModeRegister(port);
        const uint8_t oldSREG = SREG;
        cli();
        *reg |= this->pinMask;
        SREG = oldSREG;
    }
#endif

#if (__cplusplus >= 201703L) && (__GNUC__ >= 7)
    Indicator(const Indicator &) = delete;
    Indicator(Indicator &&) = delete;
    auto operator=(const Indicator &) -> Indicator & = delete;
    auto operator=(Indicator &&) -> Indicator & = delete;
#endif // (__cplusplus >= 201703L) && (__GNUC__ >= 7)

    void setState(bool stateToSet);                                      // Set the new state
    void setIndex(uint8_t indexToSet);                                   // Set the indicator index on Indicators namespace Array
    auto addActuator(uint8_t actuatorIndex) -> Indicator &;              // Add one actuator to controlled actuators vector
    auto setMode(constants::IndicatorMode indicatorMode) -> Indicator &; // Set indicator mode
    void check();                                                        // Perform the actual check
    void resizeVectors();                                                // Resize controlled actuators vector to its actual size.
    [[nodiscard]] auto getIndex() const -> uint8_t;                      // Get the indicator index on Indicators namespace Array
};

#endif // LSHCORE_PERIPHERALS_OUTPUT_INDICATOR_HPP