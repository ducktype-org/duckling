/**
 * @file floats.hpp
 *
 * @attention `<stdfloat>` should not be used, unless necessary. Floats should be used instead.
 *
 * @brief Floats is a library analogous to `<stdfloat>` with generally shorter type names
 */
#pragma once

#include <stdfloat>
#if defined(__STDCPP_FLOAT32_T__) && defined(__STDCPP_FLOAT64_T__) && defined(__STDCPP_FLOAT128_T__)
using f32  = std::float32_t;
using f64  = std::float64_t;
using f128 = std::float128_t;
#else
	#warning "Using fallback floating_point types. C++23 <stdfloat> support is not detected"

// TODOP: Explain why is that here.
// TODOP: Maybe that should be moved to CTV, not in base.
// TODOP: Properly assert sized of double and float.
using f32  = float;
using f64  = double;
using f128 = long double;
// The size of long double is not guaranteed. On some architectures it might be 80-bits in size.
// Although in out case when using gcc we should always use std::float128_t anyways.
// TODOP: static assert that MSVC will crash when compiling this.

#endif
