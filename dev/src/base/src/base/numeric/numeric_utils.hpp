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
		constexpr bool is_source_signed = std::is_signed_v<SourceType>;
		constexpr bool is_target_signed = std::is_signed_v<TargetType>;

		if constexpr (is_source_signed && is_target_signed) {  // Signed to signed.
			if constexpr (sizeof(TargetType) >= sizeof(SourceType)) return true;

			// Target is smaller the source, check bounds.
			return value >= static_cast<SourceType>(std::numeric_limits<TargetType>::min())
			    && value <= static_cast<SourceType>(std::numeric_limits<TargetType>::max());
		} else if constexpr (!is_source_signed && !is_target_signed) {  // Unsigned to unsigned.
			if constexpr (sizeof(TargetType) >= sizeof(SourceType)) return true;

			// Target is smaller the source, check bounds. Upcasting to the bigger source type is
			// safe here.
			return value <= static_cast<SourceType>(std::numeric_limits<TargetType>::max());
		} else if constexpr (is_source_signed && !is_target_signed) {  // Signed to unsigned.
			if (value < 0) return false;
			if constexpr (sizeof(TargetType) >= sizeof(SourceType)) return true;
			// Target is smaller than Source so the widening cast is safe here.
			return value <= static_cast<SourceType>(std::numeric_limits<TargetType>::max());
		} else {  // Unsigned to signed
			if constexpr (sizeof(TargetType) > sizeof(SourceType)) return true;
			// Target is smaller than Source so the widening cast is safe here.
			return value <= static_cast<SourceType>(std::numeric_limits<TargetType>::max());
		}
	}

}
