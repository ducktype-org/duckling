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
 */
#pragma once

#include <cfloat>  // For mantissa sizes.
#include <limits>
#include <stdfloat>
#if defined(__STDCPP_FLOAT32_T__) && defined(__STDCPP_FLOAT64_T__)
using f32 = std::float32_t;
using f64 = std::float64_t;
#else
// @note: floats and doubles are USUALLY 32 and 64 bits in size. This is not guaranteed by the
// standard though. Here we assert that the sizes and mantissa sizes are what we expect.
// Check if we are on a platform where floats and doubles are not IEC 559 compliant (IEEE 754).
// If so, we trigger a compile-time error with a helpful message.
static_assert(
	std::numeric_limits<float>::is_iec559, "Fallback error: 'float' must be IEC 559 compliant."
);
static_assert(
	sizeof(float) == 4 && FLT_MANT_DIG == 24,
	"Fallback error: 'float' must be 32-bits to be used as 'f32'."
);
static_assert(
	sizeof(double) == 8 && DBL_MANT_DIG == 53,
	"Fallback error: 'double' must be 64-bits to be used as 'f64'."
);
static_assert(
	std::numeric_limits<double>::is_iec559, "Fallback error: 'double' must be IEC 559 compliant."
);
using f32 = float;
using f64 = double;

#endif
