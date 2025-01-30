#pragma once

#include <concepts>
#include <type_traits>
#include <utility>
#include <ranges>
#include <array>
#include <bit>


namespace hashing {


	template<typename T>
	concept hash_algorithm = requires {
		std::is_object_v<T>;
		std::is_constructible_v<T>;
		std::is_destructible_v<T>;

		std::is_invocable_v<T, void*, usize> || std::is_invocable_v<T, char*, usize>;
		typename T::result_type;
		std::is_convertible_v<T, typename T::result_type>;
	};

	namespace detail {
		template<typename HashAlgorithm, typename T>
		concept can_add_to_hash = hash_algorithm<HashAlgorithm>
		                       && requires(HashAlgorithm& h, const T& t) { addToHash(h, t); };			// @Taw3e8 @todo: remove this?

		template<typename HashAlgorithm, typename T>
		concept can_hash_directly
			= hash_algorithm<HashAlgorithm> && requires(HashAlgorithm& h, const T& t) { h(t); };

		using std::hash;
		template<typename T>
		concept can_stdhash = requires(const T& t) { hash<T>{}(t); };


		template<typename T, template<typename...> typename Templ>
		struct is_specialization_of : std::false_type {};

		template<template<typename...> typename Templ, typename... Args>
		struct is_specialization_of<Templ<Args...>, Templ> : std::true_type {};

		template<typename T>
		concept is_tuple = is_specialization_of<std::remove_cvref_t<T>, std::tuple>::value;		

		template<typename T>
		concept can_hashDecompose = requires(const T& t) {
			{ hashDecompose(t) } -> is_tuple;
		};

		template<hash_algorithm HashAlgorithm, typename T>
		constexpr void hashAsChars(HashAlgorithm& h, const T& t) {
			std::array arr = std::bit_cast<std::array<char, sizeof(t)>, T>(t);
			h(arr.data(), arr.size());
		}

		template<typename HashAlgorithm, typename R>
		concept is_range_with_hashable_elements
			= hash_algorithm<HashAlgorithm> && std::ranges::input_range<R>
		   && requires(HashAlgorithm& h, const R& t) {
				  addToHash(h, std::declval<std::ranges::range_value_t<R>>());
			  };

		template<typename HashAlgorithm, typename R>
		concept can_hash_range_as_chars
			= hash_algorithm<HashAlgorithm> && std::ranges::contiguous_range<R>
		   && std::has_unique_object_representations_v<std::ranges::range_value_t<R>>
		   && requires(HashAlgorithm& h, const R& t) {
				  h(std::ranges::data(t),
			        std::ranges::size(t) * sizeof(std::ranges::range_value_t<R>));
			  };

		template<hash_algorithm HashAlgorithm, std::ranges::contiguous_range R>
		constexpr void hashRangeAsChars(HashAlgorithm& h, const R& t)
			requires can_hash_range_as_chars<HashAlgorithm, R> {
			h(std::ranges::data(t), std::ranges::size(t) * sizeof(std::ranges::range_value_t<R>));
		}
	}


}  // namespace hashing
