#pragma once

#include <base/ints.hpp>

namespace hashing {

	struct TypeHashCode {
		u32 value{};

		constexpr auto operator<=>(const TypeHashCode&) const = default;

		constexpr operator u32() const noexcept { return value; }
	};

	namespace detail {

		class Fnv1a_32_Consteval {
			static constexpr u32 offset_basis = 2'166'136'261u;
			static constexpr u32 FNV_prime    = (1u << 24) + (1u << 8) + 0x93u;
			u32                  state        = offset_basis;

		public:
			template<usize N>
			consteval void updatehash(const char (&arr)[N]) {
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

		struct Str {
			TypeHashCode hash_value;

			template<usize N>
			consteval Str(const char (&arr)[N]) {
				Fnv1a_32_Consteval h;
				h.updatehash(arr);
				hash_value.value = static_cast<u32>(h);
			}

			consteval operator TypeHashCode() const { return hash_value; }
		};

		template<typename T>
		consteval Str unique_string() {
#ifdef _MSC_VER
			return Str{ __FUNCDNAME__ };
#else
			return Str{ __PRETTY_FUNCTION__ };
#endif
		}

		template<typename T>
		consteval auto unique_id() {
			return static_cast<TypeHashCode>(unique_string<T>());
		}

	}  // namespace detail

	template<typename T>
	constexpr TypeHashCode type_hash_code = detail::unique_id<T>();

}  // namespace hash
