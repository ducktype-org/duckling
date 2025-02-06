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

	/**
	 * checks if the type is a specialization of a given template
	 * note: this concept works only for templates that have only type template parameters
	 */
	template<typename T, template<typename...> typename Templ>
	concept specialization_of = requires(T t) {
		[]<typename... Args>(Templ<Args...>) requires std::is_same_v<Templ<Args...>, T> {}(t);
	};


}  // namespace hashing
