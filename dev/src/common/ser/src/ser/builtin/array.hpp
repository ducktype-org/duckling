#pragma once

#include <ser/concepts.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/errc.hpp>

#include <array>
#include <cstddef>
#include <type_traits>

namespace ser::builtin {

	// Fixed extent, so nothing about the length goes on the wire - the type carries it.
	template<class T>
	struct fixed_array: ::std::false_type {};

	// Specializing on a C array is what these three exist for.
	template<class T, ::std::size_t N>
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
	struct fixed_array<T[N]>: ::std::true_type {};

	template<class T, ::std::size_t N>
	struct fixed_array<::std::array<T, N>>: ::std::true_type {};

	template<class T>
	concept array_like = fixed_array<::std::remove_cv_t<T>>::value;

	template<class A>
	struct array_element;

	template<class T, ::std::size_t N>
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
	struct array_element<T[N]> {
		using type = T;
	};

	template<class T, ::std::size_t N>
	struct array_element<::std::array<T, N>> {
		using type = T;
	};

	template<class A>
	using array_element_t = typename array_element<::std::remove_cv_t<A>>::type;

	// The extent, which the type carries and the wire does not. MIN_WIRE_SIZE_V needs it
	// to turn an array field into a byte count, and sizeof arithmetic would not do:
	// sizeof(std::array<T, N>) is only N * sizeof(T) by convention, not by rule.
	template<class A>
	struct array_length;

	template<class T, ::std::size_t N>
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
	struct array_length<T[N]>: ::std::integral_constant<::std::size_t, N> {};

	template<class T, ::std::size_t N>
	struct array_length<::std::array<T, N>>: ::std::integral_constant<::std::size_t, N> {};

	template<class A>
	inline constexpr ::std::size_t ARRAY_LENGTH_V = array_length<::std::remove_cv_t<A>>::value;

	// Elements go through full dispatch, not a bulk copy: an element may have its own
	// hook, and in M2/M3 it may be remapped through a pool. The flat bulk-memcpy path
	// is a later optimization gated by is_flat_v, and it produces the same bytes.
	template<class T, writer Ar>
	constexpr Errc writeArray(Ar& ar, const T& a) {
		using E = array_element_t<T>;
		for (const auto& e: a)
			if (const auto c = detail::dispatchWrite<E>(ar, e); c != Errc::Ok) return c;
		return Errc::Ok;
	}

	template<class T, reader Ar>
	constexpr Errc readArray(Ar& ar, T& a) {
		using E = array_element_t<T>;
		for (auto& e: a)
			if (const auto c = detail::dispatchRead<E>(ar, e); c != Errc::Ok) return c;
		return Errc::Ok;
	}

}  // namespace ser::builtin
