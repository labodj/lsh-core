/**
 * @file    config.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Defines build-time configurable parameters for the controller-to-bridge serial link.
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

#ifndef LSH_CORE_COMMUNICATION_CONSTANTS_CONFIG_HPP
#define LSH_CORE_COMMUNICATION_CONSTANTS_CONFIG_HPP

#include <ArduinoJson.h>
#include <stdint.h>

#include "internal/etl_bit.hpp"
#include "internal/user_config_bridge.hpp"
/**
 * @brief Namespace that groups compile-time constants.
 */
namespace constants
{
/**
 * @brief Constants that configure the serial link shared with `lsh-bridge`.
 */
namespace bridgeSerial
{
// Ping timers
#ifndef CONFIG_PING_INTERVAL_MS
static constexpr const uint16_t PING_INTERVAL_MS = 10000U;  //!< Default Ping interval time in ms
#else
static constexpr const uint16_t PING_INTERVAL_MS = CONFIG_PING_INTERVAL_MS;  //!< Ping interval time in ms
#endif  // CONFIG_PING_INTERVAL_MS

#ifndef CONFIG_CONNECTION_TIMEOUT_MS
static constexpr const uint16_t CONNECTION_TIMEOUT_MS = PING_INTERVAL_MS + 200U;  //!< Default Time for a connection timeout in ms
#else
static constexpr const uint16_t CONNECTION_TIMEOUT_MS = CONFIG_CONNECTION_TIMEOUT_MS;  //!< Time for a connection timeout in ms
#endif  // CONFIG_CONNECTION_TIMEOUT_MS

#ifndef CONFIG_BRIDGE_BOOT_RETRY_INTERVAL_MS
static constexpr const uint16_t BRIDGE_BOOT_RETRY_INTERVAL_MS =
    250U;  //!< Retry interval for BOOT while the bridge has not completed its startup handshake.
#else
static constexpr const uint16_t BRIDGE_BOOT_RETRY_INTERVAL_MS = CONFIG_BRIDGE_BOOT_RETRY_INTERVAL_MS;
#endif  // CONFIG_BRIDGE_BOOT_RETRY_INTERVAL_MS

#ifndef CONFIG_BRIDGE_AWAIT_STATE_TIMEOUT_MS
static constexpr const uint16_t BRIDGE_AWAIT_STATE_TIMEOUT_MS =
    1500U;  //!< Maximum time to wait for REQUEST_STATE after REQUEST_DETAILS before restarting the handshake.
#else
static constexpr const uint16_t BRIDGE_AWAIT_STATE_TIMEOUT_MS = CONFIG_BRIDGE_AWAIT_STATE_TIMEOUT_MS;
#endif  // CONFIG_BRIDGE_AWAIT_STATE_TIMEOUT_MS

#ifndef CONFIG_COM_SERIAL_BAUD
static constexpr const uint32_t COM_SERIAL_BAUD = 250000U;  //!< Default baud rate of the controller-to-bridge serial link.
#else
static constexpr const uint32_t COM_SERIAL_BAUD = CONFIG_COM_SERIAL_BAUD;  //!< Baud rate of the controller-to-bridge serial link.
#endif  // CONFIG_COM_SERIAL_BAUD

#ifndef CONFIG_COM_SERIAL_TIMEOUT_MS
static constexpr const uint8_t COM_SERIAL_TIMEOUT_MS =
    5U;  //!< Compatibility fallback used only as the default MsgPack frame idle-timeout value.
#else
static constexpr const uint8_t COM_SERIAL_TIMEOUT_MS =
    CONFIG_COM_SERIAL_TIMEOUT_MS;  //!< Compatibility fallback used only as the default MsgPack frame idle-timeout value.
#endif  // CONFIG_COM_SERIAL_TIMEOUT_MS

#ifndef CONFIG_COM_SERIAL_MSGPACK_FRAME_IDLE_TIMEOUT_MS
static constexpr const uint8_t COM_SERIAL_MSGPACK_FRAME_IDLE_TIMEOUT_MS =
    COM_SERIAL_TIMEOUT_MS;  //!< Maximum silence while one serial MsgPack frame is still incomplete.
#else
static constexpr const uint8_t COM_SERIAL_MSGPACK_FRAME_IDLE_TIMEOUT_MS =
    CONFIG_COM_SERIAL_MSGPACK_FRAME_IDLE_TIMEOUT_MS;  //!< Maximum silence while one serial MsgPack frame is still incomplete.
#endif  // CONFIG_COM_SERIAL_MSGPACK_FRAME_IDLE_TIMEOUT_MS

#ifndef CONFIG_COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP
static constexpr const uint8_t COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP =
    4U;  //!< Upper bound for fully dispatched bridge payloads in one controller loop iteration.
#else
static_assert(CONFIG_COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP > 0U, "CONFIG_COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP must be greater than zero.");
static_assert(CONFIG_COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP <= UINT8_MAX, "CONFIG_COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP must fit in uint8_t.");
static constexpr const uint8_t COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP = CONFIG_COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP;
#endif  // CONFIG_COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP
static_assert(COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP > 0U, "COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP must be greater than zero.");

#ifndef CONFIG_COM_SERIAL_FLUSH_AFTER_SEND
#ifdef LSH_DEBUG
static constexpr const bool COM_SERIAL_FLUSH_AFTER_SEND = true;  //!< Debug builds keep the conservative flush-after-send behavior.
#else
static constexpr const bool COM_SERIAL_FLUSH_AFTER_SEND = false;  //!< Release builds avoid blocking on every bridge payload by default.
#endif  // LSH_DEBUG
#else
static constexpr const bool COM_SERIAL_FLUSH_AFTER_SEND = (CONFIG_COM_SERIAL_FLUSH_AFTER_SEND != 0);
#endif  // CONFIG_COM_SERIAL_FLUSH_AFTER_SEND

constexpr uint16_t PACKED_STATE_BYTES =
    static_cast<uint16_t>((CONFIG_MAX_ACTUATORS + 7U) / 8U);  //!< Number of bytes required to represent all actuator states on wire.

/*
                Received Json Document Size, the size is computed here https://arduinojson.org/v6/assistant/
                Processor: AVR, Mode: Deserialize, Input Type: Stream
                {"p":10} -> min: 10, recommended: 24
                              -> (JSON_OBJECT_SIZE(1) + number of strings + string characters number)
                              -> (8 + 2 + 6 = 16)
                {"p":12,"s":[90,3]} -> min: JSON_ARRAY_SIZE(ceil(CONFIG_MAX_ACTUATORS / 8)) + JSON_OBJECT_SIZE(2) + 4
                                      because `SET_STATE` uses packed state bytes, not one JSON element per actuator.
                {"p":17,"t":2,"i":255,"c":255} -> min: JSON_OBJECT_SIZE(4)
                {"p":13,"i":5,"s":0} -> min: 30, recommended: 48
                                                     -> (JSON_OBJECT_SIZE(3) + number of strings + string characters number)
                                                     -> (24 + 3 + 3 = 30)
                We can use 48 as base minimum size
                */
constexpr uint16_t RECEIVED_DOC_MIN_SIZE = etl::bit_ceil(JSON_ARRAY_SIZE(PACKED_STATE_BYTES) + JSON_OBJECT_SIZE(2) +
                                                         4U);  //!< Calculated minimum size for the JSON document received from the bridge.
constexpr uint16_t RECEIVED_DOC_SIZE =
    RECEIVED_DOC_MIN_SIZE <= 48U
        ? 48U
        : RECEIVED_DOC_MIN_SIZE;  //!< Final allocated size for the received JSON document, ensuring a minimum of 48 bytes.
static_assert(RECEIVED_DOC_SIZE >= JSON_OBJECT_SIZE(4), "RECEIVED_DOC_SIZE must fit network click responses with correlation ID.");

#ifdef CONFIG_MSG_PACK
/**
 * @brief Return the MsgPack array-header size required by one packed actuator-state payload.
 *
 * `SET_STATE` is the largest inbound command in MsgPack mode. The controller
 * stores the deframed payload bytes in `rawInputBuffer`, so the sizing formula
 * must follow the binary MsgPack shape instead of the historical JSON line
 * length.
 */
constexpr uint16_t PACKED_STATE_MSGPACK_ARRAY_HEADER_SIZE =
    (PACKED_STATE_BYTES < 16U) ? 1U : 3U;  //!< `fixarray` for up to 15 bytes, `array16` beyond that.

/**
 * @brief Maximum MsgPack size of one fixed-width inbound command handled by the controller.
 * @details `NETWORK_CLICK_*` and `FAILOVER_CLICK` are the widest fixed-shape
 *          commands received by the controller in MsgPack mode:
 *          `map4 + 4 one-character keys + 4 uint8_t values`.
 */
constexpr uint16_t RAW_INPUT_BUFFER_FIXED_CMD_SIZE = 13U;

/**
 * @brief Maximum MsgPack size of a packed `SET_STATE` command received from the bridge.
 * @details Layout:
 *          - `map2` header: 1 byte
 *          - key `p`: 2 bytes
 *          - command value: 1 byte
 *          - key `s`: 2 bytes
 *          - array header: 1 or 3 bytes
 *          - packed state bytes: `PACKED_STATE_BYTES`
 */
constexpr uint16_t RAW_INPUT_BUFFER_VARIABLE_CMD_SIZE = 6U + PACKED_STATE_MSGPACK_ARRAY_HEADER_SIZE + PACKED_STATE_BYTES;

#else
/**
 * @brief Minimum JSON-line buffer size required by the widest fixed-width command.
 */
constexpr uint16_t RAW_INPUT_BUFFER_FIXED_CMD_SIZE =
    etl::bit_ceil(31U);  //!< Longest fixed JSON command plus null terminator: {"p":17,"t":2,"i":255,"c":255}\0

/**
 * @brief Maximum JSON-line size of a packed `SET_STATE` command.
 */
constexpr uint16_t RAW_INPUT_BUFFER_VARIABLE_CMD_SIZE =
    etl::bit_ceil(17U + (4U * PACKED_STATE_BYTES));  //!< Conservative worst case for {"p":12,"s":[255,...]}\n\0
#endif

constexpr uint16_t RAW_INPUT_BUFFER_SIZE =
    RAW_INPUT_BUFFER_VARIABLE_CMD_SIZE <= RAW_INPUT_BUFFER_FIXED_CMD_SIZE
        ? RAW_INPUT_BUFFER_FIXED_CMD_SIZE
        : RAW_INPUT_BUFFER_VARIABLE_CMD_SIZE;  //!< Final allocated size for the raw serial input buffer used by the active codec.

#ifdef CONFIG_MSG_PACK
/**
 * @brief Worst-case on-wire size of one framed MsgPack command.
 * @details The SLIP-like transport adds an opening delimiter, a closing
 *          delimiter, and may escape every payload byte into a two-byte
 *          sequence.
 */
constexpr uint16_t MSGPACK_SERIAL_MAX_FRAME_SIZE = static_cast<uint16_t>(2U + (RAW_INPUT_BUFFER_SIZE * 2U));

static_assert(RAW_INPUT_BUFFER_SIZE >= 13U, "RAW_INPUT_BUFFER_SIZE must fit fixed-size network click commands in MsgPack mode.");
#else
static_assert(RAW_INPUT_BUFFER_SIZE >= 31U, "RAW_INPUT_BUFFER_SIZE must fit fixed-size click/failover commands plus null terminator.");
#endif

#ifndef CONFIG_COM_SERIAL_MAX_RX_BYTES_PER_LOOP
static constexpr const uint16_t COM_SERIAL_MAX_RX_BYTES_PER_LOOP =
#ifdef CONFIG_MSG_PACK
    MSGPACK_SERIAL_MAX_FRAME_SIZE;  //!< Upper bound for raw serial bytes consumed in one controller loop iteration.
#else
    RAW_INPUT_BUFFER_SIZE;  //!< Upper bound for raw serial bytes consumed in one controller loop iteration.
#endif
#else
static constexpr const uint16_t COM_SERIAL_MAX_RX_BYTES_PER_LOOP =
    CONFIG_COM_SERIAL_MAX_RX_BYTES_PER_LOOP;  //!< Upper bound for raw serial bytes consumed in one controller loop iteration.
#endif  // CONFIG_COM_SERIAL_MAX_RX_BYTES_PER_LOOP
static_assert(COM_SERIAL_MAX_RX_BYTES_PER_LOOP > 0U, "CONFIG_COM_SERIAL_MAX_RX_BYTES_PER_LOOP must be greater than zero.");

/*
                Sent details Json Document size, the size is computed here https://arduinojson.org/v6/assistant/
                IMPORTANT: We are assuming that all keys strings and values strings are const char *
                {"p":1,"v":3,"n":"c1","a":[1,2,...],"b":[1,3,...]} -> JSON_ARRAY_SIZE(CONFIG_MAX_ACTUATORS) + JSON_ARRAY_SIZE(CONFIG_MAX_CLICKABLES) + JSON_OBJECT_SIZE(5)
                                                                    -> 8*CONFIG_MAX_ACTUATORS + 8*CONFIG_MAX_CLICKABLES + 40
                The bare minimum is 40 with no actuators nor clickables
                */
constexpr uint16_t SENT_DOC_DETAILS_SIZE = JSON_ARRAY_SIZE(CONFIG_MAX_ACTUATORS) + JSON_ARRAY_SIZE(CONFIG_MAX_CLICKABLES) +
                                           JSON_OBJECT_SIZE(5);  //!< Calculated size for the JSON document sent with device details.

/*
                Sent state Json Document size, the size is computed here https://arduinojson.org/v6/assistant/
                IMPORTANT: We are assuming that all keys strings are const char * but not values
                {"p":2,"s":[90,3]} -> JSON_ARRAY_SIZE(ceil(CONFIG_MAX_ACTUATORS / 8)) + JSON_OBJECT_SIZE(2)
                because `ACTUATORS_STATE` is serialized as packed bytes.
                The bare minimum is 16 with no actuators.
                */
constexpr uint16_t SENT_DOC_STATE_SIZE =
    JSON_ARRAY_SIZE(PACKED_STATE_BYTES) + JSON_OBJECT_SIZE(2);  //!< Calculated size for the JSON document sent with packed actuator states.

/*
                Sent network click Json Document, the size is computed here https://arduinojson.org/v6/assistant/
                IMPORTANT: We are assuming that all keys strings and values strings are const char *
                {"p":3,"t":1,"i":1,"c":42} -> JSON_OBJECT_SIZE(4) -> 32
                */
constexpr uint16_t SENT_DOC_NETWORK_CLICK_SIZE = JSON_OBJECT_SIZE(4);  //!< Calculated size for the JSON document sent for network clicks.

constexpr uint16_t SENT_DOC_MAX_SIZE = SENT_DOC_DETAILS_SIZE;  //!< The maximum possible size for any JSON document sent by the device.
}  // namespace bridgeSerial
}  // namespace constants

#endif  // LSH_CORE_COMMUNICATION_CONSTANTS_CONFIG_HPP
