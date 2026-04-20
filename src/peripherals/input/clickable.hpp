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
#include "internal/user_config_bridge.hpp"
#include "util/constants/click_results.hpp"
#include "util/constants/click_types.hpp"
#include "util/constants/timing.hpp"

namespace Clickables
{
void finalizeActuatorLinkStorage();
}

struct ClickableActuatorLinkEntry;

/**
 * @brief Lightweight read-only view over one clickable actuator-link list.
 *
 * The clickable runtime stores actuator links in compact shared pools to avoid
 * reserving three full `etl::vector` buffers inside every Clickable object.
 * This view exposes the final per-clickable slice as a small iterable object
 * without leaking the internal pool layout to callers.
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
        explicit Iterator(const ClickableActuatorLinkEntry *entry = nullptr) noexcept;

        [[nodiscard]] auto operator*() const -> uint8_t;
        auto operator++() -> Iterator &;
        [[nodiscard]] auto operator==(const Iterator &other) const -> bool;
        [[nodiscard]] auto operator!=(const Iterator &other) const -> bool;

    private:
        const ClickableActuatorLinkEntry *currentEntry = nullptr;  //!< Current compact link entry visited by the iterator.
    };

    ClickableActuatorLinksView() = default;
    ClickableActuatorLinksView(const ClickableActuatorLinkEntry *entriesBegin, uint8_t linkCount) noexcept;

    [[nodiscard]] auto begin() const -> Iterator;
    [[nodiscard]] auto end() const -> Iterator;
    [[nodiscard]] auto empty() const -> bool;
    [[nodiscard]] auto size() const -> uint8_t;

private:
    const ClickableActuatorLinkEntry *entries = nullptr;  //!< Pointer to the first compact link entry of the exposed slice.
    uint8_t count = 0U;                                   //!< Number of valid compact link entries in the slice.
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
        uint8_t isShortClickable : 1;             //!< True if the clickable is short clickable
        uint8_t isLongClickable : 1;              //!< True if the clickable is long clickable
        uint8_t isSuperLongClickable : 1;         //!< True if the clickable is super long clickable
        uint8_t isNetworkLongClickable : 1;       //!< True if this clickable has long click network actuators
        uint8_t isNetworkSuperLongClickable : 1;  //!< True if this clickable has super long click network actuators
        uint8_t isQuickClickable : 1;             //!< Derived flag. True for a short click on press when no long/super-long is configured.
        uint8_t isValid : 1;                      //!< True if it is either clickable, long clickable or super long clickable
        uint8_t isChecked : 1;                    //!< True if we checked at least once the clickable validity

        LSH_CONSTEXPR ClickableConfigFlags() noexcept :
            isShortClickable(true), isLongClickable(false), isSuperLongClickable(false), isNetworkLongClickable(false),
            isNetworkSuperLongClickable(false), isQuickClickable(false), isValid(false), isChecked(false)
        {}
    };

    // Bitfield for configuration flags to save RAM. "Hot" data, read every detection.
    ClickableConfigFlags configFlags;

#ifndef CONFIG_USE_FAST_CLICKABLES
    const uint8_t pinNumber;  //!< The pin to which the clickable is connected to, for conventional IO
#else
    const uint8_t pinMask;                  //!< Mask of the clickable, for fast IO
    const volatile uint8_t *const pinPort;  //!< Port of the clickable, for fast IO
#endif
    uint8_t index = UINT8_MAX;  //!< Clickable index on Clickables namespace Array, or `UINT8_MAX` until registration succeeds.
    const uint8_t id;           //!< Unique ID of the clickable (integer)

    // State variables for the FSM ("hot" data, move to top for <64 byte offset optimization).
    State currentState = State::IDLE;                 //!< The current state of the FSM.
    uint16_t stateAge_ms = 0U;                        //!< Elapsed time spent in the current non-idle state, saturated at 65535 ms.
    ActionFired lastActionFired = ActionFired::NONE;  //!< Tracks which timed action has already been triggered.

    // Timings ("hot" configuration, move to top)
    uint16_t longClick_ms = constants::timings::CLICKABLE_LONG_CLICK_TIME_MS;             //!< Long click time in ms
    uint16_t superLongClick_ms = constants::timings::CLICKABLE_SUPER_LONG_CLICK_TIME_MS;  //!< Super long click time in ms
protected:
    uint8_t debounce_ms = constants::timings::CLICKABLE_DEBOUNCE_TIME_MS;  //!< Debounce time in ms
private:
    // Non-boolean configuration properties (Cold data, rarely accessed in tight loops)
    constants::LongClickType longClickType = constants::LongClickType::NONE;                 //!< Long clickability setting
    constants::NoNetworkClickType longClickFallback = constants::NoNetworkClickType::NONE;   //!< Fallback for long click over network
    constants::SuperLongClickType superLongClickType = constants::SuperLongClickType::NONE;  //!< Super long clickability setting
    constants::NoNetworkClickType superLongClickFallback =
        constants::NoNetworkClickType::NONE;  //!< Fallback for super long click over network

    // Compact actuator-link metadata.
    // The real link entries live in shared pools managed by clickable.cpp.
    uint16_t shortLinksOffset = 0U;      //!< Offset of the first short-click actuator link inside the shared pool.
    uint16_t longLinksOffset = 0U;       //!< Offset of the first long-click actuator link inside the shared pool.
    uint16_t superLongLinksOffset = 0U;  //!< Offset of the first super-long-click actuator link inside the shared pool.
    uint8_t shortLinksCount = 0U;        //!< Number of short-click actuator links attached to this clickable.
    uint8_t longLinksCount = 0U;         //!< Number of long-click actuator links attached to this clickable.
    uint8_t superLongLinksCount = 0U;    //!< Number of super-long-click actuator links attached to this clickable.

public:
#ifndef CONFIG_USE_FAST_CLICKABLES
    /**
     * @brief Construct a new Clickable object, conventional IO version.
     *
     * @param pin pin number
     * @param uniqueId unique ID of the clickable.
     */
    explicit LSH_OPTIONAL_CONSTEXPR_CTOR Clickable(uint8_t pin, uint8_t uniqueId) noexcept : pinNumber(pin), id(uniqueId)
    {}
#else
    /**
     * @brief Construct a new Clickable object, fast IO version.
     *
     * @param pin pin number
     * @param uniqueId unique ID of the clickable.
     */
    explicit Clickable(uint8_t pin, uint8_t uniqueId) noexcept :
        pinMask(digitalPinToBitMask(pin)), pinPort(portInputRegister(digitalPinToPort(pin))), id(uniqueId)
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
        return (*this->pinPort & this->pinMask) != 0U;
#else
        return (static_cast<bool>(digitalRead(this->pinNumber)));
#endif
    }

    void setIndex(uint8_t indexToSet);  // Set the Clickable index on Clickables namespace Array

    // Clickability setters
    auto setClickableShort(bool shortClickable) -> Clickable &;  // Set the short clickability of the clickable
    auto setClickableLong(bool longClickable,
                          constants::LongClickType clickType = constants::LongClickType::NORMAL,
                          bool networkClickable = false,
                          constants::NoNetworkClickType fallback = constants::NoNetworkClickType::LOCAL_FALLBACK)
        -> Clickable &;  // Set long click network clickability
    auto setClickableSuperLong(bool superLongClickable,
                               constants::SuperLongClickType clickType = constants::SuperLongClickType::NORMAL,
                               bool networkClickable = false,
                               constants::NoNetworkClickType fallback = constants::NoNetworkClickType::LOCAL_FALLBACK)
        -> Clickable &;  // Set super long click network clickability

    // Actuators setter
    auto addActuator(uint8_t actuatorIndex,
                     constants::ClickType actuatorType) -> Clickable &;  // Adds one actuator link after clickable registration
    auto addActuatorShort(uint8_t actuatorIndex) -> Clickable &;         // Add a short click actuator
    auto addActuatorLong(uint8_t actuatorIndex) -> Clickable &;          // Add a long click actuator
    auto addActuatorSuperLong(uint8_t actuatorIndex) -> Clickable &;     // Add a super long click actuator

    // Timing setters
    auto setDebounceTime(uint8_t timeToSet_ms) -> Clickable &;         // Set debounce time in ms (0-255)
    auto setLongClickTime(uint16_t timeToSet_ms) -> Clickable &;       // Set long click time in ms (0-65535)
    auto setSuperLongClickTime(uint16_t timeToSet_ms) -> Clickable &;  // Set super long click time in ms (0-65535)

    // Getters
    [[nodiscard]] auto getIndex() const -> uint8_t;  // Get the Clickable index on Clickables namespace Array
    [[nodiscard]] auto getId() const -> uint8_t;     // Return unique ID of the clickable
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
    void setActuatorLinksOffset(constants::ClickType actuatorType,
                                uint16_t offsetToSet);  // Stores the compact shared-pool offset for one actuator-link list.
};

#endif  // LSH_CORE_PERIPHERALS_INPUT_CLICKABLE_HPP
