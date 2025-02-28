#pragma once

#include <type_traits>
#include <concepts>

#include <base/type_traits.hpp>

#include "hashing_algorithms.hpp"
#include "add_to_hash.hpp"
#include "type_hash_code.hpp"
#include "type_code.hpp"

namespace hashing {


	namespace detail {

		template<typename T>
		concept TypeCode_or_void = std::is_void_v<T> || base::IsInstantiationOfTypeValue<T, TypeCodeBase>;

		template<class T>
		concept has_value_type = requires { typename T::value_type; };

		// primary template handles types that do not have a nested value_type member:
		template<class TypeC>
		struct enable_TypeHC_value_type {};

		// specialization that adds TypeCode_value_type
		template<has_value_type TypeC>
		struct enable_TypeHC_value_type<TypeC> {
			using TypeCode_value_type = TypeC::value_type;
		};

		/**
		 * Checks if a given type hash code can be hashed by the given hash algorithm
		 * (it also protects from invalid instantiations outside of the immediate context
		 * that could cause a compilation error by omitting SFINAE)
		 */
		template<typename T, typename TypeC, typename HashAlgorithm>
		concept can_get_type_hash_code = requires(HashAlgorithm& h) {
			typename TypeC::value_type;
			uniqueString<T, typename TypeC::value_type, HashAlgorithm>();
			static_cast<TypeCodeBase<typename TypeC::value_type>>(
				std::declval<StrToIntegral<typename TypeC::value_type, HashAlgorithm>>()
			);
			detail::getIDFromUniqueString<T, typename TypeC::value_type, HashAlgorithm>();
			h(TYPE_HASH_CODE<T, typename TypeC::value_type, HashAlgorithm>);
		};

		template<base::IsInstantiationOfTypeValue<TypeCodeBase> TypeC, typename HashAlgorithm, typename... Ts>
		constexpr void addToHashAndAddTypeCode(HashAlgorithm& h, const Ts&... ts) noexcept {
			if (TypeC::IS_UNIQUE) {
				((addToHash(h, ts), h(TYPE_UNIQUE_CODE<Ts, typename TypeC::value_type>)), ...);
			} else {
				if constexpr ((detail::can_get_type_hash_code<Ts, TypeC, HashAlgorithm> && ...)) {
					((addToHash(h, ts), h(TYPE_HASH_CODE<Ts, typename TypeC::value_type, HashAlgorithm>)), ...);
				} else if constexpr ((detail::can_get_type_hash_code<Ts, TypeC, DefaultHashAlgorithm> && ...)) {
					((addToHash(h, ts), h(TYPE_HASH_CODE<Ts, typename TypeC::value_type, DefaultHashAlgorithm>)), ...);
				} else {
					((addToHash(h, ts), h(TYPE_HASH_CODE<Ts>)), ...);
				}
			}
		}

	}  // namespace detail

	/**
	 * @brief Callable type that obtains a hash for an object it is called with
	 * together with it's type code using the specified hash algorithm
	 *
	 * @tparam HashAlgorithm Hashing algorithm to use
	 * @tparam TypeC Type of the type code that should be appended to the hash
	 * or void if the type code should not be appended
	 */
	template<
		hash_algorithm           HashAlgorithm = DefaultHashAlgorithm,
		detail::TypeCode_or_void TypeC         = TypeCode>
	class Hash final: public detail::enable_TypeHC_value_type<TypeC> {
	public:
		static constexpr bool APPEND_TYPE_CODE = not std::is_void_v<TypeC>;
		using result_type                           = typename HashAlgorithm::result_type;

		template<typename T>
		constexpr result_type operator()(const T& t) const noexcept {
			HashAlgorithm h{};
			
			if constexpr (APPEND_TYPE_CODE)
				detail::addToHashAndAddTypeCode<TypeC, HashAlgorithm, T>(h, t);
			else
				addToHash(h, t);

			return static_cast<result_type>(h);
		}
	};

	/**
	 * @brief Callable type that obtains hash values for sequences of objects it is called with
	 * Type keeps the state between calls, so next objects can be appended
	 * Casting to result_type of the hash algorithm yields the hash value corresponding to the
	 * current state
	 *
	 * @tparam HashAlgorithm Hashing algorithm to use
	 * @tparam TypeC Type of the type code that should be appended to the hash
	 * or void if the type code should not be appended
	 */
	template<
		hash_algorithm           HashAlgorithm = DefaultHashAlgorithm,
		detail::TypeCode_or_void TypeC         = TypeCode>
	class StatefulHash final: public detail::enable_TypeHC_value_type<TypeC> {
		HashAlgorithm h{};

	public:
		static constexpr bool APPEND_TYPE_CODE = not std::is_void_v<TypeC>;
		using result_type                           = typename HashAlgorithm::result_type;

		template<typename T>
		constexpr StatefulHash& operator()(const T& t) noexcept {
			
			if constexpr (APPEND_TYPE_CODE)
				detail::addToHashAndAddTypeCode<TypeC, HashAlgorithm, T>(h, t);
			else
				addToHash(h, t);

			return *this;
		}

		template<typename... Ts>
		constexpr StatefulHash& operator()(const Ts&... ts) noexcept {
			
			if constexpr (APPEND_TYPE_CODE)
				detail::addToHashAndAddTypeCode<TypeC, HashAlgorithm, Ts...>(h, ts...);
			else
				addToHash(h, ts...);
			
			return *this;
		}

		[[nodiscard]]
		constexpr explicit operator result_type() noexcept {
			return static_cast<result_type>(h);
		}

		[[nodiscard]]
		constexpr result_type finalize() noexcept {
			return static_cast<result_type>(h);
		}
	};

	/**
	 * @brief Gets the hash value for the object using the specified hash algorithm
	 */
	template<
		hash_algorithm           HashAlgorithm = DefaultHashAlgorithm,
		detail::TypeCode_or_void TypeC         = TypeCode>
	auto justHash(const auto& t) {
		return Hash<HashAlgorithm, TypeC>{}(t);
	}

	/**
	 * @brief Variadic version of justHash()
	 */
	template<
		hash_algorithm           HashAlgorithm = DefaultHashAlgorithm,
		detail::TypeCode_or_void TypeC         = TypeCode>
	auto justHash(const auto&... ts) {
		return static_cast<StatefulHash<HashAlgorithm, TypeC>::result_type>(
			StatefulHash<HashAlgorithm, TypeC>{}(ts...)
		);
	}


}  // namespace hashing
