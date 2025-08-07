/**
 * @file num_ctv.hpp
 *
 * @brief Variant type for storing integers and floats with minimized size.
 */
#pragma once

#include <base/floats.hpp>
#include <base/ints.hpp>

#include <limits>
#include <variant>

using num_ctv = std::variant<i16, i32, i64, f32, f64, f80>;

num_ctv make_minimized_num_ctv(i64 val);

num_ctv make_minimized_num_ctv(f80 val);

template<typename T>
inline num_ctv make_minimized_num_ctv(T val) {
	if constexpr (std::is_same_v<T, i64>)
		return make_minimized_num_ctv(static_cast<i64>(val));
	else if constexpr (std::is_same_v<T, f80>)
		return make_minimized_num_ctv(static_cast<f80>(val));
	else
		static_assert(false, "Unsupported type for minimization");
}

bool operator==(const num_ctv& lhs, i64 rhs);

bool operator==(i64 lhs, const num_ctv& rhs);
