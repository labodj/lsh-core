/**
 * @file    debug.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements debugging helper functions.
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

#include "util/debug/debug.hpp"

#ifndef LSH_DEBUG // If not in debug mode
bool Debug::details::serialIsActive = false;

void Debug::NDSB()
{
    if (!details::serialIsActive)
    {
        CONFIG_DEBUG_SERIAL->begin(constants::debugConfigs::DEBUG_SERIAL_BAUD);
        Debug::details::serialIsActive = true;
    }
}
#endif