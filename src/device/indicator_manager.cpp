/**
 * @file    indicator_manager.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic for managing and operating on all system Indicators.
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

#include "device/indicator_manager.hpp"

#include "config/static_config.hpp"
#include "internal/user_config_bridge.hpp"
#include "peripherals/output/indicator.hpp"
#include "util/debug/debug.hpp"
#include "util/reset.hpp"

using namespace Debug;

namespace Indicators
{
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
etl::array<Indicator *, CONFIG_MAX_INDICATORS> indicators{};  //!< Device indicators
#endif

namespace
{
/**
 * @brief Abort setup when one indicator was registered without any controlled actuator.
 * @details Indicators with no attached actuators are configuration mistakes: in
 *          `ALL` mode they would otherwise evaluate to ON forever, and in any mode
 *          they express no real hardware dependency.
 */
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
void failIndicatorWithoutActuators()
{
    NDSB();
    CONFIG_DEBUG_SERIAL->println(F("Wrong indicator controlled actuators number"));
    delay(10000UL);
    deviceReset();
}
#endif

/**
 * @brief Abort setup when the dense indicator prefix was corrupted.
 */
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
void failNonCompactIndicatorStorage()
{
    NDSB();
    CONFIG_DEBUG_SERIAL->println(F("Wrong indicator storage"));
    delay(10000UL);
    deviceReset();
}
#endif
}  // namespace

/**
 * @brief Validate the configured indicator table before runtime checks start.
 * @details Indicator actions and controlled-actuator lists are generated as
 *          static code, so setup only has to validate the dense object table and
 *          reject indicators with no controlled actuator.
 *
 */
void finalizeSetup()
{
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
    DP_CONTEXT();

    for (uint8_t indicatorIndex = 0U; indicatorIndex < CONFIG_MAX_INDICATORS; ++indicatorIndex)
    {
        auto *const indicator = indicators[indicatorIndex];
        if (indicator == nullptr || indicator->getIndex() != indicatorIndex)
        {
            failNonCompactIndicatorStorage();
        }
        if (lsh::core::static_config::getIndicatorActuatorLinkCount(indicatorIndex) == 0U)
        {
            failIndicatorWithoutActuators();
        }
    }
#endif
}

}  // namespace Indicators
