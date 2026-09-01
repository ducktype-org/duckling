#pragma once

#include <base/comptime/type_list.hpp>
#include <base/preproc/for_each.hpp>
#include <base/preproc/ladder.hpp>

/**
 * @file
 * @brief Compile-time reflection over an aggregate's members, through structured bindings.
 * @details These are rungs for `BASE_LADDER`, not standalone macros: each expands to an
 * `else if constexpr` branch and reads `N`, `obj` and `f` from the scope it is expanded in.
 * The intended shape is
 *
 *     template<std::size_t N, class T, class F>
 *     static constexpr decltype(auto) visitMembersN(T&& obj, F&& f) {
 *         if constexpr (N == 0) return f();
 *         BASE_LADDER(BASE_LADDER_WALK)
 *     }
 *
 * @note It has to be expanded INSIDE a class the aggregate befriends when the members are
 * private: a structured binding needs access, and friendship in C++ is neither inherited nor
 * transitive. That is why this header hands out the rungs and not a ready-made walker.
 */

/** @brief One rung of the walk: bind every member of `obj` and hand them to `f`. */
#define BASE_LADDER_WALK(n)                  \
	else if constexpr (N == (n)) {           \
		auto&& [BASE_LADDER_NAMES(n)] = obj; \
		return f(BASE_LADDER_NAMES(n));      \
	}

/**
 * @brief One member as `base::FieldDecl`: its type exactly as declared, plus whether a
 * non-const lvalue reference binds to it.
 */
#define BASE_LADDER_FIELD_DECL(m) \
	::base::FieldDecl<decltype(m), (requires { ::base::bindProbe(m); })>

/** @brief One rung of the declared-type table: every member of `obj` as a `base::TypeList`. */
#define BASE_LADDER_DECLS(n)                                                       \
	else if constexpr (N == (n)) {                                                 \
		auto&& [BASE_LADDER_NAMES(n)] = obj;                                       \
		return ::base::TypeList<                                                   \
			FOR_EACH_COMMA(BASE_LADDER_FIELD_DECL, BASE_LADDER_NAMES(n))>{};       \
	}
