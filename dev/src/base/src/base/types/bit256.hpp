#pragma once

#include <base/except/exceptions.hpp>
#include <base/misc/noexcept.hpp>
#include <base/types/ints.hpp>  // IWYU pragma: export
#include <base/types/monostate.hpp>

#include <array>
#include <string>

namespace base {

	struct XXHash64 final {
		uint64_t state = PRIME5;
		uint64_t total_len = 0;

		static constexpr uint64_t PRIME1 = 11400714785074694791ULL;
		static constexpr uint64_t PRIME2 = 14029467366897019727ULL;
		static constexpr uint64_t PRIME3 =  1609587929392839161ULL;
		static constexpr uint64_t PRIME4 =  9650029242287828579ULL;
		static constexpr uint64_t PRIME5 =  2870177450012600261ULL;

		static uint64_t rotl(uint64_t x, int r) {
			return (x << r) | (x >> (64 - r));
		}

		// Add one uint64_t value to the hash
		void add(uint64_t value) {
			total_len += 8;

			uint64_t k = value;
			k *= PRIME2;
			k = rotl(k, 31);
			k *= PRIME1;

			state ^= k;
			state = rotl(state, 27) * PRIME1 + PRIME4;
		}

		// Finalize and return the hash
		[[nodiscard]]
		uint64_t finalize() const {
			uint64_t h = state + total_len;

			h ^= h >> 33;
			h *= PRIME2;
			h ^= h >> 29;
			h *= PRIME3;
			h ^= h >> 32;

			return h;
		}
	};

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

			base::XXHash64 hasher;
			hasher.add(bit256.data[0]);
			hasher.add(bit256.data[1]);
			hasher.add(bit256.data[2]);
			hasher.add(bit256.data[3]);
			return hasher.finalize();
		}
	};
}
