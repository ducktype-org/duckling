#pragma once

#include <type_traits>
#include <ranges>

#include <iostream>
#include <iomanip>
#include <sstream>

#include "hash_utils.hpp"
#include "unique_id.hpp"

namespace hashing {


	class fnv1a_32: public call_overloads {
		friend call_overloads;

		static constexpr u32 offset_basis = 2'166'136'261u;
		static constexpr u32 FNV_prime    = (1u << 24) + (1u << 8) + 0x93u;
		u32                  state        = offset_basis;

	protected:
		/*constexpr*/ void update_hash(const void* data, usize len) noexcept {
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

		constexpr fnv1a_32() = default;

		constexpr fnv1a_32(u32 state): state(state) {}

		constexpr explicit operator result_type() const noexcept {
			return static_cast<result_type>(state);
		}
	};

	class fnv1a_64: public call_overloads {
		friend call_overloads;

		static constexpr u64 offset_basis = 14'695'981'039'346'656'037ull;
		static constexpr u64 FNV_prime    = (1ull << 40) + (1ull << 8) + 0xb3ull;
		u64                  state        = offset_basis;

	protected:
		/*constexpr*/ void update_hash(const void* data, usize len) noexcept {
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

		constexpr fnv1a_64() = default;

		constexpr fnv1a_64(u64 state): state(state) {}

		constexpr explicit operator result_type() const noexcept {
			return static_cast<result_type>(state);
		}
	};

	class debug_hash: public call_overloads {
		friend call_overloads;

		enum class type { hash_code, other };
		std::vector<std::tuple<std::vector<char>, usize, type>> bytes;

	protected:
		void update_hash(const void* data, usize len) noexcept {
			update_hash(static_cast<const char*>(data), len);
		}

		constexpr void update_hash(const char* data, usize len, type type = type::other) noexcept {
			bytes.emplace_back(std::vector<char>(data, data + len), len, type);
		}

		constexpr void add_hash_code(type_hash_code_t hash) noexcept {
			std::array arr = std::bit_cast<std::array<char, sizeof(hash)>, type_hash_code_t>(hash);
			update_hash(arr.data(), arr.size(), type::hash_code);
		}

	public:
		using result_type = std::string;

		debug_hash() = default;

		explicit operator result_type() noexcept {
			std::stringstream ss;
			usize             line = 0, pos = 0;

			for (auto&& [b, len, type]: bytes) {
				for (usize i = pos, j = 0; j < len; ++i, ++j) {
					if (i % 16 == 0) {
						if (line != 0) ss << '\n';
						ss << "line " << std::setw(4) << std::setfill(' ') << line++ << ":    ";
					}
					if (i == pos) {
						if (type == type::hash_code)
							ss << "\033[1;33m";
						else
							ss << "\033[1;31m";
					}
					ss << std::hex << std::setw(2) << std::setfill('0')
					   << static_cast<u16>(static_cast<std::byte>(b[j])) << std::dec << ' ';
					if (i == pos) ss << "\033[0m";
				}
				pos = (pos + len) % 16;
			}

			return ss.str();
		}
	};


}  // namespace hash
