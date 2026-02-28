#pragma once

#include <base/except/exceptions.hpp>
#include <base/misc/noexcept.hpp>
#include <base/types/ints.hpp>  // IWYU pragma: export

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

		constexpr Bit256(const std::array<uint8_t, 32>& bytes) noexcept {
			for (size_t i = 0; i < 4; ++i) {
				data.at(i) = 0;
				for (size_t j = 0; j < 8; ++j) {
					data.at(i) <<= 8;
					data.at(i) |= bytes.at(i * 8 + j);
				}
			}
		}

		constexpr Bit256(const std::array<u32, 8>& arr) noexcept {
			for (size_t i = 0; i < 4; ++i)
				data.at(i) = (static_cast<u64>(arr.at(i * 2 + 1)) << 32) | arr.at(i * 2);
		}

		constexpr Bit256(const std::array<u64, 4>& arr) noexcept: data(arr) {}

		constexpr Bit256(u64 a, u64 b, u64 c, u64 d) noexcept: data{ a, b, c, d } {}

		constexpr Bit256(u64 a, u64 b, u64 c) noexcept: data{ a, b, c, 0 } {}

		constexpr Bit256(u64 a, u64 b) noexcept: data{ a, b, 0, 0 } {}

		constexpr Bit256(u64 a) noexcept: data{ a, 0, 0, 0 } {}

		constexpr Bit256(std::string_view hex): Bit256() {
			CORE_ASSERT(hex.size() <= 66, "Bit256 string too long");
			CORE_ASSERT(
				hex.starts_with("0x") || hex.starts_with("0X"), "Bit256 string must start with 0x"
			);
			// Remove "0x" prefix
			hex = hex.substr(2);
			for (std::size_t i = 0; hex.size() > 0; ++i) {
				const auto len = std::min(16uz, hex.size());
				auto       end = hex.substr(hex.size() - len, len);
				hex            = hex.substr(0, hex.size() - len);

				u64 value = 0;
				for (const char c: end) {
					value <<= 4;
					if (c >= '0' && c <= '9')
						value |= (c - '0');
					else if (c >= 'a' && c <= 'f')
						value |= (c - 'a' + 10);
					else if (c >= 'A' && c <= 'F')
						value |= (c - 'A' + 10);
					else
						CORE_ASSERT(
							false, "Invalid character \'" + std::string(1, c) + "\' in Bit256 string"
						);
				}
				data.at(i) = value;
			}
		}

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

		constexpr Bit256& operator^=(const Bit256& other) noexcept {
			for (usize i = 0; i < data.size(); ++i) data.at(i) ^= other.data.at(i);
			return *this;
		}

		constexpr Bit256& operator^=(unsigned char other) noexcept {
			data.at(0) ^= other;
			return *this;
		}

		constexpr Bit256& operator+=(const Bit256& other) noexcept {
			Bit256 result;
			u64    carry = 0;
			for (usize i = 0; i < data.size(); ++i) {
				const u64 sum     = data.at(i) + other.data.at(i) + carry;
				result.data.at(i) = sum;
				carry             = (sum < data.at(i)) || (carry && sum == data.at(i));
			}
			return *this = result;
		}

		Bit256& operator*=(const Bit256& other) noexcept;

		/**
		 * @brief Outputs the Bit256 object to a stream in the format {a, b, c, d}.
		 */
		friend std::ostream& operator<<(std::ostream& os, const base::Bit256& bit256);
	};

	// We use inline namespace to allow `using namespace base::literals` and also have the operator
	// in `base` namespace
	inline namespace literals {
		constexpr Bit256 operator""_Bit256(const char* str, size_t len) {
			return Bit256(std::string_view(str, len));
		}
	}
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
