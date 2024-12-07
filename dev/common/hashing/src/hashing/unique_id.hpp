#pragma once

// #include <base/ints.hpp>
#include "../../../../base/src/base/ints.hpp"

namespace hashing {

	struct type_hash_code_t {
		u32 value{};

		constexpr auto operator<=>(const type_hash_code_t&) const = default;

		constexpr operator u32() const noexcept { return value; }
	};

	namespace detail {

		class fnv1a_32_consteval {
			static constexpr u32 offset_basis = 2'166'136'261u;
			static constexpr u32 FNV_prime    = (1u << 24) + (1u << 8) + 0x93u;
			u32                  state        = offset_basis;

		public:
			template<usize N>
			consteval void update_hash(const char (&arr)[N]) {
				for (usize i = 0; i < N; ++i) {
					state ^= arr[i];
					state *= FNV_prime;
				}
			}

			using result_type = u32;

			consteval explicit operator result_type() noexcept {
				return static_cast<result_type>(state);
			}
		};

		struct str {
			type_hash_code_t hash_value;

			template<usize N>
			consteval str(const char (&arr)[N]) {
				fnv1a_32_consteval h;
				h.update_hash(arr);
				hash_value.value = static_cast<u32>(h);
			}

			consteval operator type_hash_code_t() const { return hash_value; }
		};

		template<typename T>
		consteval str unique_string() {
#ifdef _MSC_VER
			return str{ __FUNCDNAME__ };
#else
			return str{ __PRETTY_FUNCTION__ };
#endif
		}

		template<typename T>
		consteval auto unique_id() {
			return static_cast<type_hash_code_t>(unique_string<T>());
		}

	}  // namespace detail

	template<typename T>
	constexpr type_hash_code_t type_hash_code = detail::unique_id<T>();

}  // namespace hash
