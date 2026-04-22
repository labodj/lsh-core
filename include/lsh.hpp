/**
 * @file    lsh.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Main public entry point for the lsh-core library.
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

#ifndef LSH_CORE_LSH_HPP
#define LSH_CORE_LSH_HPP

#include "lsh_user_macros.hpp"

namespace lsh::core
{
void setup();  //!< Initialize the runtime once from Arduino `setup()`.

void loop();  //!< Run one firmware iteration from Arduino `loop()`.
}  // namespace lsh::core

#endif  // LSH_CORE_LSH_HPP
