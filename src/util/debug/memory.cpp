/**
 * @file    memory.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the free memory checking functions.
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

#include "util/debug/memory.hpp"

#if (ARDUINO >= 100)
#include <Arduino.h>
#else
#include <WProgram.h>
#endif  // (ARDUINO >= 100)

using AvrHeapStart = unsigned int;

/**
 * @brief Internal structure used by avr-libc to manage the free memory list.
 */
struct AvrFreeListEntry
{
    size_t size;             //!< Size of the free block payload.
    AvrFreeListEntry *next;  //!< Pointer to the next free block.
};

extern "C" {
extern AvrHeapStart avrHeapStart asm("__heap_start");
extern void *avrBreakValue asm("__brkval");
extern AvrFreeListEntry *avrFreeListHead asm("__flp");
}

/**
 * @brief Calculate the total payload currently stored in avr-libc's free list.
 * @details This helper walks the allocator free list so fragmented heap blocks
 *          contribute to the final free-memory estimate returned by `freeMemory()`.
 *
 * @return size_t Total bytes available inside the allocator free list.
 */
auto freeListSize() -> size_t
{
    size_t total = 0U;
    for (const auto *current = avrFreeListHead; current != nullptr; current = current->next)
    {
        total += 2U; /* Account for the block's header and payload */
        total += static_cast<size_t>(current->size);
    }
    return total;
}

/**
 * @brief Estimate the amount of free SRAM currently available on the MCU.
 * @details The implementation combines the stack-to-heap gap with the bytes
 *          stored in avr-libc's free list so fragmented allocations are counted too.
 *
 * @return size_t Estimated free SRAM in bytes.
 */
auto freeMemory() -> size_t
{
    size_t free_memory = 0U;
    if (reinterpret_cast<uintptr_t>(avrBreakValue) == 0U)
    {
        free_memory = reinterpret_cast<uintptr_t>(&free_memory) - reinterpret_cast<uintptr_t>(&avrHeapStart);
    }
    else
    {
        free_memory = reinterpret_cast<uintptr_t>(&free_memory) - reinterpret_cast<uintptr_t>(avrBreakValue);
        free_memory += freeListSize();
    }
    return free_memory;
}
