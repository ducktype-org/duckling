#pragma once

#include <ser/detail/hooks.hpp>
#include <ser/errc.hpp>
#include <ser/tags.hpp>

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

	// Forwarding, so the one declaration answers both "does serRead fill an lvalue" and
	// "does it also swallow an rvalue" - the second being the by-value hook that reads into
	// a copy. Dispatch only ever calls it with an lvalue.
	template<class Ar, class T>
	constexpr auto callRead(Ar& ar, T&& x) -> decltype(serRead(ar, ::std::forward<T>(x))) {
		return serRead(ar, ::std::forward<T>(x));
	}

	// The tag is a PARAMETER so that both template parameters are deduced. Passing T
	// explicitly instead works everywhere except GCC 13 and 14, which resolve the call
	// against the poisoned declaration above rather than in the instantiation context.
	template<class Ar, class T>
	constexpr auto callMake(Ar& ar, ::ser::tag<T> t) -> decltype(serMake(ar, t)) {
		return serMake(ar, t);
	}

}  // namespace ser::detail::adl_barrier

namespace ser::detail {

	// ── level 3: hooks found by ADL ───────────────────────────────────────────
	// The probes the questions in detail/hooks.hpp are asked with. Declared and never
	// defined; each one just names the barrier call for its form, whose own trailing return
	// type is what makes a missing hook a substitution failure.
	struct adl_hooks {
		template<class Ar, class T>
		static auto visit(Ar& ar, T& x) -> decltype(adl_barrier::callVisit(ar, x));

		template<class Ar, class T>
		static auto write(Ar& ar, const T& x) -> decltype(adl_barrier::callWrite(ar, x));

		template<class Ar, class T>
		static auto read(Ar& ar, T&& x)
			-> decltype(adl_barrier::callRead(ar, ::std::forward<T>(x)));

		// T first: there is no argument to deduce it from. The tag stays a PARAMETER of
		// callMake for the reason given there.
		template<class T, class Ar>
		static auto make(Ar& ar)
			-> decltype(adl_barrier::callMake(ar, ::ser::tag<::std::remove_cvref_t<T>>{}));
	};

}  // namespace ser::detail
