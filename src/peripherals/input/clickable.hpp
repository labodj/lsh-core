/**
 * @file    clickable.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Defines the Clickable class for handling button inputs and click logic.
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

#ifndef LSHCORE_PERIPHERALS_INPUT_CLICKABLE_HPP
#define LSHCORE_PERIPHERALS_INPUT_CLICKABLE_HPP

#include <etl/vector.h>
#include <stdint.h>

#include "internal/user_config_bridge.hpp"
#include "util/constants/clickresults.hpp"
#include "util/constants/clicktypes.hpp"
#include "util/constants/timing.hpp"

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
        IDLE,       //!< The button is not pressed, waiting for a press.
        DEBOUNCING, //!< A change was detected, waiting for the signal to stabilize.
        PRESSED,    //!< The press is confirmed and stable. Timing for long/super-long actions.
        RELEASED    //!< The button was just released. A transient state to determine the final action.
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
    struct ClickableConfigFlags
    {
        uint8_t isShortClickable : 1 = true;             //!< True if the clickable is short clickable
        uint8_t isLongClickable : 1 = false;             //!< True if the clickable is long clickable
        uint8_t isSuperLongClickable : 1 = false;        //!< True if the clickable is super long clickable
        uint8_t isNetworkLongClickable : 1 = false;      //!< True if this clickable has long click network actuators
        uint8_t isNetworkSuperLongClickable : 1 = false; //!< True if this clickable has super long click network actuators
        uint8_t isQuickClickable : 1 = false;            //!< Derived flag. True for a short click on press when no long/super-long is configured.
        uint8_t isValid : 1 = false;                     //!< True if it is either clickable, long clickable or super long clickable
        uint8_t isChecked : 1 = false;                   //!< True if we checked at least once the clickable validity
    };

#ifndef CONFIG_USE_FAST_CLICKABLES
    const uint8_t pinNumber; //!< The pin to which the clickable is connected to, for conventional IO
#else
    const uint8_t pinMask;                 //!< Mask of the clickable, for fast IO
    const volatile uint8_t *const pinPort; //!< Port of the clickable, for fast IO
#endif
    uint8_t index = 0U; //!< Clickable index on Clickables namespace Array
    const uint8_t id;   //!< Unique ID of the clickable (integer)

    ClickableConfigFlags configFlags; //!< Configuration is stored in the bitfield.

    // Non-boolean configuration properties remain separate.
    constants::LongClickType longClickType = constants::LongClickType::NONE;                    //!< Long clickability setting
    constants::NoNetworkClickType longClickFallback = constants::NoNetworkClickType::NONE;      //!< Fallback for long click over network
    constants::SuperLongClickType superLongClickType = constants::SuperLongClickType::NONE;     //!< Super long clickability setting
    constants::NoNetworkClickType superLongClickFallback = constants::NoNetworkClickType::NONE; //!< Fallback for super long click over network

    // State variables for the FSM remain separate for fast access ("hot" data).
    State currentState = State::IDLE;                //!< The current state of the FSM.
    uint32_t stateChangeTime = 0U;                   //!< Timestamp of the last state change.
    ActionFired lastActionFired = ActionFired::NONE; //!< Tracks which timed action has already been triggered.

    // Attached actuators
    etl::vector<uint8_t, CONFIG_MAX_ACTUATORS> actuatorsShort{};     //!< Actuators controlled via short click
    etl::vector<uint8_t, CONFIG_MAX_ACTUATORS> actuatorsLong{};      //!< Actuators controlled via long click
    etl::vector<uint8_t, CONFIG_MAX_ACTUATORS> actuatorsSuperLong{}; //!< Actuators controlled via super long click

    // Timings
    uint16_t longClick_ms = constants::timings::CLICKABLE_LONG_CLICK_TIME_MS;            //!< Long click time in ms
    uint16_t superLongClick_ms = constants::timings::CLICKABLE_SUPER_LONG_CLICK_TIME_MS; //!< Super long click time in ms

protected:
    uint8_t debounce_ms = constants::timings::CLICKABLE_DEBOUNCE_TIME_MS; //!< Debounce time in ms

public:
#ifndef CONFIG_USE_FAST_CLICKABLES
    /**
     * @brief Construct a new Clickable object, conventional IO version.
     *
     * @param pin pin number
     * @param uniqueId unique ID of the clickable.
     */
    explicit constexpr Clickable(uint8_t pin, uint8_t uniqueId) noexcept : pinNumber(pin), id(uniqueId) {}
#else
    /**
     * @brief Construct a new Clickable object, fast IO version.
     *
     * @param pin pin number
     * @param uniqueId unique ID of the clickable.
     */
    explicit Clickable(uint8_t pin, uint8_t uniqueId) noexcept : pinMask(digitalPinToBitMask(pin)), pinPort(portInputRegister(digitalPinToPort(pin))), id(uniqueId) {}
#endif

// Delete copy constructor, copy assignment operator, move constructor and move assignment operator
#if (__cplusplus >= 201703L) && (__GNUC__ >= 7)
    Clickable(const Clickable &) = delete;
    Clickable(Clickable &&) = delete;
    auto operator=(const Clickable &) -> Clickable & = delete;
    auto operator=(Clickable &&) -> Clickable & = delete;
#endif // (__cplusplus >= 201703L) && (__GNUC__ >= 7)

    [[nodiscard]] auto getState() -> bool; // Get the state of the clickable

    void setIndex(uint8_t indexToSet); // Set the Clickable index on Clickables namespace Array

    // Clickability setters
    auto setClickableShort(bool shortClickable) -> Clickable &; // Set the short clickability of the clickable
    auto setClickableLong(bool longClickable,
                          constants::LongClickType clickType = constants::LongClickType::NORMAL,
                          bool networkClickable = false,
                          constants::NoNetworkClickType fallback = constants::NoNetworkClickType::LOCAL_FALLBACK) -> Clickable &; // Set long click network clickability
    auto setClickableSuperLong(bool superLongClickable,
                               constants::SuperLongClickType clickType = constants::SuperLongClickType::NORMAL,
                               bool networkClickable = false,
                               constants::NoNetworkClickType fallback = constants::NoNetworkClickType::LOCAL_FALLBACK) -> Clickable &; // Set super long click network clickability

    // Actuators setter
    auto addActuator(uint8_t actuatorIndex, constants::ClickType actuatorType) -> Clickable &; // Add an actuator
    auto addActuatorShort(uint8_t actuatorIndex) -> Clickable &;                               // Add a short click actuator
    auto addActuatorLong(uint8_t actuatorIndex) -> Clickable &;                                // Add a long click actuator
    auto addActuatorSuperLong(uint8_t actuatorIndex) -> Clickable &;                           // Add a super long click actuator

    // Timing setters
    auto setDebounceTime(uint8_t timeToSet_ms) -> Clickable &;        // Set debounce time in ms (0-255)
    auto setLongClickTime(uint16_t timeToSet_ms) -> Clickable &;      // Set long click time in ms (0-65535)
    auto setSuperLongClickTime(uint16_t timeToSet_ms) -> Clickable &; // Set super long click time in ms (0-65535)

    // Getters
    [[nodiscard]] auto getIndex() const -> uint8_t;                                                               // Get the Clickable index on Clickables namespace Array
    [[nodiscard]] auto getId() const -> uint8_t;                                                                  // Return unique ID of the clickable
    [[nodiscard]] auto getActuators(constants::ClickType actuatorType) const -> const etl::ivector<uint8_t> *;    // Returns attached actuators of one kind
    [[nodiscard]] auto getTotalActuators(constants::ClickType actuatorType) const -> uint8_t;                     // Returns the total number of a kind of actuators
    [[nodiscard]] auto getLongClickType() const -> constants::LongClickType;                                      // Returns the type of long click
    [[nodiscard]] auto isNetworkClickable(constants::ClickType clickType) const -> bool;                          // Returns if the clickable is network clickable for long or super long click actions
    [[nodiscard]] auto getNetworkFallback(constants::ClickType clickType) const -> constants::NoNetworkClickType; // Returns the fallback type for a give network click type
    [[nodiscard]] auto getSuperLongClickType() const -> constants::SuperLongClickType;                            // Returns the type of super long click

    // Utilities
    auto check() -> bool;                                          // Check for clickable coherence, it has to be clickable, it has to be connected to something
    [[nodiscard]] auto shortClick() const -> bool;                 // Perform a short click action
    [[nodiscard]] auto longClick() const -> bool;                  // Perform a long click action
    [[nodiscard]] auto superLongClickSelective() const -> bool;    // Perform a selective super long click;
    [[nodiscard]] auto clickDetection() -> constants::ClickResult; // Processes input state and determines the click type using an internal Finite State Machine (FSM).
    void resizeVectors();                                          // Resize vectors to actual needed size
};

#endif // LSHCORE_PERIPHERALS_INPUT_CLICKABLE_HPP