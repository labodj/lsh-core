/**
 * @file    lsh_core.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Contains the core implementation of the LSH::setup() and LSH::loop() functions.
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

#include <lsh.hpp>

#include <stdint.h>

#include "communication/constants/static_payloads.hpp"
#include "communication/esp_com.hpp"
#include "communication/serializer.hpp"
#include "config/configurator.hpp"
#include "core/network_clicks.hpp"
#include "device/actuator_manager.hpp"
#include "device/clickable_manager.hpp"
#include "device/indicator_manager.hpp"
#include "internal/user_config_bridge.hpp"
#include "util/constants/clickresults.hpp"
#include "util/constants/clicktypes.hpp"
#include "util/constants/timing.hpp"
#include "util/debug/debug.hpp"
#include "util/timekeeper.hpp"

namespace LSH
{

    using namespace Debug;
    // =================================================================
    // |                         SETUP                                 |
    // =================================================================

    void setup()
    {
#ifdef LSH_DEBUG
        DSB(); // Serial begin when in debug mode
#elif defined(CONFIG_LSH_BENCH)
        NDSB(); // Begin serial if not in debug mode
#endif
        DP_CONTEXT();
        DPL(FPSTR(dStr::COMPILED_BY_GCC), FPSTR(dStr::SPACE), __GNUC__, FPSTR(dStr::POINT), __GNUC_MINOR__, FPSTR(dStr::POINT), __GNUC_PATCHLEVEL__);
        timeKeeper::update();
        EspCom::init();
        Configurator::configure();                                              // Apply configuration
        Configurator::finalizeSetup();                                          // Finalize configuration
        Serializer::serializeStaticJson(constants::payloads::StaticType::BOOT); // Start first communication, send boot payload
        DFM();
    }

    // =================================================================
    // |                      MAIN LOOP                                |
    // =================================================================

    void loop()
    {
        using constants::ClickResult;
        using constants::ClickType;
        using constants::NoNetworkClickType;
        using constants::payloads::StaticType;
        using constants::timings::ACTUATORS_AUTO_OFF_CHECK_INTERVAL_MS;
        using constants::timings::DELAY_AFTER_RECEIVE_MS;
        using constants::timings::NETWORK_CLICK_CHECK_INTERVAL_MS;

        timeKeeper::update();
        const auto now = timeKeeper::getTime();

#ifdef CONFIG_LSH_BENCH
        using constants::timings::BENCH_ITERATIONS;
        static uint32_t iterationNumber = 0U;
        iterationNumber++;
        if (iterationNumber == BENCH_ITERATIONS)
        {
            static uint32_t lastBenchTime = 0U;
            uint32_t execTime = now - lastBenchTime;
#ifdef LSH_DEBUG
            DPL(FPSTR(dStr::EXEC_TIME), FPSTR(dStr::SPACE),
                FPSTR(dStr::FOR), FPSTR(dStr::SPACE),
                BENCH_ITERATIONS, FPSTR(dStr::SPACE),
                FPSTR(dStr::ITERATIONS), FPSTR(dStr::COLON_SPACE),
                execTime);
            DFM();
#else
            CONFIG_DEBUG_SERIAL->print(F("Exec time for "));
            CONFIG_DEBUG_SERIAL->print(BENCH_ITERATIONS);
            CONFIG_DEBUG_SERIAL->print(F(" iterations: "));
            CONFIG_DEBUG_SERIAL->println(execTime);
#endif // LSH_DEBUG
            lastBenchTime = now;
            iterationNumber = 0U;
        }
#endif // CONFIG_LSH_BENCH

        // Static local vars
        static bool mustSendState = false;           //!< True if the state must be sent and light indicators must be checked, false otherwise.
        static bool mustCheckNetworkClicks = false;  //!< True if network clicks timers must be checked, false otherwise.
        static uint32_t lastClickablesCheck_ms = 0U; //!< Timestamp of the last time the clickables were checked.

        // Clickables checks (and misc...)
        if (now - lastClickablesCheck_ms) // Check approximately every millisecond (high polling rate)
        {
            Serializer::serializeStaticJson(StaticType::PING_); // Try to send ping to ESP, ~1000Hz for this function is more than enough.
            ClickType clickType = ClickType::NONE;              //!< Temp holder of clickable click type
            lastClickablesCheck_ms = now;
            for (uint8_t i = 0; i < Clickables::totalClickables; ++i)
            {
                auto *const currClickable = Clickables::clickables[i]; // Cache the pointer to the current clickable object.

                switch (currClickable->clickDetection()) // Get the clickable action and decide what to do
                {
                case ClickResult::SHORT_CLICK:       // Short click
                case ClickResult::SHORT_CLICK_QUICK: // Short quick click, only if it's not long or super long clickable
                {
                    DPL(FPSTR(dStr::CLICKABLE), FPSTR(dStr::SPACE), currClickable->getId(), FPSTR(dStr::SPACE), FPSTR(dStr::SHORT), FPSTR(dStr::SPACE), FPSTR(dStr::CLICKED));
                    clickType = ClickType::SHORT;
                    mustSendState |= currClickable->shortClick();
                }
                break; // End of short click

                case ClickResult::LONG_CLICK: // Long click
                {
                    DPL(FPSTR(dStr::CLICKABLE), FPSTR(dStr::SPACE), currClickable->getId(), FPSTR(dStr::SPACE), FPSTR(dStr::LONG), FPSTR(dStr::SPACE), FPSTR(dStr::CLICKED));
                    clickType = ClickType::LONG;
                    // Network long click
                    if (currClickable->isNetworkClickable(clickType))
                    {
                        if (EspCom::isConnected())
                        {
                            NetworkClicks::request(currClickable->getIndex(), clickType);
                            mustCheckNetworkClicks = true;
                        }
                        // If not connected and local fallback is configured perform local logic
                        else if (currClickable->getNetworkFallback(clickType) == constants::NoNetworkClickType::LOCAL_FALLBACK)
                        {
                            mustSendState |= currClickable->longClick(); // Local long click
                        }
                    }
                    else // Local long click
                    {
                        mustSendState |= currClickable->longClick();
                    }
                }
                break; // End of long click

                case ClickResult::SUPER_LONG_CLICK:
                {
                    DPL(FPSTR(dStr::CLICKABLE), FPSTR(dStr::SPACE), currClickable->getId(), FPSTR(dStr::SPACE), FPSTR(dStr::SUPER_LONG), FPSTR(dStr::SPACE), FPSTR(dStr::CLICKED));
                    clickType = ClickType::SUPER_LONG;
                    // Network super long click
                    if (currClickable->isNetworkClickable(clickType))
                    {
                        if (EspCom::isConnected())
                        {
                            NetworkClicks::request(currClickable->getIndex(), clickType);
                            mustCheckNetworkClicks = true;
                        }
                        // If not connected and local fallback is configured perform local logic
                        else if (currClickable->getNetworkFallback(clickType) == constants::NoNetworkClickType::LOCAL_FALLBACK)
                        {
                            mustSendState |= Clickables::click(currClickable, clickType); // Local super long click
                        }
                    }
                    else // Local super long click
                    {
                        mustSendState |= Clickables::click(currClickable, clickType);
                    }
                }
                break; // End of super long click

                default:
                    break;
                }
            }
        }

        // If there is something in the serial buffer try to deserialize it
        while (CONFIG_COM_SERIAL->available())
        {
            const auto dispatchResult = EspCom::receiveAndDispatch();
            mustSendState |= dispatchResult.stateChanged;
            mustCheckNetworkClicks |= dispatchResult.networkClickHandled;
        }

        // Timeout checks for long/super long network clicked clickables
        if (mustCheckNetworkClicks)
        {
            static uint32_t lastNetworkClickCheckTime_ms = 0U;                        //!< Stores last time network click timer check has been executed
            if (now - lastNetworkClickCheckTime_ms > NETWORK_CLICK_CHECK_INTERVAL_MS) // Check network click timers every N ms
            {
                lastNetworkClickCheckTime_ms = now;
                mustSendState |= NetworkClicks::checkAllNetworkClicksTimers(false);
                mustCheckNetworkClicks = NetworkClicks::thereAreActiveNetworkCLicks(); // We must check again only if there are active network clicks
            }
        }

        // Check actuators auto OFF timer
        static uint32_t lastAutoOffCheckTime_ms = 0U;                             //!< Stores last time actuators auto off timer check has been executed
        if (now - lastAutoOffCheckTime_ms > ACTUATORS_AUTO_OFF_CHECK_INTERVAL_MS) // Check every second
        {
            lastAutoOffCheckTime_ms = now;
            mustSendState |= Actuators::actuatorsAutoOffTimersCheck();
        }

        // Send new state to ESP
        if (mustSendState)
        {
            // Send state if there's no need to wait OR if we have to wait check that the delay is elapsed.
            if (now - EspCom::lastReceivedPayloadTime_ms > DELAY_AFTER_RECEIVE_MS)
            {
                Serializer::serializeActuatorsState(); // Send new state
                Indicators::indicatorsCheck();         // Check indicators
                mustSendState = false;
            }
        }

        // Serializer::serializeStaticJson(StaticType::PING); // Try to send ping to ESP
    }
}
