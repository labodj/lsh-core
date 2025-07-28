/**
 * @file    deserializer.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares the function to deserialize and dispatch commands received from the ESP bridge.
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

#ifndef LSHCORE_COMMUNICATION_DESERIALIZER_HPP
#define LSHCORE_COMMUNICATION_DESERIALIZER_HPP

#include <ArduinoJson.h>

/**
 * @brief Provides a function to deserialize and dispatch a received Json payload.
 */
namespace Deserializer
{
    struct DispatchResult
    {
        bool stateChanged = false;
        bool networkClickHandled = false;
    };

    /**
     * @brief Deserializes the JSON document and immediately executes the corresponding command.
     * @param doc The JSON document to process.
     * @return true if the command requires a subsequent, general state update to be sent.
     * @return false otherwise.
     */
    auto deserializeAndDispatch(const JsonDocument &doc) -> DispatchResult;

} // namespace Deserializer

#endif // LSHCORE_COMMUNICATION_DESERIALIZER_HPP
