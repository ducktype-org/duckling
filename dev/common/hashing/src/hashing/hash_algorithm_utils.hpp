#pragma once

#include <concepts>
#include <type_traits>
#include <utility>
#include <ranges>
#include <array>
#include <tuple>
#include <bit>

#include <base/ints.hpp>

namespace hashing {


	template<typename From, typename To>
	concept is_explicitly_convertible_to = requires(From f) { static_cast<To>(f); };

	namespace detail {

		template<typename T>
		concept hash_algorithm_impl
			= std::is_object_v<T> && std::is_constructible_v<T> && std::is_destructible_v<T>
		   && requires { typename T::result_type; }
		   && (std::is_invocable_r_v<void, T, void*, usize>
		       || std::is_invocable_r_v<void, T, char*, usize>)
		   && is_explicitly_convertible_to<T, typename T::result_type>;

	}  // namespace detail

	template<typename T>
	concept hash_algorithm = detail::hash_algorithm_impl<std::remove_cvref_t<T>>;

	// returns a reference to the base of Derived
	template<typename Base, std::derived_from<std::remove_cvref_t<Base>> Derived>
	requires(not std::is_same_v<std::remove_cvref_t<Base>, std::remove_cvref_t<Derived>>)
	constexpr const Base& getBase(const Derived& derived) noexcept {
		return static_cast<const Base&>(derived);
	}

	namespace detail {

		template<typename HashAlgorithm, typename T>
		concept can_hash_directly
			= hash_algorithm<HashAlgorithm> && requires(HashAlgorithm& h, const T& t) { h(t); };

		template<typename T>
		concept can_stdhash = requires(const T& t) { std::hash<T>{}(t); };

		template<typename T>
		concept tuple_of_refs = requires(T t) {
			[]<typename... Args>(std::tuple<Args...>)
				requires std::is_same_v<std::tuple<Args...>, T>
			          && (std::is_reference_v<Args> && ...) {}(t);
		};

		template<typename T>
		concept can_hashDecompose = requires(const T& t) {
			{ hashDecompose(t) } -> tuple_of_refs;
		};

		template<hash_algorithm HashAlgorithm, typename T>
		constexpr void hashAsChars(HashAlgorithm&& h, const T& t) {
			std::array arr = std::bit_cast<std::array<char, sizeof(t)>, T>(t);
			if constexpr (requires {
							  std::forward<HashAlgorithm>(h)(std::string_view{ arr.data(),
				                                                               arr.size() });
						  }) {
				std::forward<HashAlgorithm>(h)(std::string_view{ arr.data(), arr.size() });
			} else {
				std::forward<HashAlgorithm>(h)(arr.data(), arr.size());
			}
		}

		template<typename HashAlgorithm, typename R>
		concept can_hash_range_as_chars
			= hash_algorithm<HashAlgorithm> && std::ranges::contiguous_range<R>
		   && std::has_unique_object_representations_v<std::ranges::range_value_t<R>>
		   && requires(HashAlgorithm& h, const R& t) {
				  h(std::ranges::data(t),
			        std::ranges::size(t) * sizeof(std::ranges::range_value_t<R>));
			  };

		template<hash_algorithm HashAlgorithm, std::ranges::contiguous_range R>
		requires can_hash_range_as_chars<HashAlgorithm, R>
		constexpr void hashRangeAsChars(HashAlgorithm& h, const R& t) {
			h(std::ranges::data(t), std::ranges::size(t) * sizeof(std::ranges::range_value_t<R>));
		}

		template<typename HashAlgorithm, typename R>
		concept can_hash_range_with_unspecified_order
			= std::copy_constructible<HashAlgorithm> && std::ranges::input_range<R>
		   && requires(HashAlgorithm::result_type res) {
				  {
					  res ^= res
				  }
				  -> std::convertible_to<std::remove_cvref_t<typename HashAlgorithm::result_type>>;
			  };

		template<typename T>
		concept supports_std_get = requires {
			[]<std::size_t... Is>(std::index_sequence<Is...>) requires requires {
				(std::get<Is>(std::declval<T>()), ...);
			} {}(std::make_index_sequence<std::tuple_size<T>::value>{});
		};

	}  // namespace detail


}  // namespace hashing
