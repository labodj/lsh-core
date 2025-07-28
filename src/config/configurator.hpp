/**
 * @file    configurator.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares the static Configurator class used for device setup in user code.
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

#ifndef LSHCORE_CONFIG_CONFIGURATOR_HPP
#define LSHCORE_CONFIG_CONFIGURATOR_HPP

#include <stdint.h>
#include "internal/user_config_bridge.hpp"

// Forward declarations
class Actuator;
class Clickable;
class Indicator;

/**
 * @brief "static class" used to configure the device.
 *
 */
class Configurator
{
private:
    // Helper functions for leaner config
    static void addActuator(Actuator *actuator);    // Helper to add an actuator, for leaner config.
    static void addClickable(Clickable *clickable); // Helper to add a clickable, for leaner config.
    static void addIndicator(Indicator *indicator); // Helper to add an indicator, for leaner config.

    static auto getIndex(const Actuator &actuator) -> uint8_t;   // Helper to get an actuator index, for leaner config
    static auto getIndex(const Clickable &clickable) -> uint8_t; // Helper to get a clickable index, for leaner config
    static auto getIndex(const Indicator &indicator) -> uint8_t; // Helper to get a indicator index, for leaner config

#if defined(CONTROLLINO_MAXI) || defined(CONTROLLINO_MAXI_AUTOMATION) || defined(CONTROLLINO_MEGA)
    static void disableRtc(); // Disable Controllino RTC
    static void disableEth(); // Disable COntrollino Ethernet
#endif

public:
    Configurator() = delete;
    ~Configurator() = delete;

    // Delete copy constructor, copy assignment operator, move constructor and move assignment operator
#if (__cplusplus >= 201703L) && (__GNUC__ >= 7)
    Configurator(const Configurator &) = delete;
    Configurator(Configurator &&) = delete;
    auto operator=(const Configurator &) -> Configurator & = delete;
    auto operator=(Configurator &&) -> Configurator & = delete;
#endif // (__cplusplus >= 201703L) && (__GNUC__ >= 7)

    /**
     * @brief The main user-defined device configuration function.
     * @details This function is implemented by the user in their `src/configs/device.cpp` file.
     *          It is responsible for instantiating all Actuator, Clickable, and Indicator
     *          objects and defining their relationships and behaviors.
     */
    static void configure();
    static void finalizeSetup(); // Final steps of configuration, must be called after configuration()
};

#endif // LSHCORE_CONFIG_CONFIGURATOR_HPP
