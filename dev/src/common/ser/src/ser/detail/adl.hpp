#pragma once

#include <ser/detail/hooks.hpp>
#include <ser/errc.hpp>
#include <ser/tags.hpp>

#include <concepts>
#include <type_traits>
#include <utility>

namespace ser::detail::adl_barrier {

	// ── the barrier ───────────────────────────────────────────────────────────
	// Unqualified lookup stops at the first enclosing scope that declares the name, so these
	// four declarations keep the calls below from reaching ser::detail:: or ser:: - ADL is
	// then the only way a hook can be found. They take no arguments, so they are never
	// viable candidates themselves, and = delete makes a stray zero-argument call an error
	// rather than a link failure.
	void serVisit() = delete;
	void serWrite() = delete;
	void serRead()  = delete;
	void serMake()  = delete;

	// Trailing return types on purpose: the substitution then happens in the DECLARATION,
	// where a missing hook is a substitution failure the detectors below can see. With a
	// fixed `Errc` return type every detector would report true for every type.
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

	template<class Ar, class T>
	constexpr auto callReadByCopy(Ar& ar, T&& x) -> decltype(serRead(ar, ::std::move(x)));

	// The tag is a PARAMETER so that both template parameters are deduced. Passing T
	// explicitly instead works everywhere except GCC 13 and 14, which resolve the call
	// against the poisoned declaration above rather than in the instantiation context.
	template<class Ar, class T>
	constexpr auto callMake(Ar& ar, ::ser::tag<T> t) -> decltype(serMake(ar, t)) {
		return serMake(ar, t);
	}

}  // namespace ser::detail::adl_barrier

namespace ser::detail {

	// ── ADL hook detectors ────────────────────────────────────────────────────
	// T carries the cv-qualification of the object being visited, so a hook that only accepts
	// a non-const object is invisible on the write side - and checkHooks turns that asymmetry
	// into a message instead of two formats.
	SER_DETAIL_HOOK_VISIT(inline constexpr, ADL, adl_barrier::callVisit(ar, x))
	SER_DETAIL_HOOK_WRITE(inline constexpr, ADL, adl_barrier::callWrite(ar, x))
	SER_DETAIL_HOOK_READ(inline constexpr, ADL, adl_barrier::callRead(ar, x))
	SER_DETAIL_HOOK_MAKE(
		inline constexpr, ADL, adl_barrier::callMake(ar, ::ser::tag<::std::remove_cvref_t<T>>{})
	)
	SER_DETAIL_HOOK_READ_BY_COPY(
		inline constexpr, ADL, adl_barrier::callReadByCopy(ar, ::std::move(x))
	)


}  // namespace ser::detail
