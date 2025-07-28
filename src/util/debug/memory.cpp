/**
 * @file    memory.cpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Implements the free memory checking functions.
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

#include "util/debug/memory.hpp"

#if (ARDUINO >= 100)
#include <Arduino.h>
#else
#include <WProgram.h>
#endif // (ARDUINO >= 100)

extern unsigned int __heap_start;
extern void *__brkval;

/*
 * The free list structure as maintained by the
 * avr-libc memory allocation routines.
 */

/**
 * @brief Internal structure used by avr-libc to manage the free memory list.
 */
struct __freelist
{
    size_t sz;             //!< Size of the free block.
    struct __freelist *nx; //!< Pointer to the next free block.
};

/* The head of the free list structure */
extern struct __freelist *__flp; //!< Pointer to the head of the free list.

/* Calculates the size of the free list */
auto freeListSize() -> size_t
{
    struct __freelist *current = nullptr;
    size_t total = 0U;
    for (current = __flp; current != nullptr; current = current->nx)
    {
        total += 2U; /* Account for the block's header and payload */
        total += static_cast<size_t>(current->sz);
    }
    return total;
}

auto freeMemory() -> size_t
{
    size_t free_memory = 0U;
    if (reinterpret_cast<uintptr_t>(__brkval) == 0U)
    {
        free_memory = reinterpret_cast<uintptr_t>(&free_memory) - reinterpret_cast<uintptr_t>(&__heap_start);
    }
    else
    {
        free_memory = reinterpret_cast<uintptr_t>(&free_memory) - reinterpret_cast<uintptr_t>(__brkval);
        free_memory += freeListSize();
    }
    return free_memory;
}
