/**
 * @file    payload_utils.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Provides a constexpr utility to get pre-serialized static payloads.
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

#ifndef LSHCORE_COMMUNICATION_PAYLOAD_UTILS_HPP
#define LSHCORE_COMMUNICATION_PAYLOAD_UTILS_HPP

#include <etl/span.h>

#include "communication/constants/static_payloads.hpp"

namespace utils::payloads
{
    /**
     * @brief Gets a span pointing to the correct pre-defined static payload.
     */
    template <bool IsMsgPack>
    [[nodiscard]] constexpr auto get(constants::payloads::StaticType type) -> etl::span<const uint8_t>
    {
        using namespace constants::payloads;

        if constexpr (IsMsgPack)
        {
            switch (type)
            {
            case StaticType::BOOT:
                return MSGPACK_BOOT_BYTES;
            case StaticType::PING_:
                return MSGPACK_PING_BYTES;

            default:
                return {};
            }
        }
        else // JSON
        {
            switch (type)
            {
            case StaticType::BOOT:
                return JSON_BOOT_BYTES;
            case StaticType::PING_:
                return JSON_PING_BYTES;
            default:
                return {};
            }
        }
    }
} // namespace utils::payloads

#endif // LSHCORE_COMMUNICATION_PAYLOAD_UTILS_HPP