/**
 * @file    msgpack_serial_framing.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares the serial framing helpers used for MsgPack payloads exchanged with `lsh-bridge`.
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

#ifndef LSH_CORE_COMMUNICATION_MSGPACK_SERIAL_FRAMING_HPP
#define LSH_CORE_COMMUNICATION_MSGPACK_SERIAL_FRAMING_HPP

#include <stdint.h>

#include "communication/constants/transport.hpp"

namespace lsh::core::transport
{
/**
 * @brief Maximum deframed v3 command size for this controller's packed state.
 * ACK/FAILOVER_CLICK: map4 + four one-byte keys with fixstr headers + p,t + uint8 i,c.
 * SET_STATE: map2, keys and command (6 bytes), fixarray/array16, up to two bytes per uint8.
 * Escaping is removed before a byte enters the receiver buffer.
 */
constexpr auto maxInboundPayloadSize(uint16_t packedStateBytes) noexcept -> uint16_t
{
    constexpr uint16_t clickBytes = 15U;
    const uint16_t arrayHeader = packedStateBytes < 16U ? 1U : 3U;
    const uint16_t stateBytes = 6U + arrayHeader + 2U * packedStateBytes;
    return stateBytes > clickBytes ? stateBytes : clickBytes;
}

/**
 * @brief Outcome of feeding one raw serial byte into the MsgPack frame receiver.
 */
enum class MsgPackFrameConsumeResult : uint8_t
{
    Incomplete,     //!< The byte has been consumed, but no full payload is ready yet.
    FrameComplete,  //!< One complete deframed payload is ready in the destination buffer.
    FrameDiscarded  //!< One corrupted or oversized frame has just been dropped at its closing delimiter.
};

/**
 * @brief Incremental receiver for the SLIP-like MsgPack framing used on the bridge serial link.
 */
class MsgPackFrameReceiver
{
private:
    char *const frameBuffer;            //!< Caller-owned destination storage for the deframed payload bytes.
    const uint16_t frameCapacity;       //!< Maximum number of payload bytes accepted before the current frame is dropped.
    uint16_t frameLengthBytes = 0U;     //!< Number of deframed payload bytes currently stored in `frameBuffer`.
    uint32_t lastByteTimeMs = 0U;       //!< Real-time timestamp of the most recent byte that touched this receiver.
    bool frameStarted = false;          //!< True after the opening END until reset or completion.
    bool escapePending = false;         //!< True after an escape marker until the next byte resolves it.
    bool discardUntilFrameEnd = false;  //!< True while draining a known-bad frame until the next delimiter.

    [[nodiscard]] auto appendByte(uint8_t byte) -> bool;
    void startDiscarding() noexcept;

public:
    MsgPackFrameReceiver(char *buffer, uint16_t capacity) noexcept;

    void reset() noexcept;
    void resetIfIdle(uint32_t nowMs, uint32_t idleTimeoutMs) noexcept;
    [[nodiscard]] auto consumeByte(uint8_t byte, uint32_t nowMs) -> MsgPackFrameConsumeResult;
    [[nodiscard]] auto frameData() const -> const uint8_t *;
    [[nodiscard]] auto frameLength() const -> uint16_t;
};

}  // namespace lsh::core::transport

#endif  // LSH_CORE_COMMUNICATION_MSGPACK_SERIAL_FRAMING_HPP
