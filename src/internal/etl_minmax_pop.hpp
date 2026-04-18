/**
 * @file    etl_minmax_pop.hpp
 * @author  Jacopo Labardi (labodj)
 * @brief   Restores Arduino-style min/max macros after ETL headers.
 * @note    Intentionally has no include guard and must be paired with etl_minmax_push.hpp.
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

#ifndef LSH_CORE_ETL_MINMAX_PUSHED
#error "etl_minmax_pop.hpp included without matching etl_minmax_push.hpp"
#endif

#pragma pop_macro("max")
#pragma pop_macro("min")

#undef LSH_CORE_ETL_MINMAX_PUSHED
