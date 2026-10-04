#pragma once

#include <ser/errc.hpp>
#include <ser/internal/hooks.hpp>
#include <ser/tags.hpp>

#include <type_traits>
#include <utility>

namespace ser::internal::adl_barrier {

	/**
	 * @brief the barrier
	 * @details Unqualified lookup stops at the first enclosing scope that declares the name, so
	 * these four declarations keep the calls below from reaching ser::internal:: or ser:: - ADL is
	 * then the only way a hook can be found. They take no arguments, so they are never
	 * viable candidates themselves, and = delete makes a stray zero-argument call an error
	 * rather than a link failure.
	 */
	void serVisit() = delete;
	void serWrite() = delete;
	void serRead()  = delete;
	void serMake()  = delete;

	/**
	 * @brief Trailing return types on purpose: the substitution then happens in the DECLARATION,
	 * where a missing hook is a substitution failure the detectors below can see. With a
	 * fixed `Errc` return type every detector would report true for every type.
	 */
	template<class Ar, class T>
	constexpr auto callVisit(Ar& ar, T& x) -> decltype(serVisit(ar, x)) {
		return serVisit(ar, x);
	}

	template<class Ar, class T>
	constexpr auto callWrite(Ar& ar, const T& x) -> decltype(serWrite(ar, x)) {
		return serWrite(ar, x);
	}

	/**
	 * @brief Forwarding, so the one declaration answers both "does serRead fill an lvalue" and
	 * "does it also swallow an rvalue" - the second being the by-value hook that reads into
	 * a copy. Dispatch only ever calls it with an lvalue.
	 */
	template<class Ar, class T>
	constexpr auto callRead(Ar& ar, T&& x) -> decltype(serRead(ar, ::std::forward<T>(x))) {
		return serRead(ar, ::std::forward<T>(x));
	}

	/**
	 * @brief The tag is a PARAMETER so that both template parameters are deduced. Passing T
	 * explicitly instead works everywhere except GCC 13 and 14, which resolve the call
	 * against the poisoned declaration above rather than in the instantiation context.
	 */
	template<class Ar, class T>
	constexpr auto callMake(Ar& ar, ::ser::Tag<T> t) -> decltype(serMake(ar, t)) {
		return serMake(ar, t);
	}

} /* namespace ser::internal::adl_barrier */

namespace ser::internal {

	/**
	 * @brief level 3: hooks found by ADL
	 * @details The probes the questions in internal/hooks.hpp are asked with. Declared and never
	 * defined; each one just names the barrier call for its form, whose own trailing return
	 * type is what makes a missing hook a substitution failure.
	 */
	struct AdlHooks final {
		template<class Ar, class T>
		static auto visit(Ar& ar, T& x) -> decltype(adl_barrier::callVisit(ar, x));

		template<class Ar, class T>
		static auto write(Ar& ar, const T& x) -> decltype(adl_barrier::callWrite(ar, x));

		template<class Ar, class T>
		static auto read(Ar& ar, T&& x)
			-> decltype(adl_barrier::callRead(ar, ::std::forward<T>(x)));

		/**
		 * @brief T first: there is no argument to deduce it from. The tag stays a PARAMETER of
		 * callMake for the reason given there.
		 */
		template<class T, class Ar>
		static auto make(Ar& ar)
			-> decltype(adl_barrier::callMake(ar, ::ser::Tag<::std::remove_cvref_t<T>>{}));
	};

} /* namespace ser::internal */
