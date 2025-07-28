/**
 * @file    indicator_manager.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic for managing and operating on all system Indicators.
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

#include "device/indicator_manager.hpp"

#include "internal/user_config_bridge.hpp"
#include "peripherals/output/indicator.hpp"
#include "util/constants/wrong_config_strings.hpp"
#include "util/debug/debug.hpp"
#include "util/reset.hpp"

using namespace Debug;

namespace Indicators
{
    uint8_t totalIndicators = 0U;                                //!< Device real total indicators
    etl::array<Indicator *, CONFIG_MAX_INDICATORS> indicators{}; //!< Device indicators

    /**
     * @brief Adds an indicator to the system.
     *
     * The indicator is stored in the main array. If the maximum number of indicators
     * is exceeded, the device will reset to prevent undefined behavior.
     *
     * @param indicator A pointer to the Indicator object to add.
     */
    void addIndicator(Indicator *const indicator)
    {
        const uint8_t currentIndex = totalIndicators;
        if (currentIndex >= CONFIG_MAX_INDICATORS)
        {
            using namespace constants::wrongConfigStrings;
            NDSB(); // Begin serial if not in debug mode
            CONFIG_DEBUG_SERIAL->print(FPSTR(WRONG));
            CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
            CONFIG_DEBUG_SERIAL->print(FPSTR(INDICATORS));
            CONFIG_DEBUG_SERIAL->print(FPSTR(SPACE));
            CONFIG_DEBUG_SERIAL->println(FPSTR(NUMBER));
            delay(10000UL);
            deviceReset();
        }
        indicator->setIndex(currentIndex); // Store current index inside the object, it can be useful
        indicators[currentIndex] = indicator;
        totalIndicators++;
    }

    /**
     * @brief Performs a indicator check for indicator set.
     *
     */
    void indicatorsCheck()
    {
        for (uint8_t i = 0U; i < totalIndicators; ++i)
        {
            indicators[i]->check();
        }
    }

    /**
     * @brief Resize vectors of all indicators to the actual needed size.
     *
     */
    void finalizeSetup()
    {
        DP_CONTEXT();
        for (auto *const indicator : indicators) // Resize vectors inside indicators
        {
            indicator->resizeVectors();
        }
    }

} // namespace Indicators
