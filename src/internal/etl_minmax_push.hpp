/**
 * @file    etl_minmax_push.hpp
 * @brief   Temporarily removes Arduino-style min/max macros before ETL headers.
 * @note    Intentionally has no include guard and must be paired with etl_minmax_pop.hpp.
 */

#ifdef LSHCORE_ETL_MINMAX_PUSHED
#error "etl_minmax_push.hpp included twice without matching etl_minmax_pop.hpp"
#endif

#define LSHCORE_ETL_MINMAX_PUSHED

#pragma push_macro("min")
#pragma push_macro("max")

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif
