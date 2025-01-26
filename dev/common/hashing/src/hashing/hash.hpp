#pragma once

#include "unique_id.hpp"
#include "hash_utils.hpp"
#include "hashing_algorithms.hpp"

namespace hashing {


	// this is a template overload for the 'addToHash' function
	// if a friend function overload exists for the type, it will be used
	// this one serves as a fallback and a place where specializations for
	// types that are not ours can be added (like the built-in types)
	template<hash_algorithm HashAlgorithm, typename T>
	constexpr void addToHash(HashAlgorithm& h, const T& t) {
		// for most types we only want to add to hash some subset of their subobjects (bases + members)
		// this can be done easily by defining `hash_decompose` friend function that
		// lists subobjects in an order in which we want to hash them
		if constexpr (detail::can_hash_decompose<T>) {
			std::apply([&](auto&&... args) { (addToHash(h, args), ...); }, hash_decompose(t));
		}
		// if there is no user-defined specialization for hash_decompose nor addToHash, but the chosen
		// hashing algorithm is able to hash the type directly, we can use it (for most algorithms those
		// will be types with unique representations)
		else if constexpr (detail::can_hash_directly<HashAlgorithm, T>) {
			h(t);
		}
		// specializations for types that are not ours
		else if constexpr (std::is_floating_point_v<T>) {
			// IEEE 754 floating point numbers have multiple representations of 0:
			// -0.0 == 0.0, but they should have the same hash
			auto t_ = auto{ t };
			if (t_ == 0) t_ = 0;
			detail::hashAsChars(h, t_);
		}
		// specialation for pointers
		else if constexpr (std::is_pointer_v<T>) {
			detail::hashAsChars(h, t);
		}
		// it's likely that nullptr will have a trivial bit representation and has
		// only one value so we hash it's hash-code instead
		else if constexpr (std::is_null_pointer_v<T>) {
			h(type_hash_code<T>);
		}
		// overloads for ranges
		// if the range is contiguous and its elements have unique representations we can
		// treat it as a segment of memory and hash it directly
		else if constexpr (detail::can_hash_range_as_chars<HashAlgorithm, T>) {
			detail::hashRangeAsChars(h, t);
		}
		// overload if range is contiguous and we can add its elements to the hash
		else if constexpr (std::ranges::contiguous_range<T> && requires(std::ranges::range_value_t<T> elem) {
								 addToHash(h, elem);
							 }) {
			for (auto&& elem: t) addToHash(h, elem);
		}
		// some ranges will compare equal but keep their elements in unspecified order
		else if constexpr (std::ranges::input_range<T> && requires(std::ranges::range_value_t<T> elem, HashAlgorithm::result_type res) {
							   addToHash(h, elem);
							   { res ^= res } -> std::same_as<typename HashAlgorithm::result_type>;
							   addToHash(h, res);
						   }) {
			auto                                hash_copy = h;
			typename HashAlgorithm::result_type result{};
			for (auto&& elem: t) {
				addToHash(hash_copy, elem);
				result ^= static_cast<typename HashAlgorithm::result_type>(hash_copy);
			}
			addToHash(h, result);
		}
		// std::hash is not constexpr, so if some type needs to be hashable in compile-time,
		// its specialization should be provided above
		else if constexpr (detail::can_stdhash<T>) {
			using std::hash;
			addToHash(h, hash<T>{}(t));
		} else {
			static_assert(
				false, "Please provide an 'addToHash' or 'hash_decompose' overload for this type"
			);
		}
	}

	template<hash_algorithm HashAlgorithm = Fnv1a_64, bool AppendTypeHashCode = true>
	class Hash {
	public:
		using result_type = typename HashAlgorithm::result_type;

		template<typename T>
		constexpr result_type operator()(const T& t) const noexcept {
			HashAlgorithm h;
			addToHash(h, t);

			if constexpr (AppendTypeHashCode) h(type_hash_code<T>);

			return static_cast<result_type>(h);
		}
	};

	template<hash_algorithm HashAlgorithm = Fnv1a_64, bool AppendTypeHashCode = true>
	class StatefulHash {
		HashAlgorithm h;

	public:
		using result_type = typename HashAlgorithm::result_type;

		template<typename T>
		constexpr result_type operator()(const T& t) noexcept {
			addToHash(h, t);

			if constexpr (AppendTypeHashCode) h(type_hash_code<T>);

			return static_cast<result_type>(h);
		}

		template<typename... Ts>
		constexpr result_type operator()(const Ts&... ts) noexcept {
			if constexpr (AppendTypeHashCode)
				((addToHash(h, ts), h(type_hash_code<Ts>)), ...);
			else
				(addToHash(h, ts), ...);

			return static_cast<result_type>(h);
		}

		constexpr explicit operator result_type() noexcept { return static_cast<result_type>(h); }
	};


}  // namespace hashing
