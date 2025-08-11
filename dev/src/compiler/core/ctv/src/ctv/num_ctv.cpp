#include "num_ctv.hpp"

num_ctv makeMinimizedNumCtv(i64 val) {
	if (val >= std::numeric_limits<i16>::min() && val <= std::numeric_limits<i16>::max())
		return num_ctv{ static_cast<i16>(val) };
	else if (val >= std::numeric_limits<i32>::min() && val <= std::numeric_limits<i32>::max())
		return num_ctv{ static_cast<i32>(val) };
	else
		return num_ctv{ val };  // i64 is the smallest safe type here
}

num_ctv makeMinimizedNumCtv(f80 val) {
	if (static_cast<f80>(static_cast<f32>(val)) == val)
		return num_ctv{ static_cast<f32>(val) };
	else if (static_cast<f80>(static_cast<f64>(val)) == val)
		return num_ctv{ static_cast<f64>(val) };
	else
		return num_ctv{ val };  // Full precision needed
}

bool operator==(const num_ctv& lhs, i64 rhs) {
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

bool operator==(i64 lhs, const num_ctv& rhs) {
	return rhs == lhs;  // Reuse the logic above
}
