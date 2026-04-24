#pragma once

#include <base/except/exceptions.hpp>
#include <base/misc/noexcept.hpp>
#include <base/types/ints.hpp>  // IWYU pragma: export
#include <base/types/monostate.hpp>

#include <array>
#include <string>

namespace base {
	/**
	 * Bit256 is a 256-bit integer type used for example for SHA-256 hash values.
	 * The value is represented in an array of 4 u64s as a number in base 2^64, with the lowest
	 * letter in data[0]
	 */
	struct Bit256 final {
		std::array<u64, 4> data = {};

		static constexpr base::Monostate HASHING_CAN_HASH_BY_REPRESENTATION = {};

		constexpr Bit256() = default;

		/**
		 * @param bytes an array of 32 bytes that will be interpreted as a 256-bit number written in
		 * base 2^8 with the lowest letter in bytes[0]
		 */
		constexpr Bit256(const std::array<uint8_t, 32>& bytes) noexcept {
			for (size_t i = 0; i < 4; ++i) {
				data.at(i) = 0;
				for (size_t j = 0; j < 8; ++j) {
					data.at(i) <<= 8;
					data.at(i) |= bytes.at(i * 8 + j);
				}
			}
		}

		/**
		 * @param arr an array of u32 that will be interpreted as a 256-bit number written in base
		 * 2^32 with the lowest letter in arr[0]
		 */
		constexpr Bit256(const std::array<u32, 8>& arr) noexcept {
			for (size_t i = 0; i < 4; ++i)
				data.at(i) = (static_cast<u64>(arr.at(i * 2 + 1)) << 32) | arr.at(i * 2);
		}

		constexpr Bit256(const std::array<u64, 4>& arr) noexcept: data(arr) {}

		constexpr Bit256(u64 a, u64 b, u64 c, u64 d) noexcept: data{ a, b, c, d } {}

		constexpr Bit256(u64 a, u64 b, u64 c) noexcept: data{ a, b, c, 0 } {}

		constexpr Bit256(u64 a, u64 b) noexcept: data{ a, b, 0, 0 } {}

		constexpr Bit256(u64 a) noexcept: data{ a, 0, 0, 0 } {}

		/**
		 * Constructor taking a hex string starting with 0x
		 */
		Bit256(std::string_view hex);

		constexpr bool operator==(const Bit256& other) const noexcept = default;
		constexpr bool operator!=(const Bit256& other) const noexcept = default;

		/**
		 * @brief Converts the 256-bit integer into a hexadecimal string representation.
		 */
		[[nodiscard]] std::string toStringHex() const;

		/*
		 * @brief Converts the Bit256 to a u64 by taking the least significant 64 bits.
		 * Use this only when Bit256 was created from single u64 value.
		 */
		constexpr explicit operator u64() const RELEASE_NOEXCEPT {
			CORE_ASSERT(
				data.at(1) == 0 && data.at(2) == 0 && data.at(3) == 0,
				"Bit256 value too large to convert to u64"
			);
			return data.at(0);
		}

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
			// std::size_t hash = 0;
			// for (const auto& value: bit256.data)
			// 	hash ^= std::hash<u64>{}(value);  // Combine hashes using XOR
			// return hash;
			u64 a = bit256.data.at(0);
			u64 b = bit256.data.at(1);
			u64 c = bit256.data.at(2);
			u64 d = bit256.data.at(3);

			// Good hash combine:
			u64 hash = 0;
			hash ^= std::hash<u64>{}(a) + 0x9e377 + (hash << 6) + (hash >> 2);
			hash ^= std::hash<u64>{}(b) + 0x9e377 + (hash << 6) + (hash >> 2);
			hash ^= std::hash<u64>{}(c) + 0x9e377 + (hash << 6) + (hash >> 2);
			hash ^= std::hash<u64>{}(d) + 0x9e377 + (hash << 6) + (hash >> 2);
			return hash;			
		}
	};
}
