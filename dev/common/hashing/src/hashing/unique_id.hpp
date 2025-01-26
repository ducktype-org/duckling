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
			static constexpr u32 OFFSET_BASIS = 2'166'136'261u;
			static constexpr u32 FNV_PRIME    = (1u << 24) + (1u << 8) + 0x93u;
			u32                  state        = OFFSET_BASIS;

		public:
			consteval void updateHash(const std::string_view arr) {
				for (usize i = 0; i < arr.size(); ++i) {
					state ^= arr[i];
					state *= FNV_PRIME;
				}
			}

			using result_type = u32;

			consteval explicit operator result_type() noexcept {
				return static_cast<result_type>(state);
			}
		};

		struct Str {
			TypeHashCode hash_value;

			consteval Str(const std::string_view arr) {
				Fnv1a_32_Consteval h;
				h.updateHash(arr);
				hash_value.value = static_cast<u32>(h);
			}

			consteval operator TypeHashCode() const { return hash_value; }
		};

		template<typename T>
		consteval Str uniqueString() {
#ifdef _MSC_VER
			return Str{ __FUNCDNAME__ };
#else
			return Str{ __PRETTY_FUNCTION__ };
#endif
		}

		template<typename T>
		consteval auto uniqueId() {
			return static_cast<TypeHashCode>(uniqueString<T>());
		}

	}  // namespace detail

	template<typename T>
	constexpr TypeHashCode TYPE_HASH_CODE = detail::uniqueId<T>();

}  // namespace hash
