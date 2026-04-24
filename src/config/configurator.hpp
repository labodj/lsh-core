/**
 * @file    configurator.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Declares the static Configurator class used for device setup in user code.
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

#ifndef LSH_CORE_CONFIG_CONFIGURATOR_HPP
#define LSH_CORE_CONFIG_CONFIGURATOR_HPP

#include "internal/user_config_bridge.hpp"

/**
 * @brief Device-configuration facade backed by the generated static profile.
 */
class Configurator
{
private:
#if defined(CONTROLLINO_MAXI) || defined(CONTROLLINO_MAXI_AUTOMATION) || defined(CONTROLLINO_MEGA)
    /**
     * @brief Disable the Controllino RTC chip select during device setup.
     * @details Call from `Configurator::configure()` when the profile does not
     *          use the onboard RTC and wants the peripheral forced inactive.
     */
    static void disableRtc();

    /**
     * @brief Disable the Controllino Ethernet chip select during device setup.
     * @details Call from `Configurator::configure()` when Ethernet is not used
     *          by the AVR side and the SPI peripheral must stay deselected.
     */
    static void disableEth();
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
#endif  // (__cplusplus >= 201703L) && (__GNUC__ >= 7)

    /** @brief Generated device configuration hook emitted by the selected static profile. */
    static void configure();
    static void finalizeSetup();  // Final setup pass executed after configuration registration.
};

#endif  // LSH_CORE_CONFIG_CONFIGURATOR_HPP
