/**
 * @file    clickable_manager.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares the manager for the global collection of Clickable objects.
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

#ifndef LSH_CORE_DEVICE_CLICKABLE_MANAGER_HPP
#define LSH_CORE_DEVICE_CLICKABLE_MANAGER_HPP

#include <stdint.h>

#include "internal/etl_array.hpp"
#include "internal/user_config_bridge.hpp"
#include "util/constants/click_types.hpp"

class Clickable;

/**
 * @brief Globally stores all clickables (like buttons) and to operates over them.
 *
 */
namespace Clickables
{
extern etl::array<Clickable *, CONFIG_MAX_CLICKABLES> clickables;

void addClickable(Clickable *clickable,
                  uint8_t clickableId,
                  uint8_t clickableIndex);                            // Add one clickable to clickables vector and activate it
[[nodiscard]] auto getId(uint8_t clickableIndex) -> uint8_t;          // Returns the static clickable ID for one dense runtime index
[[nodiscard]] auto getClickable(uint8_t clickableId) -> Clickable *;  // Returns a single clickable, or nullptr if the ID is unknown
[[nodiscard]] auto getIndex(uint8_t clickableId) -> uint8_t;          // Returns a single clickable index, or UINT8_MAX if the ID is unknown
[[nodiscard]] auto tryGetIndex(uint8_t clickableId, uint8_t &clickableIndex)
    -> bool;                                                      // Returns true and writes the clickable index when the ID exists
[[nodiscard]] auto clickableExists(uint8_t clickableId) -> bool;  // Returns true if clickable exists
[[nodiscard]] auto click(const Clickable *clickable, constants::ClickType clickType)
    -> bool;  // Method for all types of clicks, since not all click can be done within clickable class
[[nodiscard]] auto click(uint8_t clickableIndex, constants::ClickType clickType) -> bool;  // Alternative method for all types of click
void finalizeActuatorLinkStorage();
void finalizeSetup();
}  // namespace Clickables

#endif  // LSH_CORE_DEVICE_CLICKABLE_MANAGER_HPP
