/**
 * @file    lsh.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Main public entry point for the lsh-core library.
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

#ifndef LSHCORE_INCLUDE_LSH_HPP
#define LSHCORE_INCLUDE_LSH_HPP

#include "lsh_user_macros.hpp"

namespace LSH
{
    /**
     * @brief Initializes the LSH-Core framework.
     * @details This function must be called once in the Arduino `setup()` function.
     *          It initializes serial communication, applies the user-defined device
     *          configuration, and prepares all managers (Actuators, Clickables, Indicators).
     */
    void setup();

    /**
     * @brief The main execution loop for the LSH-Core framework.
     * @details This function must be called continuously in the Arduino `loop()` function.
     *          It handles input polling, click detection, network communication,
     *          and timed events like actuator auto-off timers.
     */
    void loop();
}

#endif // LSHCORE_INCLUDE_LSH_HPP
