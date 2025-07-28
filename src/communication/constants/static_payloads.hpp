/**
 * @file    static_payloads.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Defines pre-serialized static message payloads stored in Flash.
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

#ifndef LSHCORE_COMMUNICATION_CONSTANTS_STATIC_PAYLOADS_HPP
#define LSHCORE_COMMUNICATION_CONSTANTS_STATIC_PAYLOADS_HPP

#include <etl/array.h>
#include <stdint.h>

namespace constants::payloads
{
    /**
     * @brief Defines the types of pre-serialized static payloads.
     */
    enum class StaticType : uint8_t
    {
        BOOT, //!< Corresponds to {"p":4}
        PING_ //!< Corresponds to {"p":5}
    };

    // --- BOOT ---
    constexpr etl::array<const uint8_t, 8> JSON_BOOT_BYTES = {'{', '"', 'p', '"', ':', '4', '}', '\n'}; //!< Pre-serialized BOOT payload in JSON format.
    constexpr etl::array<const uint8_t, 4> MSGPACK_BOOT_BYTES = {0x81, 0xA1, 0x70, 0x04};               //!< Pre-serialized BOOT payload in MsgPack format.

    // --- PING ---
    constexpr etl::array<const uint8_t, 8> JSON_PING_BYTES = {'{', '"', 'p', '"', ':', '5', '}', '\n'}; //!< Pre-serialized PING payload in JSON format.
    constexpr etl::array<const uint8_t, 4> MSGPACK_PING_BYTES = {0x81, 0xA1, 0x70, 0x05};               //!< Pre-serialized PING payload in MsgPack format.
} // namespace constants::payloads
#endif // LSHCORE_COMMUNICATION_CONSTANTS_STATIC_PAYLOADS_HPP