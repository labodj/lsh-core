/**
 * @file    serializer.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares functions for serializing data structures into JSON or MsgPack format.
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

#ifndef LSH_CORE_COMMUNICATION_SERIALIZER_HPP
#define LSH_CORE_COMMUNICATION_SERIALIZER_HPP

#include <stdint.h>

#include "communication/constants/static_payloads.hpp"
#include "util/constants/click_types.hpp"
/**
 * @brief Provide functions to emit bridge payloads with the active serial codec.
 *
 */
namespace Serializer
{
[[nodiscard]] auto serializeStaticPayload(constants::payloads::StaticType payloadType) -> bool;  // Send a static control payload
[[nodiscard]] auto serializeDetails() -> bool;                                                   // Send generated device details
[[nodiscard]] auto serializeActuatorsState() -> bool;                                            // Send packed actuator state
[[nodiscard]] auto serializeNetworkClick(uint8_t clickableIndex,
                                         constants::ClickType clickType,
                                         bool confirm,
                                         uint8_t correlationId) -> bool;  // Send a network click payload
}  // namespace Serializer

#endif  // LSH_CORE_COMMUNICATION_SERIALIZER_HPP
