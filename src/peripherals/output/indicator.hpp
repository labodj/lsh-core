/**
 * @file    indicator.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Defines the Indicator class, representing an LED or status light.
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

#ifndef LSH_CORE_PERIPHERALS_OUTPUT_INDICATOR_HPP
#define LSH_CORE_PERIPHERALS_OUTPUT_INDICATOR_HPP

#include <stdint.h>

#include "internal/cpp_features.hpp"
#include "internal/etl_vector.hpp"
#include "internal/user_config_bridge.hpp"
#include "util/constants/indicator_modes.hpp"

/**
 * @brief Represents a state indicator for one or more attached actuators, indicators are normally connected to a digital out.
 *
 */
class Indicator
{
private:
#ifndef CONFIG_USE_FAST_INDICATORS
    const uint8_t pinNumber;  //!< The pin to which the indicator is connected to, for conventional IO
#else
    const uint8_t pinMask;            //!< Mask of the indicator, for fast IO
    volatile uint8_t *const pinPort;  //!< Port of the indicator, for fast IO
#endif
    uint8_t index = 0U;                                                //!< Indicator index on Indicators namespace Array
    constants::IndicatorMode mode = constants::IndicatorMode::ANY;     //!< Indicator mode
    etl::vector<uint8_t, CONFIG_MAX_ACTUATORS> controlledActuators{};  //!< Controlled actuators indexes
    bool actualState = false;                                          //!< Actual state of the indicator

public:
#ifndef CONFIG_USE_FAST_INDICATORS
    /**
     * @brief Construct a new Indicator object using standard I/O.
     * @param pin The Arduino pin number for the indicator.
     */
    explicit LSH_OPTIONAL_CONSTEXPR_CTOR Indicator(uint8_t pin) noexcept : pinNumber(pin)
    {
        pinMode(pin, OUTPUT);  // PinMode to Output
    }
#else
    /**
     * @brief Construct a new Indicator object using fast I/O (direct port manipulation).
     * @param pin The Arduino pin number for the indicator.
     */
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

#if LSH_USING_CPP17
    Indicator(const Indicator &) = delete;
    Indicator(Indicator &&) = delete;
    auto operator=(const Indicator &) -> Indicator & = delete;
    auto operator=(Indicator &&) -> Indicator & = delete;
#endif  // LSH_USING_CPP17

    /**
     * @brief Set the state of the indicator.
     *
     * @param stateToSet the state to set true=ON, false=OFF.
     */
    inline void setState(bool stateToSet)
    {
#ifdef CONFIG_USE_FAST_INDICATORS
        if (!stateToSet)
        {
            *this->pinPort &= ~this->pinMask;
        }
        else
        {
            *this->pinPort |= this->pinMask;
        }
#else
        digitalWrite(this->pinNumber, static_cast<uint8_t>(stateToSet));
#endif
    }
    void setIndex(uint8_t indexToSet);                                    // Set the indicator index on Indicators namespace Array
    auto addActuator(uint8_t actuatorIndex) -> Indicator &;               // Add one actuator to controlled actuators vector
    auto setMode(constants::IndicatorMode indicatorMode) -> Indicator &;  // Set indicator mode
    void check();                                                         // Perform the actual check

    [[nodiscard]] auto getIndex() const -> uint8_t;  // Get the indicator index on Indicators namespace Array
};

#endif  // LSH_CORE_PERIPHERALS_OUTPUT_INDICATOR_HPP
