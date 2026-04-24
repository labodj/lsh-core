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
#include "util/constants/click_results.hpp"
#include "util/constants/click_types.hpp"
#include "util/constants/timing.hpp"

namespace Clickables
{
void finalizeActuatorLinkStorage();
}

/**
 * @brief Lightweight read-only view over one clickable actuator-link list.
 *
 * Generated static-profile accessors own the actual link topology. This view
 * keeps callers on the old iterable API while storing only the owner index,
 * click kind and link count in the temporary view object.
 */
class ClickableActuatorLinksView
{
public:
    /**
     * @brief Iterator that walks the compact actuator-link entries.
     */
    class Iterator
    {
    public:
        explicit Iterator(uint8_t ownerIndex = UINT8_MAX,
                          constants::ClickType linkType = constants::ClickType::SHORT,
                          uint8_t relativeIndex = 0U) noexcept;

        [[nodiscard]] auto operator*() const -> uint8_t;
        auto operator++() -> Iterator &;
        [[nodiscard]] auto operator==(const Iterator &other) const -> bool;
        [[nodiscard]] auto operator!=(const Iterator &other) const -> bool;

    private:
        uint8_t clickableIndex = UINT8_MAX;                            //!< Owner clickable dense index.
        constants::ClickType clickType = constants::ClickType::SHORT;  //!< Link slice kind visited by this iterator.
        uint8_t linkIndex = 0U;                                        //!< Current relative link index inside the generated static slice.
    };

    ClickableActuatorLinksView() = default;
    ClickableActuatorLinksView(uint8_t ownerIndex, constants::ClickType linkType, uint8_t linkCount) noexcept;

    [[nodiscard]] auto begin() const -> Iterator;
    [[nodiscard]] auto end() const -> Iterator;
    [[nodiscard]] auto empty() const -> bool;
    [[nodiscard]] auto size() const -> uint8_t;

private:
    uint8_t clickableIndex = UINT8_MAX;                            //!< Owner clickable dense index.
    constants::ClickType clickType = constants::ClickType::SHORT;  //!< Generated static link slice kind.
    uint8_t count = 0U;                                            //!< Number of valid generated link entries in the slice.
};

/**
 * @brief A class that represents a clickable object, like a button, and its associated logic.
 *
 */
class Clickable
{
private:
    /**
     * @brief Explicit states for the finite state machine (FSM).
     *
     */
    enum class State : uint8_t
    {
        IDLE,        //!< The button is not pressed, waiting for a press.
        DEBOUNCING,  //!< A change was detected, waiting for the signal to stabilize.
        PRESSED,     //!< The press is confirmed and stable. Timing for long/super-long actions.
        RELEASED     //!< The button was just released. A transient state to determine the final action.
    };

    /**
     * @brief This enum is used to track which timed action was fired during a press sequence.
     *
     */
    enum class ActionFired : uint8_t
    {
        NONE,
        LONG,
        SUPER_LONG
    };

    // Bitfield for configuration flags to save RAM. These are "cold" data, mostly read after setup.
    // Data structures definitions
    struct ClickableConfigFlags
    {
        uint8_t isShortClickable : 1;              //!< True if the clickable is short clickable
        uint8_t isLongClickable : 1;               //!< True if the clickable is long clickable
        uint8_t isSuperLongClickable : 1;          //!< True if the clickable is super long clickable
        uint8_t isNetworkLongClickable : 1;        //!< True if this clickable has long click network actuators
        uint8_t isNetworkSuperLongClickable : 1;   //!< True if this clickable has super long click network actuators
        uint8_t isQuickClickable : 1;              //!< Derived flag. True for a short click on press when no long/super-long is configured.
        uint8_t longNetworkFallbackDoNothing : 1;  //!< True when long network-click fallback must skip local action.
        uint8_t superLongNetworkFallbackDoNothing : 1;  //!< True when super-long network-click fallback must skip local action.

        LSH_CONSTEXPR ClickableConfigFlags() noexcept :
            isShortClickable(true), isLongClickable(false), isSuperLongClickable(false), isNetworkLongClickable(false),
            isNetworkSuperLongClickable(false), isQuickClickable(true), longNetworkFallbackDoNothing(false),
            superLongNetworkFallbackDoNothing(false)
        {}
    };

    // Bitfield for configuration flags to save RAM. "Hot" data, read every detection.
    // Keeping the whole bitfield near the start of the object helps the click
    // scanner on AVR reach the most queried configuration with short offsets.
    ClickableConfigFlags configFlags;

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
    {}
#endif
    uint8_t index = UINT8_MAX;  //!< Clickable index on Clickables namespace Array, or `UINT8_MAX` until registration succeeds.

    // State variables for the FSM ("hot" data, move to top for <64 byte offset optimization).
    State currentState = State::IDLE;                 //!< The current state of the FSM.
    uint16_t stateAge_ms = 0U;                        //!< Elapsed time spent in the current non-idle state, saturated at 65535 ms.
    ActionFired lastActionFired = ActionFired::NONE;  //!< Tracks which timed action has already been triggered.

#if !CONFIG_USE_CLICKABLE_TIMING_POOL
    // Timings ("hot" configuration, move to top)
    uint16_t longClick_ms = constants::timings::CLICKABLE_LONG_CLICK_TIME_MS;             //!< Long click time in ms
    uint16_t superLongClick_ms = constants::timings::CLICKABLE_SUPER_LONG_CLICK_TIME_MS;  //!< Super long click time in ms
#endif

    [[nodiscard]] auto getLongClickTime() const -> uint16_t;
    [[nodiscard]] auto getSuperLongClickTime() const -> uint16_t;
    void refreshQuickClickability();

public:
#ifndef CONFIG_USE_FAST_CLICKABLES
    /**
     * @brief Construct a new Clickable object, conventional IO version.
     *
     * @param pin pin number
     */
    explicit LSH_OPTIONAL_CONSTEXPR_CTOR Clickable(uint8_t pin) noexcept : pinNumber(pin)
    {}

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
    inline auto getState() const -> bool
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

    // Clickability setters
    auto setClickableShort(bool shortClickable) -> Clickable &;  // Set the short clickability of the clickable
    auto setClickableLong(bool longClickable,
                          bool networkClickable = false,
                          constants::NoNetworkClickType fallback = constants::NoNetworkClickType::LOCAL_FALLBACK)
        -> Clickable &;  // Set long click network clickability
    auto setClickableLong(bool longClickable,
                          constants::LongClickType clickType,
                          bool networkClickable = false,
                          constants::NoNetworkClickType fallback = constants::NoNetworkClickType::LOCAL_FALLBACK)
        -> Clickable &;  // Source-compatible overload; TOML stores the click type statically.
    auto setClickableSuperLong(bool superLongClickable,
                               bool networkClickable = false,
                               constants::NoNetworkClickType fallback = constants::NoNetworkClickType::LOCAL_FALLBACK)
        -> Clickable &;  // Set super long click network clickability
    auto setClickableSuperLong(bool superLongClickable,
                               constants::SuperLongClickType clickType,
                               bool networkClickable = false,
                               constants::NoNetworkClickType fallback = constants::NoNetworkClickType::LOCAL_FALLBACK)
        -> Clickable &;  // Source-compatible overload; TOML stores the click type statically.

    // Actuators setter
    auto addActuator(uint8_t actuatorIndex, constants::ClickType actuatorType)
        -> Clickable &;                                               // Kept for source compatibility; TOML links are static.
    auto addActuatorShort(uint8_t actuatorIndex) -> Clickable &;      // Add a short click actuator
    auto addActuatorLong(uint8_t actuatorIndex) -> Clickable &;       // Add a long click actuator
    auto addActuatorSuperLong(uint8_t actuatorIndex) -> Clickable &;  // Add a super long click actuator

    // Timing setters
    auto setLongClickTime(uint16_t timeToSet_ms) -> Clickable &;       // Set long click time in ms (0-65535)
    auto setSuperLongClickTime(uint16_t timeToSet_ms) -> Clickable &;  // Set super long click time in ms (0-65535)

    // Getters
    [[nodiscard]] auto getIndex() const -> uint8_t;  // Get the Clickable index on Clickables namespace Array
    [[nodiscard]] auto getActuators(constants::ClickType actuatorType) const
        -> ClickableActuatorLinksView;  // Returns the read-only view over one attached actuator-link list
    [[nodiscard]] auto getTotalActuators(constants::ClickType actuatorType) const
        -> uint8_t;                                                           // Returns the total number of a kind of actuators
    [[nodiscard]] auto getLongClickType() const -> constants::LongClickType;  // Returns the type of long click
    [[nodiscard]] auto isNetworkClickable(constants::ClickType clickType) const
        -> bool;  // Returns if the clickable is network clickable for long or super long click actions
    [[nodiscard]] auto getNetworkFallback(constants::ClickType clickType) const
        -> constants::NoNetworkClickType;  // Returns the fallback type for a give network click type
    [[nodiscard]] auto getSuperLongClickType() const -> constants::SuperLongClickType;  // Returns the type of super long click

    // Utilities
    auto check() -> bool;  // Check for clickable coherence, it has to be clickable, it has to be connected to something
    [[nodiscard]] auto shortClick() const -> bool;               // Perform a short click action
    [[nodiscard]] auto longClick() const -> bool;                // Perform a long click action
    [[nodiscard]] auto superLongClickSelective() const -> bool;  // Perform a selective super long click;
    [[nodiscard]] auto clickDetection(uint16_t elapsed_ms)
        -> constants::ClickResult;  // Advances the click FSM using the elapsed scan time and returns the detected click type.
};

#endif  // LSH_CORE_PERIPHERALS_INPUT_CLICKABLE_HPP
