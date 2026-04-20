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
#include "internal/etl_array.hpp"
#include "peripherals/output/actuator.hpp"
#include "util/debug/debug.hpp"
#include "util/reset.hpp"
#include "util/saturating_time.hpp"

/**
 * @brief One compact clickable-to-actuator link record stored inside the shared pools.
 * @details The owner index is stored as `index + 1` so zero remains available as
 *          a cheap always-invalid sentinel during setup-time sorting and scans.
 */
struct ClickableActuatorLinkEntry
{
    uint8_t ownerIndexEncoded = 0U;  //!< One-based clickable index. Zero is reserved as "unused".
    uint8_t actuatorIndex = 0U;      //!< Dense actuator index returned by `Configurator::getIndex()`.
};

using constants::ClickType;
using namespace Debug;

/**
 * @brief Construct one iterator over a clickable actuator-link slice.
 *
 * @param entry Pointer to the current compact link entry.
 */
ClickableActuatorLinksView::Iterator::Iterator(const ClickableActuatorLinkEntry *entry) noexcept : currentEntry(entry)
{}

/**
 * @brief Dereference the current compact actuator-link entry.
 *
 * @return uint8_t The dense actuator index stored in the current link.
 */
auto ClickableActuatorLinksView::Iterator::operator*() const -> uint8_t
{
    return this->currentEntry->actuatorIndex;
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
ClickableActuatorLinksView::ClickableActuatorLinksView(const ClickableActuatorLinkEntry *entriesBegin, uint8_t linkCount) noexcept :
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
etl::array<ClickableActuatorLinkEntry, CONFIG_SHORT_CLICK_ACTUATOR_LINK_STORAGE_CAPACITY>
    shortClickActuatorLinks{};  //!< Shared storage for all short-click actuator links.
etl::array<ClickableActuatorLinkEntry, CONFIG_LONG_CLICK_ACTUATOR_LINK_STORAGE_CAPACITY>
    longClickActuatorLinks{};  //!< Shared storage for all long-click actuator links.
etl::array<ClickableActuatorLinkEntry, CONFIG_SUPER_LONG_CLICK_ACTUATOR_LINK_STORAGE_CAPACITY>
    superLongClickActuatorLinks{};               //!< Shared storage for all super-long-click actuator links.
uint16_t totalShortClickActuatorLinks = 0U;      //!< Number of valid entries stored in `shortClickActuatorLinks`.
uint16_t totalLongClickActuatorLinks = 0U;       //!< Number of valid entries stored in `longClickActuatorLinks`.
uint16_t totalSuperLongClickActuatorLinks = 0U;  //!< Number of valid entries stored in `superLongClickActuatorLinks`.

/**
 * @brief Abort setup when a clickable receives links before registration.
 * @details Compact pools store only dense owner indexes, not raw pointers. A
 *          clickable therefore must already be present in `Clickables::clickables`
 *          before any `addActuator...()` call can append a link for it.
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

/**
 * @brief Append one compact clickable-to-actuator link to a shared pool.
 * @details The owner index is stored as `index + 1` so zero remains available as
 *          an always-invalid sentinel. This keeps sorting and debugging simple.
 *
 * @tparam StorageCapacity Compile-time ETL storage size of the destination pool.
 * @param storage Shared compact pool that receives the new entry.
 * @param storageLimit Real logical maximum accepted for this link category.
 * @param usedEntries Current number of valid entries already stored in the pool.
 * @param ownerIndex Dense runtime clickable index that owns the new link.
 * @param actuatorIndex Dense runtime actuator index referenced by the new link.
 * @param linkKind Human-readable pool label used if the logical limit is exceeded.
 */
template <size_t StorageCapacity>
void appendActuatorLink(etl::array<ClickableActuatorLinkEntry, StorageCapacity> &storage,
                        uint16_t storageLimit,
                        uint16_t &usedEntries,
                        uint8_t ownerIndex,
                        uint8_t actuatorIndex,
                        const __FlashStringHelper *linkKind)
{
    if (usedEntries >= storageLimit)
    {
        failActuatorLinkOverflow(linkKind);
    }

    storage[usedEntries].ownerIndexEncoded = static_cast<uint8_t>(ownerIndex + 1U);
    storage[usedEntries].actuatorIndex = actuatorIndex;
    ++usedEntries;
}

/**
 * @brief Return whether one compact clickable pool already contains the same owner/actuator pair.
 * @details This linear scan runs only during setup, so keeping it simple avoids
 *          extra RAM while still preventing duplicate links from slipping into
 *          the runtime pools.
 *
 * @tparam StorageCapacity Compile-time ETL storage size of the destination pool.
 * @param storage Shared compact pool to inspect.
 * @param usedEntries Number of valid entries currently stored in the pool.
 * @param ownerIndex Dense runtime clickable index that owns the link.
 * @param actuatorIndex Dense runtime actuator index referenced by the link.
 * @return true if the exact owner/actuator pair already exists in the pool.
 * @return false otherwise.
 */
template <size_t StorageCapacity>
auto containsActuatorLink(const etl::array<ClickableActuatorLinkEntry, StorageCapacity> &storage,
                          uint16_t usedEntries,
                          uint8_t ownerIndex,
                          uint8_t actuatorIndex) -> bool
{
    const uint8_t ownerIndexEncoded = static_cast<uint8_t>(ownerIndex + 1U);
    for (uint16_t entryIndex = 0U; entryIndex < usedEntries; ++entryIndex)
    {
        if (storage[entryIndex].ownerIndexEncoded == ownerIndexEncoded && storage[entryIndex].actuatorIndex == actuatorIndex)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Sort one compact clickable pool by owner index.
 * @details Setup runs once, so a tiny insertion sort keeps the implementation
 *          small and predictable on AVR while still producing contiguous slices
 *          for each clickable.
 *
 * @tparam StorageCapacity Compile-time ETL storage size of the destination pool.
 * @param storage Pool to sort in place.
 * @param usedEntries Number of valid entries currently stored in the pool.
 */
template <size_t StorageCapacity>
void sortActuatorLinksByOwner(etl::array<ClickableActuatorLinkEntry, StorageCapacity> &storage, uint16_t usedEntries)
{
    for (uint16_t i = 1U; i < usedEntries; ++i)
    {
        const ClickableActuatorLinkEntry entryToInsert = storage[i];
        uint16_t insertIndex = i;
        while (insertIndex > 0U && storage[insertIndex - 1U].ownerIndexEncoded > entryToInsert.ownerIndexEncoded)
        {
            storage[insertIndex] = storage[insertIndex - 1U];
            --insertIndex;
        }
        storage[insertIndex] = entryToInsert;
    }
}

/**
 * @brief Create a lightweight view over one compact clickable pool slice.
 *
 * @tparam StorageCapacity Compile-time ETL storage size of the source pool.
 * @param storage Shared compact pool that owns the entries.
 * @param offset First valid entry of the slice for the current clickable.
 * @param count Number of valid entries in the slice.
 * @return ClickableActuatorLinksView Read-only iterable view for runtime traversal.
 */
template <size_t StorageCapacity>
auto makeLinksView(const etl::array<ClickableActuatorLinkEntry, StorageCapacity> &storage, uint16_t offset, uint8_t count)
    -> ClickableActuatorLinksView
{
    if (count == 0U)
    {
        return {};
    }
    return ClickableActuatorLinksView(storage.data() + offset, count);
}

/**
 * @brief Finalize one compact clickable pool after user configuration.
 * @details Once links are sorted by owner, each clickable only needs the offset
 *          of its first entry plus the count it already tracked while links were
 *          being appended. Runtime iteration then becomes a simple contiguous walk.
 *
 * @tparam StorageCapacity Compile-time ETL storage size of the source pool.
 * @param storage Shared compact pool to finalize.
 * @param usedEntries Number of valid entries currently stored in the pool.
 * @param clickType Click type represented by the pool being finalized.
 */
template <size_t StorageCapacity>
void assignActuatorLinksOffsets(etl::array<ClickableActuatorLinkEntry, StorageCapacity> &storage,
                                uint16_t usedEntries,
                                constants::ClickType clickType)
{
    if (usedEntries == 0U)
    {
        return;
    }

    sortActuatorLinksByOwner(storage, usedEntries);

    uint8_t currentOwnerIndexEncoded = 0U;
    for (uint16_t entryIndex = 0U; entryIndex < usedEntries; ++entryIndex)
    {
        const uint8_t ownerIndexEncoded = storage[entryIndex].ownerIndexEncoded;
        if (ownerIndexEncoded == currentOwnerIndexEncoded)
        {
            continue;
        }

        currentOwnerIndexEncoded = ownerIndexEncoded;
        Clickables::clickables[ownerIndexEncoded - 1U]->setActuatorLinksOffset(clickType, entryIndex);
    }
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
    this->longClickFallback = fallback;
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
    this->superLongClickFallback = fallback;
    return *this;
}

/**
 * @brief Add an actuator to a list of actuators controlled by the clickable.
 * @details The clickable must already be registered in `Clickables`, because the
 *          compact pool stores the dense owner index assigned during registration.
 *
 * @param actuatorIndex the actuator index to be attached.
 * @param actuatorType the type of the actuator.
 * @return Clickable& the object instance.
 */
auto Clickable::addActuator(uint8_t actuatorIndex, constants::ClickType actuatorType) -> Clickable &
{
    const uint8_t clickableIndex = getRegisteredClickableIndex(this);
    if (!isRegisteredActuatorIndex(actuatorIndex))
    {
        failInvalidActuatorLink();
    }

    switch (actuatorType)
    {
    case ClickType::SHORT:
        if (containsActuatorLink(shortClickActuatorLinks, totalShortClickActuatorLinks, clickableIndex, actuatorIndex))
        {
            failDuplicateActuatorLink(F("short click"));
        }
        appendActuatorLink(shortClickActuatorLinks, CONFIG_MAX_SHORT_CLICK_ACTUATOR_LINKS, totalShortClickActuatorLinks, clickableIndex,
                           actuatorIndex, F("short click"));
        ++this->shortLinksCount;
        break;
    case ClickType::LONG:
        if (containsActuatorLink(longClickActuatorLinks, totalLongClickActuatorLinks, clickableIndex, actuatorIndex))
        {
            failDuplicateActuatorLink(F("long click"));
        }
        appendActuatorLink(longClickActuatorLinks, CONFIG_MAX_LONG_CLICK_ACTUATOR_LINKS, totalLongClickActuatorLinks, clickableIndex,
                           actuatorIndex, F("long click"));
        ++this->longLinksCount;
        break;
    case ClickType::SUPER_LONG:
        if (containsActuatorLink(superLongClickActuatorLinks, totalSuperLongClickActuatorLinks, clickableIndex, actuatorIndex))
        {
            failDuplicateActuatorLink(F("super long click"));
        }
        appendActuatorLink(superLongClickActuatorLinks, CONFIG_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS, totalSuperLongClickActuatorLinks,
                           clickableIndex, actuatorIndex, F("super long click"));
        ++this->superLongLinksCount;
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
    this->longClick_ms = timeToSet_ms;
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
    this->superLongClick_ms = timeToSet_ms;
    return *this;
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
        return this->longClickFallback;
    case ClickType::SUPER_LONG:
        return this->superLongClickFallback;
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
    this->configFlags.isChecked = true;  // We have checked the clickable
    this->configFlags.isQuickClickable =
        (this->configFlags.isShortClickable && !this->configFlags.isLongClickable && !this->configFlags.isSuperLongClickable);

    const bool hasLocalAction = (this->shortLinksCount != 0U || this->longLinksCount != 0U || this->superLongLinksCount != 0U);
    const bool hasNetworkAction = ((this->configFlags.isLongClickable && this->configFlags.isNetworkLongClickable) ||
                                   (this->configFlags.isSuperLongClickable && this->configFlags.isNetworkSuperLongClickable));

    this->configFlags.isValid =
        ((this->configFlags.isShortClickable || this->configFlags.isLongClickable || this->configFlags.isSuperLongClickable) &&
         (hasLocalAction || hasNetworkAction));
    return this->configFlags.isValid;
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
    for (const auto actuatorIndex : this->getActuators(ClickType::SHORT))
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

    switch (this->longClickType)
    {
    case LongClickType::NORMAL:
        // Check long actuators are ON or OFF (actuatorsLongOn==0: everything OFF, actuatorsLongOn==totalLongActuators: everything ON)
        for (const auto actuatorIndex : this->getActuators(ClickType::LONG))
        {
            actuatorsLongOn += static_cast<uint8_t>(localActuators[actuatorIndex]->getState());
        }
        /*
        Less than half of attached long actuators are ON -> stateToSet = true
        More or equal than half of attached long actuators are ON -> stateToSet = false
        The formula is(Long actuators ON < Total actuators / 2)
        To avoid float arithmetics and to speed things up use shift operator and swap the division with a multiplication
        (Long actuators ON x 2 < Total actuators)
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
    for (const auto actuatorIndex : this->getActuators(ClickType::LONG))
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
    for (const auto actuatorIndex : this->getActuators(ClickType::SUPER_LONG))
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

    // --- Optimization: Cache the config flags ---
    // Read the entire bitfield from RAM into a local variable once.
    // The compiler will very likely keep this in a CPU register for the fastest possible access.
    const ClickableConfigFlags localFlags = this->configFlags;

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
            // Check for super long press first (higher priority).
            if (localFlags.isSuperLongClickable && this->lastActionFired < ActionFired::SUPER_LONG &&
                this->stateAge_ms >= this->superLongClick_ms)
            {
                this->lastActionFired = ActionFired::SUPER_LONG;
                return ClickResult::SUPER_LONG_CLICK;
            }

            // Then check for long press.
            if (localFlags.isLongClickable && this->lastActionFired < ActionFired::LONG && this->stateAge_ms >= this->longClick_ms)
            {
                this->lastActionFired = ActionFired::LONG;
                return ClickResult::LONG_CLICK;
            }

            return ClickResult::NO_CLICK_KEEPING_CLICKED;
        }

        // If we are here, it means the button was released.
        // Set the next state and then fall through to process the release immediately.
        this->currentState = State::RELEASED;
        [[fallthrough]];

    case State::RELEASED:
    {
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
void Clickable::setActuatorLinksOffset(constants::ClickType actuatorType, uint16_t offsetToSet)
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

namespace Clickables
{
/**
 * @brief Sort and compact all clickable-to-actuator shared pools after user configuration.
 * @details `Configurator::configure()` may append links in any order. This helper
 *          runs once during setup, sorts every shared pool by clickable index, and
 *          stores the final slice offset inside each Clickable object. Runtime code
 *          then needs only `offset + count` to iterate over the compact pool.
 */
void finalizeActuatorLinkStorage()
{
    assignActuatorLinksOffsets(shortClickActuatorLinks, totalShortClickActuatorLinks, ClickType::SHORT);
    assignActuatorLinksOffsets(longClickActuatorLinks, totalLongClickActuatorLinks, ClickType::LONG);
    assignActuatorLinksOffsets(superLongClickActuatorLinks, totalSuperLongClickActuatorLinks, ClickType::SUPER_LONG);
}
}  // namespace Clickables
