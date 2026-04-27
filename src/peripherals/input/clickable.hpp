/**
 * @file    clickable.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Defines the Clickable class for handling button inputs and click logic.
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

#ifndef LSH_CORE_PERIPHERALS_INPUT_CLICKABLE_HPP
#define LSH_CORE_PERIPHERALS_INPUT_CLICKABLE_HPP

#include <stdint.h>

#include "internal/cpp_features.hpp"
#include "internal/pin_tag.hpp"
#include "internal/user_config_bridge.hpp"
#ifdef CONFIG_USE_FAST_CLICKABLES
#include "internal/avr_fast_io.hpp"
#endif
#include "util/constants/click_detection.hpp"
#include "util/constants/click_results.hpp"
#include "util/constants/timing.hpp"
#include "util/saturating_time.hpp"

/**
 * @brief A class that represents a clickable object, like a button, and its associated logic.
 *
 */
class Clickable
{
private:
    static constexpr uint8_t CLICKABLE_FLAG_STABLE_PRESSED = 0x01U;
    static constexpr uint8_t CLICKABLE_FLAG_CANDIDATE_PRESSED = 0x02U;
    static constexpr uint8_t CLICKABLE_FLAG_DEBOUNCING = 0x04U;
    static constexpr uint8_t CLICKABLE_FLAG_LONG_FIRED = 0x08U;
    static constexpr uint8_t CLICKABLE_FLAG_SUPER_LONG_FIRED = 0x10U;

#ifndef CONFIG_USE_FAST_CLICKABLES
    const uint8_t pinNumber;  //!< The pin to which the clickable is connected to, for conventional IO
#else
    const uint8_t pinMask;                  //!< Mask of the clickable, for fast IO
    const volatile uint8_t *const pinPort;  //!< Port of the clickable, for fast IO

    /**
     * @brief Shared fast-I/O constructor fed by a fully resolved input binding.
     *
     * Keeping the binding-to-member transfer in one non-template constructor
     * prevents the same setup boilerplate from being instantiated for every
     * compile-time pin used by the public macros, while preserving a direct
     * register read in `getState()` with no extra dispatch in the scan loop.
     */
    explicit Clickable(lsh::core::avr::FastInputPinBinding binding) noexcept : pinMask(binding.mask), pinPort(binding.pinPort)
    {
        // lsh-core expects externally pulled-down buttons. Explicitly disabling
        // the AVR pull-up avoids inheriting a previous firmware's PORT latch
        // state before the scan loop starts sampling the direct input register.
        const uint8_t oldSREG = SREG;
        cli();
        *binding.outputPort &= ~this->pinMask;
        *binding.modePort &= ~this->pinMask;
        SREG = oldSREG;
    }
#endif
    uint16_t pressAge_ms = 0U;    //!< Debounced press duration, frozen while a release is only a debounce candidate.
    uint8_t debounceAge_ms = 0U;  //!< Elapsed time spent validating the raw candidate edge.
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
    uint8_t index = UINT8_MAX;  //!< Debug/runtime-check registration index; stripped from release objects.
#endif
    uint8_t flags = 0U;  //!< Packed debounce state and once-per-press action markers.

    [[nodiscard]] auto hasClickableFlag(uint8_t flag) const noexcept -> bool
    {
        return (this->flags & flag) != 0U;
    }

    void setClickableFlag(uint8_t flag, bool enabled) noexcept
    {
        if (enabled)
        {
            this->flags |= flag;
        }
        else
        {
            this->flags &= static_cast<uint8_t>(~flag);
        }
    }

    [[nodiscard]] auto stablePressed() const noexcept -> bool
    {
        return this->hasClickableFlag(CLICKABLE_FLAG_STABLE_PRESSED);
    }

    [[nodiscard]] auto candidatePressed() const noexcept -> bool
    {
        return this->hasClickableFlag(CLICKABLE_FLAG_CANDIDATE_PRESSED);
    }

    [[nodiscard]] auto isDebouncing() const noexcept -> bool
    {
        return this->hasClickableFlag(CLICKABLE_FLAG_DEBOUNCING);
    }

    [[nodiscard]] auto releaseDebouncePending() const noexcept -> bool
    {
        return this->isDebouncing() && this->stablePressed() && !this->candidatePressed();
    }

    void clearTimedActionFlags() noexcept
    {
        this->flags &= static_cast<uint8_t>(~(CLICKABLE_FLAG_LONG_FIRED | CLICKABLE_FLAG_SUPER_LONG_FIRED));
    }

    void startDebounce(bool candidatePressed) noexcept
    {
        this->setClickableFlag(CLICKABLE_FLAG_DEBOUNCING, true);
        this->setClickableFlag(CLICKABLE_FLAG_CANDIDATE_PRESSED, candidatePressed);
        this->debounceAge_ms = 0U;
    }

    [[nodiscard]] auto confirmDebouncedEdge(bool pressed) noexcept -> constants::ClickResult
    {
        this->setClickableFlag(CLICKABLE_FLAG_DEBOUNCING, false);
        this->debounceAge_ms = 0U;
        this->setClickableFlag(CLICKABLE_FLAG_STABLE_PRESSED, pressed);
        this->setClickableFlag(CLICKABLE_FLAG_CANDIDATE_PRESSED, pressed);
        return pressed ? constants::ClickResult::NO_CLICK_KEEPING_CLICKED : constants::ClickResult::NO_CLICK;
    }

    [[nodiscard]] auto updateDebouncedEdge(bool rawPressed, uint16_t elapsed_ms) noexcept -> constants::ClickResult
    {
        using constants::ClickResult;
        using constants::timings::CLICKABLE_DEBOUNCE_TIME_MS;

        if (!this->isDebouncing())
        {
            if (rawPressed == this->stablePressed())
            {
                return ClickResult::NO_CLICK;
            }
            if (CLICKABLE_DEBOUNCE_TIME_MS == 0U)
            {
                return this->confirmDebouncedEdge(rawPressed);
            }
            this->startDebounce(rawPressed);
            return ClickResult::NO_CLICK;
        }

        if (rawPressed != this->candidatePressed())
        {
            // The raw input bounced back to the stable level before the
            // candidate edge became valid. Keep the same logical press, which
            // prevents a noisy release from arming another long click.
            this->setClickableFlag(CLICKABLE_FLAG_DEBOUNCING, false);
            this->setClickableFlag(CLICKABLE_FLAG_CANDIDATE_PRESSED, this->stablePressed());
            this->debounceAge_ms = 0U;
            return ClickResult::NO_CLICK;
        }

        const uint16_t nextDebounceAge = timeUtils::addElapsedTimeSaturated(this->debounceAge_ms, elapsed_ms);
        this->debounceAge_ms = nextDebounceAge > UINT8_MAX ? UINT8_MAX : static_cast<uint8_t>(nextDebounceAge);
        if (this->debounceAge_ms >= CLICKABLE_DEBOUNCE_TIME_MS)
        {
            return this->confirmDebouncedEdge(rawPressed);
        }
        return ClickResult::NO_CLICK;
    }

    /**
     * @brief Shared click FSM implementation for runtime and generated-static paths.
     * @details Generated code calls the `StaticConfigKnown=true` specialization.
     *          That lets AVR-GCC erase disabled click families with
     *          `if constexpr` instead of checking runtime bit flags in the
     *          button polling path. The runtime overload below keeps the same
     *          behavior for any non-generated caller.
     */
    template <bool StaticConfigKnown, uint8_t StaticDetectionFlags, uint16_t StaticLongClick_ms, uint16_t StaticSuperLongClick_ms>
    [[nodiscard]] auto clickDetectionImpl(uint16_t elapsed_ms, uint8_t detectionFlags, uint16_t longClick_ms, uint16_t superLongClick_ms)
        -> constants::ClickResult
    {
        using constants::ClickResult;
        using namespace constants::clickDetection;

        const bool wasStablePressed = this->stablePressed();
        static_cast<void>(this->updateDebouncedEdge(this->getState(), elapsed_ms));
        const bool isStablePressed = this->stablePressed();

        if (!wasStablePressed && isStablePressed)
        {
            this->pressAge_ms = 0U;
            this->clearTimedActionFlags();
            if constexpr (StaticConfigKnown)
            {
                if constexpr ((StaticDetectionFlags & QUICK_SHORT) != 0U)
                {
                    return ClickResult::SHORT_CLICK_QUICK;
                }
            }
            else if (hasFlag(detectionFlags, QUICK_SHORT))
            {
                return ClickResult::SHORT_CLICK_QUICK;
            }
            return ClickResult::NO_CLICK_KEEPING_CLICKED;
        }

        if (wasStablePressed && !isStablePressed)
        {
            const bool timedActionFired =
                this->hasClickableFlag(CLICKABLE_FLAG_LONG_FIRED) || this->hasClickableFlag(CLICKABLE_FLAG_SUPER_LONG_FIRED);
            this->pressAge_ms = 0U;
            this->clearTimedActionFlags();
            if constexpr (StaticConfigKnown)
            {
                if constexpr ((StaticDetectionFlags & QUICK_SHORT) != 0U)
                {
                    return ClickResult::NO_CLICK;
                }
                if constexpr ((StaticDetectionFlags & SHORT_ENABLED) != 0U)
                {
                    return timedActionFired ? ClickResult::NO_CLICK : ClickResult::SHORT_CLICK;
                }
                return ClickResult::NO_CLICK;
            }
            if (hasFlag(detectionFlags, QUICK_SHORT))
            {
                return ClickResult::NO_CLICK;
            }
            if (!timedActionFired)
            {
                return hasFlag(detectionFlags, SHORT_ENABLED) ? ClickResult::SHORT_CLICK : ClickResult::NO_CLICK_NOT_SHORT_CLICKABLE;
            }
            return ClickResult::NO_CLICK;
        }

        if (!isStablePressed)
        {
            return ClickResult::NO_CLICK;
        }

        if (this->releaseDebouncePending())
        {
            return ClickResult::NO_CLICK_KEEPING_CLICKED;
        }

        this->pressAge_ms = timeUtils::addElapsedTimeSaturated(this->pressAge_ms, elapsed_ms);
        if constexpr (StaticConfigKnown)
        {
            if constexpr ((StaticDetectionFlags & LONG_ENABLED) != 0U)
            {
                if (!this->hasClickableFlag(CLICKABLE_FLAG_LONG_FIRED) && this->pressAge_ms >= StaticLongClick_ms)
                {
                    this->setClickableFlag(CLICKABLE_FLAG_LONG_FIRED, true);
                    return ClickResult::LONG_CLICK;
                }
            }

            if constexpr ((StaticDetectionFlags & SUPER_LONG_ENABLED) != 0U)
            {
                if (!this->hasClickableFlag(CLICKABLE_FLAG_SUPER_LONG_FIRED) && this->pressAge_ms >= StaticSuperLongClick_ms)
                {
                    this->setClickableFlag(CLICKABLE_FLAG_SUPER_LONG_FIRED, true);
                    return ClickResult::SUPER_LONG_CLICK;
                }
            }
        }
        else
        {
            if (hasFlag(detectionFlags, LONG_ENABLED) && !this->hasClickableFlag(CLICKABLE_FLAG_LONG_FIRED) &&
                this->pressAge_ms >= longClick_ms)
            {
                this->setClickableFlag(CLICKABLE_FLAG_LONG_FIRED, true);
                return ClickResult::LONG_CLICK;
            }

            if (hasFlag(detectionFlags, SUPER_LONG_ENABLED) && !this->hasClickableFlag(CLICKABLE_FLAG_SUPER_LONG_FIRED) &&
                this->pressAge_ms >= superLongClick_ms)
            {
                this->setClickableFlag(CLICKABLE_FLAG_SUPER_LONG_FIRED, true);
                return ClickResult::SUPER_LONG_CLICK;
            }
        }
        return ClickResult::NO_CLICK_KEEPING_CLICKED;
    }

public:
#ifndef CONFIG_USE_FAST_CLICKABLES
    /**
     * @brief Construct a new Clickable object, conventional IO version.
     *
     * @param pin pin number
     */
    explicit LSH_OPTIONAL_CONSTEXPR_CTOR Clickable(uint8_t pin) noexcept : pinNumber(pin)
    {
        pinMode(pin, INPUT);
        digitalWrite(pin, LOW);
    }

    /**
     * @brief Construct a clickable from a compile-time pin tag on the slow-I/O path.
     *
     * The tag keeps the configuration DSL uniform even when the build uses the
     * portable Arduino input path.
     */
    template <uint8_t Pin>
    explicit LSH_OPTIONAL_CONSTEXPR_CTOR Clickable(lsh::core::PinTag<Pin>) noexcept : Clickable(static_cast<uint8_t>(Pin))
    {}
#else
    /**
     * @brief Construct a new Clickable object, fast IO version.
     *
     * @param pin pin number
     */
    explicit Clickable(uint8_t pin) noexcept : Clickable(lsh::core::avr::makeFastInputPinBinding(pin))
    {}

    /**
     * @brief Construct a clickable from a compile-time pin tag on the fast-I/O path.
     *
     * On supported AVR targets the binding is resolved at compile time and then
     * cached into the exact two fields used by the polling hot path.
     */
    template <uint8_t Pin>
    explicit Clickable(lsh::core::PinTag<Pin>) noexcept : Clickable(lsh::core::avr::makeFastInputPinBinding(lsh::core::PinTag<Pin>{}))
    {}
#endif

// Delete copy constructor, copy assignment operator, move constructor and move assignment operator
#if LSH_USING_CPP17
    Clickable(const Clickable &) = delete;
    Clickable(Clickable &&) = delete;
    auto operator=(const Clickable &) -> Clickable & = delete;
    auto operator=(Clickable &&) -> Clickable & = delete;
#endif  // LSH_USING_CPP17

    /**
     * @brief Get the state of the clickable if configured as INPUT with its external pulldown resistor (PIN -> BUTTON -> +12v/+5V).
     *
     * @return true if clicked.
     * @return false if not clicked.
     */
    auto getState() const -> bool
    {
#ifdef CONFIG_USE_FAST_CLICKABLES
        // The hot scan path reads the cached register directly. This is the
        // whole point of the fast-I/O variant: no Arduino helper and no
        // extra indirection while polling buttons continuously.
        return (*this->pinPort & this->pinMask) != 0U;
#else
        return (static_cast<bool>(digitalRead(this->pinNumber)));
#endif
    }

    void setIndex(uint8_t indexToSet);  // Set the Clickable index on Clickables namespace Array

    // Getters
    [[nodiscard]] auto getIndex() const -> uint8_t;  // Get the Clickable index on Clickables namespace Array

    // Utilities
    /**
     * @brief Advance the click FSM with fully generated static configuration.
     * @details The selected TOML profile already knows which click kinds are
     *          enabled and which thresholds they use. Passing those values as
     *          constants keeps the object free from static configuration bytes
     *          and lets the generated scan path constant-fold disabled checks.
     *
     * @param elapsed_ms Milliseconds elapsed since the previous clickable scan.
     * @param detectionFlags Bit mask from `constants::clickDetection`.
     * @param longClick_ms Long-click threshold for this clickable.
     * @param superLongClick_ms Super-long-click threshold for this clickable.
     * @return constants::ClickResult The detected click event, or `NO_CLICK`.
     */
    [[nodiscard]] auto clickDetection(uint16_t elapsed_ms, uint8_t detectionFlags, uint16_t longClick_ms, uint16_t superLongClick_ms)
        -> constants::ClickResult
    {
        return this->clickDetectionImpl<false, 0U, 0U, 0U>(elapsed_ms, detectionFlags, longClick_ms, superLongClick_ms);
    }

    /**
     * @brief Advance the click FSM with fully generated compile-time constants.
     */
    template <uint8_t DetectionFlags, uint16_t LongClick_ms, uint16_t SuperLongClick_ms>
    [[nodiscard]] auto clickDetection(uint16_t elapsed_ms) -> constants::ClickResult
    {
        return this->clickDetectionImpl<true, DetectionFlags, LongClick_ms, SuperLongClick_ms>(elapsed_ms, 0U, 0U, 0U);
    }
};

#endif  // LSH_CORE_PERIPHERALS_INPUT_CLICKABLE_HPP
