#pragma once

#include <type_traits>
#include <concepts>

#include <base/ints.hpp>

namespace hashing {


	/**
	 * base class for type hash codes of any length
	 */
	template<std::integral I = u32>
	struct TypeHashCodeBase {
		using value_type = I;

		value_type value{};

		constexpr auto operator<=>(const TypeHashCodeBase<I>&) const noexcept = default;

		constexpr operator I() const noexcept { return value; }
	};

	using TypeHashCode = TypeHashCodeBase<>;


}  // namespace hashing
