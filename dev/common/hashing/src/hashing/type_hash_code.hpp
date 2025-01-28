#pragma once

#include <concepts>
#include <type_traits>
#include <string_view>

#include <base/ints.hpp>

#include "type_hash_code_def.hpp"
#include "hashing_algorithms.hpp"


namespace hashing {


	namespace detail {

		class Fnv1a_32_Consteval {
			static constexpr u32 OFFSET_BASIS = 2'166'136'261u;
			static constexpr u32 FNV_PRIME    = (1u << 24) + (1u << 8) + 0x93u;
			u32                  state        = OFFSET_BASIS;

		public:
			consteval void updateHash(const std::string_view arr) {
				for (auto&& c: arr) {
					state ^= c;
					state *= FNV_PRIME;
				}
			}

			using result_type = u32;

			consteval explicit operator result_type() noexcept {
				return static_cast<result_type>(state);
			}
		};

		// template<typename I>
		// using default_hash_algorithm_for = std::conditional_t<std::same_as<I, u32>, Fnv1a_32, Fnv1a_64>;
		template<typename I>
		using default_hash_algorithm_for = Fnv1a_32_Consteval;

		template<std::integral I = u32, typename HashAlgorithm = default_hash_algorithm_for<I>>
			requires std::convertible_to<typename HashAlgorithm::result_type, I>
		struct StrToIntegral {
			TypeHashCodeBase<I> hash_value;

			consteval StrToIntegral(const std::string_view arr) {
				HashAlgorithm h;
				h.updateHash(arr);
				hash_value.value = static_cast<typename HashAlgorithm::result_type>(h);
			}

			consteval operator TypeHashCodeBase<I>() const { return hash_value; }
		};

		template<typename T, std::integral I = u32, typename HashAlgorithm = default_hash_algorithm_for<I>>
		consteval StrToIntegral<I, HashAlgorithm> uniqueString() {
#ifdef _MSC_VER
			return StrToIntegral<I, HashAlgorithm>{ __FUNCDNAME__ };
#elifndef __PRETTY_FUNCTION__
			return StrToIntegral<I, HashAlgorithm>{ __PRETTY_FUNCTION__ };
#else
	#error "Please provide a unique string for each type"
			return { "" };
#endif
		}

		template<typename T, std::integral I = u32>
		consteval auto uniqueId() {
			return static_cast<TypeHashCodeBase<I>>(uniqueString<T, I>());
		}

	}  // namespace detail

	template<typename T, std::integral I = u32>
	constexpr TypeHashCodeBase<I> TYPE_HASH_CODE = detail::uniqueId<T, I>();


}  // namespace hash




