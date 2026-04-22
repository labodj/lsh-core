/**
 * @file    checked_writer.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Writer adapter that detects short writes during one-pass serialization.
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

#ifndef LSH_CORE_COMMUNICATION_CHECKED_WRITER_HPP
#define LSH_CORE_COMMUNICATION_CHECKED_WRITER_HPP

#include <stddef.h>
#include <stdint.h>

namespace lsh::core::communication
{
/**
 * @brief Tiny sink adapter that remembers whether any write was short or invalid.
 * @details Used by one-pass JSON/MsgPack serialization paths where the runtime
 *          writes directly into the real sink instead of doing `measure*()`
 *          plus a second serialization pass. The adapter keeps the calling code
 *          simple: any short write flips `failed()` and the caller can then
 *          reject the whole frame.
 *
 * @tparam Sink concrete sink type exposing `write(uint8_t)` and
 *         `write(const uint8_t*, size_t)`.
 */
template <typename Sink> class CheckedWriter
{
public:
    /**
     * @brief Bind the adapter to one concrete destination sink.
     *
     * @param destination sink that will receive serialized bytes.
     */
    explicit CheckedWriter(Sink &destination) noexcept : sink(destination)
    {}

    /**
     * @brief Forward one byte to the sink and flag the adapter on short write.
     *
     * @param byte single byte to write.
     * @return size_t number of bytes accepted by the sink.
     */
    auto write(uint8_t byte) -> size_t
    {
        const auto writtenBytes = this->sink.write(byte);
        if (writtenBytes != 1U)
        {
            this->writeFailed = true;
        }
        return writtenBytes;
    }

    /**
     * @brief Forward a contiguous byte buffer to the sink.
     * @details Zero-length writes are treated as harmless no-ops. A null buffer
     *          for a non-zero request marks the adapter as failed because the
     *          caller attempted an invalid serialization write.
     *
     * @param buffer source bytes to write.
     * @param size requested byte count.
     * @return size_t number of bytes accepted by the sink.
     */
    auto write(const uint8_t *buffer, size_t size) -> size_t
    {
        if (size == 0U)
        {
            return 0U;
        }

        if (buffer == nullptr)
        {
            this->writeFailed = true;
            return 0U;
        }

        const auto writtenBytes = this->sink.write(buffer, size);
        if (writtenBytes != size)
        {
            this->writeFailed = true;
        }
        return writtenBytes;
    }

    /**
     * @brief Return whether any previous write failed or was truncated.
     */
    [[nodiscard]] auto failed() const noexcept -> bool
    {
        return this->writeFailed;
    }

private:
    Sink &sink;
    bool writeFailed = false;
};
}  // namespace lsh::core::communication

#endif  // LSH_CORE_COMMUNICATION_CHECKED_WRITER_HPP
