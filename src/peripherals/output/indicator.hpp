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
        // Indicators default to OFF. Prime the output latch before enabling
        // OUTPUT so setup cannot briefly expose an inherited HIGH level.
        const uint8_t oldSREG = SREG;
        cli();
        *this->pinPort &= ~this->pinMask;
        *binding.modePort |= this->pinMask;
        SREG = oldSREG;
    }
#endif
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
    uint8_t index = UINT8_MAX;  //!< Debug/runtime-check registration index; stripped from release objects.
#endif
    bool actualState = false;  //!< Actual state of the indicator

    /**
     * @brief Write the physical output without touching the cached state.
     *
     * @param stateToWrite the state to write true=ON, false=OFF.
     */
    void writeState(bool stateToWrite)
    {
#ifdef CONFIG_USE_FAST_INDICATORS
        volatile uint8_t *const port = this->pinPort;
        const uint8_t mask = this->pinMask;
        // Expand the bool to 0x00/0xFF so the atomic bit assignment needs no branch.
        const uint8_t stateMask = static_cast<uint8_t>(-static_cast<int8_t>(stateToWrite));
        const uint8_t oldSREG = SREG;
        cli();
        uint8_t portState = *port;
        portState ^= static_cast<uint8_t>((stateMask ^ portState) & mask);
        *port = portState;
        SREG = oldSREG;
#else
        digitalWrite(this->pinNumber, static_cast<uint8_t>(stateToWrite));
#endif
    }

public:
#ifndef CONFIG_USE_FAST_INDICATORS
    /**
     * @brief Construct a new Indicator object using standard I/O.
     * @param pin The Arduino pin number for the indicator.
     */
    explicit LSH_OPTIONAL_CONSTEXPR_CTOR Indicator(uint8_t pin) noexcept : pinNumber(pin)
    {
        digitalWrite(pin, LOW);
        pinMode(pin, OUTPUT);
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
        this->actualState = stateToSet;
        this->writeState(stateToSet);
    }
    void applyComputedState(bool newState)
    {
        // Generated static profiles compute the indicator expression directly.
        // Keep the state-change guard at the shared output boundary.
        if (newState == this->actualState)
        {
            return;
        }
        this->setState(newState);
    }
    void setIndex(uint8_t indexToSet);  // Set the indicator index on Indicators namespace Array

    [[nodiscard]] auto getIndex() const -> uint8_t;  // Get the indicator index on Indicators namespace Array
};

#endif  // LSH_CORE_PERIPHERALS_OUTPUT_INDICATOR_HPP
