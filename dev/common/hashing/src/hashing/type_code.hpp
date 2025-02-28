#pragma once

#include <type_traits>
#include <concepts>

#include <base/ints.hpp>

namespace hashing {


	/**
	 * base class for type codes of any length used by
	 */
	template<std::integral I = u32, bool is_unique = false>
	struct TypeCodeBase final {
		using value_type                = I;
		static constexpr bool IS_UNIQUE = is_unique;

		value_type value{};

		constexpr auto operator<=>(const TypeCodeBase<I, is_unique>&) const noexcept = default;

		constexpr operator I() const noexcept { return value; }
	};

	using TypeCode = TypeCodeBase<>;


}  // namespace hashing
