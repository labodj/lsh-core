/**
 * @file    clickable_manager.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares the manager for the global collection of Clickable objects.
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

#ifndef LSHCORE_DEVICE_CLICKABLE_MANAGER_HPP
#define LSHCORE_DEVICE_CLICKABLE_MANAGER_HPP

#include <etl/array.h>
#include <etl/map.h>
#include <stdint.h>

#include "internal/user_config_bridge.hpp"
#include "util/constants/clicktypes.hpp"

class Clickable; //!< Forward declaration

/**
 * @brief Globally stores all clickables (like buttons) and to operates over them.
 *
 */
namespace Clickables
{
    extern uint8_t totalClickables;                                         //!< Device real total Clickables
    extern etl::array<Clickable *, CONFIG_MAX_CLICKABLES> clickables;       //!< Device clickables
    extern etl::map<uint8_t, uint8_t, CONFIG_MAX_CLICKABLES> clickablesMap; //!< Device clickables map (UUID (numeric)-> clickables index)

    void addClickable(Clickable *clickable);                                                      // Add one clickable to clickables vector and activate it
    [[nodiscard]] auto getClickable(uint8_t clickableId) -> Clickable *;                          // Returns a single clickable
    [[nodiscard]] auto getIndex(uint8_t clickableId) -> uint8_t;                                  // Returns a single clickable index
    [[nodiscard]] auto clickableExists(uint8_t clickableId) -> bool;                              // Returns true if clickable exists
    [[nodiscard]] auto click(const Clickable *clickable, constants::ClickType clickType) -> bool; // Method for all types of clicks, since not all click can be done within clickable class
    [[nodiscard]] auto click(uint8_t clickableIndex, constants::ClickType clickType) -> bool;     // Alternative method for all types of click
    void finalizeSetup();                                                                         // Resize vectors of all clickables to the actual needed size
} // namespace Clickables

#endif // LSHCORE_DEVICE_CLICKABLE_MANAGER_HPP
