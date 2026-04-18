/**
 * @file    cpp_features.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Centralized C++ feature-detection macros for lsh-core.
 *
 * Mirrors the ETL approach so the codebase can use a single compatibility
 * layer instead of scattering raw `__cplusplus` checks.
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

#ifndef LSH_CORE_INTERNAL_CPP_FEATURES_HPP
#define LSH_CORE_INTERNAL_CPP_FEATURES_HPP

#include "internal/etl_minmax_push.hpp"
#include <etl/platform.h>
#include "internal/etl_minmax_pop.hpp"

#define LSH_USING_CPP11 ETL_USING_CPP11
#define LSH_USING_CPP14 ETL_USING_CPP14
#define LSH_USING_CPP17 ETL_USING_CPP17
#define LSH_USING_CPP20 ETL_USING_CPP20
#define LSH_USING_CPP23 ETL_USING_CPP23

#define LSH_CONSTEXPR ETL_CONSTEXPR
#define LSH_CONSTEXPR11 ETL_CONSTEXPR11
#define LSH_CONSTEXPR14 ETL_CONSTEXPR14
#define LSH_CONSTEXPR17 ETL_CONSTEXPR17
#define LSH_CONSTEXPR20 ETL_CONSTEXPR20

// Optional constructors that can become constexpr on sufficiently modern
// toolchains. The default keeps them enabled when the compiler exposes the
// C++20 relaxed constexpr model, while still allowing explicit force on/off.
#if defined(LSH_DISABLE_AGGRESSIVE_CONSTEXPR_CTORS)
#define LSH_OPTIONAL_CONSTEXPR_CTOR
#elif defined(LSH_ENABLE_AGGRESSIVE_CONSTEXPR_CTORS)
#define LSH_OPTIONAL_CONSTEXPR_CTOR constexpr
#elif defined(__cpp_constexpr) && (__cpp_constexpr >= 201907L)
#define LSH_OPTIONAL_CONSTEXPR_CTOR constexpr
#else
#define LSH_OPTIONAL_CONSTEXPR_CTOR
#endif

#endif  // LSH_CORE_INTERNAL_CPP_FEATURES_HPP
