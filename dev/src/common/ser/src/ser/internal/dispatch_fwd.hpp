#pragma once

#include <ser/concepts.hpp>
#include <ser/errc.hpp>

#include <type_traits>

/**
 * @file
 * @brief Declarations only, to break the cycle between dispatch.hpp and the adapters: dispatch.hpp
 * needs the array serializers defined, and array.hpp calls dispatchWrite for every element.
 * The call is a qualified name, looked up at definition time, so no include order fixes it.
 * The archives include this too, for decoupling rather than for a cycle.
 */

namespace ser::internal {

	template<class T, Writer Ar>
	constexpr Errc dispatchWrite(Ar& ar, const T& x);
	template<class T, Reader Ar>
	constexpr Errc dispatchRead(Ar& ar, T& x);

	/**
	 * @brief A function cannot return a C array, so `make` is not available for T[N].
	 * Such a field is still readable in place through dispatchRead.
	 */
	template<class T, Reader Ar>
	requires(!::std::is_array_v<T>) constexpr T dispatchMake(Ar& ar);

} /* namespace ser::internal */
