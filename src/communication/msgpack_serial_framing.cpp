/**
 * @file    msgpack_serial_framing.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the serial framing helpers used for MsgPack payloads exchanged with `lsh-bridge`.
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

#include "communication/msgpack_serial_framing.hpp"

namespace lsh::core::transport
{
/**
 * @brief Construct one incremental MsgPack frame receiver around caller-owned storage.
 *
 * @param buffer destination storage used for the deframed payload bytes.
 * @param capacity maximum number of payload bytes that fit in `buffer`.
 */
MsgPackFrameReceiver::MsgPackFrameReceiver(char *buffer, uint16_t capacity) noexcept : frameBuffer(buffer), frameCapacity(capacity)
{}

/**
 * @brief Forget the current receive state and return to the idle state.
 * @details The destination buffer content is left untouched because callers
 *          trust only the first `frameLength()` bytes after `FrameComplete`.
 */
void MsgPackFrameReceiver::reset() noexcept
{
    this->frameLengthBytes = 0U;
    this->lastByteTimeMs = 0U;
    this->escapePending = false;
    this->discardUntilFrameEnd = false;
}

/**
 * @brief Drop a partially received frame that has been silent for too long.
 * @details This timeout is only a housekeeping guard for truncated frames. It
 *          is not part of the framing itself and never defines frame boundaries.
 *
 * @param nowMs current real-time tick in milliseconds.
 * @param idleTimeoutMs maximum allowed silence while one frame is in progress.
 */
void MsgPackFrameReceiver::resetIfIdle(uint32_t nowMs, uint32_t idleTimeoutMs) noexcept
{
    if (idleTimeoutMs == 0U)
    {
        return;
    }

    if (!this->discardUntilFrameEnd && !this->escapePending && this->frameLengthBytes == 0U)
    {
        return;
    }

    if ((nowMs - this->lastByteTimeMs) >= idleTimeoutMs)
    {
        this->reset();
    }
}

/**
 * @brief Append one deframed byte to the payload buffer.
 *
 * @param byte payload byte to append.
 * @return true if the byte fit in the configured destination buffer.
 * @return false if appending it would overflow the buffer.
 */
auto MsgPackFrameReceiver::appendByte(uint8_t byte) -> bool
{
    if (this->frameLengthBytes >= this->frameCapacity)
    {
        return false;
    }

    this->frameBuffer[this->frameLengthBytes++] = static_cast<char>(byte);
    return true;
}

/**
 * @brief Enter discard mode until the next frame delimiter arrives.
 * @details Once a frame is known to be malformed or too large, the receiver
 *          ignores every following byte until the next delimiter so the next
 *          payload starts from a clean state.
 */
void MsgPackFrameReceiver::startDiscarding() noexcept
{
    this->frameLengthBytes = 0U;
    this->escapePending = false;
    this->discardUntilFrameEnd = true;
}

/**
 * @brief Feed one raw serial byte into the framed MsgPack receiver.
 * @details The transport format is `END + escaped(payload) + END`. The
 *          receiver therefore understands only the delimiter and the two
 *          escaped reserved bytes; the payload itself remains pure MsgPack.
 *
 * @param byte raw serial byte just read from the UART.
 * @param nowMs current real-time tick in milliseconds.
 * @return MsgPackFrameConsumeResult describing whether a frame became ready or
 *         one malformed frame has just been dropped.
 */
auto MsgPackFrameReceiver::consumeByte(uint8_t byte, uint32_t nowMs) -> MsgPackFrameConsumeResult
{
    this->lastByteTimeMs = nowMs;

    if (this->discardUntilFrameEnd)
    {
        if (byte == MSGPACK_FRAME_END)
        {
            this->reset();
            return MsgPackFrameConsumeResult::FrameDiscarded;
        }
        return MsgPackFrameConsumeResult::Incomplete;
    }

    if (this->escapePending)
    {
        this->escapePending = false;
        switch (byte)
        {
        case MSGPACK_FRAME_ESCAPED_END:
            if (this->appendByte(MSGPACK_FRAME_END))
            {
                return MsgPackFrameConsumeResult::Incomplete;
            }
            this->startDiscarding();
            return MsgPackFrameConsumeResult::Incomplete;

        case MSGPACK_FRAME_ESCAPED_ESCAPE:
            if (this->appendByte(MSGPACK_FRAME_ESCAPE))
            {
                return MsgPackFrameConsumeResult::Incomplete;
            }
            this->startDiscarding();
            return MsgPackFrameConsumeResult::Incomplete;

        default:
            if (byte == MSGPACK_FRAME_END)
            {
                this->reset();
                return MsgPackFrameConsumeResult::FrameDiscarded;
            }
            this->startDiscarding();
            return MsgPackFrameConsumeResult::Incomplete;
        }
    }

    if (byte == MSGPACK_FRAME_END)
    {
        if (this->frameLengthBytes == 0U)
        {
            return MsgPackFrameConsumeResult::Incomplete;
        }
        return MsgPackFrameConsumeResult::FrameComplete;
    }

    if (byte == MSGPACK_FRAME_ESCAPE)
    {
        this->escapePending = true;
        return MsgPackFrameConsumeResult::Incomplete;
    }

    if (this->appendByte(byte))
    {
        return MsgPackFrameConsumeResult::Incomplete;
    }

    this->startDiscarding();
    return MsgPackFrameConsumeResult::Incomplete;
}

/**
 * @brief Return the beginning of the completed payload buffer.
 *
 * @return const std::uint8_t * pointer to the first deframed payload byte.
 */
auto MsgPackFrameReceiver::frameData() const -> const uint8_t *
{
    return reinterpret_cast<const uint8_t *>(this->frameBuffer);
}

/**
 * @brief Return the current payload length assembled by the receiver.
 *
 * @return std::uint16_t number of valid payload bytes currently stored in `frameBuffer`.
 */
auto MsgPackFrameReceiver::frameLength() const -> uint16_t
{
    return this->frameLengthBytes;
}

/**
 * @brief Construct one streaming MsgPack frame writer around an arbitrary byte sink.
 *
 * @param destination byte sink that receives the framed payload.
 */
MsgPackFrameWriter::MsgPackFrameWriter(Print &destination) noexcept : output(destination)
{}

/**
 * @brief Emit the delimiter that starts one new framed MsgPack payload.
 * @details The opening delimiter also flushes any stale partial state that may
 *          still exist on the receiver after noise or a peer reset.
 *
 * @return true if the delimiter has been accepted by the output sink.
 * @return false otherwise.
 */
auto MsgPackFrameWriter::beginFrame() -> bool
{
    return this->output.write(MSGPACK_FRAME_END) == 1U;
}

/**
 * @brief Emit the delimiter that terminates one framed MsgPack payload.
 *
 * @return true if the delimiter has been accepted by the output sink.
 * @return false otherwise.
 */
auto MsgPackFrameWriter::endFrame() -> bool
{
    return this->output.write(MSGPACK_FRAME_END) == 1U;
}

/**
 * @brief Write one payload byte, escaping reserved transport markers when needed.
 *
 * @param byte raw MsgPack payload byte to send.
 * @return true if the byte, or its escaped representation, has been accepted.
 * @return false otherwise.
 */
auto MsgPackFrameWriter::writeEscapedByte(uint8_t byte) -> bool
{
    if (byte == MSGPACK_FRAME_END)
    {
        return this->output.write(MSGPACK_FRAME_ESCAPE) == 1U && this->output.write(MSGPACK_FRAME_ESCAPED_END) == 1U;
    }

    if (byte == MSGPACK_FRAME_ESCAPE)
    {
        return this->output.write(MSGPACK_FRAME_ESCAPE) == 1U && this->output.write(MSGPACK_FRAME_ESCAPED_ESCAPE) == 1U;
    }

    return this->output.write(byte) == 1U;
}

/**
 * @brief Write one raw payload byte through the framed MsgPack transport.
 *
 * @param byte raw MsgPack payload byte.
 * @return std::size_t `1` if the payload byte has been accepted, `0` otherwise.
 */
auto MsgPackFrameWriter::write(uint8_t byte) -> size_t
{
    return this->writeEscapedByte(byte) ? 1U : 0U;
}

/**
 * @brief Write a contiguous raw payload buffer through the framed MsgPack transport.
 *
 * @param buffer pointer to the raw MsgPack payload bytes.
 * @param size number of payload bytes available in `buffer`.
 * @return std::size_t number of payload bytes accepted from `buffer`.
 */
auto MsgPackFrameWriter::write(const uint8_t *buffer, size_t size) -> size_t
{
    if (buffer == nullptr)
    {
        return 0U;
    }

    size_t writtenPayloadBytes = 0U;
    while (writtenPayloadBytes < size)
    {
        if (!this->writeEscapedByte(buffer[writtenPayloadBytes]))
        {
            break;
        }
        ++writtenPayloadBytes;
    }

    return writtenPayloadBytes;
}

}  // namespace lsh::core::transport
