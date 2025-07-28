/**
 * @file    reset.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Provides a utility function for performing a hardware reset via watchdog.
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

#ifndef LSHCORE_UTIL_RESET_HPP
#define LSHCORE_UTIL_RESET_HPP

#include <avr/wdt.h>

/**
 * @brief Performs a hardware reset using the watchdog timer.
 *
 * It's marked 'inline' to satisfy the One Definition Rule, allowing it to be
 * defined in a header included by multiple source files without linker errors.
 *
 * It's also marked 'noinline' to explicitly tell the compiler not to expand
 * this function at the call site, which prevents warnings and aligns with
 *  the optimizer's decision, as this call is unlikely.
 */
[[noreturn]] __attribute__((noinline)) inline void deviceReset()
{
    cli();
    wdt_enable(WDTO_15MS);
    while (true)
    {
        // Wait for the watchdog to bite.
    }
}

#endif // LSHCORE_UTILS_RESET_HPP
