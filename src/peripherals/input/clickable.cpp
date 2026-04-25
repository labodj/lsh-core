/**
 * @file    clickable.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the small out-of-line parts of the Clickable class.
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

/**
 * @brief Store the dense runtime index assigned by the generated static profile.
 *
 * @param indexToSet Dense clickable index.
 */
void Clickable::setIndex(uint8_t indexToSet)
{
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
    this->index = indexToSet;
#else
    // Release code uses generated direct scan blocks and does not need to keep
    // a per-button registration index in SRAM. The setter remains as a no-op so
    // generated configure() code and external tests can share one API surface.
    static_cast<void>(indexToSet);
#endif
}

/**
 * @brief Return the dense runtime index assigned by the generated static profile.
 *
 * @return uint8_t Clickable index.
 */
auto Clickable::getIndex() const -> uint8_t
{
#if defined(LSH_DEBUG) || defined(LSH_STATIC_CONFIG_RUNTIME_CHECKS)
    return this->index;
#else
    return UINT8_MAX;
#endif
}
