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

		[[nodiscard]] constexpr std::string toStringHex() const {
			std::string ret;
			ret.reserve(64);
			for (const auto& d: data)
				for (int i = 0; i < 16; ++i)
					ret += std::string_view("0123456789abcdef").at(((d >> (60 - i * 4)) & 0xF));
			return ret;
		}
	};
}
