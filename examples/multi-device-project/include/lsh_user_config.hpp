/**
 * @file    lsh_user_config.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Selects the example lsh-core device profile according to the active build flag.
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

#ifndef LSH_USER_CONFIG_HPP
#define LSH_USER_CONFIG_HPP

#if defined(LSH_BUILD_J1)
#include "lsh_configs/j1_config.hpp"
#elif defined(LSH_BUILD_J2)
#include "lsh_configs/j2_config.hpp"
#endif

#endif  // LSH_USER_CONFIG_HPP
