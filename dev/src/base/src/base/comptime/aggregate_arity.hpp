#pragma once

#include <base/preproc/ladder.hpp>

#include <cstddef>
#include <type_traits>
#include <utility>

/**
 * @file
 * @brief How many clauses an aggregate's braces take, probed without naming a single member.
 * @details The count `base/comptime/member_walk.hpp` needs to pick a rung: it is what a
 * structured binding has to spell out, and C++23 has no other way to ask for it.
 */

namespace base {

	/** @brief What ELIDED_ARITY_V answers when no clause count compiled at all. */
	inline constexpr ::std::size_t NO_ARITY = static_cast<::std::size_t>(-1);

	namespace internal {

		/*
		 * the counting probe
		 * Converts to anything and is deliberately never defined: it appears only inside a
		 * requires-expression, where the initialization is never evaluated. Clang reports
		 * Wundefined-inline anyway, because it counts being CHOSEN by overload resolution as
		 * being used, so the warning is silenced exactly here.
		 */
#if defined(__clang__)
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wundefined-inline"
#endif
		struct AnyInit final {
			template<class T>
			constexpr operator T() const; /* NOT defined, on purpose */
		};
#if defined(__clang__)
	#pragma clang diagnostic pop
#endif

		/**
		 * @brief one probe, and why one is enough
		 * @details T{ p, p, p } - the largest N that compiles is the field count. Each clause
		 * COPY-INITIALIZES its member, which is the context where a conversion function is a
		 * first-class path, so the probe reaches a member whatever its constructors look like.
		 * A clause in BRACES would have to go through the member's copy constructor instead,
		 * and that is where compilers part company - hence one probe, unbraced.
		 *
		 * THE PRICE IS C ARRAYS. An unbraced clause is subject to brace elision, and nothing
		 * converts to `int[3]`, so an array field consumes one clause PER ELEMENT and the
		 * count comes out too large.
		 */
		template<class T, ::std::size_t... I>
		consteval bool initWith() {
			return requires { T{ (void(I), AnyInit{})... }; };
		}

		template<class T, ::std::size_t... I>
		consteval bool initWithSeq(::std::index_sequence<I...>) {
			return initWith<T, I...>();
		}

		/**
		 * @brief the scan
		 * @details The valid clause counts form ONE CONTIGUOUS RUN, so the largest valid N is
		 * found by walking up and stopping one past the last success. Best carries that
		 * success as a template argument, because only a template argument can drive the `if
		 * constexpr` that stops. A recursion and not a fold: a fold instantiates the probe over
		 * the whole range for every type, while the scan spends about as many as the type has
		 * fields.
		 */
		template<class T, ::std::size_t N, ::std::size_t Best>
		consteval ::std::size_t scanArity() {
			if constexpr (N > LADDER_MAX + 1)
				return Best;
			else if constexpr (initWithSeq<T>(::std::make_index_sequence<N>{}))
				return scanArity<T, N + 1, N>();        /* keep going */
			else if constexpr (Best != NO_ARITY)
				return Best;                            /* one past the top */
			else
				return scanArity<T, N + 1, NO_ARITY>(); /* nothing has fit yet */
		}

		template<class T, ::std::size_t... I>
		consteval bool listInitWith() {
			return requires { T{ { (void(I), AnyInit{}) }... }; };
		}

		template<class T, ::std::size_t... I>
		consteval bool listInitWithSeq(::std::index_sequence<I...>) {
			return listInitWith<T, I...>();
		}

		/** @brief Converts to nothing except a base of T. */
		template<class T>
		struct BaseInit final {
			template<class U>
			requires(!::std::is_same_v<U, T> && ::std::is_base_of_v<U, T>)
			constexpr operator U() const; /* NOT defined, on purpose */
		};

	}  // namespace internal

	/**
	 * @brief The most unbraced clauses T's braces accept, or NO_ARITY when none do.
	 * @details Probed one PAST the ladder's limit on purpose. A type whose braces accept any
	 * number of clauses - a std::initializer_list constructor - never stops, and reporting
	 * LADDER_MAX + 1 tells it apart from a type that genuinely has LADDER_MAX fields.
	 */
	template<class T>
	inline constexpr ::std::size_t ELIDED_ARITY_V = internal::scanArity<T, 0, NO_ARITY>();

	/**
	 * @brief "Are these braces a LIST?" A constructor stops at its own arity, an
	 * initializer_list constructor never does.
	 * @note It costs LADDER_MAX + 1 clauses, so ask it only where the answer is needed.
	 */
	template<class T>
	inline constexpr bool ACCEPTS_ANY_LENGTH_V
		= internal::listInitWithSeq<T>(::std::make_index_sequence<LADDER_MAX + 1>{});

	/**
	 * @brief Whether the aggregate T has a base class.
	 * @details Aggregate initialization treats a base class as an ELEMENT - D{ B{1}, 2 } - while
	 * a structured binding sees only data members, so a base silently makes ELIDED_ARITY_V
	 * wrong. Detected the way Boost.PFR does it: if the first element accepts a probe that
	 * converts to nothing but a base of T, the first element is a base.
	 */
	template<class T>
	inline constexpr bool HAS_BASE_V = requires { T{ internal::BaseInit<T>{} }; };

}  // namespace base
