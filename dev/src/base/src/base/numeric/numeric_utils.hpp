#pragma once

#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>

namespace base {
	/**
	 * @brief Checks if a value of a source arithmetic type can be safely represented in a target
	 * arithmetic type without overflow/underflow or other invalid conversions.
	 * @tparam Target The target integral type.
	 * @tparam Source The source integral type.
	 * @param value The value to check.
	 * @return True if the value fits, false otherwise.
	 */
	template<typename TargetType, typename SourceType>
	requires(std::is_arithmetic_v<TargetType> && std::is_arithmetic_v<SourceType>)
	constexpr bool fitsIn(SourceType value) {
		constexpr bool IS_SOURCE_INTEGRAL = std::is_integral_v<SourceType>;
		constexpr bool IS_TARGET_INTEGRAL = std::is_integral_v<TargetType>;

		if constexpr (IS_SOURCE_INTEGRAL && IS_TARGET_INTEGRAL) {  // Integral to integral.
			return std::in_range<TargetType>(value);
		} else if constexpr (!IS_SOURCE_INTEGRAL
		                     && IS_TARGET_INTEGRAL) {  // Floating point to integral.
			if (std::isnan(value) || std::isinf(value)) return false;
			SourceType truncated = std::trunc(value);
			return value >= static_cast<SourceType>(std::numeric_limits<TargetType>::min())
			    && value <= static_cast<SourceType>(std::numeric_limits<TargetType>::max());
		} else if constexpr (IS_SOURCE_INTEGRAL
		                     && !IS_TARGET_INTEGRAL) {  // Integral to floating point.
			// Check if we lose no precision when casting to the desired integral type.
			return static_cast<SourceType>(static_cast<TargetType>(value)) == value;
		} else {  // Floating point to floating point.
			if constexpr (sizeof(TargetType) >= sizeof(SourceType)) return true;
			if (std::isnan(value)) return true;
			if (std::isinf(value)) return std::numeric_limits<TargetType>::has_infinity;
			return value >= -std::numeric_limits<TargetType>::max()
			    && value <= std::numeric_limits<TargetType>::max();
		}
	}

}
