/**
 * @file ints.hpp
 *
 * @attention `<cstdint>` should not be used, unless necessary. Ints should be used instead.
 *
 * @brief Ints is a library analogous to `<cstdint>` with generally shorter type names
 * and one big improvement: `u8` and `i8` types are strongly typed.
 */
#pragma once

#include "strongly_typed_int.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <variant>

STRONG_TYPEDEF_INT(u8, uint8_t);
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

STRONG_TYPEDEF_INT(i8, int8_t);
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;
using f80 = long double;

using num_ctv = std::variant<i16, i32, i64, f32, f64, f80>;

inline num_ctv make_minimized_num_ctv(i64 val) {
	if (val >= std::numeric_limits<i16>::min() && val <= std::numeric_limits<i16>::max())
		return num_ctv{ static_cast<i16>(val) };
	else if (val >= std::numeric_limits<i32>::min() && val <= std::numeric_limits<i32>::max())
		return num_ctv{ static_cast<i32>(val) };
	else
		return num_ctv{ val };  // i64 is the smallest safe type here
}

inline num_ctv make_minimized_num_ctv(f80 val) {
	if (static_cast<f80>(static_cast<f32>(val)) == val)
		return num_ctv{ static_cast<f32>(val) };
	else if (static_cast<f80>(static_cast<f64>(val)) == val)
		return num_ctv{ static_cast<f64>(val) };
	else
		return num_ctv{ val };  // Full precision needed
}

template<typename>
inline constexpr bool always_false = false;

template<typename T>
inline num_ctv make_minimized_num_ctv(T val) {
	if constexpr (std::is_same_v<T, i64>)
		return make_minimized_num_ctv(static_cast<i64>(val));
	else if constexpr (std::is_same_v<T, f80>)
		return make_minimized_num_ctv(static_cast<f80>(val));
	else
		static_assert(always_false<T>, "Unsupported type for minimization");
}

inline bool operator==(const num_ctv& lhs, i64 rhs) {
	return std::visit(
		[&](auto val) -> bool {
			if constexpr (std::is_convertible_v<decltype(val), i64>)
				return static_cast<i64>(val) == rhs;
			else
				return false;
		},
		lhs
	);
}

inline bool operator==(i64 lhs, const num_ctv& rhs) {
	return rhs == lhs;  // Reuse the logic above
}

using uchar = unsigned char;

using byte  = std::byte;
using usize = std::size_t;

// Code might break if following does not hold:
static_assert(sizeof(byte) == sizeof(char));
static_assert(sizeof(byte) == sizeof(u8));
