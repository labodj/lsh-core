/**
 * @file    serializer.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares functions for serializing data structures into JSON or MsgPack format.
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

#ifndef LSHCORE_COMMUNICATION_SERIALIZER_HPP
#define LSHCORE_COMMUNICATION_SERIALIZER_HPP

#include <ArduinoJson.h>
#include <stdint.h>

#include "communication/constants/static_payloads.hpp"
#include "util/constants/clicktypes.hpp"
/**
 * @brief Provide functions to prepare and serialize Json payloads.
 *
 */
namespace Serializer
{
    void serializeStaticJson(constants::payloads::StaticType payloadType);                            // Send a static json payload
    void serializeDetails();                                                                          // Prepare and send json details payload
    void serializeActuatorsState();                                                                   // Prepare and send a json actuators state payload
    void serializeNetworkClick(uint8_t clickableIndex, constants::ClickType clickType, bool confirm); // Prepare and send a json network click payload
} // namespace Serializer

#endif // LSHCORE_COMMUNICATION_SERIALIZER_HPP
