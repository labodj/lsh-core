/**
 * @file    actuator_manager.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares the manager for the global collection of Actuator objects.
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

#ifndef LSHCORE_DEVICE_ACTUATOR_MANAGER_HPP
#define LSHCORE_DEVICE_ACTUATOR_MANAGER_HPP

#include <etl/array.h>
#include <etl/map.h>
#include <etl/vector.h>
#include <stdint.h>

#include "internal/user_config_bridge.hpp"
class Actuator; //!< Forward declaration

/**
 * @brief Globally stores all actuators (relays) and to operates over them.
 *
 */
namespace Actuators
{
    extern uint8_t totalActuators;                                                 //!< Device real total Actuators
    extern etl::array<Actuator *, CONFIG_MAX_ACTUATORS> actuators;                 //!< All device actuators (like relays)
    extern etl::map<uint8_t, uint8_t, CONFIG_MAX_ACTUATORS> actuatorsMap;          //!< Device actuators map (UUID (integer) -> actuator index)
    extern etl::vector<uint8_t, CONFIG_MAX_ACTUATORS> actuatorsWithAutoOffIndexes; //!< Indexes of actuators with auto off functionality active

    void addActuator(Actuator *actuator);                                                                  // Add one actuator to actuators vector and activate it
    [[nodiscard]] auto getActuator(uint8_t actuatorId) -> Actuator *;                                      // Returns a single actuator
    [[nodiscard]] auto getIndex(uint8_t actuatorId) -> uint8_t;                                            // Returns a single actuator index
    [[nodiscard]] auto actuatorExists(uint8_t actuatorId) -> bool;                                         // Returns true if actuator exists
    [[nodiscard]] auto actuatorsAutoOffTimersCheck() -> bool;                                              // Performs an auto-off timer check for actuators
    [[nodiscard]] auto turnOffAllActuators() -> bool;                                                      // Turns off all actuators
    [[nodiscard]] auto turnOffUnprotectedActuators() -> bool;                                              // Turns off unprotected actuators
    [[nodiscard]] auto setAllActuatorsState(const etl::array<bool, CONFIG_MAX_ACTUATORS> &states) -> bool; // Set the state for all actuators
    void finalizeSetup();                                                                                  // Populates actuatorsWithAutoOffIndexes
} // namespace Actuators

#endif // LSHCORE_DEVICE_ACTUATOR_MANAGER_HPP
