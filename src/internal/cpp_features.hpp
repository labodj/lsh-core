/**
 * @file    cpp_features.hpp
 * @brief   Centralized C++ feature-detection macros for lsh-core.
 *
 * Mirrors the ETL approach so the codebase can use a single compatibility
 * layer instead of scattering raw `__cplusplus` checks.
 */

#ifndef LSHCORE_INTERNAL_CPP_FEATURES_HPP
#define LSHCORE_INTERNAL_CPP_FEATURES_HPP

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

#endif // LSHCORE_INTERNAL_CPP_FEATURES_HPP
