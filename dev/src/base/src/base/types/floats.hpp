/**
 * @file floats.hpp
 *
 * @attention `<stdfloat>` should not be used, unless necessary. Floats should be used instead.
 *
 * @brief Floats is a library analogous to `<stdfloat>` with generally shorter type names
 */
#pragma once

#include <stdfloat>
#if defined(__STDCPP_FLOAT16_T__) && defined(__STDCPP_FLOAT32_T__) \
	&& defined(__STDCPP_FLOAT64_T__) && defined(__STDCPP_FLOAT_128_T__)
using f16  = std::float16_t;
using f32  = std::float32_t;
using f64  = std::float64_t;
using f128 = std::float128_t;
#else
	#warning "Using fallback floating_point types. C++23 <stdfloat> support is not detected"

// Fallbacks if types aren't defined
using f16 = float;  // Float is 32-bit, but theres nothing better for that without `std::float16_t`.
using f32 = float;
using f64 = double;
using f128 = long double;

#endif
using f80 = long double;

#if defined(__STDCPP_FLOAT16_T__) 
	#warning "Hello"
#endif

#if defined(__STDCPP_FLOAT32_T__) 
	#warning "Hello"
#endif
#if defined(__STDCPP_FLOAT64_T__) 
	#warning "Hello"
#endif
#if defined(__STDCPP_FLOAT128_T__) 
	#warning "Hello"
#endif