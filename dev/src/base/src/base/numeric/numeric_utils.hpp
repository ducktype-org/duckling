#pragma once

#include <limits>
#include <type_traits>

namespace base {
	/**
	 * @brief Checks if a value of a source integral type can be safely represented in a target
	 * integral type without overflow or underflow.
	 * @tparam Target The target integral type.
	 * @tparam Source The source integral type.
	 * @param value The value to check.
	 * @return True if the value fits, false otherwise.
	 */
	template<typename TargetType, typename SourceType>
	requires(std::is_integral_v<TargetType> && std::is_integral_v<SourceType>)
	constexpr bool fitsIn(SourceType value) {
		if constexpr (std::is_signed_v<TargetType> == std::is_signed_v<SourceType>) {
			return value >= static_cast<SourceType>(std::numeric_limits<TargetType>::min())
			    && value <= static_cast<SourceType>(std::numeric_limits<TargetType>::max());
		} else if constexpr (std::is_unsigned_v<SourceType> && std::is_unsigned_v<TargetType>) {
			return value <= static_cast<SourceType>(std::numeric_limits<TargetType>::max());
		} else {
			return value >= 0
			    && static_cast<SourceType>(value) <= std::numeric_limits<TargetType>::max();
		}
	}

}
