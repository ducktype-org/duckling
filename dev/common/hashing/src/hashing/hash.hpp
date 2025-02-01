#pragma once

#include <concepts>

#include "hashing_algorithms.hpp"
#include "type_hash_code.hpp"

namespace hashing {


	template<hash_algorithm HashAlgorithm = Fnv1a_64, bool AppendTypeHashCode = true>
	class Hash {
	public:
		using result_type = typename HashAlgorithm::result_type;

		template<typename T>
		constexpr result_type operator()(const T& t) const noexcept {
			HashAlgorithm h;
			addToHash(h, t);

			if constexpr (AppendTypeHashCode) {
				if constexpr (requires { h(TYPE_HASH_CODE<T, result_type, HashAlgorithm>); })
					h(TYPE_HASH_CODE<T, result_type, HashAlgorithm>);
				else
					h(TYPE_HASH_CODE<T>);
			}
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

			if constexpr (AppendTypeHashCode) {
				if constexpr (requires { h(TYPE_HASH_CODE<T, result_type, HashAlgorithm>); })
					h(TYPE_HASH_CODE<T, result_type, HashAlgorithm>);
				else
					h(TYPE_HASH_CODE<T>);
			}
			return static_cast<result_type>(h);
		}

		template<typename... Ts>
		constexpr result_type operator()(const Ts&... ts) noexcept {
			if constexpr (AppendTypeHashCode)
				if constexpr (requires {
								  ((addToHash(h, ts),
					                h(TYPE_HASH_CODE<Ts, result_type, HashAlgorithm>)),
					               ...);
							  })
					((addToHash(h, ts), h(TYPE_HASH_CODE<Ts, result_type, HashAlgorithm>)), ...);
				else
					((addToHash(h, ts), h(TYPE_HASH_CODE<Ts>)), ...);
			else
				(addToHash(h, ts), ...);

			return static_cast<result_type>(h);
		}

		constexpr explicit operator result_type() noexcept { return static_cast<result_type>(h); }
	};


}  // namespace hashing
