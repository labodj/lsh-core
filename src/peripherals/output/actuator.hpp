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

#include "device/actuator_manager.hpp"
#include "internal/cpp_features.hpp"
#include "internal/pin_tag.hpp"
#include "internal/user_config_bridge.hpp"
#ifdef CONFIG_USE_FAST_ACTUATORS
#include "internal/avr_fast_io.hpp"
#endif
#include "util/constants/timing.hpp"
#include "util/time_keeper.hpp"

#if !CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES && (LSH_EFFECTIVE_ACTUATOR_DEBOUNCE_TIME_MS != 0U || LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS > 0)
#define LSH_CORE_ACTUATOR_NEEDS_LOCAL_SWITCH_TIME 1
#else
#define LSH_CORE_ACTUATOR_NEEDS_LOCAL_SWITCH_TIME 0
#endif

#if LSH_CORE_ACTUATOR_NEEDS_LOCAL_SWITCH_TIME || (CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES && LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS > 0)
#define LSH_CORE_ACTUATOR_NEEDS_SWITCH_TIMESTAMP 1
#else
#define LSH_CORE_ACTUATOR_NEEDS_SWITCH_TIMESTAMP 0
#endif

/**
 * @brief Represents an actuator (relay) attached to a digital pin.
 *
 */
class Actuator
{
private:
    static constexpr uint8_t ACTUATOR_FLAG_ACTUAL_STATE = 0x01U;
    static constexpr uint8_t ACTUATOR_FLAG_PROTECTED = 0x02U;

    static constexpr auto initialFlags(bool normalState) noexcept -> uint8_t
    {
        return normalState ? ACTUATOR_FLAG_ACTUAL_STATE : 0U;
    }

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
    explicit Actuator(lsh::core::avr::FastOutputPinBinding binding, bool normalState) noexcept :
        pinMask(binding.mask), pinPort(binding.pinPort), flags(initialFlags(normalState))
    {
        // Prime the output latch before enabling the pin as OUTPUT. That keeps
        // boot-time electrical transitions deterministic even if the previous
        // firmware or bootloader left the port register in an unexpected state.
        const uint8_t oldSREG = SREG;
        cli();
        if (!normalState)
        {
            *this->pinPort &= ~this->pinMask;
        }
        else
        {
            *this->pinPort |= this->pinMask;
        }
        *binding.modePort |= this->pinMask;
        SREG = oldSREG;
    }
#endif
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
    uint8_t index = UINT8_MAX;  //!< Debug/runtime-check registration index; stripped from release objects.
#endif
    uint8_t flags = 0U;  //!< Packed default/current/protection flags.
#if LSH_CORE_ACTUATOR_NEEDS_LOCAL_SWITCH_TIME
    uint32_t lastTimeSwitched = 0U;  //!< Last time the actuator performed a switch
#endif
    /**
     * @brief Drive the physical output pin using the configured I/O backend.
     *
     * @details The generated static path, the public runtime path and tests all
     *          share this helper so the direct-port and Arduino fallbacks cannot
     *          drift semantically.
     */
    void writePinState(bool state)
    {
#ifdef CONFIG_USE_FAST_ACTUATORS
        if (!state)
        {
            *this->pinPort &= ~this->pinMask;
        }
        else
        {
            *this->pinPort |= this->pinMask;
        }
#else
        digitalWrite(this->pinNumber, static_cast<uint8_t>(state));
#endif
    }

    /**
     * @brief Mirror the physical state into the packed object-local flag byte.
     *
     * @details Keeping this separate from `writePinState()` makes the ordering
     *          explicit: the pin is switched first, then software state is made
     *          visible to later generated logic.
     */
    void updateCachedStateFlag(bool state)
    {
        if (state)
        {
            this->flags |= ACTUATOR_FLAG_ACTUAL_STATE;
        }
        else
        {
            this->flags &= static_cast<uint8_t>(~ACTUATOR_FLAG_ACTUAL_STATE);
        }
    }

    /**
     * @brief Return whether applying `state` would actually change the relay.
     *
     * @details This is deliberately an 8-bit flag test so hot paths can reject
     *          no-op writes before reading `millis()` or evaluating debounce.
     */
    [[nodiscard]] auto wouldChangeState(bool state) const -> bool
    {
        const uint8_t stateFlag = state ? ACTUATOR_FLAG_ACTUAL_STATE : 0U;
        return (this->flags & ACTUATOR_FLAG_ACTUAL_STATE) != stateFlag;
    }

    /**
     * @brief Return true when debounce allows a real state transition.
     *
     * @details When debounce and local switch timestamps compile out, this helper
     *          becomes a constant `true`; the `now_ms` argument is consumed only
     *          to keep one shared call shape for static and runtime paths.
     */
    [[nodiscard]] auto debounceAllowsSwitch(uint32_t now_ms) const -> bool
    {
#if LSH_CORE_ACTUATOR_NEEDS_LOCAL_SWITCH_TIME
        using constants::timings::ACTUATOR_DEBOUNCE_TIME_MS;
        return ACTUATOR_DEBOUNCE_TIME_MS == 0U || (now_ms - this->lastTimeSwitched >= ACTUATOR_DEBOUNCE_TIME_MS);
#else
        static_cast<void>(now_ms);
        return true;
#endif
    }

    /**
     * @brief Apply one state transition through the runtime-index path.
     *
     * @details This path exists for public object APIs and debug/runtime-check
     *          builds. Generated release code should prefer
     *          `applyStateChangeStatic<ActuatorIndex>()` so the index, packed
     *          state byte and optional compact switch-time slot remain
     *          compile-time facts.
     */
    [[nodiscard]] auto applyStateChange(bool state, uint32_t now_ms, uint8_t actuatorIndex) -> bool;

    /**
     * @brief Apply one state transition with the dense actuator index as a type.
     *
     * @details Static profiles know `ActuatorIndex` at generation time. Passing
     *          it as a template argument lets AVR-GCC fold the packed-state
     *          update and the optional compact auto-off timestamp recording into
     *          direct byte/bit accesses.
     */
    template <uint8_t ActuatorIndex>
    [[nodiscard]] __attribute__((always_inline)) inline auto applyStateChangeStatic(bool state, uint32_t now_ms) -> bool
    {
        static_assert(ActuatorIndex < CONFIG_MAX_ACTUATORS, "ActuatorIndex is outside the generated static profile.");
        if (!this->debounceAllowsSwitch(now_ms))
        {
            return false;
        }

        this->writePinState(state);
        this->updateCachedStateFlag(state);
#if LSH_CORE_ACTUATOR_NEEDS_LOCAL_SWITCH_TIME
        this->lastTimeSwitched = now_ms;
#endif
        Actuators::updatePackedStateStatic<ActuatorIndex>(state);
#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES && LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS > 0
        Actuators::recordSwitchTime(ActuatorIndex, now_ms);
#endif
        return true;
    }

    /**
     * @brief Return the runtime-registration index only in builds that keep it.
     *
     * Release generated paths call `setStateStatic<index>()`, so storing the
     * dense index in every actuator would waste one SRAM byte per relay. The
     * public non-static setters remain usable for direct object tests, but they
     * deliberately cannot update the generated packed-state shadow in a stripped
     * release object because the index byte no longer exists.
     */
    [[nodiscard]] auto runtimeIndex() const -> uint8_t
    {
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
        return this->index;
#else
        return UINT8_MAX;
#endif
    }

public:
#ifndef CONFIG_USE_FAST_ACTUATORS
    /**
     * @brief Construct a new Actuator object, conventional IO version.
     *
     * @param pin pin number
     * @param normalState the default state of the actuator.
     */
    explicit LSH_OPTIONAL_CONSTEXPR_CTOR Actuator(uint8_t pin, bool normalState = false) noexcept :
        pinNumber(pin), flags(initialFlags(normalState))
    {
        // Set the output latch first, then enable OUTPUT. This mirrors the fast
        // path and avoids a short wrong-level pulse during construction.
        digitalWrite(pin, static_cast<uint8_t>(normalState));
        pinMode(pin, OUTPUT);
    }

    /**
     * @brief Construct an actuator from a compile-time pin tag on the slow-I/O path.
     *
     * The tag keeps the public macro surface uniform even when fast I/O is
     * disabled or unavailable for the current build target.
     */
    template <uint8_t Pin>
    explicit LSH_OPTIONAL_CONSTEXPR_CTOR Actuator(lsh::core::PinTag<Pin>, bool normalState = false) noexcept :
        Actuator(static_cast<uint8_t>(Pin), normalState)
    {}
#else
    /**
     * @brief Construct a new Actuator object, fast IO version.
     *
     * @param pin pin number
     * @param normalState the default state of the actuator.
     */
    explicit Actuator(uint8_t pin, bool normalState = false) noexcept : Actuator(lsh::core::avr::makeFastOutputPinBinding(pin), normalState)
    {}

    /**
     * @brief Construct an actuator from a compile-time pin tag on the fast-I/O path.
     *
     * On supported AVR boards this resolves the port and mask without touching
     * the Arduino PROGMEM lookup tables in the translation unit that instantiates
     * the actuator.
     */
    template <uint8_t Pin>
    explicit Actuator(lsh::core::PinTag<Pin>, bool normalState = false) noexcept :
        Actuator(lsh::core::avr::makeFastOutputPinBinding(lsh::core::PinTag<Pin>{}), normalState)
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
    [[nodiscard]] auto setState(bool state, uint32_t now_ms)
        -> bool;  // Sets the new state using a caller-cached timestamp for generated multi-actuator paths.
    [[nodiscard]] auto setStateForIndex(uint8_t actuatorIndex, bool state)
        -> bool;  // Sets state while providing the generated dense actuator index.
    [[nodiscard]] auto setStateForIndex(uint8_t actuatorIndex, bool state, uint32_t now_ms)
        -> bool;  // Sets state with generated dense index and caller-cached timestamp.

    template <uint8_t ActuatorIndex> [[nodiscard]] auto setStateStatic(bool state) -> bool
    {
        if (!this->wouldChangeState(state))
        {
            return false;
        }
#if LSH_CORE_ACTUATOR_NEEDS_SWITCH_TIMESTAMP
        return this->applyStateChangeStatic<ActuatorIndex>(state, timeKeeper::getTime());
#else
        return this->applyStateChangeStatic<ActuatorIndex>(state, 0U);
#endif
    }

    template <uint8_t ActuatorIndex> [[nodiscard]] auto setStateStatic(bool state, uint32_t now_ms) -> bool
    {
        if (!this->wouldChangeState(state))
        {
            return false;
        }
        return this->applyStateChangeStatic<ActuatorIndex>(state, now_ms);
    }

    void setIndex(uint8_t indexToSet);  // Set the actuator index on Actuators namespace Array
    auto setProtected(bool hasProtection)
        -> Actuator &;  // Set protection against global "turn-off" actions (e.g., a general super long click).

    // Getters
    [[nodiscard]] auto getIndex() const -> uint8_t;  // Get the actuator index on Actuators namespace Array
    [[nodiscard]] auto getState() const -> bool;     // Returns the state of the actuator (false=OFF, true=ON)

    // Utils
    [[nodiscard]] auto toggleState() -> bool;                 // Switch the actuator
    [[nodiscard]] auto toggleState(uint32_t now_ms) -> bool;  // Switch the actuator using a caller-cached timestamp
    [[nodiscard]] auto toggleStateForIndex(uint8_t actuatorIndex) -> bool;
    [[nodiscard]] auto toggleStateForIndex(uint8_t actuatorIndex, uint32_t now_ms) -> bool;

    template <uint8_t ActuatorIndex> [[nodiscard]] auto toggleStateStatic() -> bool
    {
        return this->setStateStatic<ActuatorIndex>((this->flags & ACTUATOR_FLAG_ACTUAL_STATE) == 0U);
    }

    template <uint8_t ActuatorIndex> [[nodiscard]] auto toggleStateStatic(uint32_t now_ms) -> bool
    {
        return this->setStateStatic<ActuatorIndex>((this->flags & ACTUATOR_FLAG_ACTUAL_STATE) == 0U, now_ms);
    }

    [[nodiscard]] auto checkAutoOffTimer(uint32_t now_ms, uint32_t autoOffTimer_ms)
        -> bool;  // Checks the provided auto-off timer using a caller-cached timestamp.
    [[nodiscard]] auto checkAutoOffTimerForIndex(uint8_t actuatorIndex, uint32_t now_ms, uint32_t autoOffTimer_ms)
        -> bool;  // Checks auto-off while providing the generated dense actuator index.
};

#endif  // LSH_CORE_PERIPHERALS_OUTPUT_ACTUATOR_HPP
