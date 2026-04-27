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
using PackedActuatorStateBytes = etl::array<uint8_t, CONFIG_PACKED_ACTUATOR_STATE_STORAGE_CAPACITY>;

#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
extern etl::array<Actuator *, CONFIG_MAX_ACTUATORS> actuators;
#endif
/**
 * @brief Canonical packed actuator-state shadow kept aligned with runtime changes.
 * @details The bridge serializer consumes this compact representation directly,
 *          so actuator state reporting no longer has to rebuild protocol bytes
 *          by rescanning every actuator object.
 */
extern PackedActuatorStateBytes packedActuatorStates;

[[nodiscard]] auto getId(uint8_t actuatorIndex) -> uint8_t;        // Returns the static actuator ID for one dense runtime index
[[nodiscard]] auto getActuator(uint8_t actuatorId) -> Actuator *;  // Returns a single actuator, or nullptr if the ID is unknown
[[nodiscard]] auto getIndex(uint8_t actuatorId) -> uint8_t;        // Returns a single actuator index, or UINT8_MAX if the ID is unknown
[[nodiscard]] auto tryGetIndex(uint8_t actuatorId, uint8_t &actuatorIndex)
    -> bool;                                                    // Returns true and writes the actuator index when the ID exists
[[nodiscard]] auto actuatorExists(uint8_t actuatorId) -> bool;  // Returns true if actuator exists
void recordSwitchTime(uint8_t actuatorIndex, uint32_t now_ms);  // Records a state-change time for compact actuator timer storage.
#if CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES && LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS > 0
[[nodiscard]] auto checkCompactAutoOffTimer(uint8_t autoOffIndex,
                                            uint8_t actuatorIndex,
                                            Actuator &actuator,
                                            uint32_t now_ms,
                                            uint32_t timer_ms) -> bool;  // Checks one generated compact auto-off entry.
#endif

/**
 * @brief Keep the compact actuator-state shadow aligned with one runtime state change.
 *
 * @param actuatorIndex dense runtime actuator index.
 * @param state new actuator state.
 */
void updatePackedState(uint8_t actuatorIndex, bool state);

/**
 * @brief Keep the packed actuator-state shadow aligned using a generated index.
 *
 * @details Generated profiles already know the dense actuator index at compile
 *          time. Keeping this tiny helper in the header lets AVR-GCC fold the
 *          byte index and bit mask instead of paying a runtime shift/mask path
 *          for every static actuator transition.
 *
 * @tparam ActuatorIndex Dense runtime actuator index from the static profile.
 * @param state new actuator state.
 */
template <uint8_t ActuatorIndex> __attribute__((always_inline)) inline void updatePackedStateStatic(bool state)
{
    static_assert(ActuatorIndex < CONFIG_MAX_ACTUATORS, "ActuatorIndex is outside the generated static profile.");
    constexpr uint8_t byteIndex = static_cast<uint8_t>(ActuatorIndex >> 3U);
    constexpr uint8_t bitMask = static_cast<uint8_t>(1U << (ActuatorIndex & 0x07U));
    if (state)
    {
        packedActuatorStates[byteIndex] |= bitMask;
    }
    else
    {
        packedActuatorStates[byteIndex] &= static_cast<uint8_t>(~bitMask);
    }
}

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
