#pragma once

#include <ser/errc.hpp>

#include <concepts>
#include <type_traits>

// ── the shape of every hook question ─────────────────────────────────────────
// The three levels a hook can live at - ser::serializer<T>, the class itself, ADL - are
// asked the same things about the same four hook forms, and only the call expression
// differs. So the questions are spelled once here and the call is passed in.
//
// `DECL` is the declaration prefix: `static constexpr` inside a class, `inline constexpr`
// in a namespace. The call comes last so its commas need no protection, and it may name
// `ar` and `x`.

// clang-format off

/**
 * @brief Declares HAS_<NAME>_V and HAS_<NAME>_LOOSE_V for one hook.
 *
 * The loose one drops the return-type requirement, so "loose but not strict" is exactly
 * "the hook is there and returns the wrong thing" - the most common way to write one, and
 * the one whose default diagnostic points nowhere near the cause.
 */
#define SER_DETAIL_HOOK_PAIR(DECL, NAME, RET, PARAMS, ...)                     \
	template<class T, class Ar>                                                \
	DECL bool HAS_##NAME##_V = requires PARAMS {                               \
		{ __VA_ARGS__ } -> ::std::same_as<RET>;                                \
	};                                                                         \
	template<class T, class Ar>                                                \
	DECL bool HAS_##NAME##_LOOSE_V = requires PARAMS { __VA_ARGS__; };

/** @brief The symmetric form. T carries the cv-qualification of the object. */
#define SER_DETAIL_HOOK_VISIT(DECL, LEVEL, ...) \
	SER_DETAIL_HOOK_PAIR(DECL, LEVEL##_VISIT, Errc, (Ar& ar, T& x), __VA_ARGS__)

/** @brief Write: the object exists and is const. */
#define SER_DETAIL_HOOK_WRITE(DECL, LEVEL, ...)                     \
	SER_DETAIL_HOOK_PAIR(                                           \
		DECL, LEVEL##_WRITE, Errc,                                  \
		(Ar& ar, const ::std::remove_cvref_t<T>& x), __VA_ARGS__    \
	)

/** @brief Read: the object exists and is filled in place. */
#define SER_DETAIL_HOOK_READ(DECL, LEVEL, ...)                 \
	SER_DETAIL_HOOK_PAIR(                                      \
		DECL, LEVEL##_READ, Errc,                              \
		(Ar& ar, ::std::remove_cvref_t<T>& x), __VA_ARGS__     \
	)

/** @brief Make: there is no object yet, so the hook returns one by value. */
#define SER_DETAIL_HOOK_MAKE(DECL, LEVEL, ...)                       \
	SER_DETAIL_HOOK_PAIR(                                            \
		DECL, LEVEL##_MAKE, ::std::remove_cvref_t<T>,                \
		(Ar& ar), __VA_ARGS__                                        \
	)

/**
 * @brief Declares HAS_<NAME>_RVALUE_V: the hook accepts an RVALUE object.
 */
#define SER_DETAIL_HOOK_TAKES_RVALUE(DECL, NAME, PARAMS, ...) \
	template<class T, class Ar>                               \
	DECL bool HAS_##NAME##_RVALUE_V = requires PARAMS { __VA_ARGS__; };

/** @brief The rvalue probe for the read form. */
#define SER_DETAIL_HOOK_READ_BY_COPY(DECL, LEVEL, ...) \
	SER_DETAIL_HOOK_TAKES_RVALUE(                      \
		DECL, LEVEL##_READ,                            \
		(Ar& ar, ::std::remove_cvref_t<T>&& x), __VA_ARGS__ \
	)

/**
 * @brief Declares NAMES_<NAME>_V: the hook's name exists, whatever it can be called with.
 *
 * An id-expression for a function TEMPLATE cannot be formed without arguments to deduce
 * from, so this is true for exactly a single NON-template declaration - a hook pinned to
 * one concrete archive type. Compared against the archive in use, that is how a hook that
 * works is told from a hook that is being silently ignored.
 */
#define SER_DETAIL_HOOK_NAMED(DECL, NAME, ...) \
	template<class T>                          \
	DECL bool NAMES_##NAME##_V = requires { __VA_ARGS__; };

/**
 * @brief Declares the five per-level summaries, each asked with a real archive.
 *
 * `SCOPE` qualifies the detectors: `access::` for the two that need the type's own access
 * rights, empty for the ADL ones.
 */
#define SER_DETAIL_HOOK_SUMMARY(LEVEL, SCOPE)                                              \
	template<class T, class Ar>                                                            \
	inline constexpr bool LEVEL##_VISIT_WRITE_V                                            \
		= SCOPE HAS_##LEVEL##_VISIT_V<const T, writer_for<Ar>>;                            \
	template<class T, class Ar>                                                            \
	inline constexpr bool LEVEL##_VISIT_READ_V                                             \
		= SCOPE HAS_##LEVEL##_VISIT_V<T, reader_for<Ar>>;                                  \
	template<class T, class Ar>                                                            \
	inline constexpr bool LEVEL##_WRITE_V = SCOPE HAS_##LEVEL##_WRITE_V<T, writer_for<Ar>>; \
	template<class T, class Ar>                                                            \
	inline constexpr bool LEVEL##_READ_V = SCOPE HAS_##LEVEL##_READ_V<T, reader_for<Ar>>;   \
	template<class T, class Ar>                                                            \
	inline constexpr bool LEVEL##_MAKE_V = SCOPE HAS_##LEVEL##_MAKE_V<T, reader_for<Ar>>;   \
	template<class T, class Ar>                                                            \
	inline constexpr bool LEVEL##_READ_BY_COPY_V                                           \
		= SCOPE HAS_##LEVEL##_READ_RVALUE_V<T, reader_for<Ar>>;

/**
 * @brief The three levels of one hook form, as a disjunction: does ANY level answer it.
 */
#define SER_DETAIL_HOOK_ANY_LEVEL(FORM) \
	(TRAIT_##FORM##_V<T, Ar> || MEMBER_##FORM##_V<T, Ar> || ADL_##FORM##_V<T, Ar>)

/**
 * @brief Declares <FORM>_WRONG_RETURN_V: a hook of this form exists at some level and none
 * of them got the return type right.
 *
 * Fires only when NO level got the form right, so a correct trait hook still wins over a
 * broken in-class one instead of blocking the build. `ARCHIVE` is the direction the form
 * is asked in.
 */
#define SER_DETAIL_HOOK_WRONG_RETURN(FORM, ARCHIVE)                                   \
	template<class T, class Ar>                                                       \
	inline constexpr bool FORM##_WRONG_RETURN_V                                       \
		= (access::HAS_TRAIT_##FORM##_LOOSE_V<T, ARCHIVE<Ar>>                         \
	       || access::HAS_MEMBER_##FORM##_LOOSE_V<T, ARCHIVE<Ar>>                     \
	       || HAS_ADL_##FORM##_LOOSE_V<T, ARCHIVE<Ar>>)                               \
	   && !SER_DETAIL_HOOK_ANY_LEVEL(FORM);

/**
 * @brief Declares NONGENERIC_<FORM>_HOOK_V: the name is declared but the archive in use
 * cannot call it, so the hook is there and being ignored.
 *
 * That means a plain function pinned to some other concrete archive type, so dispatch walks
 * past it into the builtin or automatic path. It cannot see an overload set, a template
 * constrained to one archive, or an ADL free function.
 */
#define SER_DETAIL_HOOK_NONGENERIC(FORM)                                      \
	template<class T, class Ar>                                               \
	inline constexpr bool NONGENERIC_##FORM##_HOOK_V                          \
		= (access::NAMES_TRAIT_##FORM##_V<T> && !TRAIT_##FORM##_V<T, Ar>)     \
	   || (access::NAMES_MEMBER_##FORM##_V<T> && !MEMBER_##FORM##_V<T, Ar>);

// clang-format on
