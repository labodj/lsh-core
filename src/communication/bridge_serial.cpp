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

#include "communication/deserializer.hpp"
#include "communication/constants/config.hpp"
#include "communication/constants/protocol.hpp"
#include "util/debug/debug.hpp"
#include "util/time_keeper.hpp"

namespace BridgeSerial
{
using namespace Debug;

uint32_t lastSentPayloadTime_ms = 0U;      //!< Last time a payload has been sent.
uint32_t lastReceivedPayloadTime_ms = 0U;  //!< Last time a valid payload has been received.
bool firstValidPayloadReceived = false;    //!< True after the first valid payload has been received.
#ifndef CONFIG_MSG_PACK
char rawInputBuffer[constants::bridgeSerial::RAW_INPUT_BUFFER_SIZE];  //!< Raw buffer for incoming serial data.
size_t bufferedBytesCount = 0U;                                       //!< Number of bytes currently stored in `rawInputBuffer`.
bool discardUntilNewline = false;                                     //!< True after an overflow until the trailing newline is consumed.
#endif

/**
 * @brief Initialize the serial port used to talk with `lsh-bridge`.
 * @details The controller and the bridge share one hardware serial link.
 *          This helper applies the configured baud rate and read timeout once
 *          during startup so every later send/receive path can assume the port
 *          is already configured correctly.
 */
void init()
{
    CONFIG_COM_SERIAL->begin(constants::bridgeSerial::COM_SERIAL_BAUD, SERIAL_8N1);
    CONFIG_COM_SERIAL->setTimeout(constants::bridgeSerial::COM_SERIAL_TIMEOUT_MS);
}

/**
 * @brief Send one JSON document to the bridge.
 *
 * This helper centralizes the actual wire write so every payload goes through
 * the same codec selection, optional flush policy and timestamp bookkeeping.
 *
 * @param documentToSend JsonDocument to be sent.
 */
void sendJson(const JsonDocument &documentToSend)
{
    DP_CONTEXT();
#ifdef CONFIG_MSG_PACK
    serializeMsgPack(documentToSend, *CONFIG_COM_SERIAL);
#else
    serializeJson(documentToSend, *CONFIG_COM_SERIAL);
    CONFIG_COM_SERIAL->write("\n", 1);  // Add a newline character after sending the JSON payload.
#endif  // CONFIG_MSG_PACK
    if constexpr (constants::bridgeSerial::COM_SERIAL_FLUSH_AFTER_SEND)
    {
        // Conservative default: this path is the currently validated, known-good setup.
        // The compile-time switch exists only to evaluate whether the link stays
        // resilient even without a blocking flush after each send.
        CONFIG_COM_SERIAL->flush();
    }
    DP(FPSTR(dStr::JSON_SENT), FPSTR(dStr::COLON_SPACE));
    DPJ(documentToSend);
    updateLastSentTime();
}

/**
 * @brief Reads from the communication serial port, processes complete messages, and dispatches commands.
 * @details This function handles both MsgPack and JSON-newline protocols based on compilation flags.
 *          For JSON, it buffers incoming bytes until a newline is detected, then parses the message.
 *          For MsgPack, it relies on ArduinoJson stream deserialization directly on the serial stream.
 *          Upon receiving a valid message, it calls Deserializer::deserializeAndDispatch to execute the command.
 * @return Deserializer::DispatchResult A struct indicating if the command changed the device state.
 */
auto receiveAndDispatch() -> Deserializer::DispatchResult
{
    static StaticJsonDocument<constants::bridgeSerial::RECEIVED_DOC_SIZE> receivedDocument;
#ifdef CONFIG_MSG_PACK
    if (!CONFIG_COM_SERIAL->available())
    {
        return {};
    }

    receivedDocument.clear();
    const DeserializationError deserializationError = deserializeMsgPack(receivedDocument, *CONFIG_COM_SERIAL);
    if (deserializationError != DeserializationError::Ok)
    {
        DPL(deserializationError.f_str());
        return {};
    }

    DP(FPSTR(dStr::JSON_RECEIVED), FPSTR(dStr::COLON_SPACE));
    DPJ(receivedDocument);
    firstValidPayloadReceived = true;
    lastReceivedPayloadTime_ms = timeKeeper::getTime();
    return Deserializer::deserializeAndDispatch(receivedDocument);

#else
    while (CONFIG_COM_SERIAL->available())
    {
        const char receivedChar = CONFIG_COM_SERIAL->read();

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
                    firstValidPayloadReceived = true;
                    lastReceivedPayloadTime_ms = timeKeeper::getTime();
                    return Deserializer::deserializeAndDispatch(receivedDocument);
                }
                else
                {
                    DPL(deserializationError.f_str());
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
    return {};  // No complete message received in this cycle
#endif
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
    return ((timeKeeper::getTime() - lastSentPayloadTime_ms) > PING_INTERVAL_MS);
}

/**
 * @brief Record that a payload has just been transmitted to the bridge.
 * @details The rest of the runtime uses this timestamp to throttle pings and
 *          to reason about link activity without touching `millis()` again.
 */
void updateLastSentTime()
{
    DP_CONTEXT();
    lastSentPayloadTime_ms = timeKeeper::getTime();
}

/**
 * @brief Check whether the bridge is still considered connected.
 * @details The bridge is considered online only after the first valid payload
 *          has been received and only while the receive timeout window stays open.
 *
 * @return true if the bridge answered recently enough.
 * @return false if the bridge never answered or timed out.
 */
auto isConnected() -> bool
{
    DP_CONTEXT();
    using constants::bridgeSerial::CONNECTION_TIMEOUT_MS;
    return (firstValidPayloadReceived && ((timeKeeper::getTime() - lastReceivedPayloadTime_ms) < CONNECTION_TIMEOUT_MS));
}

}  // namespace BridgeSerial
