/**
 * @file    deserializer.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares the function that deserializes and dispatches commands received from `lsh-bridge`.
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

#ifndef LSH_CORE_COMMUNICATION_DESERIALIZER_HPP
#define LSH_CORE_COMMUNICATION_DESERIALIZER_HPP

#include <ArduinoJson.h>

/**
 * @brief Provides the entry point that validates and dispatches one inbound bridge payload.
 */
namespace Deserializer
{
/**
 * @brief Represents the result of a dispatch operation.
 * @details Contains flags indicating the outcome of a command dispatched from the bridge runtime.
 */
struct DispatchResult
{
    bool stateChanged = false;         //!< True if the device state was changed by the command.
    bool networkClickHandled = false;  //!< True if network click timer processing must remain active after this dispatch.
};

auto deserializeAndDispatch(const JsonDocument &doc) -> DispatchResult;

}  // namespace Deserializer

#endif  // LSH_CORE_COMMUNICATION_DESERIALIZER_HPP
