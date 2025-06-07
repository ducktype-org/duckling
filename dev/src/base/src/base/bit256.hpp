#pragma once

#include "ints.hpp"  // IWYU pragma: export

#include <array>
#include <string>
#include <vector>

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

		constexpr Bit256(u64 a, u64 b, u64 c, u64 d) noexcept: data{ a, b, c, d } {}

		constexpr Bit256(u64 a, u64 b, u64 c) noexcept: data{ a, b, c, 0 } {}

		constexpr Bit256(u64 a, u64 b) noexcept: data{ a, b, 0, 0 } {}

		constexpr Bit256(u64 a) noexcept: data{ a, 0, 0, 0 } {}

		constexpr bool operator==(const Bit256& other) const noexcept = default;
		constexpr bool operator!=(const Bit256& other) const noexcept = default;

		/**
		 * @brief Converts the 256-bit integer into a hexadecimal string representation.
		 */
		[[nodiscard]] std::string toStringHex() const;

		/**
		 * @brief Serialize the Bit256 object into a vector of bytes
		 */
		[[nodiscard]] std::vector<uint8_t> serialize() const;

		/**
		 * @brief Deserialize a Bit256 object from a vector of bytes
		 */
		static Bit256 deserialize(const std::vector<uint8_t>& buffer, const usize& offset);

		/**
		 * @brief Returns the size of the serialized Bit256 object in bytes.
		 */
		[[nodiscard]] static constexpr usize serializedSize() noexcept { return sizeof(data); }

		constexpr bool operator<(const Bit256& other) const noexcept {
			for (usize i = 4;
			     i-- > 0;) {  // Iterate from the most significant to the least significant
				if (data.at(i) < other.data.at(i)) return true;
				if (data.at(i) > other.data.at(i)) return false;
			}
			return false;
		}

		constexpr bool operator>(const Bit256& other) const noexcept { return other < *this; }

		/**
		 * @brief Outputs the Bit256 object to a stream in the format {a, b, c, d}.
		 */
		friend std::ostream& operator<<(std::ostream& os, const base::Bit256& bit256);
	};
}

namespace std {
	template<>
	struct hash<base::Bit256> {
		std::size_t operator()(const base::Bit256& bit256) const noexcept {
			std::size_t hash = 0;
			for (const auto& value: bit256.data)
				hash ^= std::hash<u64>{}(value);  // Combine hashes using XOR
			return hash;
		}
	};
}
