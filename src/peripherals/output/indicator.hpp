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
#include "internal/pin_tag.hpp"
#include "internal/user_config_bridge.hpp"
#ifdef CONFIG_USE_FAST_INDICATORS
#include "internal/avr_fast_io.hpp"
#endif

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

    /**
     * @brief Shared fast-I/O constructor fed by a fully resolved output binding.
     *
     * Indicators keep only the mask and output register in the instance so the
     * steady-state `setState()` path is a single direct register update.
     */
    explicit Indicator(lsh::core::avr::FastOutputPinBinding binding) noexcept : pinMask(binding.mask), pinPort(binding.pinPort)
    {
        const uint8_t oldSREG = SREG;
        cli();
        *binding.modePort |= this->pinMask;
        SREG = oldSREG;
    }
#endif
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
    uint8_t index = UINT8_MAX;  //!< Debug/runtime-check registration index; stripped from release objects.
#endif
    bool actualState = false;  //!< Actual state of the indicator

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

    /**
     * @brief Construct an indicator from a compile-time pin tag on the slow-I/O path.
     *
     * This keeps the public DSL consistent across builds even when the target
     * does not use the AVR fast-I/O backend.
     */
    template <uint8_t Pin>
    explicit LSH_OPTIONAL_CONSTEXPR_CTOR Indicator(lsh::core::PinTag<Pin>) noexcept : Indicator(static_cast<uint8_t>(Pin))
    {}
#else
    /**
     * @brief Construct a new Indicator object using fast I/O (direct port manipulation).
     * @param pin The Arduino pin number for the indicator.
     */
    explicit Indicator(uint8_t pin) noexcept : Indicator(lsh::core::avr::makeFastOutputPinBinding(pin))
    {}

    /**
     * @brief Construct an indicator from a compile-time pin tag on the fast-I/O path.
     *
     * The compile-time tag lets the AVR helper resolve the final registers once
     * during construction, while `setState()` stays a plain direct port write.
     */
    template <uint8_t Pin>
    explicit Indicator(lsh::core::PinTag<Pin>) noexcept : Indicator(lsh::core::avr::makeFastOutputPinBinding(lsh::core::PinTag<Pin>{}))
    {}
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
    void setState(bool stateToSet)
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
    void applyComputedState(bool newState)
    {
        // Generated static profiles compute the indicator expression directly.
        // Keep the state-change guard here so refreshIndicators() can avoid the
        // old index-based Indicator::check() path in release builds.
        if (newState == this->actualState)
        {
            return;
        }
        this->actualState = newState;
        this->setState(newState);
    }
    void setIndex(uint8_t indexToSet);  // Set the indicator index on Indicators namespace Array
    void check();                       // Perform the actual check

    [[nodiscard]] auto getIndex() const -> uint8_t;  // Get the indicator index on Indicators namespace Array
};

#endif  // LSH_CORE_PERIPHERALS_OUTPUT_INDICATOR_HPP
