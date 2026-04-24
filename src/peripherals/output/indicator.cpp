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

etl::array<uint8_t, CONFIG_INDICATOR_ACTUATOR_LINK_STORAGE_CAPACITY>
    indicatorActuatorLinks{};               //!< Shared storage for all indicator-to-actuator links.
uint16_t totalIndicatorActuatorLinks = 0U;  //!< Number of valid entries stored in `indicatorActuatorLinks`.

/**
 * @brief Abort setup when an indicator receives links before registration.
 * @details The compact pool stores only actuator indexes at runtime. The
 *          indicator must already have a dense runtime index so setup can
 *          maintain its `offset + count` slice while links are appended in any
 *          order.
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
 * @brief Abort setup when compact indicator slice metadata is internally inconsistent.
 */
void failCorruptIndicatorActuatorLinks()
{
    NDSB();
    CONFIG_DEBUG_SERIAL->println(F("Corrupt indicator actuator links"));
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
 * @brief Return whether the indicator slice already contains the same actuator link.
 *
 * @param offset First entry of the indicator slice inside the shared pool.
 * @param count Number of links already attached to the indicator.
 * @param actuatorIndex Dense runtime actuator index referenced by the link.
 * @return true if the exact actuator link already exists in the indicator slice.
 * @return false otherwise.
 */
auto containsIndicatorActuatorLink(IndicatorActuatorLinkOffset offset, uint8_t count, uint8_t actuatorIndex) -> bool
{
    for (uint8_t relativeIndex = 0U; relativeIndex < count; ++relativeIndex)
    {
        if (indicatorActuatorLinks[static_cast<uint16_t>(offset) + relativeIndex] == actuatorIndex)
        {
            return true;
        }
    }
    return false;
}

void shiftIndicatorOffsetsAfterInsertion(uint16_t insertionIndex)
{
    for (uint8_t indicatorIndex = 0U; indicatorIndex < Indicators::totalIndicators; ++indicatorIndex)
    {
        auto *const indicator = Indicators::indicators[indicatorIndex];
        if (indicator != nullptr)
        {
            indicator->shiftControlledActuatorsOffsetAfterInsertion(insertionIndex);
        }
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
 * @details The indicator must already be registered so setup can maintain its
 *          stable `offset + count` slice while links are inserted in any order.
 *
 * @param actuatorIndex index of actuator to be added.
 * @return Indicator& the object instance.
 */
auto Indicator::addActuator(uint8_t actuatorIndex) -> Indicator &
{
    static_cast<void>(getRegisteredIndicatorIndex(this));
    if (!isRegisteredActuatorIndex(actuatorIndex))
    {
        failInvalidIndicatorActuatorLink();
    }
    if (totalIndicatorActuatorLinks >= CONFIG_MAX_INDICATOR_ACTUATOR_LINKS)
    {
        failIndicatorActuatorLinkOverflow();
    }
    if (containsIndicatorActuatorLink(this->controlledActuatorsOffset, this->controlledActuatorsCount, actuatorIndex))
    {
        failDuplicateIndicatorActuatorLink();
    }

    uint16_t insertionIndex = totalIndicatorActuatorLinks;
    if (this->controlledActuatorsCount == 0U)
    {
        this->controlledActuatorsOffset = static_cast<IndicatorActuatorLinkOffset>(totalIndicatorActuatorLinks);
    }
    else
    {
        insertionIndex = static_cast<uint16_t>(this->controlledActuatorsOffset) + this->controlledActuatorsCount;
        if (insertionIndex > totalIndicatorActuatorLinks)
        {
            failCorruptIndicatorActuatorLinks();
        }
        for (uint16_t moveIndex = totalIndicatorActuatorLinks; moveIndex > insertionIndex; --moveIndex)
        {
            indicatorActuatorLinks[moveIndex] = indicatorActuatorLinks[static_cast<uint16_t>(moveIndex - 1U)];
        }
        if (insertionIndex != totalIndicatorActuatorLinks)
        {
            shiftIndicatorOffsetsAfterInsertion(insertionIndex);
        }
    }

    indicatorActuatorLinks[insertionIndex] = actuatorIndex;
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
        for (const auto *currentLink = linksBegin; currentLink != linksEnd; ++currentLink)
        {
            newState |= actuators[*currentLink]->getState();
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
        for (const auto *currentLink = linksBegin; currentLink != linksEnd; ++currentLink)
        {
            newState &= actuators[*currentLink]->getState();
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
        for (const auto *currentLink = linksBegin; currentLink != linksEnd; ++currentLink)
        {
            totalControlledActuatorsOn += static_cast<uint8_t>(actuators[*currentLink]->getState());
        }

        /*
         * Equivalent to: actuators ON > controlled actuators / 2.
         * Doubling the left side avoids floating-point arithmetic and division.
         */
        newState = (totalControlledActuatorsOn << 1U) > this->controlledActuatorsCount;
    }
    break;
    default:
        return;  // Wrong IndicatorMode config
    }

    // If the state did not change, avoid touching the output pin.
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
void Indicator::setControlledActuatorsOffset(IndicatorActuatorLinkOffset offsetToSet)
{
    this->controlledActuatorsOffset = offsetToSet;
}

/**
 * @brief Shift this indicator slice when setup inserts before it in the shared pool.
 *
 * @param insertionIndex Shared-pool index where a new indicator link was inserted.
 */
void Indicator::shiftControlledActuatorsOffsetAfterInsertion(uint16_t insertionIndex)
{
    if (this->controlledActuatorsCount != 0U && static_cast<uint16_t>(this->controlledActuatorsOffset) >= insertionIndex)
    {
        ++this->controlledActuatorsOffset;
    }
}

namespace Indicators
{
/**
 * @brief Finalize the shared indicator-to-actuator pool after user configuration.
 * @details Links are inserted directly into compact per-indicator slices during
 *          setup. This keeps runtime storage to one byte per link while still
 *          accepting configuration code that appends links in arbitrary order.
 */
void finalizeActuatorLinkStorage()
{}
}  // namespace Indicators
