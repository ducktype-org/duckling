#pragma once

#include <ser/builtin/scalar.hpp>
#include <ser/concepts.hpp>
#include <ser/errc.hpp>

#include <type_traits>
#include <utility>

namespace ser {

	/**
	 * @brief The value range of an unscoped enum that has no fixed underlying type.
	 *
	 * Specialize it with MIN and MAX - either as enumerators or as plain integers - to make
	 * such an enum readable:
	 *
	 *     enum Kind { First, Second, Third };
	 *     template<>
	 *     struct ser::EnumRange<Kind> {
	 *         static constexpr Kind MIN = First;
	 *         static constexpr Kind MAX = Third;
	 *     };
	 *
	 */
	template<class T>
	struct EnumRange;

} /* namespace ser */

namespace ser::builtin {

	template<class T>
	concept EnumLike = ::std::is_enum_v<::std::remove_cv_t<T>>;

	/*
	 * The underlying type is what goes in the stream, so changing it changes the format -
	 * ser::schemaHash is what catches that. No enumerator validation on read: without
	 * reflection there is no list to check against, and a scoped enum holding an unlisted
	 * value is well-defined as long as it fits.
	 */

	/**
	 * @brief List-initialization from the underlying type is the discriminator: it is valid for a
	 * fixed underlying type and for nothing else.
	 */
	template<class T>
	inline constexpr bool ENUM_CONVERSION_DEFINED_V
		= ::std::is_scoped_enum_v<::std::remove_cv_t<T>>
	   || requires { ::std::remove_cv_t<T>{ ::std::underlying_type_t<::std::remove_cv_t<T>>{} }; };

	template<class T>
	inline constexpr bool HAS_ENUM_RANGE_V = requires {
		::ser::EnumRange<::std::remove_cv_t<T>>::MIN;
		::ser::EnumRange<::std::remove_cv_t<T>>::MAX;
	};

	template<class T, Writer Ar>
	constexpr Errc writeEnum(Ar& ar, const T& v) {
		using U = ::std::remove_cv_t<T>;
		return writeScalar(ar, ::std::to_underlying(static_cast<U>(v)));
	}

	template<class T, Reader Ar>
	constexpr Errc readEnum(Ar& ar, T& v) {
		using E = ::std::remove_cv_t<T>;
		using U = ::std::underlying_type_t<E>;

		static_assert(
			ENUM_CONVERSION_DEFINED_V<E> || HAS_ENUM_RANGE_V<E>,
			"ser: this is an unscoped enum with no fixed underlying type, so a value off a "
			"corrupt stream that is outside its enumerator range is UNDEFINED BEHAVIOUR "
			"rather than a wrong value - and nothing here can compute that range. Two ways "
			"out:\n"
			"  give it a fixed underlying type - enum E : std::uint32_t { ... } - after which "
			"every value of that type is a defined one\n"
			"  or declare the range: template<> struct ser::EnumRange<E> { static constexpr "
			"E MIN = ..., MAX = ...; };"
		);

		U raw{};
		if (const auto e = readScalar(ar, raw); e != Errc::Ok) return e;

		if constexpr (!ENUM_CONVERSION_DEFINED_V<E>) {
			if (raw < static_cast<U>(::ser::EnumRange<E>::MIN)
			    || raw > static_cast<U>(::ser::EnumRange<E>::MAX))
				return Errc::InvalidValue;
		}

		v = static_cast<E>(raw);
		return Errc::Ok;
	}

} /* namespace ser::builtin */
