/**
 * @file num_ctv.hpp
 *
 * @brief Variant type for storing integers and floats with minimized size.
 */
#pragma once

#include <base/floats.hpp>
#include <base/ints.hpp>

#include <limits>
#include <ostream>
#include <variant>

using num_ctv = std::variant<i16, i32, i64, f16, f32, f64, f80>;

num_ctv makeMinimizedNumCtv(i64 val);

num_ctv makeMinimizedNumCtv(f80 val);

template<typename T>
inline num_ctv makeMinimizedNumCtv(T val) {
	if constexpr (std::is_same_v<T, i64>)
		return makeMinimizedNumCtv(static_cast<i64>(val));
	else if constexpr (std::is_same_v<T, f80>)
		return makeMinimizedNumCtv(static_cast<f80>(val));
	else
		static_assert(false, "Unsupported type for minimization");
}

bool operator==(const num_ctv& lhs, i64 rhs);

bool operator==(i64 lhs, const num_ctv& rhs);

void print(num_ctv value, std::ostream& output);
