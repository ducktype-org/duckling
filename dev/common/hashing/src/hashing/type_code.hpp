#pragma once

#include <concepts>

#include <base/ints.hpp>

namespace hashing {


	/**
	 * Class for storing codes of types of any length. Used by TYPE_HASH_CODE and TYPE_UNIQUE_CODE
	 *
	 * @tparam I - type of the value of the type code
	 * @tparam is_unique - if true, the type code is unique for each type
	 */
	template<std::integral I = u32, bool is_unique = true>
	struct TypeCode final {
		using value_type                = I;
		static constexpr bool IS_UNIQUE = is_unique;

		value_type value{};

		constexpr auto operator<=>(const TypeCode<I, is_unique>&) const noexcept = default;

		constexpr operator I() const noexcept { return value; }
	};


}  // namespace hashing
