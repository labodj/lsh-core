/**
 * @file    esp_com.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares functions for managing the low-level serial communication with the ESP bridge.
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

#ifndef LSHCORE_COMMUNICATION_ESP_COM_HPP
#define LSHCORE_COMMUNICATION_ESP_COM_HPP

#include <ArduinoJson.h>
#include <stdint.h>

#include "communication/constants/config.hpp"
#include "communication/deserializer.hpp"
#include "internal/user_config_bridge.hpp"

/**
 * @brief Perform communication via Serial.
 *
 */
namespace EspCom
{
    extern uint32_t lastSentPayloadTime_ms;     //!< Last time a payload has been sent
    extern uint32_t lastReceivedPayloadTime_ms; //!< Last time a valid payload has been received
    extern bool firstValidPayloadReceived;      //!< True after the first valid payload has been received
#ifndef CONFIG_MSG_PACK
    extern char inputBuffer[constants::espComConfigs::RAW_INPUT_BUFFER_SIZE];
    extern size_t bytesRead;
#endif

    void init();                                               // Initialize serial
    void sendJson(const JsonDocument &json);                   // Sends a json payload
    auto receiveAndDispatch() -> Deserializer::DispatchResult; // Receives and processes a json payload.

    // Utils
    [[nodiscard]] auto canPing() -> bool;     // Returns if the device can send ping or not
    void updateLastSentTime();                // Set last time payload has been sent to now
    [[nodiscard]] auto isConnected() -> bool; // Returns if esp is connected or not
} // namespace EspCom

#endif // LSHCORE_COMMUNICATION_ESP_COM_HPP
