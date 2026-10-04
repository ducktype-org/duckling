#pragma once

#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/errc.hpp>

#include <cstddef>
#include <type_traits>

namespace ser::builtin {

	template<class T>
	concept ScalarLike = ::std::is_arithmetic_v<::std::remove_cv_t<T>>
	                  || ::std::is_same_v<::std::remove_cv_t<T>, ::std::byte>;

	/**
	 * @brief long double is not a wire type
	 * @details sizeof(long double) is 16 on x86-64 and only 10 of those bytes are value
	 */
	template<class T>
	inline constexpr bool SCALAR_IS_WIRE_SAFE_V
		= !::std::is_same_v<::std::remove_cv_t<T>, long double>;

#define SER_INTERNAL_ASSERT_SCALAR_WIRE_SAFE(T)                                           \
	static_assert(                                                                        \
		SCALAR_IS_WIRE_SAFE_V<T>,                                                         \
		"ser: long double cannot go on the wire - it has more bytes than it has value, "  \
		"so the padding between them would be written as it happened to be and the same " \
		"number would not give the same stream twice. Use double, or store the "          \
		"significant bytes yourself through a serializer<T> of your own."                 \
	);

	/**
	 * @brief bool is not "just one byte"
	 * @details sizeof(bool) is implementation-defined, and a bool holding anything other than 0 or 1
	 * is undefined behaviour - memcpy from a corrupted stream would poison every later branch
	 * on it. So bool goes on the wire as an explicit 0/1 byte and comes back validated.
	 */

	template<class T, Writer Ar>
	constexpr Errc writeScalar(Ar& ar, const T& v) {
		using U = ::std::remove_cv_t<T>;
		SER_INTERNAL_ASSERT_SCALAR_WIRE_SAFE(U)
		if constexpr (::std::is_same_v<U, bool>)
			return ar.writeRaw(static_cast<::std::byte>(v ? 1 : 0));
		else
			return ar.writeRaw(v);
	}

	template<class T, Reader Ar>
	constexpr Errc readScalar(Ar& ar, T& v) {
		using U = ::std::remove_cv_t<T>;
		SER_INTERNAL_ASSERT_SCALAR_WIRE_SAFE(U)
		if constexpr (::std::is_same_v<U, bool>) {
			::std::byte b{};
			if (const auto e = ar.readRaw(b); e != Errc::Ok) return e;
			if (b != ::std::byte{ 0 } && b != ::std::byte{ 1 }) return Errc::InvalidValue;
			v = (b == ::std::byte{ 1 });
			return Errc::Ok;
		} else {
			return ar.readRaw(v);
		}
	}

	template<class T>
	inline constexpr ::std::size_t SCALAR_WIRE_SIZE
		= ::std::is_same_v<::std::remove_cv_t<T>, bool> ? ::std::size_t{ 1 } : sizeof(T);

} /* namespace ser::builtin */
