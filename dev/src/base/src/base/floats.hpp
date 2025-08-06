/**
 * @file floats.hpp
 *
 * @attention `<stdfloat>` should not be used, unless necessary. Ints should be used instead.
 *
 * @brief Floats is a library analogous to `<stdfloat>` with generally shorter type names
 * and no big improvement such as in ints: `u8` and `i8` types are strongly typed.
 */
#pragma once

#include <stdfloat>


using f32 = float;
using f64 = double;
using f80 = long double;