/**
 * @file    bridge_serial.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the serial communication logic, including sending and receiving data.
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

#include "communication/bridge_serial.hpp"

#include <Print.h>

#include "communication/checked_writer.hpp"
#include "communication/deserializer.hpp"
#include "communication/msgpack_serial_framing.hpp"
#include "communication/constants/config.hpp"
#include "communication/constants/protocol.hpp"
#include "util/debug/debug.hpp"
#include "util/saturating_time.hpp"
#include "util/time_keeper.hpp"

namespace BridgeSerial
{
using namespace Debug;
#ifdef CONFIG_MSG_PACK
using lsh::core::transport::MsgPackFrameConsumeResult;
using lsh::core::transport::MsgPackFrameWriter;
#endif

uint16_t sendIdleAge_ms = UINT16_MAX;     //!< Elapsed idle time since the last payload was sent, saturated at 65535 ms.
uint16_t receiveIdleAge_ms = UINT16_MAX;  //!< Elapsed idle time since the last valid payload was received, saturated at 65535 ms.

namespace
{
// Keep the receive-side storage in namespace scope instead of function-local
// statics. On AVR this avoids the extra guard variable and the one-time
// initialization check emitted for local static objects in `receiveAndDispatch()`.
StaticJsonDocument<constants::bridgeSerial::RECEIVED_DOC_SIZE>
    receivedDocument;  //!< Reusable receive-side JSON/MsgPack document owned by the bridge RX path.
#ifdef CONFIG_MSG_PACK
char rawInputBuffer[constants::bridgeSerial::RAW_INPUT_BUFFER_SIZE];  //!< Raw payload buffer for one complete deframed MsgPack payload.
lsh::core::transport::MsgPackFrameReceiver
    msgPackFrameReceiver(rawInputBuffer,
                         static_cast<uint16_t>(sizeof(rawInputBuffer)));  //!< Incremental deframer used by the MsgPack serial transport.
#else
char rawInputBuffer[constants::bridgeSerial::RAW_INPUT_BUFFER_SIZE];  //!< Raw line buffer for newline-delimited JSON payloads.
size_t bufferedBytesCount = 0U;                                       //!< Number of bytes currently stored in `rawInputBuffer`.
bool discardUntilNewline = false;                                     //!< True after an overflow until the trailing newline is consumed.
#endif

}  // namespace

/**
 * @brief Initialize the serial port used to talk with `lsh-bridge`.
 * @details The controller and the bridge share one hardware serial link.
 *          This helper applies the configured baud rate once during startup so
 *          every later send/receive path can assume the port is already ready.
 */
void init()
{
    CONFIG_COM_SERIAL->HardwareSerial::begin(constants::bridgeSerial::COM_SERIAL_BAUD, SERIAL_8N1);
}

/**
 * @brief Send one JSON document to the bridge.
 *
 * This helper centralizes the actual wire write so every payload goes through
 * the same codec selection, optional framing, optional flush policy and
 * timestamp bookkeeping.
 *
 * @param documentToSend JsonDocument to be sent.
 * @return true if the full payload and any required transport delimiters were
 *         accepted by the UART.
 * @return false if serialization produced no payload bytes or if the UART
 *         accepted only part of the frame.
 */
auto sendJson(const JsonDocument &documentToSend) -> bool
{
    DP_CONTEXT();
#ifdef CONFIG_MSG_PACK
    MsgPackFrameWriter framedWriter(*CONFIG_COM_SERIAL);
    if (!framedWriter.beginFrame())
    {
        return false;
    }

    lsh::core::communication::CheckedWriter<MsgPackFrameWriter> checkedWriter(framedWriter);
    const size_t writtenPayloadBytes = serializeMsgPack(documentToSend, checkedWriter);
    const bool frameEnded = framedWriter.endFrame();
    if (writtenPayloadBytes == 0U || checkedWriter.failed() || !frameEnded)
    {
        return false;
    }
#else
    lsh::core::communication::CheckedWriter<Print> checkedWriter(*CONFIG_COM_SERIAL);
    const size_t writtenPayloadBytes = serializeJson(documentToSend, checkedWriter);
    const size_t writtenDelimiterBytes =
        checkedWriter.write(static_cast<uint8_t>('\n'));  // Add a newline character after sending the JSON payload.
    if (writtenPayloadBytes == 0U || writtenDelimiterBytes != 1U || checkedWriter.failed())
    {
        return false;
    }
#endif  // CONFIG_MSG_PACK
    if constexpr (constants::bridgeSerial::COM_SERIAL_FLUSH_AFTER_SEND)
    {
        // Conservative default: this path is the currently validated, known-good setup.
        // The compile-time switch exists only to evaluate whether the link stays
        // resilient even without a blocking flush after each send.
        CONFIG_COM_SERIAL->HardwareSerial::flush();
    }
    DP(FPSTR(dStr::JSON_SENT), FPSTR(dStr::COLON_SPACE));
    DPJ(documentToSend);
    updateLastSentTime();
    return true;
}

/**
 * @brief Consume a bounded amount of serial input and dispatch at most one payload.
 * @details This helper is intentionally bounded both by bytes and by payload count.
 *          The caller can therefore keep the controller loop fair even if the UART
 *          is carrying noise, truncated frames or a long burst of valid commands.
 *
 * @param maxBytesToConsume Maximum number of raw UART bytes this call may drain.
 * @return ReceiveResult Byte-consumption and dispatch information for this receive attempt.
 */
auto receiveAndDispatch(uint16_t maxBytesToConsume) -> ReceiveResult
{
    ReceiveResult receiveResult;
    if (maxBytesToConsume == 0U)
    {
        return receiveResult;
    }

#ifdef CONFIG_MSG_PACK
    const uint32_t nowRealTime_ms = timeKeeper::getRealTime();
    msgPackFrameReceiver.resetIfIdle(nowRealTime_ms, constants::bridgeSerial::COM_SERIAL_MSGPACK_FRAME_IDLE_TIMEOUT_MS);

    while (CONFIG_COM_SERIAL->HardwareSerial::available() && receiveResult.consumedBytes < maxBytesToConsume)
    {
        const int rawByte = CONFIG_COM_SERIAL->HardwareSerial::read();
        if (rawByte < 0)
        {
            break;
        }
        ++receiveResult.consumedBytes;

        const auto consumeResult = msgPackFrameReceiver.consumeByte(static_cast<uint8_t>(rawByte), nowRealTime_ms);
        if (consumeResult == MsgPackFrameConsumeResult::FrameDiscarded)
        {
            DPL(F("Discarded malformed framed MsgPack bridge payload."));
            return receiveResult;
        }

        if (consumeResult != MsgPackFrameConsumeResult::FrameComplete)
        {
            continue;
        }

        receivedDocument.clear();
        const DeserializationError deserializationError =
            deserializeMsgPack(receivedDocument, msgPackFrameReceiver.frameData(), msgPackFrameReceiver.frameLength());
        msgPackFrameReceiver.reset();
        if (deserializationError != DeserializationError::Ok)
        {
            DPL(deserializationError.f_str());
            return receiveResult;
        }

        DP(FPSTR(dStr::JSON_RECEIVED), FPSTR(dStr::COLON_SPACE));
        DPJ(receivedDocument);
        receiveIdleAge_ms = 0U;
        receiveResult.dispatch = Deserializer::deserializeAndDispatch(receivedDocument);
        receiveResult.payloadDispatched = true;
        return receiveResult;
    }
    return receiveResult;

#else
    while (CONFIG_COM_SERIAL->HardwareSerial::available() && receiveResult.consumedBytes < maxBytesToConsume)
    {
        const int rawByte = CONFIG_COM_SERIAL->HardwareSerial::read();
        if (rawByte < 0)
        {
            break;
        }
        const char receivedChar = static_cast<char>(rawByte);
        ++receiveResult.consumedBytes;

        if (discardUntilNewline)
        {
            if (receivedChar == '\n')
            {
                discardUntilNewline = false;
            }
            continue;
        }

        // A newline character marks the end of a potential message.
        if (receivedChar == '\n')
        {
            // Ensure the buffer isn't empty (e.g., just a standalone '\n' was received).
            if (bufferedBytesCount > 0U)
            {
                rawInputBuffer[bufferedBytesCount] = '\0';  // Null-terminate the completed string.

                // The message is complete. Attempt to parse it.
                const DeserializationError deserializationError = deserializeJson(receivedDocument, rawInputBuffer);
                bufferedBytesCount = 0U;  // Reset buffer for next msg

                if (deserializationError == DeserializationError::Ok)
                {
                    DP(FPSTR(dStr::JSON_RECEIVED), FPSTR(dStr::COLON_SPACE));
                    DPJ(receivedDocument);
                    receiveIdleAge_ms = 0U;
                    receiveResult.dispatch = Deserializer::deserializeAndDispatch(receivedDocument);
                    receiveResult.payloadDispatched = true;
                    return receiveResult;
                }
                else
                {
                    DPL(deserializationError.f_str());
                    return receiveResult;
                }
            }
            // If we receive a newline with an empty buffer, just ignore it and continue.
        }
        // If the character is not a newline, append it to the buffer if there's space.
        else if (bufferedBytesCount < (sizeof(rawInputBuffer) - 1U))
        {
            rawInputBuffer[bufferedBytesCount++] = receivedChar;
        }
        else
        {
            // Handle the rare case of a buffer overflow.
            // Discard the corrupt message and ignore every following byte until the
            // newline terminator, otherwise the payload tail could be misread as a
            // fresh command on the next iterations.
            DPL("Buffer overflow!");
            bufferedBytesCount = 0U;
            discardUntilNewline = true;
        }
    }
    return receiveResult;  // No complete message received in this cycle
#endif
}

/**
 * @brief Advance the ping idle timer used by `canPing()`.
 * @details The main loop computes one cached elapsed time for bridge
 *          housekeeping and feeds it here. Reusing that delta avoids another
 *          32-bit timestamp comparison in `canPing()` while keeping bridge-link
 *          liveness independent from the clickable scan policy.
 *
 * @param elapsed_ms Milliseconds elapsed since the previous bridge housekeeping pass.
 */
void tickSendIdleTimer(uint16_t elapsed_ms)
{
    if (elapsed_ms == 0U)
    {
        return;
    }
    sendIdleAge_ms = timeUtils::addElapsedTimeSaturated(sendIdleAge_ms, elapsed_ms);
    receiveIdleAge_ms = timeUtils::addElapsedTimeSaturated(receiveIdleAge_ms, elapsed_ms);
}

/**
 * @brief Check whether the bridge link may emit another heartbeat.
 * @details Heartbeats are rate-limited using `PING_INTERVAL_MS` so the controller
 *          does not flood the serial line when the rest of the loop is otherwise idle.
 *
 * @return true if enough time elapsed since the last transmitted payload.
 * @return false if the heartbeat interval is still active.
 */
auto canPing() -> bool
{
    // DP_CONTEXT(); // Bloats the serial output
    using constants::bridgeSerial::PING_INTERVAL_MS;
    return (sendIdleAge_ms > PING_INTERVAL_MS);
}

/**
 * @brief Record that a payload has just been transmitted to the bridge.
 * @details Any successful send resets the idle age to zero so the next ping
 *          cannot be emitted until the serial line stayed quiet for the full
 *          heartbeat interval again.
 */
void updateLastSentTime()
{
    DP_CONTEXT();
    sendIdleAge_ms = 0U;
}

/**
 * @brief Check whether the bridge is still considered connected.
 * @details The bridge is considered online only after the first valid payload
 *          has reset the receive idle age and only while the receive timeout
 *          window stays open. The age is advanced once by the main loop using
 *          the shared bridge-housekeeping delta, avoiding 32-bit timestamp
 *          subtraction in network-click routing.
 *
 * @return true if the bridge answered recently enough.
 * @return false if the bridge never answered or timed out.
 */
auto isConnected() -> bool
{
    DP_CONTEXT();
    using constants::bridgeSerial::CONNECTION_TIMEOUT_MS;
    return receiveIdleAge_ms < CONNECTION_TIMEOUT_MS;
}

/**
 * @brief Source-compatible bridge liveness overload.
 * @details Liveness is now tracked by `receiveIdleAge_ms`, so the caller's
 *          timestamp is ignored.
 *
 * @param now Ignored legacy timestamp.
 * @return true if the bridge answered recently enough.
 * @return false if the bridge never answered or timed out.
 */
auto isConnected(uint32_t now) -> bool
{
    static_cast<void>(now);
    return isConnected();
}

}  // namespace BridgeSerial
