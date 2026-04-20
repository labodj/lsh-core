/**
 * @file    payload_utils.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Provides a constexpr utility to get pre-serialized static payloads.
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

#ifndef LSH_CORE_COMMUNICATION_PAYLOAD_UTILS_HPP
#define LSH_CORE_COMMUNICATION_PAYLOAD_UTILS_HPP

#include "communication/constants/static_payloads.hpp"
#include "internal/etl_span.hpp"

namespace utils::payloads
{
/**
 * @brief Gets the final serial transport bytes for one compile-time static payload.
 *
 * The protocol generator emits both raw payload bytes and serial-ready bytes.
 * `lsh-core` only needs the serial form here, so the returned span may be
 * written directly to the controller UART without any extra runtime framing.
 */
template <bool IsMsgPack> [[nodiscard]] constexpr auto getSerial(constants::payloads::StaticType type) -> etl::span<const uint8_t>
{
    using namespace constants::payloads;

    if constexpr (IsMsgPack)
    {
        switch (type)
        {
        case StaticType::BOOT:
            return MSGPACK_SERIAL_BOOT_BYTES;
        case StaticType::PING_:
            return MSGPACK_SERIAL_PING_BYTES;

        default:
            return {};
        }
    }
    else  // JSON
    {
        switch (type)
        {
        case StaticType::BOOT:
            return JSON_SERIAL_BOOT_BYTES;
        case StaticType::PING_:
            return JSON_SERIAL_PING_BYTES;
        default:
            return {};
        }
    }
}
}  // namespace utils::payloads

#endif  // LSH_CORE_COMMUNICATION_PAYLOAD_UTILS_HPP
