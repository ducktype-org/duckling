#pragma once

#include <type_traits>
#include <concepts>

#include "hashing_algorithms.hpp"
#include "type_hash_code.hpp"
#include "addToHash.hpp"

namespace hashing {


	namespace detail {

		template<typename T>
		concept TypeHC_or_void = std::is_void_v<T> || specialization_of<T, TypeHashCodeBase>;

		template<class T>
		concept has_value_type = requires { typename T::value_type; };

		// primary template handles types that do not have a nested value_type member:
		template<class TypeHC>
		struct enable_TypeHC_value_type {};

		// specialization that adds TypeHC_value_type
		template<has_value_type TypeHC>
		struct enable_TypeHC_value_type<TypeHC> {
			using TypeHC_value_type = TypeHC::value_type;
		};

		// checks if type hash code can be added to the hash algorithm
		// (protects from invalid instantiations outside of the immediate context)
		template<typename T, typename TypeHC, typename HashAlgorithm>
		concept can_get_type_hash_code = requires(HashAlgorithm& h) {
			typename TypeHC::value_type;
			uniqueString<T, typename TypeHC::value_type, HashAlgorithm>();
			static_cast<TypeHashCodeBase<typename TypeHC::value_type>>(
				std::declval<StrToIntegral<typename TypeHC::value_type, HashAlgorithm>>()
			);
			detail::uniqueId<T, typename TypeHC::value_type, HashAlgorithm>();
			h(TYPE_HASH_CODE<T, typename TypeHC::value_type, HashAlgorithm>);
		};

	}  // namespace detail

	/**
	 * callable type that hashes objects appending their type codes
	 */
	template<
		hash_algorithm         HashAlgorithm = DefaultHashAlgorithm,
		detail::TypeHC_or_void TypeHC        = TypeHashCode>
	class Hash: public detail::enable_TypeHC_value_type<TypeHC> {
	public:
		static constexpr bool APPEND_TYPE_HASH_CODE = not std::is_void_v<TypeHC>;
		using result_type                           = typename HashAlgorithm::result_type;

		template<typename T>
		constexpr result_type operator()(const T& t) const noexcept {
			HashAlgorithm h{};
			addToHash(h, t);

			if constexpr (APPEND_TYPE_HASH_CODE) {
				if constexpr (detail::can_get_type_hash_code<T, TypeHC, HashAlgorithm>)
					h(TYPE_HASH_CODE<T, typename TypeHC::value_type, HashAlgorithm>);
				else
					h(TYPE_HASH_CODE<T>);
			}
			return static_cast<result_type>(h);
		}
	};

	/**
	 * callable type that hashes objects appending their type codes
	 * keeps the state between calls
	 */
	template<
		hash_algorithm         HashAlgorithm = DefaultHashAlgorithm,
		detail::TypeHC_or_void TypeHC        = TypeHashCode>
	class StatefulHash: public detail::enable_TypeHC_value_type<TypeHC> {
		HashAlgorithm h{};

	public:
		static constexpr bool APPEND_TYPE_HASH_CODE = not std::is_void_v<TypeHC>;
		using result_type                           = typename HashAlgorithm::result_type;

		template<typename T>
		constexpr result_type operator()(const T& t) noexcept {
			addToHash(h, t);

			if constexpr (APPEND_TYPE_HASH_CODE) {
				if constexpr (detail::can_get_type_hash_code<T, TypeHC, HashAlgorithm>)
					h(TYPE_HASH_CODE<T, typename TypeHC::value_type, HashAlgorithm>);
				else
					h(TYPE_HASH_CODE<T>);
			}
			return static_cast<result_type>(h);
		}

		template<typename... Ts>
		constexpr result_type operator()(const Ts&... ts) noexcept {
			if constexpr (APPEND_TYPE_HASH_CODE) {
				if constexpr ((detail::can_get_type_hash_code<Ts, TypeHC, HashAlgorithm> && ...)) {
					((addToHash(h, ts),
					  h(TYPE_HASH_CODE<Ts, typename TypeHC::value_type, HashAlgorithm>)),
					 ...);
				} else {
					((addToHash(h, ts), h(TYPE_HASH_CODE<Ts>)), ...);
				}
			} else {
				addToHash(h, ts...);
			}

			return static_cast<result_type>(h);
		}

		[[nodiscard]]
		constexpr explicit operator result_type() noexcept {
			return static_cast<result_type>(h);
		}
	};


}  // namespace hashing
