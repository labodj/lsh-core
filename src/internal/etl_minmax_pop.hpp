/**
 * @file    etl_minmax_pop.hpp
 * @brief   Restores Arduino-style min/max macros after ETL headers.
 * @note    Intentionally has no include guard and must be paired with etl_minmax_push.hpp.
 */

#ifndef LSHCORE_ETL_MINMAX_PUSHED
#error "etl_minmax_pop.hpp included without matching etl_minmax_push.hpp"
#endif

#pragma pop_macro("max")
#pragma pop_macro("min")

#undef LSHCORE_ETL_MINMAX_PUSHED
