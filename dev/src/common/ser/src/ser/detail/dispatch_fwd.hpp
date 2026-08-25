#pragma once

#include <ser/concepts.hpp>
#include <ser/errc.hpp>

#include <type_traits>

// Declarations only, to break the cycle between dispatch.hpp and the adapters: dispatch.hpp
// needs builtin::writeArray defined, and array.hpp calls dispatchWrite for every element.
// The call is a qualified name, looked up at definition time, so no include order fixes it.
// The archives include this too, for decoupling rather than for a cycle.

namespace ser::detail {

	template<class T, writer Ar>
	constexpr Errc dispatchWrite(Ar& ar, const T& x);
	template<class T, reader Ar>
	constexpr Errc dispatchRead(Ar& ar, T& x);

	// A function cannot return a C array, so `make` is not available for T[N].
	// Such a field is still readable in place through dispatchRead.
	template<class T, reader Ar>
	requires(!::std::is_array_v<T>) constexpr T dispatchMake(Ar& ar);

}  // namespace ser::detail
