#pragma once

#include <ser/builtin/scalar.hpp>
#include <ser/concepts.hpp>
#include <ser/errc.hpp>

#include <type_traits>
#include <utility>

namespace ser::builtin {

	template<class T>
	concept enum_like = ::std::is_enum_v<::std::remove_cv_t<T>>;

	// The underlying type is what goes on the wire, so changing it changes the format -
	// schema_hash (block F) is what catches that.
	//
	// No enumerator validation on read: without reflection there is no list to check
	// against, and a scoped enum holding an unlisted value is well-defined as long as
	// it fits the underlying type. Types that need validation provide their own
	// serializer<T> and return Errc::InvalidValue from it.

	template<class T, writer Ar>
	constexpr Errc writeEnum(Ar& ar, const T& v) {
		using U = ::std::remove_cv_t<T>;
		return writeScalar(ar, ::std::to_underlying(static_cast<U>(v)));
	}

	template<class T, reader Ar>
	constexpr Errc readEnum(Ar& ar, T& v) {
		using U = ::std::underlying_type_t<::std::remove_cv_t<T>>;
		U raw{};
		if (const auto e = readScalar(ar, raw); e != Errc::Ok) return e;
		v = static_cast<::std::remove_cv_t<T>>(raw);
		return Errc::Ok;
	}

}  // namespace ser::builtin
