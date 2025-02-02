#pragma once

#include <type_traits>
#include <ranges>
#include <vector>

#include <base/ints.hpp>

#include "type_hash_code_def.hpp"
#include "hash_algorithm_utils.hpp"
#include "CallOverloads_utils.hpp"

namespace hashing {


	namespace detail {

		template<std::unsigned_integral I>
		class Fnv1a_Constants;

		template<>
		class Fnv1a_Constants<u32> {
		protected:
			static constexpr u32 OFFSET_BASIS = 2'166'136'261u;
			static constexpr u32 FNV_PRIME    = (1u << 24) + (1u << 8) + 0x93u;
		};

		template<>
		class Fnv1a_Constants<u64> {
		protected:
			static constexpr u64 OFFSET_BASIS = 14'695'981'039'346'656'037ull;
			static constexpr u64 FNV_PRIME    = (1ull << 40) + (1ull << 8) + 0xb3ull;
		};

	}  // namespace detail

	template<std::unsigned_integral I>
	class Fnv1a: public CallOverloads, protected detail::Fnv1a_Constants<I> {
		friend CallOverloads;
		using detail::Fnv1a_Constants<I>::OFFSET_BASIS;
		using detail::Fnv1a_Constants<I>::FNV_PRIME;

		I state = OFFSET_BASIS;

	protected:
		// @C++26 constexpr (static_cast from void*)
		void updateHash(const void* data, usize len) noexcept {
			updateHash(std::string_view{ static_cast<const char*>(data), len });
		}

		constexpr void updateHash(const std::string_view sv) noexcept {
			for (auto&& c: sv) {
				state ^= c;
				state *= FNV_PRIME;
			}
		}

	public:
		using result_type = I;

		constexpr Fnv1a() = default;

		constexpr Fnv1a(I state): state(state) {}

		constexpr explicit operator result_type() const noexcept {
			return static_cast<result_type>(state);
		}
	};

	using Fnv1a_32 = Fnv1a<u32>;
	using Fnv1a_64 = Fnv1a<u64>;

	class DebugHash: public CallOverloads {
		friend CallOverloads;

		enum class Type : std::uint8_t { HashCode, Other };
		std::vector<std::tuple<std::vector<char>, usize, Type>> bytes;

	protected:
		// @C++26 constexpr (static_cast from void*)
		void updateHash(const void* data, usize len) noexcept {
			updateHash(std::string_view{ static_cast<const char*>(data), len });
		}

		constexpr void updateHash(const std::string_view sv, Type type = Type::Other) noexcept {
			bytes.emplace_back(std::vector<char>(sv.begin(), sv.end()), sv.size(), type);
		}

		template<specialization_of<TypeHashCodeBase> TypeHC>
		constexpr void addHashCode(TypeHC hash) noexcept {
			auto arr = std::bit_cast<std::array<char, sizeof(TypeHC)>, TypeHC>(hash);
			updateHash(std::string_view{ arr.data(), arr.size() }, Type::HashCode);
		}

	public:
		using result_type = std::string;

		constexpr DebugHash() = default;

		explicit constexpr operator result_type() noexcept {
			std::string ret;
			usize       line = 0, pos = 0;

			constexpr std::string_view yellow = "\033[1;33m";
			constexpr std::string_view red    = "\033[1;31m";
			constexpr std::string_view reset  = "\033[0m";

			auto append_line_number = [](std::string& str, usize num) {
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
			auto append_byte_hex = [](std::string& str, std::byte b) {
				constexpr std::string_view hex = "0123456789ABCDEF";
				str += hex[std::to_integer<unsigned>(b >> 4)];
				str += hex[std::to_integer<unsigned>(b & std::byte{ 0xF })];
			};

			for (auto&& [b, len, type]: bytes) {
				for (usize i = pos, j = 0; j < len; ++i, ++j) {
					if (i % 16 == 0) {
						if (line != 0) ret += '\n';
						append_line_number(ret, line++);
					}
					if (i == pos) ret += (type == Type::HashCode ? yellow : red);
					append_byte_hex(ret, static_cast<std::byte>(b[j]));
					ret += ' ';
					if (i == pos) ret += reset;
				}
				pos = (pos + len) % 16;
			}

			return ret;
		}
	};

	template<typename I>
	using default_hash_algorithm_for
		= std::conditional_t<sizeof(I) <= sizeof(u32), Fnv1a_32, Fnv1a_64>;


}  // namespace hashing
