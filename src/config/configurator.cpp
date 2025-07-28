/**
 * @file    configurator.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the helper functions of the Configurator class.
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

#include "config/configurator.hpp"

#include "device/actuator_manager.hpp"
#include "device/clickable_manager.hpp"
#include "device/indicator_manager.hpp"
#include "peripherals/input/clickable.hpp"
#include "peripherals/output/actuator.hpp"
#include "peripherals/output/indicator.hpp"
#include "util/debug/debug.hpp"

/**
 * @brief Add an actuator to device, shadows Actuators::addActuator for leaner config.
 *
 * @param actuator actuator to add.
 */
void Configurator::addActuator(Actuator *const actuator)
{
    Actuators::addActuator(actuator);
}

/**
 * @brief Add a clickable to device, shadows Clickables::addClickable for leaner config.
 *
 * @param clickable clickable to add.
 */
void Configurator::addClickable(Clickable *const clickable)
{
    Clickables::addClickable(clickable);
}

/**
 * @brief Add an indicator to device, shadows Indicators::addIndicator for leaner config.
 *
 * @param Indicator indicator to add.
 */
void Configurator::addIndicator(Indicator *const indicator)
{
    Indicators::addIndicator(indicator);
}

/**
 * @brief Helper to get an actuator index, for leaner config.
 *
 * @param actuator the actuator.
 * @return uint8_t actuator index.
 */
auto Configurator::getIndex(const Actuator &actuator) -> uint8_t
{
    return actuator.getIndex();
}

/**
 * @brief Helper to get a clickable index, for leaner config.
 *
 * @param clickable the clickable.
 * @return uint8_t clickable index.
 */
auto Configurator::getIndex(const Clickable &clickable) -> uint8_t
{
    return clickable.getIndex();
}

/**
 * @brief Helper to get an indicator index, for leaner config.
 *
 * @param indicator the indicator
 * @return uint8_t indicator index.
 */
auto Configurator::getIndex(const Indicator &indicator) -> uint8_t
{
    return indicator.getIndex();
}

/**
 * @brief Final steps of configuration, must be called after configuration().
 *
 */
void Configurator::finalizeSetup()
{
    Actuators::finalizeSetup();
    Clickables::finalizeSetup();
    Indicators::finalizeSetup();
}

#if defined(CONTROLLINO_MAXI) || defined(CONTROLLINO_MAXI_AUTOMATION) || defined(CONTROLLINO_MEGA)
#include "internal/user_config_bridge.hpp"

#ifdef CONFIG_USE_DIRECT_IO
#include <DirectIO.h>
#endif
/**
 * @brief Disables the Controllino's Real-Time Clock (RTC).
 *
 */
void Configurator::disableRtc()
{
    pinMode(CONTROLLINO_RTC_CHIP_SELECT, OUTPUT);
    digitalWrite(CONTROLLINO_RTC_CHIP_SELECT, LOW); // Disable RTC
}

/**
 * @brief Disables the Controllino's Ethernet controller.
 *
 */
void Configurator::disableEth()
{
    pinMode(CONTROLLINO_ETHERNET_CHIP_SELECT, OUTPUT);
    digitalWrite(CONTROLLINO_ETHERNET_CHIP_SELECT, HIGH); // Disable Ethernet
}
#endif // defined(CONTROLLINO_MAXI) || defined(CONTROLLINO_MAXI_AUTOMATION) || defined(CONTROLLINO_MEGA)
