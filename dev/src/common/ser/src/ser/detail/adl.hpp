#pragma once

#include <ser/errc.hpp>
#include <ser/tags.hpp>

#include <concepts>
#include <type_traits>

namespace ser::detail::adl_barrier {

	// ── the barrier ───────────────────────────────────────────────────────────
	// Unqualified lookup stops at the first enclosing scope that declares the name, so
	// these four declarations keep the calls below from ever reaching ser::detail:: or
	// ser:: - argument-dependent lookup is then the only way a hook can be found.
	// Without them an unqualified serWrite(ar, x) inside the library would happily bind
	// to a library symbol of that name and the user's hook would never be consulted.
	//
	// They take no arguments, so they are never viable candidates themselves; = delete
	// makes a stray zero-argument call an error rather than a link failure.
	void serVisit() = delete;
	void serWrite() = delete;
	void serRead()  = delete;
	void serMake()  = delete;

	// Trailing return types on purpose. The substitution then happens in the DECLARATION,
	// where a missing hook is a plain substitution failure the detectors below can see.
	// With a fixed `Errc` return type the declaration would always be well-formed and
	// every detector would report true for every type.
	template<class Ar, class T>
	constexpr auto callVisit(Ar& ar, T& x) -> decltype(serVisit(ar, x)) {
		return serVisit(ar, x);
	}

	template<class Ar, class T>
	constexpr auto callWrite(Ar& ar, const T& x) -> decltype(serWrite(ar, x)) {
		return serWrite(ar, x);
	}

	template<class Ar, class T>
	constexpr auto callRead(Ar& ar, T& x) -> decltype(serRead(ar, x)) {
		return serRead(ar, x);
	}

	// The tag is a PARAMETER so that both template parameters are deduced. Making T
	// explicit here works everywhere except GCC - see HAS_ADL_MAKE_V below.
	template<class Ar, class T>
	constexpr auto callMake(Ar& ar, ::ser::tag<T> t) -> decltype(serMake(ar, t)) {
		return serMake(ar, t);
	}

}  // namespace ser::detail::adl_barrier

namespace ser::detail {

	// ── ADL hook detectors ────────────────────────────────────────────────────
	// T carries the cv-qualification of the object being visited: dispatchWrite asks
	// HAS_ADL_VISIT_V<const U, Ar>, dispatchRead asks HAS_ADL_VISIT_V<U, Ar>. A hook
	// that only accepts a non-const object is therefore invisible on the write side,
	// and checkHooks turns that asymmetry into a message instead of two formats.

	template<class T, class Ar>
	inline constexpr bool HAS_ADL_VISIT_V = requires(Ar& ar, T& x) {
		{ adl_barrier::callVisit(ar, x) } -> ::std::same_as<Errc>;
	};

	template<class T, class Ar>
	inline constexpr bool HAS_ADL_WRITE_V = requires(Ar& ar, const ::std::remove_cvref_t<T>& x) {
		{ adl_barrier::callWrite(ar, x) } -> ::std::same_as<Errc>;
	};

	template<class T, class Ar>
	inline constexpr bool HAS_ADL_READ_V = requires(Ar& ar, ::std::remove_cvref_t<T>& x) {
		{ adl_barrier::callRead(ar, x) } -> ::std::same_as<Errc>;
	};

	// The tag travels as a function argument, not as an explicit template argument, to
	// work around a GCC bug (13 and 14; clang 19 and MSVC accept the code below).
	// Substituting the explicit T while Ar is still unknown leaves the call
	// type-dependent, so it must be resolved in the instantiation context, where ADL
	// would find the hook; GCC instead resolves it against the poisoned declaration
	// right away. It is not overload resolution getting this wrong - a barrier declared
	// `void serMake(int) = delete`, never viable for a two-argument call, fails the
	// same way. Standalone reproducer, g++ -std=c++20:
	//
	//     namespace user { struct X {}; template <class Ar> int serMake(Ar&, X) { return 0; } }
	//     namespace bar {
	//         void serMake() = delete;
	//         template <class T, class Ar> auto call(Ar& ar) -> decltype(serMake(ar, T{}));
	//     }
	//     struct Ar {};
	//     static_assert(requires(Ar& a) { bar::call<user::X>(a); });
	//     int main() {}
	//
	//     error: no matching function for call to 'call<user::X>(Ar&)'
	//     In substitution of 'decltype (bar::serMake(ar, T{})) bar::call(Ar&)
	//                         [with T = user::X; Ar = <missing>]'
	//     error: use of deleted function 'void bar::serMake()'
	template<class T, class Ar>
	inline constexpr bool HAS_ADL_MAKE_V = requires(Ar& ar) {
		{
			adl_barrier::callMake(ar, ::ser::tag<::std::remove_cvref_t<T>>{})
		} -> ::std::same_as<::std::remove_cvref_t<T>>;
	};

	// ── the same four, without the return-type requirement ────────────────────
	// The difference between "loose but not strict" is exactly "the hook is there and
	// returns the wrong thing", which is the single most common way to write one.
	template<class T, class Ar>
	inline constexpr bool HAS_ADL_VISIT_LOOSE_V
		= requires(Ar& ar, T& x) { adl_barrier::callVisit(ar, x); };

	template<class T, class Ar>
	inline constexpr bool HAS_ADL_WRITE_LOOSE_V
		= requires(Ar& ar, const ::std::remove_cvref_t<T>& x) { adl_barrier::callWrite(ar, x); };

	template<class T, class Ar>
	inline constexpr bool HAS_ADL_READ_LOOSE_V
		= requires(Ar& ar, ::std::remove_cvref_t<T>& x) { adl_barrier::callRead(ar, x); };

	template<class T, class Ar>
	inline constexpr bool HAS_ADL_MAKE_LOOSE_V
		= requires(Ar& ar) { adl_barrier::callMake(ar, ::ser::tag<::std::remove_cvref_t<T>>{}); };

}  // namespace ser::detail
