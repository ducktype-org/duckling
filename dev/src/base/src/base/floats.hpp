/**
 * @file floats.hpp
 *
 * @attention `<stdfloat>` should not be used, unless necessary. Floats should be used instead.
 *
 * @brief Floats is a library analogous to `<stdfloat>` with generally shorter type names
 */
#pragma once

#include <stdfloat>
// Use feature-test macro to satisfy linter; float**_t is available at compile time
#if defined(__STDCPP_FLOAT64_T__)
using f16  = std::float16_t;
using f32  = std::float32_t;
using f64  = std::float64_t;
using f128 = std::float128_t;
#else
	#include <cstdint>
// Fallbacks if types aren't defined
using f16  = uint16_t;  // nothing better for that
using f32  = float;
using f64  = double;
using f128 = long double;
#endif
// No standardized float80_t yet
using f80 = long double;
