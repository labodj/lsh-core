/**
 * @file    actuator_manager.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares the manager for the global collection of Actuator objects.
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

#ifndef LSH_CORE_DEVICE_ACTUATOR_MANAGER_HPP
#define LSH_CORE_DEVICE_ACTUATOR_MANAGER_HPP

#include <stdint.h>

#include "internal/etl_array.hpp"
#include "internal/etl_bitset.hpp"
#if !CONFIG_USE_ACTUATOR_ID_LUT
#include "internal/etl_map.hpp"
#endif
#include "internal/user_config_bridge.hpp"
class Actuator;

/**
 * @brief Globally stores all actuators (relays) and to operates over them.
 *
 */
namespace Actuators
{
/**
 * @brief Compact shadow of the full actuator runtime state in protocol bit order.
 * @details Bit 0 maps to actuator index 0, bit 1 to actuator index 1, and so on.
 *          The serializer can therefore emit `ACTUATORS_STATE` directly from this
 *          cached representation without rescanning every actuator object.
 */
using PackedActuatorStateBitset = etl::bitset<CONFIG_MAX_ACTUATORS>;

extern uint8_t totalActuators;
extern etl::array<Actuator *, CONFIG_MAX_ACTUATORS> actuators;
#if CONFIG_USE_ACTUATOR_ID_LUT
extern etl::array<uint8_t, CONFIG_MAX_ACTUATOR_ID + 1U> actuatorIndexById;
#else
extern etl::map<uint8_t, uint8_t, CONFIG_MAX_ACTUATORS> actuatorsMap;
#endif
/**
 * @brief Canonical packed actuator-state shadow kept aligned with runtime changes.
 * @details The bridge serializer consumes this compact representation directly,
 *          so actuator state reporting no longer has to rebuild protocol bytes
 *          by rescanning every actuator object.
 */
extern PackedActuatorStateBitset packedActuatorStates;

void addActuator(Actuator *actuator);                              // Add one actuator to actuators vector and activate it
[[nodiscard]] auto getActuator(uint8_t actuatorId) -> Actuator *;  // Returns a single actuator, or nullptr if the ID is unknown
[[nodiscard]] auto getIndex(uint8_t actuatorId) -> uint8_t;        // Returns a single actuator index, or UINT8_MAX if the ID is unknown
[[nodiscard]] auto tryGetIndex(uint8_t actuatorId, uint8_t &actuatorIndex)
    -> bool;                                                            // Returns true and writes the actuator index when the ID exists
[[nodiscard]] auto actuatorExists(uint8_t actuatorId) -> bool;          // Returns true if actuator exists
void setAutoOffTimer(uint8_t actuatorIndex, uint32_t time_ms);          // Store or remove an actuator auto-off timer
[[nodiscard]] auto hasAutoOffTimer(uint8_t actuatorIndex) -> bool;      // Returns true if an actuator has an auto-off timer
[[nodiscard]] auto getAutoOffTimer(uint8_t actuatorIndex) -> uint32_t;  // Returns the auto-off timer for an actuator
[[nodiscard]] auto checkAutoOffTimer(uint8_t actuatorIndex, uint32_t autoOffTimer_ms)
    -> bool;                                                    // Checks one actuator against the manager-owned switch timestamp.
void recordSwitchTime(uint8_t actuatorIndex, uint32_t now_ms);  // Records a state-change time for compact actuator timer storage.
[[nodiscard]] auto actuatorsAutoOffTimersCheck() -> bool;       // Performs an auto-off timer check for actuators
[[nodiscard]] auto turnOffAllActuators() -> bool;               // Turns off all actuators
[[nodiscard]] auto turnOffUnprotectedActuators() -> bool;       // Turns off unprotected actuators
[[nodiscard]] auto setAllActuatorsState(const etl::array<bool, CONFIG_MAX_ACTUATORS> &states) -> bool;  // Set the state for all actuators

/**
 * @brief Keep the compact actuator-state shadow aligned with one runtime state change.
 *
 * @param actuatorIndex dense runtime actuator index.
 * @param state new actuator state.
 */
void updatePackedState(uint8_t actuatorIndex, bool state);

/**
 * @brief Return the number of packed bytes required by the active actuator topology.
 *
 * @return uint8_t number of bytes needed by the compact `ACTUATORS_STATE` payload.
 */
[[nodiscard]] auto getPackedStateByteCount() -> uint8_t;

/**
 * @brief Return one packed actuator-state byte in protocol wire order.
 *
 * @param byteIndex packed byte index.
 * @return uint8_t packed actuator-state byte, or zero when `byteIndex` is out of range.
 */
[[nodiscard]] auto getPackedStateByte(uint8_t byteIndex) -> uint8_t;

void finalizeSetup();  // Final validation pass after configuration registration.
}  // namespace Actuators

#endif  // LSH_CORE_DEVICE_ACTUATOR_MANAGER_HPP
