/**
 * @file    esp_com.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the serial communication logic, including sending and receiving data.
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

#include "communication/esp_com.hpp"

#include "communication/deserializer.hpp"
#include "util/debug/debug.hpp"
#include "util/timekeeper.hpp"

namespace EspCom
{
    using namespace Debug;

    uint32_t lastSentPayloadTime_ms = 0U;     //!< Last time a payload has been sent
    uint32_t lastReceivedPayloadTime_ms = 0U; //!< Last time a valid payload has been received
    bool firstValidPayloadReceived = false;   //!< True after the first valid payload has been received
#ifndef CONFIG_MSG_PACK
    char inputBuffer[constants::espComConfigs::RAW_INPUT_BUFFER_SIZE]; //!< Raw buffer for incoming serial data.
    size_t bytesRead = 0;                                              //!< Number of bytes currently in the inputBuffer.
#endif

    /**
     * @brief Init serial
     *
     */
    void init()
    {
        CONFIG_COM_SERIAL->begin(constants::espComConfigs::COM_SERIAL_BAUD, SERIAL_8N1);
        CONFIG_COM_SERIAL->setTimeout(constants::espComConfigs::COM_SERIAL_TIMEOUT_MS);
    }

    /**
     * @brief Sends a JsonDocument via Serial.
     *
     * @param json JsonDocument to be sent.
     */
    void sendJson(const JsonDocument &json)
    {
        DP_CONTEXT();
#ifdef CONFIG_MSG_PACK
        serializeMsgPack(json, *CONFIG_COM_SERIAL);
#else
        serializeJson(json, *CONFIG_COM_SERIAL);
        CONFIG_COM_SERIAL->write("\n", 1); // Add a newline character after sending the JSON payload.
#endif // CONFIG_MSG_PACK
        CONFIG_COM_SERIAL->flush();
        DP(FPSTR(dStr::JSON_SENT), FPSTR(dStr::COLON_SPACE));
        DPJ(json);
        updateLastSentTime();
    }

    /**
     * @brief Reads from the communication serial port, processes complete messages, and dispatches commands.
     * @details This function handles both MsgPack and JSON-newline protocols based on compilation flags.
     *          For JSON, it buffers incoming bytes until a newline is detected, then parses the message.
     *          For MsgPack, it attempts to deserialize directly from the stream.
     *          Upon receiving a valid message, it calls Deserializer::deserializeAndDispatch to execute the command.
     * @return Deserializer::DispatchResult A struct indicating if the command changed the device state.
     */
    auto receiveAndDispatch() -> Deserializer::DispatchResult
    {
#ifdef CONFIG_MSG_PACK
        StaticJsonDocument<constants::espComConfigs::RECEIVED_DOC_SIZE> doc;
        DeserializationError err = deserializeMsgPack(doc, *CONFIG_COM_SERIAL);

        if (err == DeserializationError::Ok)
        {
            DP(FPSTR(dStr::JSON_RECEIVED), FPSTR(dStr::COLON_SPACE));
            DPJ(doc);
            firstValidPayloadReceived = true;
            lastReceivedPayloadTime_ms = timeKeeper::getTime();
            return Deserializer::deserializeAndDispatch(doc);
        }

        if (err == DeserializationError::IncompleteInput)
        {
            return {}; // Not an error, wait for more data
        }

        DPL(err.f_str());
        while (CONFIG_COM_SERIAL->available())
            CONFIG_COM_SERIAL->read(); // Empties buffer
        return {};

#else
        while (CONFIG_COM_SERIAL->available())
        {
            char receivedChar = CONFIG_COM_SERIAL->read();

            // A newline character marks the end of a potential message.
            if (receivedChar == '\n')
            {
                // Ensure the buffer isn't empty (e.g., just a standalone '\n' was received).
                if (bytesRead > 0)
                {
                    inputBuffer[bytesRead] = '\0'; // Null-terminate the completed string.

                    // The message is complete. Attempt to parse it.
                    StaticJsonDocument<constants::espComConfigs::RECEIVED_DOC_SIZE> doc;
                    const DeserializationError err = deserializeJson(doc, inputBuffer);
                    bytesRead = 0; // Reset buffer for next msg

                    if (err == DeserializationError::Ok)
                    {
                        DP(FPSTR(dStr::JSON_RECEIVED), FPSTR(dStr::COLON_SPACE));
                        DPJ(doc);
                        firstValidPayloadReceived = true;
                        lastReceivedPayloadTime_ms = timeKeeper::getTime();
                        return Deserializer::deserializeAndDispatch(doc);
                    }
                    else
                    {
                        DPL(err.f_str());
                    }
                }
                // If we receive a newline with an empty buffer, just ignore it and continue.
            }
            // If the character is not a newline, append it to the buffer if there's space.
            else if (bytesRead < (sizeof(inputBuffer) - 1))
            {
                inputBuffer[bytesRead++] = receivedChar;
            }
            else
            {
                // Handle the rare case of a buffer overflow.
                // Discard the corrupt message and start over to prevent parsing errors.
                DPL("Buffer overflow!");
                bytesRead = 0;
            }
        }
        return {}; // No complete message received in this cycle
#endif
    }

    /**
     * @brief Ping minimum interval check.
     *
     * @return true if the device can send ping.
     * @return false if the device can't send ping.
     */
    auto canPing() -> bool
    {
        // DP_CONTEXT(); // Bloats the serial output
        using constants::espComConfigs::PING_INTERVAL_MS;
        return ((timeKeeper::getTime() - lastSentPayloadTime_ms) > PING_INTERVAL_MS);
    }

    /**
     * @brief Set last time payload has been sent to now.
     *
     */
    void updateLastSentTime()
    {
        DP_CONTEXT();
        lastSentPayloadTime_ms = timeKeeper::getTime();
    }

    /**
     * @brief Checks if the ESP is considered connected.
     *
     * The assumption is based on having received a valid payload (like a PING or any other command)
     * within the defined `CONNECTION_TIMEOUT_MS`.
     *
     * @return true if the ESP is connected.
     * @return false if the ESP is not connected (or timed out).
     */
    auto isConnected() -> bool
    {
        DP_CONTEXT();
        using constants::espComConfigs::CONNECTION_TIMEOUT_MS;
        return (firstValidPayloadReceived && ((timeKeeper::getTime() - lastReceivedPayloadTime_ms) < CONNECTION_TIMEOUT_MS));
    }

} // namespace EspCom
