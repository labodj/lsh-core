/**
 * @file    static_config.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares cold generated accessors for the selected static profile.
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

#ifndef LSH_CORE_CONFIG_STATIC_CONFIG_HPP
#define LSH_CORE_CONFIG_STATIC_CONFIG_HPP

#include <stdint.h>

#include "util/constants/click_types.hpp"

namespace lsh::core::static_config
{
static constexpr uint8_t CLICK_SCAN_STATE_CHANGED = 0x01U;    //!< A local click action changed at least one actuator state.
static constexpr uint8_t CLICK_SCAN_NETWORK_PENDING = 0x02U;  //!< A network-click request was accepted and needs timeout polling.

// These declarations are implemented by the generated static profile selected
// at build time. They form the narrow ABI between hand-written runtime code and
// generated topology code: fixed code can ask for IDs, dispatch cold bridge
// commands, or run hot generated scans without storing TOML topology in SRAM.
[[nodiscard]] auto getActuatorId(uint8_t actuatorIndex) noexcept -> uint8_t;
[[nodiscard]] auto getClickableId(uint8_t clickableIndex) noexcept -> uint8_t;
[[nodiscard]] auto getActuatorIndexById(uint8_t actuatorId) noexcept -> uint8_t;
[[nodiscard]] auto getClickableIndexById(uint8_t clickableId) noexcept -> uint8_t;
[[nodiscard]] auto getAutoOffActuatorIndex(uint8_t autoOffIndex) noexcept -> uint8_t;
[[nodiscard]] auto getAutoOffIndexByActuatorIndex(uint8_t actuatorIndex) noexcept -> uint8_t;
[[nodiscard]] auto getAutoOffTimer(uint8_t actuatorIndex) noexcept -> uint32_t;
[[nodiscard]] auto getIndicatorActuatorLinkCount(uint8_t indicatorIndex) noexcept -> uint8_t;
[[nodiscard]] auto setActuatorStateById(uint8_t actuatorId, bool state) noexcept -> bool;
[[nodiscard]] auto writeDetailsPayload() noexcept -> bool;
[[nodiscard]] auto runShortClick(uint8_t clickableIndex) noexcept -> bool;
[[nodiscard]] auto runLongClick(uint8_t clickableIndex) noexcept -> bool;
[[nodiscard]] auto runSuperLongClick(uint8_t clickableIndex) noexcept -> bool;
[[nodiscard]] auto runClick(uint8_t clickableIndex, constants::ClickType clickType) noexcept -> bool;
[[nodiscard]] auto runNetworkClickFallback(uint8_t clickableIndex, constants::ClickType clickType) noexcept -> bool;
[[nodiscard]] auto getNetworkClickSlotCount(uint8_t clickableIndex) noexcept -> uint8_t;
[[nodiscard]] auto getNetworkClickSlotIndex(uint8_t clickableIndex, constants::ClickType clickType) noexcept -> uint8_t;
[[nodiscard]] auto getNetworkClickClickableIndex(uint8_t slotIndex) noexcept -> uint8_t;
[[nodiscard]] auto getNetworkClickType(uint8_t slotIndex) noexcept -> constants::ClickType;
[[nodiscard]] auto isClickableConfigurationValid(uint8_t clickableIndex) noexcept -> bool;
[[nodiscard]] auto scanClickables(uint16_t elapsed_ms) noexcept -> uint8_t;
[[nodiscard]] auto turnOffAllActuators() noexcept -> bool;
[[nodiscard]] auto turnOffUnprotectedActuators() noexcept -> bool;
[[nodiscard]] auto checkPulseTimers(uint16_t elapsed_ms) noexcept -> bool;
[[nodiscard]] auto checkAutoOffTimers(uint32_t now_ms) noexcept -> bool;
[[nodiscard]] auto applyPackedActuatorStateByte(uint8_t byteIndex, uint8_t packedByte) noexcept -> bool;
[[nodiscard]] auto computeIndicatorState(uint8_t indicatorIndex) noexcept -> bool;
void refreshIndicators() noexcept;
}  // namespace lsh::core::static_config

#endif  // LSH_CORE_CONFIG_STATIC_CONFIG_HPP
