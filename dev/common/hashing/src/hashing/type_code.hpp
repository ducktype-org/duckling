#pragma once

#include <type_traits>
#include <concepts>

#include <base/ints.hpp>

namespace hashing {


	/**
	 * base class for type hash codes of any length
	 */
	template<std::integral I = u32>
	struct TypeCodeBase final {
		using value_type = I;

		value_type value{};

		constexpr auto operator<=>(const TypeCodeBase<I>&) const noexcept = default;

		constexpr operator I() const noexcept { return value; }
	};

	using TypeCode = TypeCodeBase<>;


}  // namespace hashing
