#pragma once

/**
 * @file
 * @brief The hook DETECTORS and the refusals built on them - `ser`'s diagnostic layer.
 * @details Split out of `<ser/access.hpp>` so that header carries only what a serializable
 * type writes by hand: `ser::Members<N>`, `ser::Access` and `SER_FRIEND`. Everything here
 * answers "which hook does this type have, and is it the hook the author meant" - questions
 * the library asks itself. Nothing here is part of the public surface.
 * @note `ser::Access` itself stays public and keeps its own in-class detectors: access
 * control is part of SFINAE, so a detector that must see a PRIVATE hook has to live in the
 * class the type befriends. Only the layer above it moved.
 */

#include <ser/access.hpp>
#include <ser/concepts.hpp>
#include <ser/errc.hpp>
#include <ser/internal/adl.hpp>
#include <ser/internal/hooks.hpp>
#include <ser/serializer.hpp>

#include <tuple>
#include <type_traits>

namespace ser::internal {

	/** @brief the types SER_DESCRIBE named */
	template<class T>
	struct TupleFieldList;

	template<class... Es>
	struct TupleFieldList<::std::tuple<Es...>> {
		using type = ::base::TypeList<::std::remove_cvref_t<Es>...>;
	};

	template<class T>
	using DescribedTypesT = typename TupleFieldList<
		decltype(Access::described(::std::declval<const ::std::remove_cv_t<T>&>()))>::type;

	/**
	 * @brief the other direction of an archive
	 * @details Half the questions checkHooks asks are about the direction it is NOT in - from
	 * dispatchWrite, whether the type can be read back - and answering one needs a REAL
	 * archive carrying the same Ctx, so a hook reaching for ar.pool<P>() compiles during
	 * detection.
	 *
	 * ser::In<Ctx> is exact. The writer is exact only in its context, the buffer being the
	 * caller's choice; that can matter only for a hook pinned to one concrete
	 * out<Buf, Ctx>, which NONGENERIC_*_HOOK_V reports anyway.
	 */
	template<class Ar>
	using ReaderFor = ::std::conditional_t<Reader<Ar>, Ar, In<typename Ar::ContextType>>;

	template<class Ar>
	using WriterFor
		= ::std::conditional_t<Writer<Ar>, Ar, Out<::std::span<::std::byte>, typename Ar::ContextType>>;

	/**
	 * @brief per-level summaries
	 * @details The questions in internal/hooks.hpp take whatever archive they are given; these fill
	 * in the one the form is really asked in, so "does this type declare a hook" and "which rung
	 * will dispatch take" are the same question asked twice. LVL_ takes a level tag, ANY_ folds the
	 * three of them.
	 */
	template<class L, class T, class Ar>
	inline constexpr bool LVL_VISIT_WRITE_V = HAS_VISIT_V<L, const T, WriterFor<Ar>>;

	template<class L, class T, class Ar>
	inline constexpr bool LVL_VISIT_READ_V = HAS_VISIT_V<L, T, ReaderFor<Ar>>;

	template<class L, class T, class Ar>
	inline constexpr bool LVL_WRITE_V = HAS_WRITE_V<L, T, WriterFor<Ar>>;

	template<class L, class T, class Ar>
	inline constexpr bool LVL_READ_V = HAS_READ_V<L, T, ReaderFor<Ar>>;

	template<class L, class T, class Ar>
	inline constexpr bool LVL_MAKE_V = HAS_MAKE_V<L, T, ReaderFor<Ar>>;

	template<class L, class T, class Ar>
	inline constexpr bool LVL_READ_BY_COPY_V = HAS_READ_RVALUE_V<L, T, ReaderFor<Ar>>;

	/**
	 * @brief A variable template cannot be passed as a template argument, so the fold over the
	 * three levels is spelled out once per form rather than written generically.
	 */
	template<class T, class Ar>
	inline constexpr bool ANY_VISIT_WRITE_V
		= LVL_VISIT_WRITE_V<Access::TraitHooks, T, Ar>
	   || LVL_VISIT_WRITE_V<Access::MemberHooks, T, Ar> || LVL_VISIT_WRITE_V<AdlHooks, T, Ar>;

	template<class T, class Ar>
	inline constexpr bool ANY_VISIT_READ_V
		= LVL_VISIT_READ_V<Access::TraitHooks, T, Ar>
	   || LVL_VISIT_READ_V<Access::MemberHooks, T, Ar> || LVL_VISIT_READ_V<AdlHooks, T, Ar>;

	template<class T, class Ar>
	inline constexpr bool ANY_WRITE_V
		= LVL_WRITE_V<Access::TraitHooks, T, Ar> || LVL_WRITE_V<Access::MemberHooks, T, Ar>
	   || LVL_WRITE_V<AdlHooks, T, Ar>;

	template<class T, class Ar>
	inline constexpr bool ANY_READ_V
		= LVL_READ_V<Access::TraitHooks, T, Ar> || LVL_READ_V<Access::MemberHooks, T, Ar>
	   || LVL_READ_V<AdlHooks, T, Ar>;

	template<class T, class Ar>
	inline constexpr bool ANY_MAKE_V
		= LVL_MAKE_V<Access::TraitHooks, T, Ar> || LVL_MAKE_V<Access::MemberHooks, T, Ar>
	   || LVL_MAKE_V<AdlHooks, T, Ar>;

	/**
	 * @brief does this type describe its own format?
	 * @details Either direction, any level, any form. This is what keeps a type with its own
	 * serializer from ever being decomposed field by field. It takes Ar because the answer
	 * only means anything relative to the archive that will do the work.
	 */
	template<class T, class Ar>
	inline constexpr bool HAS_ANY_WRITE_HOOK_V = ANY_WRITE_V<T, Ar> || ANY_VISIT_WRITE_V<T, Ar>;

	template<class T, class Ar>
	inline constexpr bool HAS_ANY_READ_HOOK_V
		= ANY_READ_V<T, Ar> || ANY_MAKE_V<T, Ar> || ANY_VISIT_READ_V<T, Ar>;

	template<class T, class Ar>
	inline constexpr bool HAS_ANY_MAKE_HOOK_V = ANY_MAKE_V<T, Ar>;

	/** @brief Every form of every level: the union of the two directions. */
	template<class T, class Ar>
	inline constexpr bool HAS_CUSTOM_SERIALIZER_V
		= HAS_ANY_WRITE_HOOK_V<T, Ar> || HAS_ANY_READ_HOOK_V<T, Ar>;

	/**
	 * @brief wrong return type
	 * @details A hook of this form exists at some level and none of them got the return type right.
	 * Each fires only when NO level got the form right, so a correct trait hook still wins
	 * over a broken in-class one instead of blocking the build.
	 */
	template<class T, class Ar>
	inline constexpr bool WRITE_WRONG_RETURN_V
		= (HAS_WRITE_LOOSE_V<Access::TraitHooks, T, WriterFor<Ar>>
	       || HAS_WRITE_LOOSE_V<Access::MemberHooks, T, WriterFor<Ar>>
	       || HAS_WRITE_LOOSE_V<AdlHooks, T, WriterFor<Ar>>)
	   && !ANY_WRITE_V<T, Ar>;

	template<class T, class Ar>
	inline constexpr bool READ_WRONG_RETURN_V
		= (HAS_READ_LOOSE_V<Access::TraitHooks, T, ReaderFor<Ar>>
	       || HAS_READ_LOOSE_V<Access::MemberHooks, T, ReaderFor<Ar>>
	       || HAS_READ_LOOSE_V<AdlHooks, T, ReaderFor<Ar>>)
	   && !ANY_READ_V<T, Ar>;

	template<class T, class Ar>
	inline constexpr bool MAKE_WRONG_RETURN_V
		= (HAS_MAKE_LOOSE_V<Access::TraitHooks, T, ReaderFor<Ar>>
	       || HAS_MAKE_LOOSE_V<Access::MemberHooks, T, ReaderFor<Ar>>
	       || HAS_MAKE_LOOSE_V<AdlHooks, T, ReaderFor<Ar>>)
	   && !ANY_MAKE_V<T, Ar>;

	/**
	 * @brief visit is the only form asked in both directions, so it is six terms rather than three.
	 */
	template<class T, class Ar>
	inline constexpr bool VISIT_WRONG_RETURN_V
		= (HAS_VISIT_LOOSE_V<Access::TraitHooks, T, ReaderFor<Ar>>
	       || HAS_VISIT_LOOSE_V<Access::TraitHooks, const T, WriterFor<Ar>>
	       || HAS_VISIT_LOOSE_V<Access::MemberHooks, T, ReaderFor<Ar>>
	       || HAS_VISIT_LOOSE_V<Access::MemberHooks, const T, WriterFor<Ar>>
	       || HAS_VISIT_LOOSE_V<AdlHooks, T, ReaderFor<Ar>>
	       || HAS_VISIT_LOOSE_V<AdlHooks, const T, WriterFor<Ar>>)
	   && !(ANY_VISIT_READ_V<T, Ar> || ANY_VISIT_WRITE_V<T, Ar>);

	/**
	 * @brief a hook this archive cannot reach
	 * @details The name is declared but the archive in use cannot call it, so the hook is there and
	 * being ignored - a plain function pinned to some other concrete archive type, and
	 * dispatch walks past it into the builtin or automatic path. It cannot see an overload
	 * set, a template constrained to one archive, or an ADL free function, so only the two
	 * levels that have a name to look up are asked.
	 */
	template<class T, class Ar>
	inline constexpr bool NONGENERIC_WRITE_HOOK_V
		= (Access::NAMES_TRAIT_WRITE_V<T> && !LVL_WRITE_V<Access::TraitHooks, T, Ar>)
	   || (Access::NAMES_MEMBER_WRITE_V<T> && !LVL_WRITE_V<Access::MemberHooks, T, Ar>);

	template<class T, class Ar>
	inline constexpr bool NONGENERIC_READ_HOOK_V
		= (Access::NAMES_TRAIT_READ_V<T> && !LVL_READ_V<Access::TraitHooks, T, Ar>)
	   || (Access::NAMES_MEMBER_READ_V<T> && !LVL_READ_V<Access::MemberHooks, T, Ar>);

	template<class T, class Ar>
	inline constexpr bool NONGENERIC_MAKE_HOOK_V
		= (Access::NAMES_TRAIT_MAKE_V<T> && !LVL_MAKE_V<Access::TraitHooks, T, Ar>)
	   || (Access::NAMES_MEMBER_MAKE_V<T> && !LVL_MAKE_V<Access::MemberHooks, T, Ar>);

	template<class T, class Ar>
	inline constexpr bool NONGENERIC_VISIT_HOOK_V
		= (Access::NAMES_TRAIT_VISIT_V<T>
	       && !(LVL_VISIT_WRITE_V<Access::TraitHooks, T, Ar> || LVL_VISIT_READ_V<Access::TraitHooks, T, Ar>)
	      )
	   || (Access::NAMES_MEMBER_VISIT_V<T>
	       && !(LVL_VISIT_WRITE_V<Access::MemberHooks, T, Ar> || LVL_VISIT_READ_V<Access::MemberHooks, T, Ar>)
	   );

	/** @brief a hook that is not static */
	template<class T, class Ar>
	inline constexpr bool NONSTATIC_VISIT_HOOK_V
		= Access::CALLS_MEMBER_VISIT_V<T, ReaderFor<Ar>>
	   && !(LVL_VISIT_READ_V<Access::MemberHooks, T, Ar>
	        || LVL_VISIT_WRITE_V<Access::MemberHooks, T, Ar>);

	template<class T, class Ar>
	inline constexpr bool NONSTATIC_WRITE_HOOK_V = Access::CALLS_MEMBER_WRITE_V<T, WriterFor<Ar>>
	                                            && !LVL_WRITE_V<Access::MemberHooks, T, Ar>;

	template<class T, class Ar>
	inline constexpr bool NONSTATIC_READ_HOOK_V
		= Access::CALLS_MEMBER_READ_V<T, ReaderFor<Ar>> && !LVL_READ_V<Access::MemberHooks, T, Ar>;

	template<class T, class Ar>
	inline constexpr bool NONSTATIC_MAKE_HOOK_V
		= Access::CALLS_MEMBER_MAKE_V<T, ReaderFor<Ar>> && !LVL_MAKE_V<Access::MemberHooks, T, Ar>;

	/**
	 * @brief Any of the four. Dispatch uses it to stay quiet: once checkHooks has said "the
	 * hook you wrote is being ignored", a second complaint that the type cannot be
	 * serialized at all is noise pointing away from the cause.
	 */
	template<class T, class Ar>
	inline constexpr bool NONGENERIC_ANY_HOOK_V
		= NONGENERIC_VISIT_HOOK_V<T, Ar> || NONGENERIC_WRITE_HOOK_V<T, Ar>
	   || NONGENERIC_READ_HOOK_V<T, Ar> || NONGENERIC_MAKE_HOOK_V<T, Ar>
	   || NONSTATIC_VISIT_HOOK_V<T, Ar> || NONSTATIC_WRITE_HOOK_V<T, Ar>
	   || NONSTATIC_READ_HOOK_V<T, Ar> || NONSTATIC_MAKE_HOOK_V<T, Ar>;

	/*
	 * checkHooks<T, Ar>()
	 * Called at the top of all three dispatch contexts. Everything here is a compile-time
	 * diagnostic and produces no code; the order matters, because the most specific message
	 * about a hook that exists has to come before the vaguer ones about hooks that do not.
	 *
	 * `serMake` with `serRead` is allowed - they answer different questions. `serVisit`
	 * with `serWrite` is not: both answer "write this one", and nothing says which format
	 * was meant.
	 */

	/**
	 * @brief A visit hook that binds only a non-const object is found when reading and missed
	 * when writing, so writing would silently fall through to the builtin or automatic
	 * path and the two directions would disagree about the format.
	 */
#define SER_INTERNAL_ASSERT_VISIT_TAKES_CONST(L, WHAT, SIGNATURE)                      \
	static_assert(                                                                     \
		!(LVL_VISIT_READ_V<L, T, Ar> && !LVL_VISIT_WRITE_V<L, T, Ar>),                 \
		"ser: " WHAT                                                                   \
		" accepts a non-const object only, so it is invisible when "                   \
		"writing and the two directions would use different formats. Take the object " \
		"as a deduced reference: " SIGNATURE                                           \
	);

	/** @brief Both answer "write this one", and nothing says which format was meant. */
#define SER_INTERNAL_ASSERT_VISIT_NOT_PAIRED(L, WHAT, KEEP)              \
	static_assert(                                                       \
		!((LVL_VISIT_WRITE_V<L, T, Ar> || LVL_VISIT_READ_V<L, T, Ar>)    \
	      && (LVL_WRITE_V<L, T, Ar> || LVL_READ_V<L, T, Ar>) ),          \
		"ser: " WHAT                                                     \
		". They answer the same question and nothing says which format " \
		"was meant. Keep " KEEP                                          \
	);

	/**
	 * @brief A hook that reads into a copy: the call succeeds, returns Errc::Ok, and the caller's
	 * object is exactly as it was. One missing `&`.
	 */
#define SER_INTERNAL_ASSERT_READ_FILLS(L, WHAT, SIGNATURE)                                 \
	static_assert(                                                                         \
		!(LVL_READ_V<L, T, Ar> && LVL_READ_BY_COPY_V<L, T, Ar>),                           \
		"ser: " WHAT                                                                       \
		" takes the object BY VALUE, or by const reference, so reading fills a copy that " \
		"is thrown away - the call returns Errc::Ok and the caller's object is "           \
		"untouched. Take a mutable reference: " SIGNATURE                                  \
	);

	/**
	 * @brief Not static, so no detector can name it and dispatch never finds it. A virtual method
	 * that only plays the part of a hook is the one legitimate reason to have the name
	 * here, which is why the message offers the rename.
	 */
#define SER_INTERNAL_ASSERT_HOOK_STATIC(COND, WHAT, SIGNATURE)                         \
	static_assert(                                                                     \
		!(COND),                                                                       \
		"ser: " WHAT                                                                   \
		" is not static, so nothing can name it and dispatch does not find it - the "  \
		"type is serialized as if the hook were not there. Make it static: " SIGNATURE \
		"\n  If it is a virtual method that only plays the part of a hook, give it "   \
		"another name - ser reserves serVisit, serWrite, serRead and serMake."         \
	);

	template<class T, class Ar>
	constexpr void checkHooks() {
		/*
		 * A hook nothing can name comes first: every message below it describes a hook
		 * that at least exists as far as the library is concerned.
		 */
		SER_INTERNAL_ASSERT_HOOK_STATIC(
			(NONSTATIC_VISIT_HOOK_V<T, Ar>),
			"this type's serVisit",
			"static ser::Errc serVisit(ser::ReaderOrWriter auto& ar, auto& self) { return "
			"ar(self.a, self.b); }"
		)
		SER_INTERNAL_ASSERT_HOOK_STATIC(
			(NONSTATIC_WRITE_HOOK_V<T, Ar>),
			"this type's serWrite",
			"static ser::Errc serWrite(ser::Writer auto& ar, const T& x) { return ar(x.a); }"
		)
		SER_INTERNAL_ASSERT_HOOK_STATIC(
			(NONSTATIC_READ_HOOK_V<T, Ar>),
			"this type's serRead",
			"static ser::Errc serRead(ser::Reader auto& ar, T& x) { return ar(x.a); }"
		)
		SER_INTERNAL_ASSERT_HOOK_STATIC(
			(NONSTATIC_MAKE_HOOK_V<T, Ar>),
			"this type's serMake",
			"static T serMake(ser::Reader auto& ar) { return T{ ser::subMake<A>(ar) }; }"
			"\n  There is no object yet when serMake runs, so a non-static one could never "
			"be called at all."
		)

		/*
		 * A hook the archive in use cannot call is a hook that silently does nothing;
		 * everything below it is about hooks that are actually being used.
		 */
		static_assert(
			!NONGENERIC_VISIT_HOOK_V<T, Ar>,
			"ser: this type declares a visit hook that the archive in use cannot call, so "
			"it is being ignored. Make it a template on the archive:\n"
			"  static ser::Errc serVisit(ser::ReaderOrWriter auto& ar, auto& self) { return "
			"ar(self.a, self.b); }"
		);

		static_assert(
			!NONGENERIC_WRITE_HOOK_V<T, Ar>,
			"ser: this type declares a write hook that the archive in use cannot call, so "
			"it is being ignored. Make it a template on the archive:\n"
			"  static ser::Errc serWrite(ser::Writer auto& ar, const T& x) { return ar(x.a, x.b); "
			"}"
		);

		static_assert(
			!NONGENERIC_READ_HOOK_V<T, Ar>,
			"ser: this type declares a read hook that the archive in use cannot call, so "
			"it is being ignored. Make it a template on the archive:\n"
			"  static ser::Errc serRead(ser::Reader auto& ar, T& x) { return ar(x.a, x.b); }"
		);

		static_assert(
			!NONGENERIC_MAKE_HOOK_V<T, Ar>,
			"ser: this type declares a make hook that the archive in use cannot call, so "
			"it is being ignored. Make it a template on the archive:\n"
			"  static T serMake(ser::Reader auto& ar) { return T{ ser::subMake<A>(ar) }; }"
		);

		static_assert(
			!VISIT_WRONG_RETURN_V<T, Ar>,
			"ser: a serVisit hook for this type exists but does not return ser::Errc.\n"
			"  in the class:  static ser::Errc serVisit(ser::ReaderOrWriter auto& ar, auto& "
			"self) { return ar(self.a, "
			"self.b); }\n"
			"  by ADL:        ser::Errc serVisit(ser::ReaderOrWriter auto& ar, auto& self)\n"
			"  by trait:      ser::Serializer<T>::visit, same signature"
		);

		static_assert(
			!WRITE_WRONG_RETURN_V<T, Ar>,
			"ser: a serWrite hook for this type exists but does not return ser::Errc.\n"
			"  static ser::Errc serWrite(ser::Writer auto& ar, const T& x) { return ar(x.a, x.b); "
			"}"
		);

		static_assert(
			!READ_WRONG_RETURN_V<T, Ar>,
			"ser: a serRead hook for this type exists but does not return ser::Errc.\n"
			"  static ser::Errc serRead(ser::Reader auto& ar, T& x) { return ar(x.a, x.b); }"
		);

		static_assert(
			!MAKE_WRONG_RETURN_V<T, Ar>,
			"ser: a serMake hook for this type exists but does not return T by value.\n"
			"  static T serMake(ser::Reader auto& ar) { return T{ ser::subMake<A>(ar), "
			"ser::subMake<B>(ar) }; }\n"
			"  Braces, not parentheses - the order in which constructor arguments are "
			"evaluated is unspecified, and that would make the byte order compiler-dependent."
		);

		SER_INTERNAL_ASSERT_READ_FILLS(
			Access::TraitHooks,
			"serializer<T>::read",
			"static ser::Errc read(ser::Reader auto& ar, T& x)."
		)
		SER_INTERNAL_ASSERT_READ_FILLS(
			Access::MemberHooks,
			"T::serRead",
			"static ser::Errc serRead(ser::Reader auto& ar, T& x)."
		)
		SER_INTERNAL_ASSERT_READ_FILLS(
			AdlHooks,
			"the ADL serRead for this type",
			"ser::Errc serRead(ser::Reader auto& ar, T& x)."
		)
		SER_INTERNAL_ASSERT_VISIT_TAKES_CONST(
			Access::TraitHooks,
			"serializer<T>::visit",
			"static ser::Errc visit(auto& ar, auto& self)."
		)
		SER_INTERNAL_ASSERT_VISIT_TAKES_CONST(
			Access::MemberHooks,
			"T::serVisit",
			"static ser::Errc serVisit(ser::ReaderOrWriter auto& ar, auto& self)."
		)
		SER_INTERNAL_ASSERT_VISIT_TAKES_CONST(
			AdlHooks,
			"the ADL serVisit for this type",
			"ser::Errc serVisit(ser::ReaderOrWriter auto& ar, auto& self)."
		)

		SER_INTERNAL_ASSERT_VISIT_NOT_PAIRED(
			Access::TraitHooks,
			"serializer<T> declares both visit and write/read",
			"visit for a symmetric format, or the write/read pair for an asymmetric one."
		)
		SER_INTERNAL_ASSERT_VISIT_NOT_PAIRED(
			Access::MemberHooks,
			"this type declares both serVisit and serWrite/serRead",
			"serVisit for a symmetric format, or the serWrite/serRead pair for an "
			"asymmetric one."
		)
		SER_INTERNAL_ASSERT_VISIT_NOT_PAIRED(
			AdlHooks,
			"this type has both an ADL serVisit and an ADL serWrite/serRead",
			"serVisit for a symmetric format, or the pair for an asymmetric one."
		)

		/**
		 * @brief Both pairing rules go quiet when a hook is being ignored: with the hook out of
		 * sight the type looks half-serializable, and saying so would send the reader
		 * looking for a missing hook instead of at the one they wrote.
		 */
		if constexpr (!NONGENERIC_ANY_HOOK_V<T, Ar>) {
			static_assert(
				!(HAS_ANY_WRITE_HOOK_V<T, Ar> && !HAS_ANY_READ_HOOK_V<T, Ar>),
				"ser: this type can be written but never read back - it declares a write hook "
				"and no matching read hook. Add serRead(ar, x) to fill an existing object, or "
				"serMake(ar) to build a fresh one; a type with const fields or no default "
				"constructor needs serMake."
			);

			static_assert(
				!(HAS_ANY_READ_HOOK_V<T, Ar> && !HAS_ANY_WRITE_HOOK_V<T, Ar>),
				"ser: this type can be read but never written - it declares a read hook and no "
				"matching write hook. Add serWrite(ar, x), or use serVisit for both "
				"directions at once."
			);
		}
	}

} /* namespace ser::internal */
