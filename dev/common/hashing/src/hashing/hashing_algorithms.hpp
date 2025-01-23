#pragma once

#include <type_traits>
#include <ranges>
#include <vector>

#include <base/ints.hpp>

#include "unique_id.hpp"
#include "hash_utils.hpp"

namespace hashing {


	class Fnv1a_32: public CallOverloads {
		friend CallOverloads;

		static constexpr u32 offset_basis = 2'166'136'261u;
		static constexpr u32 FNV_prime    = (1u << 24) + (1u << 8) + 0x93u;
		u32                  state        = offset_basis;

	protected:
		void update_hash(const void* data, usize len) noexcept {
			update_hash(static_cast<const char*>(data), len);
		}

		constexpr void update_hash(const char* data, usize len) noexcept {
			for (usize i = 0; i < len; ++i) {
				state ^= data[i];
				state *= FNV_prime;
			}
		}

	public:
		using result_type = u32;

		constexpr Fnv1a_32() = default;

		constexpr Fnv1a_32(u32 state): state(state) {}

		constexpr explicit operator result_type() const noexcept {
			return static_cast<result_type>(state);
		}
	};

	class Fnv1a_64: public CallOverloads {
		friend CallOverloads;

		static constexpr u64 offset_basis = 14'695'981'039'346'656'037ull;
		static constexpr u64 FNV_prime    = (1ull << 40) + (1ull << 8) + 0xb3ull;
		u64                  state        = offset_basis;

	protected:
		void update_hash(const void* data, usize len) noexcept {
			update_hash(static_cast<const char*>(data), len);
		}

		constexpr void update_hash(const char* data, usize len) noexcept {
			for (usize i = 0; i < len; ++i) {
				state ^= data[i];
				state *= FNV_prime;
			}
		}

	public:
		using result_type = u64;

		constexpr Fnv1a_64() = default;

		constexpr Fnv1a_64(u64 state): state(state) {}

		constexpr explicit operator result_type() const noexcept {
			return static_cast<result_type>(state);
		}
	};

	class DebugHash: public CallOverloads {
		friend CallOverloads;

		enum class type { hash_code, other };
		std::vector<std::tuple<std::vector<char>, usize, type>> bytes;

	protected:
		void update_hash(const void* data, usize len) noexcept {
			update_hash(static_cast<const char*>(data), len);
		}

		constexpr void update_hash(const char* data, usize len, type type = type::other) noexcept {
			bytes.emplace_back(std::vector<char>(data, data + len), len, type);
		}

		constexpr void add_hash_code(TypeHashCode hash) noexcept {
			std::array arr = std::bit_cast<std::array<char, sizeof(hash)>, TypeHashCode>(hash);
			update_hash(arr.data(), arr.size(), type::hash_code);
		}

	public:
		using result_type = std::string;

		DebugHash() = default;

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
					num_str += '0' + num % 10;
					num /= 10;
				} while (num != 0);
				str += std::string(4 - num_str.size(), ' ');
				str += num_str;
				str += ":    ";
			};
			auto append_hex = [](std::string& str, std::byte b) {
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
					if (i == pos) ret += (type == type::hash_code ? yellow : red);
					append_hex(ret, static_cast<std::byte>(b[j]));
					ret += ' ';
					if (i == pos) ret += reset;
				}
				pos = (pos + len) % 16;
			}

			return ret;
		}
	};


}  // namespace hash
