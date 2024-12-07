#pragma once

#include "unique_id.hpp"
#include "hash_utils.hpp"
#include "hashing_algorithms.hpp"

namespace hashing {

	template<hash_algorithm HashAlgorithm, typename T>
	constexpr void add_to_hash(HashAlgorithm& h, const T& t) {
		if constexpr (detail::can_hash_decompose<T>) {
			std::apply([&](auto&&... args) { (add_to_hash(h, args), ...); }, hash_decompose(t));
		} else if constexpr (detail::can_hash_directly<HashAlgorithm, T>) {
			h(t);
		}
		// specializations for types that are not ours:
		else if constexpr (std::is_floating_point_v<T>) {
			auto t_ = auto{ t };
			if (t_ == 0) t_ = 0;
			detail::hash_as_chars(h, t_);
		} else if constexpr (std::is_pointer_v<T>) {
			detail::hash_as_chars(h, t);
		} else if constexpr (std::is_null_pointer_v<T>) {
			h(type_hash_code<T>);
		}
		// overloads for ranges
		else if constexpr (std::ranges::contiguous_range<T> && requires(std::ranges::range_value_t<T> elem) {
							   add_to_hash(h, elem);
						   }) {
			for (auto&& elem: t) add_to_hash(h, elem);
		}
		// some ranges will compare equal but keep their elements in unspecified order
		else if constexpr (std::ranges::input_range<T> && requires(std::ranges::range_value_t<T> elem, HashAlgorithm::result_type res) {
							   add_to_hash(h, elem);
							   { res ^= res } -> std::same_as<typename HashAlgorithm::result_type>;
							   add_to_hash(h, res);
						   }) {
			auto                                hash_copy = h;
			typename HashAlgorithm::result_type result{};
			for (auto&& elem: t) {
				add_to_hash(hash_copy, elem);
				result ^= static_cast<typename HashAlgorithm::result_type>(hash_copy);
			}
			add_to_hash(h, result);
		}
		// std::hash is not constexpr, so if some type needs to be hashable in compile-time,
		// its specialization should be provided above
		else if constexpr (detail::can_stdhash<T>) {
			using std::hash;
			add_to_hash(h, hash<T>{}(t));
		} else {
			static_assert(false, "Please provide a specialization for this type");
		}
	}

	namespace detail {

		template<hash_algorithm HashAlgorithm, typename T>
		constexpr void apply_hash(HashAlgorithm& h, const T& t) {
			if constexpr (detail::can_add_to_hash<HashAlgorithm, T>) {
				add_to_hash(h, t);
			} else {
				static_assert(
					false,
					"Please provide an 'add_to_hash' or 'hash_decompose' overload for this type"
				);
			}
		}
	}  // namespace detail

	template<hash_algorithm HashAlgorithm = fnv1a_64, bool AppendTypeHashCode = true>
	class hash {
	public:
		using result_type = typename HashAlgorithm::result_type;

		template<typename T>
		constexpr result_type operator()(const T& t) const noexcept {
			HashAlgorithm h;
			detail::apply_hash(h, t);

			if constexpr (AppendTypeHashCode) h(type_hash_code<T>);

			return static_cast<result_type>(h);
		}
	};

	template<hash_algorithm HashAlgorithm = fnv1a_64, bool AppendTypeHashCode = true>
	class stateful_hash {
		HashAlgorithm h;

	public:
		using result_type = typename HashAlgorithm::result_type;

		template<typename T>
		constexpr result_type operator()(const T& t) noexcept {
			detail::apply_hash(h, t);

			if constexpr (AppendTypeHashCode) h(type_hash_code<T>);

			return static_cast<result_type>(h);
		}

		template<typename... Ts>
		constexpr result_type operator()(const Ts&... ts) noexcept {
			if constexpr (AppendTypeHashCode)
				((detail::apply_hash(h, ts), detail::apply_hash(h, type_hash_code<Ts>)), ...);
			else
				(detail::apply_hash(h, ts), ...);

			return static_cast<result_type>(h);
		}

		constexpr explicit operator result_type() noexcept { return static_cast<result_type>(h); }
	};


}  // namespace hashing
