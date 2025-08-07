/**
 * @file floats.hpp
 *
 * @attention `<stdfloat>` should not be used, unless necessary. Ints should be used instead.
 *
 * @brief Floats is a library analogous to `<stdfloat>` with generally shorter type names
 */
#pragma once

#include <stdfloat>

using f16  = std::float16_t;
using f32  = std::float32_t;
using f64  = std::float64_t;
using f80  = long double;  // no float80_t
using f128 = std::float128_t;
