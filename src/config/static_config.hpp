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
#include "util/constants/indicator_modes.hpp"

namespace lsh::core::static_config
{
[[nodiscard]] auto getActuatorId(uint8_t actuatorIndex) noexcept -> uint8_t;
[[nodiscard]] auto getClickableId(uint8_t clickableIndex) noexcept -> uint8_t;
[[nodiscard]] auto getActuatorIndexById(uint8_t actuatorId) noexcept -> uint8_t;
[[nodiscard]] auto getClickableIndexById(uint8_t clickableId) noexcept -> uint8_t;
[[nodiscard]] auto getAutoOffActuatorIndex(uint8_t autoOffIndex) noexcept -> uint8_t;
[[nodiscard]] auto getAutoOffIndexByActuatorIndex(uint8_t actuatorIndex) noexcept -> uint8_t;
[[nodiscard]] auto getAutoOffTimer(uint8_t actuatorIndex) noexcept -> uint32_t;
[[nodiscard]] auto getLongClickType(uint8_t clickableIndex) noexcept -> constants::LongClickType;
[[nodiscard]] auto getSuperLongClickType(uint8_t clickableIndex) noexcept -> constants::SuperLongClickType;
[[nodiscard]] auto getLongClickTime(uint8_t clickableIndex) noexcept -> uint16_t;
[[nodiscard]] auto getSuperLongClickTime(uint8_t clickableIndex) noexcept -> uint16_t;
[[nodiscard]] auto getShortClickActuatorLinkCount(uint8_t clickableIndex) noexcept -> uint8_t;
[[nodiscard]] auto getLongClickActuatorLinkCount(uint8_t clickableIndex) noexcept -> uint8_t;
[[nodiscard]] auto getSuperLongClickActuatorLinkCount(uint8_t clickableIndex) noexcept -> uint8_t;
[[nodiscard]] auto getClickActuatorLinkCount(constants::ClickType clickType, uint8_t clickableIndex) noexcept -> uint8_t;
[[nodiscard]] auto getShortClickActuatorLink(uint8_t clickableIndex, uint8_t linkIndex) noexcept -> uint8_t;
[[nodiscard]] auto getLongClickActuatorLink(uint8_t clickableIndex, uint8_t linkIndex) noexcept -> uint8_t;
[[nodiscard]] auto getSuperLongClickActuatorLink(uint8_t clickableIndex, uint8_t linkIndex) noexcept -> uint8_t;
[[nodiscard]] auto getClickActuatorLink(constants::ClickType clickType, uint8_t clickableIndex, uint8_t linkIndex) noexcept -> uint8_t;
[[nodiscard]] auto getIndicatorMode(uint8_t indicatorIndex) noexcept -> constants::IndicatorMode;
[[nodiscard]] auto getIndicatorActuatorLinkCount(uint8_t indicatorIndex) noexcept -> uint8_t;
[[nodiscard]] auto getIndicatorActuatorLink(uint8_t indicatorIndex, uint8_t linkIndex) noexcept -> uint8_t;
[[nodiscard]] auto getDetailsPayloadSize() noexcept -> uint16_t;
[[nodiscard]] auto getDetailsPayloadByte(uint16_t byteIndex) noexcept -> uint8_t;
[[nodiscard]] auto writeDetailsPayload() noexcept -> bool;
[[nodiscard]] auto runShortClick(uint8_t clickableIndex) noexcept -> bool;
[[nodiscard]] auto runLongClick(uint8_t clickableIndex) noexcept -> bool;
[[nodiscard]] auto runSuperLongClick(uint8_t clickableIndex) noexcept -> bool;
[[nodiscard]] auto turnOffAllActuators() noexcept -> bool;
[[nodiscard]] auto turnOffUnprotectedActuators() noexcept -> bool;
[[nodiscard]] auto applyPackedActuatorStateByte(uint8_t byteIndex, uint8_t packedByte) noexcept -> bool;
[[nodiscard]] auto computeIndicatorState(uint8_t indicatorIndex) noexcept -> bool;
}  // namespace lsh::core::static_config

#endif  // LSH_CORE_CONFIG_STATIC_CONFIG_HPP
