/**
 * @file    configurator.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the helper functions of the Configurator class.
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

#include "config/configurator.hpp"

#include "device/actuator_manager.hpp"
#include "device/clickable_manager.hpp"
#include "device/indicator_manager.hpp"

/**
 * @brief Final steps of configuration, must be called after configuration().
 *
 */
void Configurator::finalizeSetup()
{
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
    Actuators::finalizeSetup();
    Clickables::finalizeSetup();
    Indicators::finalizeSetup();
#endif
}

#define LSH_STATIC_CONFIG_IMPLEMENTATION_PASS 1
#include LSH_STATIC_CONFIG_INCLUDE
#undef LSH_STATIC_CONFIG_IMPLEMENTATION_PASS

#if defined(CONTROLLINO_MAXI) || defined(CONTROLLINO_MAXI_AUTOMATION) || defined(CONTROLLINO_MEGA)
/**
 * @brief Disable the Controllino's Real-Time Clock chip select.
 * @details This helper is intentionally kept on `Configurator` so a device
 *          profile can opt out of the onboard RTC from inside `configure()`
 *          without spreading board-specific pin knowledge across user code.
 */
void Configurator::disableRtc()
{
    pinMode(CONTROLLINO_RTC_CHIP_SELECT, OUTPUT);
    digitalWrite(CONTROLLINO_RTC_CHIP_SELECT, LOW);
}

/**
 * @brief Disable the Controllino Ethernet controller chip select.
 * @details Use this from `Configurator::configure()` when the AVR firmware does
 *          not own Ethernet and the controller must remain deselected on SPI.
 */
void Configurator::disableEth()
{
    pinMode(CONTROLLINO_ETHERNET_CHIP_SELECT, OUTPUT);
    digitalWrite(CONTROLLINO_ETHERNET_CHIP_SELECT, HIGH);
}
#endif  // defined(CONTROLLINO_MAXI) || defined(CONTROLLINO_MAXI_AUTOMATION) || defined(CONTROLLINO_MEGA)
