/**
 * @file    clickable.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the logic of the Clickable class, including its state machine.
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

#include "peripherals/input/clickable.hpp"

#include "device/actuator_manager.hpp"
#include "device/clickable_manager.hpp"
#include "internal/etl_algorithm.hpp"
#include "internal/etl_array.hpp"
#include "peripherals/output/actuator.hpp"
#include "util/debug/debug.hpp"
#include "util/reset.hpp"
#include "util/saturating_time.hpp"

using constants::ClickType;
using namespace Debug;

/**
 * @brief Construct one iterator over a clickable actuator-link slice.
 *
 * @param entry Pointer to the current compact link entry.
 */
ClickableActuatorLinksView::Iterator::Iterator(const uint8_t *entry) noexcept : currentEntry(entry)
{}

/**
 * @brief Dereference the current compact actuator-link entry.
 *
 * @return uint8_t The dense actuator index stored in the current link.
 */
auto ClickableActuatorLinksView::Iterator::operator*() const -> uint8_t
{
    return *this->currentEntry;
}

/**
 * @brief Advance the iterator to the next compact actuator-link entry.
 *
 * @return Iterator& This iterator after the increment.
 */
auto ClickableActuatorLinksView::Iterator::operator++() -> Iterator &
{
    ++this->currentEntry;
    return *this;
}

/**
 * @brief Compare two compact actuator-link iterators for equality.
 *
 * @param other The iterator to compare with.
 * @return true if both iterators point to the same compact entry.
 * @return false otherwise.
 */
auto ClickableActuatorLinksView::Iterator::operator==(const Iterator &other) const -> bool
{
    return this->currentEntry == other.currentEntry;
}

/**
 * @brief Compare two compact actuator-link iterators for inequality.
 *
 * @param other The iterator to compare with.
 * @return true if the iterators point to different compact entries.
 * @return false otherwise.
 */
auto ClickableActuatorLinksView::Iterator::operator!=(const Iterator &other) const -> bool
{
    return !(*this == other);
}

/**
 * @brief Construct a read-only view over one compact clickable actuator-link slice.
 *
 * @param entriesBegin Pointer to the first compact link entry of the slice.
 * @param linkCount Number of valid entries in the slice.
 */
ClickableActuatorLinksView::ClickableActuatorLinksView(const uint8_t *entriesBegin, uint8_t linkCount) noexcept :
    entries(entriesBegin), count(linkCount)
{}

/**
 * @brief Return an iterator to the beginning of the compact actuator-link slice.
 *
 * @return Iterator Iterator that points to the first valid compact link entry.
 */
auto ClickableActuatorLinksView::begin() const -> Iterator
{
    return Iterator(this->entries);
}

/**
 * @brief Return an iterator one past the end of the compact actuator-link slice.
 *
 * @return Iterator Iterator that points one element past the last valid entry.
 */
auto ClickableActuatorLinksView::end() const -> Iterator
{
    if (this->entries == nullptr)
    {
        return Iterator(nullptr);
    }
    return Iterator(this->entries + this->count);
}

/**
 * @brief Return whether the compact actuator-link slice contains no entries.
 *
 * @return true if the slice is empty.
 * @return false otherwise.
 */
auto ClickableActuatorLinksView::empty() const -> bool
{
    return this->count == 0U;
}

/**
 * @brief Return the number of compact actuator-link entries in the slice.
 *
 * @return uint8_t Total number of valid entries.
 */
auto ClickableActuatorLinksView::size() const -> uint8_t
{
    return this->count;
}

namespace
{
etl::array<uint8_t, CONFIG_SHORT_CLICK_ACTUATOR_LINK_STORAGE_CAPACITY>
    shortClickActuatorLinks{};  //!< Shared storage for all short-click actuator links.
etl::array<uint8_t, CONFIG_LONG_CLICK_ACTUATOR_LINK_STORAGE_CAPACITY>
    longClickActuatorLinks{};  //!< Shared storage for all long-click actuator links.
etl::array<uint8_t, CONFIG_SUPER_LONG_CLICK_ACTUATOR_LINK_STORAGE_CAPACITY>
    superLongClickActuatorLinks{};               //!< Shared storage for all super-long-click actuator links.
uint16_t totalShortClickActuatorLinks = 0U;      //!< Number of valid entries stored in `shortClickActuatorLinks`.
uint16_t totalLongClickActuatorLinks = 0U;       //!< Number of valid entries stored in `longClickActuatorLinks`.
uint16_t totalSuperLongClickActuatorLinks = 0U;  //!< Number of valid entries stored in `superLongClickActuatorLinks`.

#if CONFIG_USE_CLICKABLE_TIMING_POOL
struct ClickableTimingOverride
{
    uint16_t longClick_ms = 0U;
    uint16_t superLongClick_ms = 0U;
};

etl::array<ClickableTimingOverride, CONFIG_CLICKABLE_TIMING_STORAGE_CAPACITY>
    clickableTimingOverrides{};  //!< Shared storage for clickables that override default timings.
uint8_t totalClickableTimingOverrides = 0U;
#endif

/**
 * @brief Abort setup when a clickable receives links before registration.
 * @details Compact pools store only actuator indexes at runtime. A clickable
 *          must already have a dense runtime index so setup can maintain its
 *          `offset + count` slice while links are appended in any order.
 */
void failUnregisteredClickable()
{
    NDSB();
    CONFIG_DEBUG_SERIAL->println(F("Wrong clickable registration order"));
    delay(10000);
    deviceReset();
}

/**
 * @brief Abort setup when one compact clickable pool is smaller than the real configuration.
 *
 * @param linkKind Human-readable pool label used in the debug error message.
 */
void failActuatorLinkOverflow(const __FlashStringHelper *linkKind)
{
    NDSB();
    CONFIG_DEBUG_SERIAL->print(F("Wrong "));
    CONFIG_DEBUG_SERIAL->print(linkKind);
    CONFIG_DEBUG_SERIAL->println(F(" actuator links number"));
    delay(10000);
    deviceReset();
}

/**
 * @brief Abort setup when a clickable references an actuator that is not registered.
 * @details `Configurator::getIndex(actuator)` returns `UINT8_MAX` until the
 *          actuator is registered. This helper treats that sentinel as a
 *          configuration error.
 */
void failInvalidActuatorLink()
{
    NDSB();
    CONFIG_DEBUG_SERIAL->println(F("Wrong actuator registration order or index"));
    delay(10000);
    deviceReset();
}

/**
 * @brief Abort setup when one click type links the same actuator more than once.
 * @details Duplicate links are not harmless: for example, a duplicated short
 *          link would toggle the same relay twice and cancel the action.
 *
 * @param linkKind Human-readable click type label used in the debug error message.
 */
void failDuplicateActuatorLink(const __FlashStringHelper *linkKind)
{
    NDSB();
    CONFIG_DEBUG_SERIAL->print(F("Duplicate "));
    CONFIG_DEBUG_SERIAL->print(linkKind);
    CONFIG_DEBUG_SERIAL->println(F(" actuator link"));
    delay(10000);
    deviceReset();
}

/**
 * @brief Abort setup when compact clickable slice metadata is internally inconsistent.
 *
 * @param linkKind Human-readable click type label used in the debug error message.
 */
void failCorruptActuatorLinks(const __FlashStringHelper *linkKind)
{
    NDSB();
    CONFIG_DEBUG_SERIAL->print(F("Corrupt "));
    CONFIG_DEBUG_SERIAL->print(linkKind);
    CONFIG_DEBUG_SERIAL->println(F(" actuator links"));
    delay(10000);
    deviceReset();
}

#if CONFIG_USE_CLICKABLE_TIMING_POOL
void failClickableTimingOverrideOverflow()
{
    NDSB();
    CONFIG_DEBUG_SERIAL->println(F("Wrong clickable timing overrides number"));
    delay(10000);
    deviceReset();
}

auto getTimingOverride(uint8_t encodedIndex) -> ClickableTimingOverride *
{
    if (encodedIndex == 0U || encodedIndex > totalClickableTimingOverrides)
    {
        return nullptr;
    }
    return &clickableTimingOverrides[static_cast<uint8_t>(encodedIndex - 1U)];
}

auto getOrCreateTimingOverride(uint8_t &encodedIndex) -> ClickableTimingOverride &
{
    if (encodedIndex == 0U)
    {
        if (totalClickableTimingOverrides >= CONFIG_MAX_CLICKABLE_TIMING_OVERRIDES)
        {
            failClickableTimingOverrideOverflow();
        }
        ++totalClickableTimingOverrides;
        encodedIndex = totalClickableTimingOverrides;
        auto &entry = clickableTimingOverrides[static_cast<uint8_t>(encodedIndex - 1U)];
        entry.longClick_ms = constants::timings::CLICKABLE_LONG_CLICK_TIME_MS;
        entry.superLongClick_ms = constants::timings::CLICKABLE_SUPER_LONG_CLICK_TIME_MS;
    }
    return clickableTimingOverrides[static_cast<uint8_t>(encodedIndex - 1U)];
}
#endif

/**
 * @brief Resolve the dense runtime index of one already-registered clickable.
 * @details This double-checks both the stored index and the manager array slot so
 *          every compact link is attached to the intended clickable owner.
 *
 * @param clickable Clickable that is about to receive a compact actuator link.
 * @return uint8_t Dense runtime index assigned by `Clickables::addClickable()`.
 */
auto getRegisteredClickableIndex(const Clickable *clickable) -> uint8_t
{
    if (clickable->getIndex() >= Clickables::totalClickables || Clickables::clickables[clickable->getIndex()] != clickable)
    {
        failUnregisteredClickable();
    }
    return clickable->getIndex();
}

/**
 * @brief Return whether the provided dense actuator index already maps to one registered actuator.
 * @details Link configuration stores dense actuator indexes, not IDs. This helper
 *          validates those indexes before they enter the compact shared pools.
 *
 * @param actuatorIndex Dense actuator index about to be stored inside a link pool.
 * @return true if the index currently refers to one registered actuator.
 * @return false if the index is out of range or still points to an unregistered slot.
 */
auto isRegisteredActuatorIndex(uint8_t actuatorIndex) -> bool
{
    return actuatorIndex < Actuators::totalActuators && Actuators::actuators[actuatorIndex] != nullptr;
}

template <size_t StorageCapacity>
auto containsActuatorLink(const etl::array<uint8_t, StorageCapacity> &storage,
                          ClickableActuatorLinkOffset offset,
                          uint8_t count,
                          uint8_t actuatorIndex) -> bool
{
    for (uint8_t relativeIndex = 0U; relativeIndex < count; ++relativeIndex)
    {
        if (storage[static_cast<uint16_t>(offset) + relativeIndex] == actuatorIndex)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Shift compact slice offsets after inserting inside the middle of one shared pool.
 */
void shiftClickableOffsetsAfterInsertion(constants::ClickType actuatorType, uint16_t insertionIndex)
{
    for (uint8_t clickableIndex = 0U; clickableIndex < Clickables::totalClickables; ++clickableIndex)
    {
        auto *const clickable = Clickables::clickables[clickableIndex];
        if (clickable != nullptr)
        {
            clickable->shiftActuatorLinksOffsetAfterInsertion(actuatorType, insertionIndex);
        }
    }
}

/**
 * @brief Insert one actuator index into the owner's compact slice.
 * @details Setup may append links in arbitrary owner order. When the target
 *          slice is not at the end of the pool, later entries are shifted by
 *          one slot and their owner offsets are updated. Runtime storage still
 *          contains only actuator indexes.
 */
template <size_t StorageCapacity>
void appendActuatorLink(etl::array<uint8_t, StorageCapacity> &storage,
                        uint16_t storageLimit,
                        uint16_t &usedEntries,
                        ClickableActuatorLinkOffset &offset,
                        uint8_t &count,
                        uint8_t actuatorIndex,
                        constants::ClickType actuatorType,
                        const __FlashStringHelper *linkKind)
{
    if (usedEntries >= storageLimit)
    {
        failActuatorLinkOverflow(linkKind);
    }

    if (containsActuatorLink(storage, offset, count, actuatorIndex))
    {
        failDuplicateActuatorLink(linkKind);
    }

    uint16_t insertionIndex = usedEntries;
    if (count == 0U)
    {
        offset = static_cast<ClickableActuatorLinkOffset>(usedEntries);
    }
    else
    {
        insertionIndex = static_cast<uint16_t>(offset) + count;
        if (insertionIndex > usedEntries)
        {
            failCorruptActuatorLinks(linkKind);
        }
        for (uint16_t moveIndex = usedEntries; moveIndex > insertionIndex; --moveIndex)
        {
            storage[moveIndex] = storage[static_cast<uint16_t>(moveIndex - 1U)];
        }
        if (insertionIndex != usedEntries)
        {
            shiftClickableOffsetsAfterInsertion(actuatorType, insertionIndex);
        }
    }

    storage[insertionIndex] = actuatorIndex;
    ++usedEntries;
    ++count;
}

template <size_t StorageCapacity>
auto makeLinksView(const etl::array<uint8_t, StorageCapacity> &storage, ClickableActuatorLinkOffset offset, uint8_t count)
    -> ClickableActuatorLinksView
{
    if (count == 0U)
    {
        return {};
    }

    const auto storageSize = static_cast<uint16_t>(storage.size());
    if (static_cast<uint16_t>(offset) >= storageSize)
    {
        return {};
    }

    const uint16_t remainingEntries = static_cast<uint16_t>(storageSize - static_cast<uint16_t>(offset));
    if (count > remainingEntries)
    {
        return {};
    }

    return ClickableActuatorLinksView(&storage[static_cast<uint16_t>(offset)], count);
}

}  // namespace

/**
 * @brief Store the Clickable index on Clickables namespace Array.
 *
 * @param indexToSet index to set.
 */
void Clickable::setIndex(uint8_t indexToSet)
{
    this->index = indexToSet;
}

/**
 * @brief Set clickable short clickability.
 *
 * @param shortClickable if the clickable is short clickable.
 * @return Clickable& the object instance.
 */
auto Clickable::setClickableShort(bool shortClickable) -> Clickable &
{
    this->configFlags.isShortClickable = shortClickable;
    return *this;
}

/**
 * @brief Set clickable long clickability locally and over network.
 *
 * @param longClickable if the clickable is long clickable.
 * @param clickType to set the type of long click.
 * @param networkClickable if the clickable is long clickable over network.
 * @param fallback the fallback type (if network isn't working).
 * @return Clickable& the object instance.
 */
auto Clickable::setClickableLong(bool longClickable,
                                 constants::LongClickType clickType,
                                 bool networkClickable,
                                 constants::NoNetworkClickType fallback) -> Clickable &
{
    this->configFlags.isLongClickable = longClickable;
    this->longClickType = clickType;
    this->configFlags.isNetworkLongClickable = networkClickable;
    this->configFlags.longNetworkFallbackDoNothing = fallback != constants::NoNetworkClickType::LOCAL_FALLBACK;
    return *this;
}

/**
 * @brief Set clickable super long clickability locally and over network.
 *
 * @param superLongClickable if the clickable is super long clickable.
 * @param clickType to set the type of super long click.
 * @param networkClickable if the clickable is super long clickable over network.
 * @param fallback the fallback type (if network isn't working).
 * @return Clickable& the object instance.
 */
auto Clickable::setClickableSuperLong(bool superLongClickable,
                                      constants::SuperLongClickType clickType,
                                      bool networkClickable,
                                      constants::NoNetworkClickType fallback) -> Clickable &
{
    this->configFlags.isSuperLongClickable = superLongClickable;
    this->superLongClickType = clickType;
    this->configFlags.isNetworkSuperLongClickable = networkClickable;
    this->configFlags.superLongNetworkFallbackDoNothing = fallback != constants::NoNetworkClickType::LOCAL_FALLBACK;
    return *this;
}

/**
 * @brief Add an actuator to a list of actuators controlled by the clickable.
 * @details The clickable must already be registered in `Clickables`, because the
 *          compact pool needs the owner's stable dense index to maintain its
 *          `offset + count` slice while setup inserts links in any order.
 *
 * @param actuatorIndex the actuator index to be attached.
 * @param actuatorType the type of the actuator.
 * @return Clickable& the object instance.
 */
auto Clickable::addActuator(uint8_t actuatorIndex, constants::ClickType actuatorType) -> Clickable &
{
    static_cast<void>(getRegisteredClickableIndex(this));
    if (!isRegisteredActuatorIndex(actuatorIndex))
    {
        failInvalidActuatorLink();
    }

    switch (actuatorType)
    {
    case ClickType::SHORT:
        appendActuatorLink(shortClickActuatorLinks, CONFIG_MAX_SHORT_CLICK_ACTUATOR_LINKS, totalShortClickActuatorLinks,
                           this->shortLinksOffset, this->shortLinksCount, actuatorIndex, ClickType::SHORT, F("short click"));
        break;
    case ClickType::LONG:
        appendActuatorLink(longClickActuatorLinks, CONFIG_MAX_LONG_CLICK_ACTUATOR_LINKS, totalLongClickActuatorLinks, this->longLinksOffset,
                           this->longLinksCount, actuatorIndex, ClickType::LONG, F("long click"));
        break;
    case ClickType::SUPER_LONG:
        appendActuatorLink(superLongClickActuatorLinks, CONFIG_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS, totalSuperLongClickActuatorLinks,
                           this->superLongLinksOffset, this->superLongLinksCount, actuatorIndex, ClickType::SUPER_LONG,
                           F("super long click"));
        break;
    default:
        break;  // Actuator Type mismatch
    }
    return *this;
}

/**
 * @brief Add a short click actuator.
 *
 * @param actuatorIndex actuator index to be attached.
 * @return Clickable& the object instance.
 */
auto Clickable::addActuatorShort(uint8_t actuatorIndex) -> Clickable &
{
    this->addActuator(actuatorIndex, ClickType::SHORT);
    return *this;
}

/**
 * @brief Add a long click actuator.
 *
 * @param actuatorIndex actuator index to be attached.
 * @return Clickable& the object instance.
 */
auto Clickable::addActuatorLong(uint8_t actuatorIndex) -> Clickable &
{
    this->addActuator(actuatorIndex, ClickType::LONG);
    return *this;
}

/**
 * @brief Add a super long click actuator.
 *
 * @param actuatorIndex actuator index to be attached.
 * @return Clickable& the object instance.
 */
auto Clickable::addActuatorSuperLong(uint8_t actuatorIndex) -> Clickable &
{
    this->addActuator(actuatorIndex, ClickType::SUPER_LONG);
    return *this;
}

/**
 * @brief Set debounce time.
 *
 * @param timeToSet_ms debounce time in ms.
 * @return Clickable& the object instance.
 */
auto Clickable::setDebounceTime(uint8_t timeToSet_ms) -> Clickable &
{
    this->debounce_ms = timeToSet_ms;
    return *this;
}

/**
 * @brief Set long click time.
 *
 * @param timeToSet_ms long click time in ms.
 * @return Clickable& the object instance.
 */
auto Clickable::setLongClickTime(uint16_t timeToSet_ms) -> Clickable &
{
#if CONFIG_USE_CLICKABLE_TIMING_POOL
    getOrCreateTimingOverride(this->timingOverrideIndexEncoded).longClick_ms = timeToSet_ms;
#else
    this->longClick_ms = timeToSet_ms;
#endif
    return *this;
}

/**
 * @brief Set super long click time.
 *
 * @param timeToSet_ms super long click time in ms.
 * @return Clickable& the object instance.
 */
auto Clickable::setSuperLongClickTime(uint16_t timeToSet_ms) -> Clickable &
{
#if CONFIG_USE_CLICKABLE_TIMING_POOL
    getOrCreateTimingOverride(this->timingOverrideIndexEncoded).superLongClick_ms = timeToSet_ms;
#else
    this->superLongClick_ms = timeToSet_ms;
#endif
    return *this;
}

auto Clickable::getLongClickTime() const -> uint16_t
{
#if CONFIG_USE_CLICKABLE_TIMING_POOL
    const auto *const timingOverride = getTimingOverride(this->timingOverrideIndexEncoded);
    return timingOverride == nullptr ? constants::timings::CLICKABLE_LONG_CLICK_TIME_MS : timingOverride->longClick_ms;
#else
    return this->longClick_ms;
#endif
}

auto Clickable::getSuperLongClickTime() const -> uint16_t
{
#if CONFIG_USE_CLICKABLE_TIMING_POOL
    const auto *const timingOverride = getTimingOverride(this->timingOverrideIndexEncoded);
    return timingOverride == nullptr ? constants::timings::CLICKABLE_SUPER_LONG_CLICK_TIME_MS : timingOverride->superLongClick_ms;
#else
    return this->superLongClick_ms;
#endif
}

/**
 * @brief Get the Clickable index on Clickables namespace Array.
 *
 * @return uint8_t clickable index.
 */
auto Clickable::getIndex() const -> uint8_t
{
    return this->index;
}

/**
 * @brief Get the unique ID of the clickable.
 *
 * @return uint8_t unique ID of the clickable.
 */
auto Clickable::getId() const -> uint8_t
{
    return this->id;
}

/**
 * @brief Get the read-only view of attached actuators.
 * @details The returned view is intentionally tiny: it stores only `pointer + count`
 *          and iterates over the already-compacted slice belonging to this clickable.
 *
 * @param actuatorType the type of actuator vector to get.
 * @return ClickableActuatorLinksView the attached actuators compact view.
 */
auto Clickable::getActuators(constants::ClickType actuatorType) const -> ClickableActuatorLinksView
{
    switch (actuatorType)
    {
    case ClickType::SHORT:
        return makeLinksView(shortClickActuatorLinks, this->shortLinksOffset, this->shortLinksCount);
    case ClickType::LONG:
        return makeLinksView(longClickActuatorLinks, this->longLinksOffset, this->longLinksCount);
    case ClickType::SUPER_LONG:
        return makeLinksView(superLongClickActuatorLinks, this->superLongLinksOffset, this->superLongLinksCount);
    default:
        return {};  // Actuator Type mismatch
    }
}

/**
 * @brief Get the number of attached actuators of one kind.
 *
 * @param actuatorType the type of the actuator.
 * @return uint8_t the number of attached actuators of one kind.
 */
auto Clickable::getTotalActuators(constants::ClickType actuatorType) const -> uint8_t
{
    switch (actuatorType)
    {
    case ClickType::SHORT:
        return this->shortLinksCount;
    case ClickType::LONG:
        return this->longLinksCount;
    case ClickType::SUPER_LONG:
        return this->superLongLinksCount;
    default:
        return 0U;  // Actuator Type mismatch
    }
}

/**
 * @brief Get the type of long click.
 *
 * @return constants::LongClickType the type of long click.
 */
auto Clickable::getLongClickType() const -> constants::LongClickType
{
    return this->longClickType;
}

/**
 * @brief Get if the clickable performs network clicks.
 *
 * @param clickType for which click type you want to know the network clickability.
 * @return true if the clickable in network clickable.
 * @return false if the clickable isn't network clickable.
 */
auto Clickable::isNetworkClickable(constants::ClickType clickType) const -> bool
{
    switch (clickType)
    {
    case ClickType::LONG:
        return this->configFlags.isNetworkLongClickable;
    case ClickType::SUPER_LONG:
        return this->configFlags.isNetworkSuperLongClickable;
    default:
        return false;
    }
}

/**
 * @brief Get the fallback type for a network click type.
 *
 * @param clickType the click type.
 * @return constants::NoNetworkClickType the fallback type.
 */
auto Clickable::getNetworkFallback(constants::ClickType clickType) const -> constants::NoNetworkClickType
{
    using constants::NoNetworkClickType;
    switch (clickType)
    {
    case ClickType::LONG:
        if (!this->configFlags.isNetworkLongClickable)
        {
            return NoNetworkClickType::NONE;
        }
        return this->configFlags.longNetworkFallbackDoNothing ? NoNetworkClickType::DO_NOTHING : NoNetworkClickType::LOCAL_FALLBACK;
    case ClickType::SUPER_LONG:
        if (!this->configFlags.isNetworkSuperLongClickable)
        {
            return NoNetworkClickType::NONE;
        }
        return this->configFlags.superLongNetworkFallbackDoNothing ? NoNetworkClickType::DO_NOTHING : NoNetworkClickType::LOCAL_FALLBACK;
    default:
        return NoNetworkClickType::NONE;
    }
}

/**
 * @brief Get the type of super long click.
 *
 * @return constants::SuperLongClickType the type of super long click.
 */
auto Clickable::getSuperLongClickType() const -> constants::SuperLongClickType
{
    return this->superLongClickType;
}

/**
 * @brief Validates the clickable's configuration.
 *
 * A clickable is considered valid if it's enabled for at least one click type
 * (short, long, or super-long) AND has at least one real action behind it.
 * A "real action" can be either:
 * - one or more attached local actuators, or
 * - a network-click path for long or super-long clicks.
 *
 * This keeps the cached validity flag aligned with the real runtime behaviour,
 * where some installations intentionally use long/super-long clicks only over
 * the bridge without any local actuator attached to that same clickable.
 *
 * @return true if the clickable has a valid and actionable configuration, false otherwise.
 */
auto Clickable::check() -> bool
{
    this->configFlags.isQuickClickable =
        (this->configFlags.isShortClickable && !this->configFlags.isLongClickable && !this->configFlags.isSuperLongClickable);

    const bool hasLocalAction = (this->shortLinksCount != 0U || this->longLinksCount != 0U || this->superLongLinksCount != 0U);
    const bool hasNetworkAction = ((this->configFlags.isLongClickable && this->configFlags.isNetworkLongClickable) ||
                                   (this->configFlags.isSuperLongClickable && this->configFlags.isNetworkSuperLongClickable));

    return ((this->configFlags.isShortClickable || this->configFlags.isLongClickable || this->configFlags.isSuperLongClickable) &&
            (hasLocalAction || hasNetworkAction));
}

/**
 * @brief Perform a short click action.
 *
 * It toggles the state of the actuator, if it was ON it turns it OFF and vice versa.
 *
 * @return true if any actuator changed state.
 * @return false otherwise.
 */
auto Clickable::shortClick() const -> bool
{
    if (!this->configFlags.isShortClickable)
    {
        return false;
    }

    bool anyActuatorChangedState = false;
    auto *const localActuators = Actuators::actuators.data();
    const auto shortActuators = this->getActuators(ClickType::SHORT);
    for (const auto actuatorIndex : shortActuators)
    {
        anyActuatorChangedState |= localActuators[actuatorIndex]->toggleState();
    }

    return anyActuatorChangedState;
}

/**
 * @brief Perform a long click action.
 *
 * The action depends on the longClickType configuration.
 * If NORMAL -> Switch ON if more than half of attached long actuators are OFF, switch OFF otherwise.
 * If ON_ONLY -> Switch ON attached long actuators.
 * If OFF_ONLY -> Switch OFF attached long actuators.
 *
 * @return true if any actuator changed state.
 * @return false otherwise.
 */
auto Clickable::longClick() const -> bool
{
    using constants::LongClickType;

    if (!this->configFlags.isLongClickable)
    {
        return false;
    }

    bool stateToSet = false;       // The state to be set to long actuators
    uint8_t actuatorsLongOn = 0U;  // Number of active long actuators
    auto *const localActuators = Actuators::actuators.data();
    const auto longActuators = this->getActuators(ClickType::LONG);

    switch (this->longClickType)
    {
    case LongClickType::NORMAL:
        // Check long actuators are ON or OFF (actuatorsLongOn==0: everything OFF, actuatorsLongOn==totalLongActuators: everything ON)
        actuatorsLongOn =
            etl::accumulate(longActuators.begin(), longActuators.end(), static_cast<uint8_t>(0U), [&](uint8_t total, uint8_t actuatorIndex)
                            { return static_cast<uint8_t>(total + static_cast<uint8_t>(localActuators[actuatorIndex]->getState())); });
        /*
         * If fewer than half of the attached long-click actuators are ON, turn
         * them ON; otherwise turn them OFF. Doubling the left side avoids
         * floating-point arithmetic and division:
         *   long actuators ON * 2 < total long actuators
         */
        stateToSet = ((actuatorsLongOn << 1U) < this->longLinksCount);
        break;
    case LongClickType::ON_ONLY:
        stateToSet = true;
        break;
    case LongClickType::OFF_ONLY:
        stateToSet = false;
        break;
    default:
        return false;  // Not a valid longClickType
    }

    bool anyActuatorChangedState = false;
    // Set stateToSet to all actuators in the long actuators list
    for (const auto actuatorIndex : longActuators)
    {
        anyActuatorChangedState |= localActuators[actuatorIndex]->setState(stateToSet);
    }
    return anyActuatorChangedState;
}

/**
 * @brief Perform a super long click selective action.
 *
 * Turns off super long unprotected actuators.
 *
 * @return true if any actuator changed state.
 * @return false otherwise.
 */
auto Clickable::superLongClickSelective() const -> bool
{
    using constants::SuperLongClickType;
    if (!this->configFlags.isSuperLongClickable || this->superLongClickType != SuperLongClickType::SELECTIVE)
    {
        return false;
    }

    bool anyActuatorChangedState = false;
    auto *const localActuators = Actuators::actuators.data();
    const auto superLongActuators = this->getActuators(ClickType::SUPER_LONG);
    for (const auto actuatorIndex : superLongActuators)
    {
        if (!localActuators[actuatorIndex]->hasProtection())
        {
            anyActuatorChangedState |= localActuators[actuatorIndex]->setState(false);
        }
    }
    return anyActuatorChangedState;
}

/**
 * @brief Perform the clickable state detection.
 * @details The main loop computes the elapsed scan time once and passes it here
 *          for every clickable. The clickable then keeps only a 16-bit elapsed
 *          age for the current state instead of re-running 32-bit timestamp
 *          arithmetic on every scan. This keeps the AVR hot path smaller while
 *          preserving the same debounce and long-press semantics.
 *
 * @param elapsed_ms Milliseconds elapsed since the previous clickable scan.
 * @return constants::ClickResult the type of the click.
 */
auto Clickable::clickDetection(uint16_t elapsed_ms) -> constants::ClickResult
{
    using constants::ClickResult;

    // Read pin state once per call for consistency.
    const bool isPressed = this->getState();
    if (this->currentState != State::IDLE)
    {
        this->stateAge_ms = timeUtils::addElapsedTimeSaturated(this->stateAge_ms, elapsed_ms);
    }

    switch (this->currentState)
    {
    case State::IDLE:
        if (isPressed)
        {
            this->currentState = State::DEBOUNCING;
            this->stateAge_ms = 0U;
        }
        break;

    case State::DEBOUNCING:
        if (this->stateAge_ms >= this->debounce_ms)
        {
            if (isPressed)
            {
                // Press confirmed. Transition to PRESSED state.
                this->currentState = State::PRESSED;
                this->stateAge_ms = 0U;  // This is the official start time of the press.
                this->lastActionFired = ActionFired::NONE;

                // If it's a "quick click" button, fire the action on press.
                const ClickableConfigFlags localFlags = this->configFlags;
                if (localFlags.isQuickClickable)
                {
                    return ClickResult::SHORT_CLICK_QUICK;
                }
            }
            else
            {
                // It was just a bounce/noise. Return to IDLE.
                this->currentState = State::IDLE;
            }
        }
        break;

    case State::PRESSED:
        if (isPressed)
        {
            const ClickableConfigFlags localFlags = this->configFlags;
            // Long must be emitted before super-long even if a slow scan jumps
            // directly beyond both thresholds. This lets one held button send
            // the long network click first and the super-long network click on
            // the following scan, preserving the intended action sequence.
            if (localFlags.isLongClickable && this->lastActionFired < ActionFired::LONG && this->stateAge_ms >= this->getLongClickTime())
            {
                this->lastActionFired = ActionFired::LONG;
                return ClickResult::LONG_CLICK;
            }

            if (localFlags.isSuperLongClickable && this->lastActionFired < ActionFired::SUPER_LONG &&
                this->stateAge_ms >= this->getSuperLongClickTime())
            {
                this->lastActionFired = ActionFired::SUPER_LONG;
                return ClickResult::SUPER_LONG_CLICK;
            }

            return ClickResult::NO_CLICK_KEEPING_CLICKED;
        }

        // If we are here, it means the button was released.
        // Set the next state and then fall through to process the release immediately.
        this->currentState = State::RELEASED;
        [[fallthrough]];

    case State::RELEASED:
    {
        const ClickableConfigFlags localFlags = this->configFlags;
        // This state is entered immediately after a release.
        // The FSM is now reset for the next cycle.
        this->currentState = State::IDLE;
        this->stateAge_ms = 0U;

        // Ignore the release if a quick click action was already fired on press.
        if (localFlags.isQuickClickable)
        {
            return ClickResult::NO_CLICK;
        }

        // If no timed action was fired during the press, it's a short click.
        if (this->lastActionFired == ActionFired::NONE)
        {
            return localFlags.isShortClickable ? ClickResult::SHORT_CLICK : ClickResult::NO_CLICK_NOT_SHORT_CLICKABLE;
        }

        // A timed action was already fired, so the release event itself doesn't trigger a new action.
        return ClickResult::NO_CLICK;
    }
    }
    return ClickResult::NO_CLICK;
}

/**
 * @brief Store the compact actuator-link offset for one click type.
 *
 * @param actuatorType Click type whose compact slice offset must be updated.
 * @param offsetToSet First entry offset inside the shared compact pool.
 */
void Clickable::setActuatorLinksOffset(constants::ClickType actuatorType, ClickableActuatorLinkOffset offsetToSet)
{
    switch (actuatorType)
    {
    case ClickType::SHORT:
        this->shortLinksOffset = offsetToSet;
        break;
    case ClickType::LONG:
        this->longLinksOffset = offsetToSet;
        break;
    case ClickType::SUPER_LONG:
        this->superLongLinksOffset = offsetToSet;
        break;
    default:
        break;
    }
}

/**
 * @brief Shift one click-type slice when setup inserts before it in a shared pool.
 *
 * @param actuatorType Click type whose pool received the inserted link.
 * @param insertionIndex Shared-pool index where the new link was inserted.
 */
void Clickable::shiftActuatorLinksOffsetAfterInsertion(constants::ClickType actuatorType, uint16_t insertionIndex)
{
    auto shiftIfAffected = [insertionIndex](ClickableActuatorLinkOffset &offset, uint8_t count)
    {
        if (count != 0U && static_cast<uint16_t>(offset) >= insertionIndex)
        {
            ++offset;
        }
    };

    switch (actuatorType)
    {
    case ClickType::SHORT:
        shiftIfAffected(this->shortLinksOffset, this->shortLinksCount);
        break;
    case ClickType::LONG:
        shiftIfAffected(this->longLinksOffset, this->longLinksCount);
        break;
    case ClickType::SUPER_LONG:
        shiftIfAffected(this->superLongLinksOffset, this->superLongLinksCount);
        break;
    default:
        break;
    }
}

namespace Clickables
{
/**
 * @brief Finalize clickable-to-actuator shared pools after user configuration.
 * @details Links are inserted directly into compact per-clickable slices during
 *          setup. This keeps runtime storage to one byte per link while still
 *          accepting configuration code that appends links in arbitrary order.
 */
void finalizeActuatorLinkStorage()
{}
}  // namespace Clickables
