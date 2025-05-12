#pragma once

#include "ints.hpp"  // IWYU pragma: export

#include <array>
#include <string>

namespace base {
	/**
	 * Bit256 is a 256-bit integer type used for example for SHA-256 hash values.
	 * It is represented as an array of 4 64-bit integers.
	 */
	struct Bit256 {
		std::array<u64, 4> data = {};

		constexpr Bit256() = default;

		constexpr Bit256(const std::array<u32, 8>& arr) noexcept {
			for (size_t i = 0; i < 4; ++i)
				data.at(i) = (static_cast<u64>(arr.at(i * 2)) << 32) | arr.at(i * 2 + 1);
		}

		constexpr bool operator==(const Bit256& other) const noexcept = default;
		constexpr bool operator!=(const Bit256& other) const noexcept = default;

		/**
		 * @brief Converts the 256-bit integer into a hexadecimal string representation.
		 */
		[[nodiscard]] std::string toStringHex() const;
	};
}
