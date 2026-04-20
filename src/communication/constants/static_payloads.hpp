/**
 * @file    static_payloads.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief Defines target-specific pre-serialized static payload bytes for raw and serial transports.
 * @note Do not edit manually. Run tools/generate_lsh_protocol.py instead.
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

#ifndef LSH_CORE_COMMUNICATION_CONSTANTS_STATIC_PAYLOADS_HPP
#define LSH_CORE_COMMUNICATION_CONSTANTS_STATIC_PAYLOADS_HPP

#include <stdint.h>
#include "../../internal/etl_array.hpp"

namespace constants::payloads
{
    enum class StaticType : uint8_t
    {
        BOOT,
        PING_,
    };

    // --- BOOT ---
    inline constexpr etl::array<uint8_t, 7> JSON_RAW_BOOT_BYTES = {'{', '"', 'p', '"', ':', '4', '}'};
    inline constexpr etl::array<uint8_t, 8> JSON_SERIAL_BOOT_BYTES = {'{', '"', 'p', '"', ':', '4', '}', '\n'};
    inline constexpr etl::array<uint8_t, 4> MSGPACK_RAW_BOOT_BYTES = {0x81, 0xA1, 0x70, 0x04};
    inline constexpr etl::array<uint8_t, 6> MSGPACK_SERIAL_BOOT_BYTES = {0xC0, 0x81, 0xA1, 0x70, 0x04, 0xC0};

    // --- PING ---
    inline constexpr etl::array<uint8_t, 7> JSON_RAW_PING_BYTES = {'{', '"', 'p', '"', ':', '5', '}'};
    inline constexpr etl::array<uint8_t, 8> JSON_SERIAL_PING_BYTES = {'{', '"', 'p', '"', ':', '5', '}', '\n'};
    inline constexpr etl::array<uint8_t, 4> MSGPACK_RAW_PING_BYTES = {0x81, 0xA1, 0x70, 0x05};
    inline constexpr etl::array<uint8_t, 6> MSGPACK_SERIAL_PING_BYTES = {0xC0, 0x81, 0xA1, 0x70, 0x05, 0xC0};

} // namespace constants::payloads

#endif // LSH_CORE_COMMUNICATION_CONSTANTS_STATIC_PAYLOADS_HPP
