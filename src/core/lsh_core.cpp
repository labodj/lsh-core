/**
 * @file    lsh_core.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Contains the core implementation of the lsh::core::setup() and lsh::core::loop() functions.
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

#include <lsh.hpp>

#include <stdint.h>

#include "communication/constants/static_payloads.hpp"
#include "communication/bridge_serial.hpp"
#include "communication/bridge_sync.hpp"
#include "communication/serializer.hpp"
#include "config/configurator.hpp"
#include "config/static_config.hpp"
#include "core/network_clicks.hpp"
#include "internal/user_config_bridge.hpp"
#include "util/constants/timing.hpp"
#include "util/debug/debug.hpp"
#include "util/saturating_time.hpp"
#include "util/time_keeper.hpp"

namespace lsh::core
{

using namespace Debug;
// =================================================================
// |                         SETUP                                 |
// =================================================================

/**
 * @brief Initialize the `lsh-core` runtime.
 * @details This entry point must be called exactly once from Arduino `setup()`.
 *          It initializes the optional debug serial, refreshes the cached loop
 *          time, starts the controller-to-bridge serial link, applies the
 *          user-defined topology, and then starts the bridge resynchronization
 *          handshake so the bridge must request fresh details and state before
 *          sending mutating commands.
 */
void setup()
{
#ifdef LSH_DEBUG
    DSB();  // Serial begin when in debug mode
#elif defined(CONFIG_LSH_BENCH)
    NDSB();  // Begin serial if not in debug mode
#endif
    DP_CONTEXT();
    DPL(FPSTR(dStr::COMPILED_BY_GCC), FPSTR(dStr::SPACE), __GNUC__, FPSTR(dStr::POINT), __GNUC_MINOR__, FPSTR(dStr::POINT),
        __GNUC_PATCHLEVEL__);
    timeKeeper::update();
    BridgeSerial::init();
    Configurator::configure();      // Apply user configuration and register the real runtime topology.
    Configurator::finalizeSetup();  // Finalize setup for the actually registered devices only.
    // After any controller reboot or config change, the bridge must ask for
    // REQUEST_DETAILS and REQUEST_STATE before mutating commands are trusted.
    BridgeSync::begin();
    DFM();
}

// =================================================================
// |                      MAIN LOOP                                |
// =================================================================

/**
 * @brief Execute one controller loop iteration.
 * @details This function is the hot runtime path and must be called from Arduino
 *          `loop()` forever. It refreshes the cached time, scans local inputs,
 *          handles inbound bridge traffic, manages network-click timeouts and
 *          actuator auto-off timers, then publishes the latest local state back
 *          to the bridge when a refresh is pending and the receive grace period
 *          has elapsed.
 */
void loop()
{
#if LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS > 0
    using constants::timings::ACTUATORS_AUTO_OFF_CHECK_INTERVAL_MS;
#endif
#if LSH_STATIC_CONFIG_CLICKABLES > 0
    using constants::timings::CLICKABLE_SCAN_INTERVAL_MS;
#endif
    using constants::timings::DELAY_AFTER_RECEIVE_MS;
#if CONFIG_USE_NETWORK_CLICKS
    using constants::timings::NETWORK_CLICK_CHECK_INTERVAL_MS;
#endif

    timeKeeper::update();
    const auto now = timeKeeper::getTime();
    static uint32_t lastLoopTime_ms = 0U;  //!< Previous cached loop timestamp; one 32-bit delta feeds every periodic gate.
    const uint32_t elapsedSinceLastLoop_ms = now - lastLoopTime_ms;
    lastLoopTime_ms = now;
    // AVR pays noticeably for repeated uint32_t subtractions. The loop does one
    // wrap-safe 32-bit delta, then every periodic subsystem consumes the same
    // saturated 16-bit age because none of these intervals needs multi-minute
    // precision.
    const uint16_t loopElapsed_ms = (elapsedSinceLastLoop_ms > UINT16_MAX) ? UINT16_MAX : static_cast<uint16_t>(elapsedSinceLastLoop_ms);

#ifdef CONFIG_LSH_BENCH
    using constants::timings::BENCH_ITERATIONS;
    static uint32_t benchmarkIterationCount = 0U;
    benchmarkIterationCount++;
    if (benchmarkIterationCount == BENCH_ITERATIONS)
    {
        static uint32_t lastBenchmarkTime_ms = 0U;
        const uint32_t elapsedBenchmarkTime_ms = now - lastBenchmarkTime_ms;
#ifdef LSH_DEBUG
        DPL(FPSTR(dStr::EXEC_TIME), FPSTR(dStr::SPACE), FPSTR(dStr::FOR), FPSTR(dStr::SPACE), BENCH_ITERATIONS, FPSTR(dStr::SPACE),
            FPSTR(dStr::ITERATIONS), FPSTR(dStr::COLON_SPACE), elapsedBenchmarkTime_ms);
        DFM();
#else
        CONFIG_DEBUG_SERIAL->print(F("Exec time for "));
        CONFIG_DEBUG_SERIAL->print(BENCH_ITERATIONS);
        CONFIG_DEBUG_SERIAL->print(F(" iterations: "));
        CONFIG_DEBUG_SERIAL->println(elapsedBenchmarkTime_ms);
#endif  // LSH_DEBUG
        lastBenchmarkTime_ms = now;
        benchmarkIterationCount = 0U;
    }
#endif  // CONFIG_LSH_BENCH

    // These flags persist across loop iterations so the runtime can defer bridge I/O
    // without losing the fact that a state refresh or timeout scan is still required.
    static bool mustTransmitStateToBridge = false;  //!< True when the bridge must receive a fresh actuator snapshot.
#if LSH_STATIC_CONFIG_INDICATORS > 0
    static bool mustRefreshIndicators = false;  //!< True when local indicators must be refreshed from actuator states.
#endif
#if CONFIG_USE_NETWORK_CLICKS
    static bool mustPollNetworkClickTimeouts = false;  //!< True while at least one network click transaction still needs timeout handling.
#endif
#if LSH_STATIC_CONFIG_CLICKABLES > 0
    static uint16_t clickableScanAge_ms = 0U;  //!< Saturated age since the last input scan pass.
#endif
    uint8_t receivedPayloadsThisLoop = 0U;  //!< Number of bridge payloads already dispatched in this loop iteration.
    uint16_t receivedBytesThisLoop = 0U;    //!< Raw UART bytes already consumed in this loop iteration.

    auto noteActuatorStateChanged = [&](bool stateChanged)
    {
        if (stateChanged)
        {
            mustTransmitStateToBridge = true;
#if LSH_STATIC_CONFIG_INDICATORS > 0
            mustRefreshIndicators = true;
#endif
        }
    };

    auto drainBridgeRx = [&](uint8_t maxPayloadsToDispatch, uint16_t maxBytesToConsume)
    {
        while (receivedPayloadsThisLoop < maxPayloadsToDispatch && receivedBytesThisLoop < maxBytesToConsume &&
               CONFIG_COM_SERIAL->HardwareSerial::available())
        {
            const uint16_t remainingByteBudget = static_cast<uint16_t>(maxBytesToConsume - receivedBytesThisLoop);
            const auto receiveResult = BridgeSerial::receiveAndDispatch(remainingByteBudget);
            if (receiveResult.consumedBytes == 0U)
            {
                break;
            }

            receivedBytesThisLoop = static_cast<uint16_t>(receivedBytesThisLoop + receiveResult.consumedBytes);
            noteActuatorStateChanged(receiveResult.dispatch.stateChanged);
#if CONFIG_USE_NETWORK_CLICKS
            mustPollNetworkClickTimeouts |= receiveResult.dispatch.networkClickHandled;
#endif
            if (receiveResult.payloadDispatched)
            {
                ++receivedPayloadsThisLoop;
            }
        }
    };

    if (loopElapsed_ms > 0U)
    {
        // Bridge heartbeat pacing and handshake retries intentionally use their
        // own elapsed-time gate. This keeps controller-link liveness
        // independent from the clickable scan policy while still avoiding extra
        // timestamp work on raw Arduino `loop()` iterations that happened
        // inside the same cached millisecond.
        BridgeSerial::tickSendIdleTimer(loopElapsed_ms);
        BridgeSync::tick(loopElapsed_ms);
    }

    // Rescue one pending inbound payload before deciding whether the bridge is
    // alive for long/super-long click routing. Without this bounded pre-drain,
    // a valid frame already waiting in the UART could only refresh liveness on
    // the next loop iteration and a network-clickable press could spuriously
    // fall back to the local action on the timeout edge.
#if CONFIG_USE_NETWORK_CLICKS
    if (CONFIG_COM_SERIAL->HardwareSerial::available() && !BridgeSerial::isConnected())
    {
        drainBridgeRx(1U, constants::bridgeSerial::COM_SERIAL_MAX_RX_BYTES_PER_LOOP);
    }
#endif

    // Scan local inputs only after the configured interval elapsed.
    // The loop still passes the full accumulated delta to the clickable FSM, so
    // debounce and long-click timing stay correct even when the scan policy is
    // slower than the historical ~1 kHz default or when the MCU is briefly busy.
#if LSH_STATIC_CONFIG_CLICKABLES > 0
    clickableScanAge_ms = timeUtils::addElapsedTimeSaturated(clickableScanAge_ms, loopElapsed_ms);
    if (clickableScanAge_ms >= CLICKABLE_SCAN_INTERVAL_MS)
    {
        const uint16_t clickableElapsed_ms = clickableScanAge_ms;
        clickableScanAge_ms = 0U;

        const uint8_t clickScanResultFlags = lsh::core::static_config::scanClickables(clickableElapsed_ms);
        noteActuatorStateChanged((clickScanResultFlags & lsh::core::static_config::CLICK_SCAN_STATE_CHANGED) != 0U);
#if CONFIG_USE_NETWORK_CLICKS
        mustPollNetworkClickTimeouts |= (clickScanResultFlags & lsh::core::static_config::CLICK_SCAN_NETWORK_PENDING) != 0U;
#endif
    }
#endif

    // If there is something in the serial buffer try to deserialize it.
    // The per-loop cap prevents bridge bursts from monopolising the controller
    // for too many consecutive payloads in one iteration.
    drainBridgeRx(constants::bridgeSerial::COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP, constants::bridgeSerial::COM_SERIAL_MAX_RX_BYTES_PER_LOOP);

#if CONFIG_USE_NETWORK_CLICKS
    // Timeout checks for long/super long network clicked clickables
    if (mustPollNetworkClickTimeouts)
    {
        static uint16_t networkClickCheckAge_ms = 0U;  //!< Saturated age since the last network-click timeout sweep.
        networkClickCheckAge_ms = timeUtils::addElapsedTimeSaturated(networkClickCheckAge_ms, loopElapsed_ms);
        if (networkClickCheckAge_ms > NETWORK_CLICK_CHECK_INTERVAL_MS)  // Check network click timers every N ms
        {
            networkClickCheckAge_ms = 0U;
            noteActuatorStateChanged(NetworkClicks::checkAllNetworkClicksTimers(false));
            mustPollNetworkClickTimeouts = NetworkClicks::thereAreActiveNetworkClicks();
        }
    }
#endif

// Check actuators auto OFF timer
#if LSH_STATIC_CONFIG_AUTO_OFF_ACTUATORS > 0
    static uint16_t autoOffCheckAge_ms = 0U;  //!< Saturated age since the last generated auto-off timer sweep.
    autoOffCheckAge_ms = timeUtils::addElapsedTimeSaturated(autoOffCheckAge_ms, loopElapsed_ms);
    if (autoOffCheckAge_ms > ACTUATORS_AUTO_OFF_CHECK_INTERVAL_MS)  // Check every second
    {
        autoOffCheckAge_ms = 0U;
        noteActuatorStateChanged(lsh::core::static_config::checkAutoOffTimers(now));
    }
#endif

#if LSH_STATIC_CONFIG_INDICATORS > 0
    if (mustRefreshIndicators)
    {
        lsh::core::static_config::refreshIndicators();
        mustRefreshIndicators = false;
    }
#endif

    // Publish the latest controller state to the bridge only after the grace period
    // that protects the link from immediate reply collisions after an inbound frame.
    if (mustTransmitStateToBridge)
    {
        if (BridgeSerial::receiveIdleAge_ms > DELAY_AFTER_RECEIVE_MS)
        {
            if (Serializer::serializeActuatorsState())
            {
                mustTransmitStateToBridge = false;
            }
        }
    }

    // Serializer::serializeStaticPayload(StaticType::PING); // Try to send ping to ESP
}
}  // namespace lsh::core
