/**
 * @file Auto-generated from shared/lsh_protocol.json.
 * @brief Defines target-specific pre-serialized static payload bytes.
 * @note Do not edit manually. Run tools/generate_lsh_protocol.py instead.
 */

#ifndef LSHCORE_COMMUNICATION_CONSTANTS_STATIC_PAYLOADS_HPP
#define LSHCORE_COMMUNICATION_CONSTANTS_STATIC_PAYLOADS_HPP

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
    inline constexpr etl::array<uint8_t, 8> JSON_BOOT_BYTES = {'{', '"', 'p', '"', ':', '4', '}', '\n'};
    inline constexpr etl::array<uint8_t, 6> MSGPACK_BOOT_BYTES = {0x04, 0x00, 0x81, 0xA1, 0x70, 0x04};

    // --- PING ---
    inline constexpr etl::array<uint8_t, 8> JSON_PING_BYTES = {'{', '"', 'p', '"', ':', '5', '}', '\n'};
    inline constexpr etl::array<uint8_t, 6> MSGPACK_PING_BYTES = {0x04, 0x00, 0x81, 0xA1, 0x70, 0x05};

} // namespace constants::payloads

#endif // LSHCORE_COMMUNICATION_CONSTANTS_STATIC_PAYLOADS_HPP
