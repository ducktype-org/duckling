#pragma once

#include "unique_id.hpp"
#include "hash_utils.hpp"
#include "hashing_algorithms.hpp"

namespace hashing {


	template<hash_algorithm HashAlgorithm, typename T>
	constexpr void add_to_hash(HashAlgorithm& h, const T& t)
		requires detail::can_hash_directly<HashAlgorithm, T> || detail::can_hash_decompose<T>
	          || std::is_floating_point_v<T> || detail::can_stdhash<T> {
		if constexpr (detail::can_hash_directly<HashAlgorithm, T>) {
			h(t);
		} else if constexpr (detail::can_hash_decompose<T>) {
			std::apply([&](auto&&... args) { (add_to_hash(h, args), ...); }, hash_decompose(t));
		} else if constexpr (std::is_floating_point_v<T>) {
			auto t_ = auto{ t };
			if (t_ == 0) t_ = 0;
			std::array<char, sizeof(t_)> arr = std::bit_cast<std::array<char, sizeof(t_)>, T>(t_);
			h(arr.data(), arr.size());
		} else if constexpr (detail::can_stdhash<T>) {
			using std::hash;
			add_to_hash(h, hash<T>{}(t));
		} else {
			static_assert(false, "Please provide a specialization for this type");
		}
	}

	namespace detail {
		static constexpr bool AllowForStdHash = true;

		template<hash_algorithm HashAlgorithm, typename T>
		constexpr void apply_hash(HashAlgorithm& h, const T& t) {
			if constexpr (detail::can_add_to_hash<HashAlgorithm, T>) {
				add_to_hash(h, t);
			} else if constexpr (detail::can_hash_directly<HashAlgorithm, T>) {
				static_assert(
					false, "can_add_to_hash: should never happen as add_to_hash should exist"
				);
				h(t);
			} else if constexpr (detail::can_hash_decompose<T>) {
				static_assert(false, "apply: shoult never happen as add_to_hash should exist");
				std::apply([&](auto&&... args) { (apply_hash(h, args), ...); }, hash_decompose(t));
			} else if constexpr (detail::AllowForStdHash && detail::can_stdhash<T>) {
				static_assert(false, "stdHash: should never happen as add_to_hash should exist");
				using std::hash;
				h(hash<T>{}(t));
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
