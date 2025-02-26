#pragma once

#include <string_view>
#include <type_traits>
#include <concepts>
#include <ranges>
#include <vector>
#include <string>
#include <tuple>
#include <array>
#include <span>
#include <bit>

#include <base/ints.hpp>

#include "hash_algorithm_utils.hpp"
#include "type_code.hpp"

namespace hashing {


	namespace detail {

		template<std::integral I>
		class Fnv1a_Constants;

		template<std::integral I>
		requires(sizeof(I) <= sizeof(u32)) class Fnv1a_Constants<I> {
		protected:
			static constexpr u32 OFFSET_BASIS = 2'166'136'261u;
			static constexpr u32 FNV_PRIME    = (1u << 24) + (1u << 8) + 0x93u;
		};

		template<std::integral I>
		requires(sizeof(I) > sizeof(u32)) class Fnv1a_Constants<I> {
		protected:
			static constexpr u64 OFFSET_BASIS = 14'695'981'039'346'656'037ull;
			static constexpr u64 FNV_PRIME    = (1ull << 40) + (1ull << 8) + 0xb3ull;
		};

	}  // namespace detail

	/**
	 * FNV-1a hash algorithm, fast and simple with reasonably good distribution
	 * though not meant for cryptographic purposes
	 */
	template<std::unsigned_integral I>
	class Fnv1a final: protected detail::Fnv1a_Constants<I> {
		using detail::Fnv1a_Constants<I>::OFFSET_BASIS;
		using detail::Fnv1a_Constants<I>::FNV_PRIME;

		I state = OFFSET_BASIS;

	public:
		template<byte_like B, std::size_t N>
		constexpr auto& operator()(const std::span<B, N> span) noexcept {
			for (auto&& c: span) {
				state ^= static_cast<const unsigned char>(c);
				state *= FNV_PRIME;
			}
			return *this;
		}

		template<typename T>
		requires((std::has_unique_object_representations_v<T> && !std::ranges::range<T>) )
		constexpr auto& operator()(const T& t) noexcept {
			const std::array arr = std::bit_cast<std::array<std::byte, sizeof(T)>, T>(t);
			this->operator()(std::span{ arr.data(), arr.size() });
			return *this;
		}

		template<typename R>
		requires(std::ranges::contiguous_range<R> && std::has_unique_object_representations_v<std::ranges::range_value_t<R>>)
		constexpr auto& operator()(const R& range) noexcept {
			for (auto&& it: range) {
				using val_t    = std::ranges::range_value_t<R>;
				const auto arr = std::bit_cast<std::array<std::byte, sizeof(val_t)>, val_t>(it);
				this->operator()(std::span{ arr.data(), arr.size() });
			}
			return *this;
		}

		using result_type = I;

		constexpr Fnv1a() = default;

		constexpr Fnv1a(I state): state(state) {}

		constexpr explicit operator result_type() const noexcept {
			return static_cast<result_type>(state);
		}
	};

	using Fnv1a_32 = Fnv1a<u32>;
	using Fnv1a_64 = Fnv1a<u64>;

	/**
	 * hash algorithm that keeps the bytes of the hashed objects
	 * and can be converted to a string that represents the bytes in hex
	 */
	class DebugHash final {
		enum class Type : std::uint8_t { TypeCode, Other };
		std::vector<std::tuple<std::vector<char>, usize, Type>> bytes;

	public:
		// spans of bytes
		template<byte_like B, std::size_t N>
		constexpr void operator()(const std::span<B, N> span, Type type = Type::Other) noexcept {
			std::vector<char> vec;
			vec.reserve(span.size());
			for (auto&& c: span) vec.push_back(static_cast<char>(c));
			bytes.emplace_back(std::move(vec), span.size(), type);
		}

		// type codes
		template<base::IsInstantiationOf<TypeCodeBase> TypeC>
		constexpr void operator()(TypeC hash) noexcept {
			auto arr = std::bit_cast<std::array<char, sizeof(TypeC)>, TypeC>(hash);
			this->operator()(std::span{ arr.data(), arr.size() }, Type::TypeCode);
		}

		// objects with unique representations but not ranges nor type codes
		template<typename T>
		requires(not base::IsInstantiationOf<T, TypeCodeBase> && std::has_unique_object_representations_v<T> && not std::ranges::range<T>)
		constexpr void operator()(const T& t) noexcept {
			const std::array arr = std::bit_cast<std::array<char, sizeof(T)>, T>(t);
			this->operator()(std::span{ arr.data(), arr.size() }, Type::Other);
		}

		// contiguous ranges
		template<typename R>
		requires(std::ranges::contiguous_range<R> && std::has_unique_object_representations_v<std::ranges::range_value_t<R>>)
		constexpr auto& operator()(const R& range) noexcept {
			for (auto&& it: range) {
				using val_t    = std::ranges::range_value_t<R>;
				const auto arr = std::bit_cast<std::array<std::byte, sizeof(val_t)>, val_t>(it);
				this->operator()(std::span{ arr.data(), arr.size() }, Type::Other);
			}
			return *this;
		}

		using result_type = std::string;

		explicit constexpr operator result_type() noexcept {
			std::string ret;
			usize       line = 0, pos = 0;

			constexpr std::string_view yellow = "\033[1;33m";
			constexpr std::string_view red    = "\033[1;31m";
			constexpr std::string_view reset  = "\033[0m";

			// stringstream is not usable in constexpr
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
					if (i == pos) ret += (type == Type::TypeCode ? yellow : red);
					append_byte_hex(ret, static_cast<std::byte>(b[j]));
					ret += ' ';
					if (i == pos) ret += reset;
				}
				pos = (pos + len) % 16;
			}

			return ret;
		}
	};

	using DefaultHashAlgorithm = Fnv1a_64;

	template<typename I>
	using default_hash_algorithm_for
		= std::conditional_t<sizeof(I) <= sizeof(u32), Fnv1a_32, Fnv1a_64>;


}  // namespace hashing
