#pragma once

#include "hash_algorithm_utils.hpp"
#include "type_code.hpp"

#include <base/ints.hpp>
#include <base/type_traits.hpp>

#include <array>
#include <bit>
#include <concepts>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace hashing {


	namespace detail {

		/**
		 * Primary template of FNV-1a constants for different integer sizes.
		 * Note that there is no definition - only the specializations are to be used
		 */
		template<std::integral I>
		class Fnv1a_Constants;

		/**
		 * Specialization of FNV-1a constants for integer sizes <= 32 bits
		 */
		template<std::integral I>
		requires(sizeof(I) <= sizeof(u32)) class Fnv1a_Constants<I> {
		protected:
			static constexpr u32 OFFSET_BASIS = 2'166'136'261u;
			static constexpr u32 FNV_PRIME    = (1u << 24) + (1u << 8) + 0x93u;
		};

		/**
		 * Specialization of FNV-1a constants for integer sizes > 32 bits
		 */
		template<std::integral I>
		requires(sizeof(I) > sizeof(u32)) class Fnv1a_Constants<I> {
		protected:
			static constexpr u64 OFFSET_BASIS = 14'695'981'039'346'656'037ull;
			static constexpr u64 FNV_PRIME    = (1ull << 40) + (1ull << 8) + 0xb3ull;
		};

	}  // namespace detail

	/**
	 * FNV-1a hash algorithm, fast and simple with reasonably good distribution,
	 * though not meant for cryptographic purposes
	 */
	template<std::unsigned_integral I>
	class Fnv1a final: protected detail::Fnv1a_Constants<I> {
		using detail::Fnv1a_Constants<I>::OFFSET_BASIS;
		using detail::Fnv1a_Constants<I>::FNV_PRIME;

		I state = OFFSET_BASIS;

	public:
		constexpr Fnv1a<I>& operator()(detail::span_of_bytes auto span) noexcept {
			for (auto&& c: span) {
				state ^= static_cast<unsigned char>(c);
				state *= FNV_PRIME;
			}
			return *this;
		}

		using result_type = I;

		constexpr Fnv1a() = default;

		constexpr Fnv1a(I state): state(state) {}

		constexpr result_type finalize() const noexcept { return static_cast<result_type>(state); }
	};

	using Fnv1a_32 = Fnv1a<u32>;
	using Fnv1a_64 = Fnv1a<u64>;

	/**
	 * Hash algorithm that keeps the bytes of the hashed objects
	 * and can be converted to a string that represents the bytes in hex
	 */
	class DebugHash final {
		enum class Type : std::uint8_t { Code, Other };
		std::vector<std::tuple<std::vector<char>, usize, Type>> bytes;

	public:
		// Spans of bytes
		constexpr void operator()(detail::span_of_bytes auto span, Type type = Type::Other) noexcept {
			std::vector<char> vec;
			vec.reserve(span.size());
			for (auto&& c: span) vec.push_back(static_cast<char>(c));
			bytes.emplace_back(std::move(vec), vec.size(), type);
		}

		// Type codes
		template<base::IsInstantiationOfTypeValue<TypeCode> TypeC>
		constexpr void operator()(TypeC hash) noexcept {
			const auto arr = std::bit_cast<std::array<const std::byte, sizeof(TypeC)>, TypeC>(hash);
			this->operator()(std::span{ arr.data(), arr.size() }, Type::Code);
		}

		using result_type = std::string;

		constexpr result_type finalize() {
			std::string ret;
			usize       line = 0, pos = 0;

			constexpr std::string_view yellow = "\033[1;33m";
			constexpr std::string_view red    = "\033[1;31m";
			constexpr std::string_view reset  = "\033[0m";

			// Note: stringstream is not usable in constexpr
			static constexpr auto append_line_number = [](std::string& str, usize num) {
				str += "line ";
				std::string num_str;
				do {
					num_str += static_cast<char>(static_cast<usize>('0') + num % 10);
					num /= 10;
				} while (num != 0);
				str += std::string(4 - num_str.size(), ' ');
				str += num_str;
				str += ":    ";
			};
			static constexpr auto append_byte_hex = [](std::string& str, std::byte b) {
				constexpr static std::string_view hex = "0123456789ABCDEF";
				str += hex[std::to_integer<unsigned>(b >> 4)];
				str += hex[std::to_integer<unsigned>(b & std::byte{ 0xF })];
			};

			for (auto&& [b, len, type]: bytes) {
				for (usize i = pos, j = 0; j < len; ++i, ++j) {
					if (i % 16 == 0) {
						if (line != 0) ret += '\n';
						append_line_number(ret, line++);
					}
					if (i == pos) ret += (type == Type::Code ? yellow : red);
					append_byte_hex(ret, static_cast<std::byte>(b[j]));
					ret += ' ';
					if (i == pos) ret += reset;
				}
				pos = (pos + len) % 16;
			}

			return ret;
		}
	};

	/**
	 * The default hash algorithm
	 */
	using DefaultHashAlgorithm = Fnv1a_64;

	/**
	 * The default hash algorithm for the given integer type
	 */
	template<typename I>
	using default_hash_algorithm_for
		= std::conditional_t<sizeof(I) <= sizeof(u32), Fnv1a_32, Fnv1a_64>;


}  // namespace hashing
