/**
 * @file floats.hpp
 *
 * @attention `<stdfloat>` should not be used, unless necessary. Floats should be used instead.
 *
 * @brief Floats is a library analogous to `<stdfloat>` with generally shorter type names
 *
 * @note: This file provdes support for compilers or std implementations which don't provide the
 * <stdfloat> support yet. Floating point numbers are quite problematic when working with different
 * architectures since they're implementation dependent.
 *
 * When <stdfloat> is not supported, we fallback to standard types, which USUALLY have the expected
 * sizes and static_assert that their sizes are what we expect.
 *
 * On gcc with C++23 <stdfloat> is supported so we use std::floatXXX_T, where we have the sizes
 * guaranteed.
 * On clang <stdfloat> is not supported at all, even in the newest standard.
 * On MSVC <stdfloat> is not supported at all and `long double` is just an alias on `double`. This
 * means this file won't compile with MSVC.
 */
#pragma once

#include <cfloat>  // For mantissa sizes.
#include <stdfloat>
#if defined(__STDCPP_FLOAT32_T__) && defined(__STDCPP_FLOAT64_T__) && defined(__STDCPP_FLOAT128_T__)
using f32  = std::float32_t;
using f64  = std::float64_t;
using f128 = std::float128_t;
#else
	#warning "Using fallback floating_point types. C++23 <stdfloat> support is not detected"

// @note: floats and doubles are USUALLY 32 and 64 bits in size. This is not guaranteed by the
// standard though. Here we assert that the sizes and mantissa sizes are what we expect.
static_assert(
	sizeof(float) == 4 && FLT_MANT_DIG == 24,
	"Fallback error: 'float' must be 32-bits to be used as 'f32'."
);
using f32 = float;
static_assert(
	sizeof(double) == 8 && DBL_MANT_DIG == 53,
	"Fallback error: 'double' must be 64-bits to be used as 'f64'."
);
using f64 = double;

// @TODO #1498: When compiling with gcc and C++23 we're gonna use <stdfloat> anyways, thus the sizes
// of floats will be guaranteed. When compiling with clang we're gonna run into an issue where
// `sizeof(long double)` == 128, but the underlying floating point may be 80-bits in size (thats
// usually the case on most x86 architectures) and is only aligned to 128-bits. This means
// currently, when compiling with clang, our f128 type is actually an f80 under the hood.
static_assert(
	sizeof(long double) == 16, "Fallback error: 'long double' must be 128-bits to be used as 'f128'."
);
	#if LDBL_MANT_DIG != 113
		#warning \
			"long double has a size of 80-bits on this architecture. The f128 type will have will have a different size than expected"
	#endif
using f128 = long double;

#endif
