/**
 * @file std_compat.hpp
 *
 * @brief Compatibility shims for C++23 standard library features on platforms where they may not
 * be available (e.g., macOS with older standard library versions).
 *
 * This header provides implementations for C++23 features that may not be available in all
 * standard library implementations, ensuring portability across different platforms.
 */
#pragma once

#include <type_traits>
#include <utility>

namespace std {
	// Check if std::to_underlying is already available (C++23 feature)
	// If not, provide a compatibility implementation
#if __cplusplus < 202300L || (defined(__APPLE__) && (!defined(_LIBCPP_VERSION) || _LIBCPP_VERSION < 170000))
	/**
	 * @brief Converts an enumeration to its underlying type (C++23 compatibility shim).
	 *
	 * This implementation is provided for platforms where std::to_underlying is not available
	 * in the standard library, such as older versions of libc++ on macOS.
	 *
	 * @tparam Enum The enumeration type
	 * @param e The enumeration value
	 * @return The underlying type value
	 */
	template<typename Enum>
		requires std::is_enum_v<Enum>
	constexpr std::underlying_type_t<Enum> to_underlying(Enum e) noexcept {
		return static_cast<std::underlying_type_t<Enum>>(e);
	}
#endif
}  // namespace std
