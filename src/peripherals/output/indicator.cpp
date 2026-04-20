/**
 * @file    indicator.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic of the Indicator class.
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

#include "peripherals/output/indicator.hpp"

#include "device/actuator_manager.hpp"
#include "device/indicator_manager.hpp"
#include "internal/etl_array.hpp"
#include "peripherals/output/actuator.hpp"
#include "util/debug/debug.hpp"
#include "util/reset.hpp"

namespace
{
using namespace Debug;

/**
 * @brief One compact indicator-to-actuator link record stored inside the shared pool.
 * @details The owner index is stored as `index + 1` so zero remains available as
 *          a cheap always-invalid sentinel during setup-time sorting and scans.
 */
struct IndicatorActuatorLinkEntry
{
    uint8_t ownerIndexEncoded = 0U;  //!< One-based indicator index. Zero is reserved as "unused".
    uint8_t actuatorIndex = 0U;      //!< Dense actuator index returned by `Configurator::getIndex()`.
};

etl::array<IndicatorActuatorLinkEntry, CONFIG_INDICATOR_ACTUATOR_LINK_STORAGE_CAPACITY>
    indicatorActuatorLinks{};               //!< Shared storage for all indicator-to-actuator links.
uint16_t totalIndicatorActuatorLinks = 0U;  //!< Number of valid entries stored in `indicatorActuatorLinks`.

/**
 * @brief Abort setup when an indicator receives links before registration.
 * @details The compact pool stores only the dense runtime owner index, so the
 *          indicator must already be present in `Indicators::indicators`.
 */
void failUnregisteredIndicator()
{
    NDSB();
    CONFIG_DEBUG_SERIAL->println(F("Wrong indicator registration order"));
    delay(10000);
    deviceReset();
}

/**
 * @brief Abort setup when the indicator compact pool is smaller than the real configuration.
 */
void failIndicatorActuatorLinkOverflow()
{
    NDSB();
    CONFIG_DEBUG_SERIAL->println(F("Wrong indicator actuator links number"));
    delay(10000);
    deviceReset();
}

/**
 * @brief Abort setup when an indicator references an actuator that is not registered yet.
 * @details This catches both wrong registration order and manually provided dense
 *          indexes that fall outside the real actuator array.
 */
void failInvalidIndicatorActuatorLink()
{
    NDSB();
    CONFIG_DEBUG_SERIAL->println(F("Wrong indicator actuator registration order or index"));
    delay(10000);
    deviceReset();
}

/**
 * @brief Abort setup when the same indicator/actuator pair is configured twice.
 */
void failDuplicateIndicatorActuatorLink()
{
    NDSB();
    CONFIG_DEBUG_SERIAL->println(F("Duplicate indicator actuator link"));
    delay(10000);
    deviceReset();
}

/**
 * @brief Resolve the dense runtime index of one already-registered indicator.
 *
 * @param indicator Indicator that is about to receive a compact actuator link.
 * @return uint8_t Dense runtime index assigned by `Indicators::addIndicator()`.
 */
auto getRegisteredIndicatorIndex(const Indicator *indicator) -> uint8_t
{
    if (indicator->getIndex() >= Indicators::totalIndicators || Indicators::indicators[indicator->getIndex()] != indicator)
    {
        failUnregisteredIndicator();
    }
    return indicator->getIndex();
}

/**
 * @brief Return whether the provided dense actuator index already maps to one registered actuator.
 *
 * @param actuatorIndex Dense actuator index about to be stored inside the indicator pool.
 * @return true if the index currently refers to one registered actuator.
 * @return false if the index is out of range or still points to an unregistered slot.
 */
auto isRegisteredActuatorIndex(uint8_t actuatorIndex) -> bool
{
    return actuatorIndex < Actuators::totalActuators && Actuators::actuators[actuatorIndex] != nullptr;
}

/**
 * @brief Return whether the shared indicator pool already contains the same owner/actuator pair.
 *
 * @param ownerIndex Dense runtime indicator index that owns the link.
 * @param actuatorIndex Dense runtime actuator index referenced by the link.
 * @return true if the exact owner/actuator pair already exists.
 * @return false otherwise.
 */
auto containsIndicatorActuatorLink(uint8_t ownerIndex, uint8_t actuatorIndex) -> bool
{
    const uint8_t ownerIndexEncoded = static_cast<uint8_t>(ownerIndex + 1U);
    for (uint16_t entryIndex = 0U; entryIndex < totalIndicatorActuatorLinks; ++entryIndex)
    {
        if (indicatorActuatorLinks[entryIndex].ownerIndexEncoded == ownerIndexEncoded &&
            indicatorActuatorLinks[entryIndex].actuatorIndex == actuatorIndex)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Sort the shared indicator compact pool by owner index.
 * @details Setup runs once, so insertion sort keeps the implementation small and
 *          stable while still producing contiguous per-indicator slices.
 */
void sortIndicatorActuatorLinksByOwner()
{
    for (uint16_t i = 1U; i < totalIndicatorActuatorLinks; ++i)
    {
        const IndicatorActuatorLinkEntry entryToInsert = indicatorActuatorLinks[i];
        uint16_t insertIndex = i;
        while (insertIndex > 0U && indicatorActuatorLinks[insertIndex - 1U].ownerIndexEncoded > entryToInsert.ownerIndexEncoded)
        {
            indicatorActuatorLinks[insertIndex] = indicatorActuatorLinks[insertIndex - 1U];
            --insertIndex;
        }
        indicatorActuatorLinks[insertIndex] = entryToInsert;
    }
}
}  // namespace

/**
 * @brief Set the indicator index on Indicators namespace Array.
 *
 * @param indexToSet index to set.
 */
void Indicator::setIndex(uint8_t indexToSet)
{
    this->index = indexToSet;
}

/**
 * @brief Add one actuator to the compact list controlled by this indicator.
 * @details The indicator must already be registered, otherwise no dense owner
 *          index exists yet for the shared compact pool.
 *
 * @param actuatorIndex index of actuator to be added.
 * @return Indicator& the object instance.
 */
auto Indicator::addActuator(uint8_t actuatorIndex) -> Indicator &
{
    const uint8_t indicatorIndex = getRegisteredIndicatorIndex(this);
    if (!isRegisteredActuatorIndex(actuatorIndex))
    {
        failInvalidIndicatorActuatorLink();
    }
    if (totalIndicatorActuatorLinks >= CONFIG_MAX_INDICATOR_ACTUATOR_LINKS)
    {
        failIndicatorActuatorLinkOverflow();
    }
    if (containsIndicatorActuatorLink(indicatorIndex, actuatorIndex))
    {
        failDuplicateIndicatorActuatorLink();
    }

    indicatorActuatorLinks[totalIndicatorActuatorLinks].ownerIndexEncoded = static_cast<uint8_t>(indicatorIndex + 1U);
    indicatorActuatorLinks[totalIndicatorActuatorLinks].actuatorIndex = actuatorIndex;
    ++totalIndicatorActuatorLinks;
    ++this->controlledActuatorsCount;
    return *this;
}

/**
 * @brief Set the mode of the indicator.
 *
 * @param indicatorMode the mode to set.
 * @return Indicator& the object instance.
 */
auto Indicator::setMode(constants::IndicatorMode indicatorMode) -> Indicator &
{
    using constants::IndicatorMode;
    switch (indicatorMode)
    {
    case IndicatorMode::ANY:
    case IndicatorMode::ALL:
    case IndicatorMode::MAJORITY:
        this->mode = indicatorMode;
        break;
    default:
        break;  // Not valid mode
    }
    return *this;
}

/**
 * @brief Switch the indicator based on controlled actuators status.
 *
 * The behavior depends on mode setting:
 *
 * If this->mode = ANY -> If any controlled actuator is ON switch ON the indicator, OFF otherwise.
 * If this->mode = ALL -> If all controlled actuators are ON switch ON the indicator, OFF otherwise.
 * If this->mode = MAJORITY -> If the majority of controlled actuators are ON switch ON the indicator, OFF otherwise.
 *
 * Indicators with zero controlled actuators are treated as OFF here as a final
 * safety net, even though setup validation should already reject that config.
 *
 */
void Indicator::check()
{
    using constants::IndicatorMode;

    if (this->controlledActuatorsCount == 0U)
    {
        if (this->actualState)
        {
            this->actualState = false;
            this->setState(false);
        }
        return;
    }

    // Cache the pointer to the global actuators array
    auto *const actuators = Actuators::actuators.data();

    // Evaluate new state
    bool newState;
    switch (this->mode)
    {
    case IndicatorMode::ANY:
    {
        newState = false;
        const auto *const linksBegin = indicatorActuatorLinks.data() + this->controlledActuatorsOffset;
        const auto *const linksEnd = linksBegin + this->controlledActuatorsCount;
        for (auto *currentLink = linksBegin; currentLink != linksEnd; ++currentLink)
        {
            newState |= actuators[currentLink->actuatorIndex]->getState();
            if (newState)
            {
                break;
            }
        }
    }
    break;
    case IndicatorMode::ALL:
    {
        newState = true;
        const auto *const linksBegin = indicatorActuatorLinks.data() + this->controlledActuatorsOffset;
        const auto *const linksEnd = linksBegin + this->controlledActuatorsCount;
        for (auto *currentLink = linksBegin; currentLink != linksEnd; ++currentLink)
        {
            newState &= actuators[currentLink->actuatorIndex]->getState();
            if (!newState)
            {
                break;
            }
        }
    }
    break;
    case IndicatorMode::MAJORITY:
    {
        uint8_t totalControlledActuatorsOn = 0U;
        const auto *const linksBegin = indicatorActuatorLinks.data() + this->controlledActuatorsOffset;
        const auto *const linksEnd = linksBegin + this->controlledActuatorsCount;
        for (auto *currentLink = linksBegin; currentLink != linksEnd; ++currentLink)
        {
            totalControlledActuatorsOn += static_cast<uint8_t>(actuators[currentLink->actuatorIndex]->getState());
        }

        /*
         * The formula is (Total Actuators ON > Controlled actuators / 2)
         * To avoid float arithmetics and to speed things up use shift operator and swap the division with a multiplication
         * (Total actuators On x 2 > controlled actuators)
         */
        newState = (totalControlledActuatorsOn << 1U) > this->controlledActuatorsCount;
    }
    break;
    default:
        return;  // Wrong IndicatorMode config
    }

    // If the state is the same as old one exit
    if (newState == this->actualState)
    {
        return;  // Same state, do nothing
    }
    this->actualState = newState;  // Store new state
    this->setState(newState);      // Apply new state
}

/**
 * @brief Get the indicator index on Indicators namespace Array.
 *
 * @return uint8_t indicator index.
 */
auto Indicator::getIndex() const -> uint8_t
{
    return this->index;
}

/**
 * @brief Return whether this indicator controls at least one actuator.
 *
 * @return true if the indicator has at least one attached actuator.
 * @return false otherwise.
 */
auto Indicator::hasAttachedActuators() const -> bool
{
    return this->controlledActuatorsCount != 0U;
}

/**
 * @brief Store the compact controlled-actuator offset for this indicator.
 *
 * @param offsetToSet First entry offset inside the shared indicator-to-actuator pool.
 */
void Indicator::setControlledActuatorsOffset(uint16_t offsetToSet)
{
    this->controlledActuatorsOffset = offsetToSet;
}

namespace Indicators
{
/**
 * @brief Sort and compact the shared indicator-to-actuator pool after user configuration.
 * @details Indicators append links while the user configuration runs. This helper
 *          sorts the shared pool by indicator index once, then stores the final
 *          compact slice offset inside each Indicator object. Runtime checks then
 *          only need `offset + count`, with no per-indicator ETL vector buffers.
 */
void finalizeActuatorLinkStorage()
{
    if (totalIndicatorActuatorLinks == 0U)
    {
        return;
    }

    sortIndicatorActuatorLinksByOwner();

    uint8_t currentOwnerIndexEncoded = 0U;
    for (uint16_t entryIndex = 0U; entryIndex < totalIndicatorActuatorLinks; ++entryIndex)
    {
        const uint8_t ownerIndexEncoded = indicatorActuatorLinks[entryIndex].ownerIndexEncoded;
        if (ownerIndexEncoded == currentOwnerIndexEncoded)
        {
            continue;
        }

        currentOwnerIndexEncoded = ownerIndexEncoded;
        indicators[ownerIndexEncoded - 1U]->setControlledActuatorsOffset(entryIndex);
    }
}
}  // namespace Indicators
