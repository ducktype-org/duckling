#pragma once

#include <ser/archive/in.hpp>
#include <ser/archive/out.hpp>
#include <ser/concepts.hpp>
#include <ser/detail/adl.hpp>
#include <ser/detail/hooks.hpp>
#include <ser/detail/ladder.hpp>
#include <ser/detail/meta.hpp>
#include <ser/errc.hpp>
#include <ser/serializer.hpp>

#include <concepts>
#include <cstddef>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ser {

	// ── ser::members<N> ───────────────────────────────────────────────────────
	// The opt-in field count. It skips the counting phase and, together with
	// `friend ser::access`, is what lets the structured-bindings ladder see private
	// fields. A wrong N is a compile error, so the format cannot drift silently.
	template<::std::size_t N>
	struct members {
		static constexpr ::std::size_t COUNT = N;
	};

	// ── ser::access ───────────────────────────────────────────────────────────
	// Every in-class hook detector lives HERE rather than in ser::detail because access
	// control is part of SFINAE: a detector written in ser::detail reports false for a
	// private serVisit that ser::access can call perfectly well. The trait-level detectors
	// have no access problem and live here anyway, so the whole priority table reads in one
	// place.
	//
	// T carries the cv-qualification of the object for the symmetric `visit` form, so a hook
	// binding only a non-const object is invisible on the write side - which
	// detail::checkHooks reports instead of letting the two directions use different formats.
	struct access {
		// ── level 1: ser::serializer<T> ───────────────────────────────────────
		SER_DETAIL_HOOK_VISIT(
			static constexpr, TRAIT, serializer<::std::remove_cvref_t<T>>::visit(ar, x)
		)
		SER_DETAIL_HOOK_WRITE(
			static constexpr, TRAIT, serializer<::std::remove_cvref_t<T>>::write(ar, x)
		)
		SER_DETAIL_HOOK_READ(
			static constexpr, TRAIT, serializer<::std::remove_cvref_t<T>>::read(ar, x)
		)
		SER_DETAIL_HOOK_MAKE(static constexpr, TRAIT, serializer<::std::remove_cvref_t<T>>::make(ar))
		SER_DETAIL_HOOK_READ_BY_COPY(
			static constexpr, TRAIT, serializer<::std::remove_cvref_t<T>>::read(ar, ::std::move(x))
		)

		// ── level 2: hooks declared in the class ──────────────────────────────
		SER_DETAIL_HOOK_VISIT(static constexpr, MEMBER, ::std::remove_cvref_t<T>::serVisit(ar, x))
		SER_DETAIL_HOOK_WRITE(static constexpr, MEMBER, ::std::remove_cvref_t<T>::serWrite(ar, x))
		SER_DETAIL_HOOK_READ(static constexpr, MEMBER, ::std::remove_cvref_t<T>::serRead(ar, x))
		SER_DETAIL_HOOK_MAKE(static constexpr, MEMBER, ::std::remove_cvref_t<T>::serMake(ar))
		SER_DETAIL_HOOK_READ_BY_COPY(
			static constexpr, MEMBER, ::std::remove_cvref_t<T>::serRead(ar, ::std::move(x))
		)

		// ── "the name is there, but this archive cannot call it" ──────────────
		SER_DETAIL_HOOK_NAMED(
			static constexpr, TRAIT_VISIT, serializer<::std::remove_cvref_t<T>>::visit
		)
		SER_DETAIL_HOOK_NAMED(
			static constexpr, TRAIT_WRITE, serializer<::std::remove_cvref_t<T>>::write
		)
		SER_DETAIL_HOOK_NAMED(
			static constexpr, TRAIT_READ, serializer<::std::remove_cvref_t<T>>::read
		)
		SER_DETAIL_HOOK_NAMED(
			static constexpr, TRAIT_MAKE, serializer<::std::remove_cvref_t<T>>::make
		)

		SER_DETAIL_HOOK_NAMED(static constexpr, MEMBER_VISIT, ::std::remove_cvref_t<T>::serVisit)
		SER_DETAIL_HOOK_NAMED(static constexpr, MEMBER_WRITE, ::std::remove_cvref_t<T>::serWrite)
		SER_DETAIL_HOOK_NAMED(static constexpr, MEMBER_READ, ::std::remove_cvref_t<T>::serRead)
		SER_DETAIL_HOOK_NAMED(static constexpr, MEMBER_MAKE, ::std::remove_cvref_t<T>::serMake)

		// ── "the hook is there, but it is not static" ─────────────
		template<class T, class Ar>
		static constexpr bool CALLS_MEMBER_VISIT_V = requires(Ar& ar, ::std::remove_cvref_t<T>& t) {
			t.serVisit(ar);
		} || requires(Ar& ar, ::std::remove_cvref_t<T>& t) { t.serVisit(ar, t); };

		template<class T, class Ar>
		static constexpr bool CALLS_MEMBER_WRITE_V
			= requires(Ar& ar, const ::std::remove_cvref_t<T>& c) { c.serWrite(ar); }
		   || requires(Ar& ar, const ::std::remove_cvref_t<T>& c) { c.serWrite(ar, c); }
		   || requires(Ar& ar, ::std::remove_cvref_t<T>& t) { t.serWrite(ar); };

		template<class T, class Ar>
		static constexpr bool CALLS_MEMBER_READ_V = requires(Ar& ar, ::std::remove_cvref_t<T>& t) {
			t.serRead(ar);
		} || requires(Ar& ar, ::std::remove_cvref_t<T>& t) { t.serRead(ar, t); };

		template<class T, class Ar>
		static constexpr bool CALLS_MEMBER_MAKE_V
			= requires(Ar& ar, ::std::remove_cvref_t<T>& t) { t.serMake(ar); };

		// ── the calls ─────────────────────────────────────────────────────────
		// These are the reason a private hook works at all: the call site is inside the
		// class the type befriended. dispatch never names T::ser* directly.
		template<class T, class Ar>
		static constexpr Errc callVisit(Ar& ar, T& x) {
			return ::std::remove_cvref_t<T>::serVisit(ar, x);
		}

		template<class T, class Ar>
		static constexpr Errc callWrite(Ar& ar, const T& x) {
			return T::serWrite(ar, x);
		}

		template<class T, class Ar>
		static constexpr Errc callRead(Ar& ar, T& x) {
			return T::serRead(ar, x);
		}

		// T first: there is no argument to deduce it from.
		template<class T, class Ar>
		static constexpr T callMake(Ar& ar) {
			return T::serMake(ar);
		}

		// ── the opt-in field count ────────────────────────────────────────────
		// `using ser_members = ser::members<2>;` inside the class, private if you like -
		// which is why the detector is here. It exists because COUNTING and DECOMPOSING
		// have different requirements: decomposition works on any class whose members are
		// accessible here, while the count is probed with aggregate initialization, which a
		// non-aggregate does not support.
		template<class T>
		static constexpr bool NAMES_MEMBER_COUNT_V
			= requires { ::std::remove_cvref_t<T>::ser_members::COUNT; };

		template<class T>
		static constexpr ::std::size_t declaredMemberCount() {
			return ::std::remove_cvref_t<T>::ser_members::COUNT;
		}

		// ── what SER_DESCRIBE left behind ─────────────────────────────────────
		// SER_DESCRIBE is usually written in a private section, so a detector in ser::detail
		// reports false for it and the caller then compares EVERY member - including the
		// ones the description deliberately left out, which come back default-constructed.
		template<class T>
		static constexpr bool HAS_DESCRIBED_V = requires(const ::std::remove_cvref_t<T>& x) {
			::std::remove_cvref_t<T>::ser_described(x);
		};

		template<class T>
		static constexpr auto described(const T& x) {
			return ::std::remove_cvref_t<T>::ser_described(x);
		}

		template<class T>
		static constexpr bool HAS_FIELD_NAMES_V
			= requires { ::std::remove_cvref_t<T>::ser_field_names; };

		template<class T>
		static constexpr const char* fieldName(::std::size_t i) {
			return ::std::remove_cvref_t<T>::ser_field_names[i];
		}

		template<class T>
		static constexpr bool HAS_SCHEMA_AS_V
			= requires { typename ::std::remove_cvref_t<T>::ser_schema_as; };

		template<class T>
		using schema_as_t = typename ::std::remove_cvref_t<T>::ser_schema_as;

		template<class T>
		static constexpr bool HAS_SCHEMA_NAME_V = requires {
			{ ::std::remove_cvref_t<T>::ser_schema_name } -> ::std::convertible_to<const char*>;
		};

		template<class T>
		static constexpr const char* schemaName() {
			return ::std::remove_cvref_t<T>::ser_schema_name;
		}

		// ── the structured-bindings ladder ────────────────────────────────────
		// Calls f with every member of obj as an lvalue. The arity is a template
		// parameter rather than something computed here, so this stays free of
		// describe_bind.hpp and the include graph has no cycle: counting needs the
		// ladder (member_type_t), the ladder needs nothing.
		template<::std::size_t N, class T, class F>
		// The ladder binds obj and calls f in place; forwarding either would change
		// what the members bind as, which is the whole contract of the walk.
		// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
		static constexpr decltype(auto) visitMembersN(T&& obj, F&& f) {
			static_assert(
				N <= ::ser::detail::LADDER_MAX,
				"ser: this type has more than 64 members, which is the limit of the "
				"structured-bindings ladder. Split it, or give it a serVisit hook."
			);
			if constexpr (N == 0) {
				static_assert(
					::std::is_empty_v<::std::remove_cvref_t<T>>,
					"ser: this type declares zero members - `using ser_members = "
					"ser::members<0>;` - but it has data. That format writes no bytes and a "
					"read leaves every field default-constructed, silently. Declare the real "
					"count, or drop the declaration and let the walk count the fields."
				);
				(void) obj;
				return f();
			}
			SER_DETAIL_LADDER(SER_DETAIL_LADDER_WALK)
		}

		// "Can a non-const lvalue reference bind to this?" Never defined, and used only
		// inside a requires-expression, so nothing is ever called. One thing answers no:
		// a bit-field, for which the candidate is simply not viable - a substitution
		// failure rather than a hard error.
		template<class X>
		static void bindProbe(X&);  // NOT defined, on purpose

		// The declared type of every field, which only a structured binding can still
		// see - see the note on field_decl in detail/meta.hpp. Never called either:
		// callers ask for decltype of it.
		template<::std::size_t N, class T>
		// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
		static constexpr auto fieldDeclsN(T&& obj) {
			static_assert(
				N <= ::ser::detail::LADDER_MAX,
				"ser: this type has more than 64 members, which is the limit of the "
				"structured-bindings ladder. Split it, or give it a serVisit hook."
			);
			if constexpr (N == 0) {
				(void) obj;
				return ::ser::detail::type_list<>{};
			}
			SER_DETAIL_LADDER(SER_DETAIL_LADDER_DECLS)
		}
	};

// A type opts in with one line. `friend ser::access;` works just as well; the macro
// exists so the spelling does not have to be remembered.
#define SER_FRIEND friend struct ::ser::access;

}  // namespace ser

namespace ser::detail {

	// ── the types SER_DESCRIBE named ─────────
	template<class T>
	struct tuple_field_list;

	template<class... Es>
	struct tuple_field_list<::std::tuple<Es...>> {
		using type = type_list<::std::remove_cvref_t<Es>...>;
	};

	template<class T>
	using described_types_t = typename tuple_field_list<
		decltype(access::described(::std::declval<const ::std::remove_cv_t<T>&>()))>::type;

	// ── the other direction of an archive ─────────────────────────────────────
	// Half the questions checkHooks asks are about the direction it is NOT in - from
	// dispatchWrite, whether the type can be read back - and answering one needs a REAL
	// archive carrying the same Ctx, so a hook reaching for ar.pool<P>() compiles during
	// detection.
	//
	// ser::in<Ctx> is exact. The writer is exact only in its context, the buffer being the
	// caller's choice; that can matter only for a hook pinned to one concrete
	// out<Buf, Ctx>, which NONGENERIC_*_HOOK_V reports anyway.
	template<class Ar>
	using reader_for = ::std::conditional_t<reader<Ar>, Ar, in<typename Ar::context_type>>;

	template<class Ar>
	using writer_for
		= ::std::conditional_t<writer<Ar>, Ar, out<::std::span<::std::byte>, typename Ar::context_type>>;

	// ── per-level summaries ───────────────────────────────────────────────────
	// Every one of them asks with a real archive, so "does this type declare a hook" and
	// "which rung will dispatch take" are the same question asked twice.
	SER_DETAIL_HOOK_SUMMARY(TRAIT, access::)
	SER_DETAIL_HOOK_SUMMARY(MEMBER, access::)
	SER_DETAIL_HOOK_SUMMARY(ADL, )

	// ── does this type describe its own format? ───────────────────────────────
	// Either direction, any level, any form. This is what keeps a type with its own
	// serializer from ever being decomposed field by field. It takes Ar because the answer
	// only means anything relative to the archive that will do the work.
	template<class T, class Ar>
	inline constexpr bool HAS_ANY_WRITE_HOOK_V
		= SER_DETAIL_HOOK_ANY_LEVEL(WRITE) || SER_DETAIL_HOOK_ANY_LEVEL(VISIT_WRITE);

	template<class T, class Ar>
	inline constexpr bool HAS_ANY_READ_HOOK_V
		= SER_DETAIL_HOOK_ANY_LEVEL(READ) || SER_DETAIL_HOOK_ANY_LEVEL(MAKE)
	   || SER_DETAIL_HOOK_ANY_LEVEL(VISIT_READ);

	template<class T, class Ar>
	inline constexpr bool HAS_ANY_MAKE_HOOK_V = SER_DETAIL_HOOK_ANY_LEVEL(MAKE);

	// Every form of every level: the union of the two directions.
	template<class T, class Ar>
	inline constexpr bool HAS_CUSTOM_SERIALIZER_V
		= HAS_ANY_WRITE_HOOK_V<T, Ar> || HAS_ANY_READ_HOOK_V<T, Ar>;

	// ── wrong return type ─────────────────────────────────────────────────────
	SER_DETAIL_HOOK_WRONG_RETURN(WRITE, writer_for)
	SER_DETAIL_HOOK_WRONG_RETURN(READ, reader_for)
	SER_DETAIL_HOOK_WRONG_RETURN(MAKE, reader_for)

	// visit is asked in both directions, so it cannot go through the macro above.
	template<class T, class Ar>
	inline constexpr bool VISIT_WRONG_RETURN_V
		= (access::HAS_TRAIT_VISIT_LOOSE_V<T, reader_for<Ar>>
	       || access::HAS_TRAIT_VISIT_LOOSE_V<const T, writer_for<Ar>>
	       || access::HAS_MEMBER_VISIT_LOOSE_V<T, reader_for<Ar>>
	       || access::HAS_MEMBER_VISIT_LOOSE_V<const T, writer_for<Ar>>
	       || HAS_ADL_VISIT_LOOSE_V<T, reader_for<Ar>>
	       || HAS_ADL_VISIT_LOOSE_V<const T, writer_for<Ar>>)
	   && !(SER_DETAIL_HOOK_ANY_LEVEL(VISIT_READ) || SER_DETAIL_HOOK_ANY_LEVEL(VISIT_WRITE));

	// ── a hook this archive cannot reach ──────────────────────────────────────
	FOR_EACH(SER_DETAIL_HOOK_NONGENERIC, WRITE, READ, MAKE)

	template<class T, class Ar>
	inline constexpr bool NONGENERIC_VISIT_HOOK_V
		= (access::NAMES_TRAIT_VISIT_V<T>
	       && !(TRAIT_VISIT_WRITE_V<T, Ar> || TRAIT_VISIT_READ_V<T, Ar>) )
	   || (access::NAMES_MEMBER_VISIT_V<T>
	       && !(MEMBER_VISIT_WRITE_V<T, Ar> || MEMBER_VISIT_READ_V<T, Ar>) );

	// ── a hook that is not static ─────────────────────────────────────────────
	template<class T, class Ar>
	inline constexpr bool NONSTATIC_VISIT_HOOK_V
		= access::CALLS_MEMBER_VISIT_V<T, reader_for<Ar>>
	   && !(MEMBER_VISIT_READ_V<T, Ar> || MEMBER_VISIT_WRITE_V<T, Ar>);

	template<class T, class Ar>
	inline constexpr bool NONSTATIC_WRITE_HOOK_V
		= access::CALLS_MEMBER_WRITE_V<T, writer_for<Ar>> && !MEMBER_WRITE_V<T, Ar>;

	template<class T, class Ar>
	inline constexpr bool NONSTATIC_READ_HOOK_V
		= access::CALLS_MEMBER_READ_V<T, reader_for<Ar>> && !MEMBER_READ_V<T, Ar>;

	template<class T, class Ar>
	inline constexpr bool NONSTATIC_MAKE_HOOK_V
		= access::CALLS_MEMBER_MAKE_V<T, reader_for<Ar>> && !MEMBER_MAKE_V<T, Ar>;

	// Any of the four. Dispatch uses it to stay quiet: once checkHooks has said "the
	// hook you wrote is being ignored", a second complaint that the type cannot be
	// serialized at all is noise pointing away from the cause.
	template<class T, class Ar>
	inline constexpr bool NONGENERIC_ANY_HOOK_V
		= NONGENERIC_VISIT_HOOK_V<T, Ar> || NONGENERIC_WRITE_HOOK_V<T, Ar>
	   || NONGENERIC_READ_HOOK_V<T, Ar> || NONGENERIC_MAKE_HOOK_V<T, Ar>
	   || NONSTATIC_VISIT_HOOK_V<T, Ar> || NONSTATIC_WRITE_HOOK_V<T, Ar>
	   || NONSTATIC_READ_HOOK_V<T, Ar> || NONSTATIC_MAKE_HOOK_V<T, Ar>;

	// ── checkHooks<T, Ar>() ──────────────────────────────────────────────────
	// Called at the top of all three dispatch contexts. Everything here is a compile-time
	// diagnostic and produces no code; the order matters, because the most specific message
	// about a hook that exists has to come before the vaguer ones about hooks that do not.
	//
	// `serMake` with `serRead` is allowed - they answer different questions. `serVisit`
	// with `serWrite` is not: both answer "write this one", and nothing says which format
	// was meant.

	// A visit hook that binds only a non-const object is found when reading and missed
	// when writing, so writing would silently fall through to the builtin or automatic
	// path and the two directions would disagree about the format.
#define SER_DETAIL_ASSERT_VISIT_TAKES_CONST(LEVEL, WHAT, SIGNATURE)                    \
	static_assert(                                                                     \
		!(LEVEL##_VISIT_READ_V<T, Ar> && !LEVEL##_VISIT_WRITE_V<T, Ar>),               \
		"ser: " WHAT                                                                   \
		" accepts a non-const object only, so it is invisible when "                   \
		"writing and the two directions would use different formats. Take the object " \
		"as a deduced reference: " SIGNATURE                                           \
	);

	// Both answer "write this one", and nothing says which format was meant.
#define SER_DETAIL_ASSERT_VISIT_NOT_PAIRED(LEVEL, WHAT, KEEP)            \
	static_assert(                                                       \
		!((LEVEL##_VISIT_WRITE_V<T, Ar> || LEVEL##_VISIT_READ_V<T, Ar>)  \
	      && (LEVEL##_WRITE_V<T, Ar> || LEVEL##_READ_V<T, Ar>) ),        \
		"ser: " WHAT                                                     \
		". They answer the same question and nothing says which format " \
		"was meant. Keep " KEEP                                          \
	);

	// A hook that reads into a copy: the call succeeds, returns Errc::Ok, and the caller's
	// object is exactly as it was. One missing `&`.
#define SER_DETAIL_ASSERT_READ_FILLS(WHICH, WHAT, SIGNATURE)                               \
	static_assert(                                                                         \
		!(WHICH##_V<T, Ar> && WHICH##_BY_COPY_V<T, Ar>),                                   \
		"ser: " WHAT                                                                       \
		" takes the object BY VALUE, or by const reference, so reading fills a copy that " \
		"is thrown away - the call returns Errc::Ok and the caller's object is "           \
		"untouched. Take a mutable reference: " SIGNATURE                                  \
	);

	// Not static, so no detector can name it and dispatch never finds it. A virtual method
	// that only plays the part of a hook is the one legitimate reason to have the name
	// here, which is why the message offers the rename.
#define SER_DETAIL_ASSERT_HOOK_STATIC(FORM, WHAT, SIGNATURE)                           \
	static_assert(                                                                     \
		!NONSTATIC_##FORM##_HOOK_V<T, Ar>,                                             \
		"ser: " WHAT                                                                   \
		" is not static, so nothing can name it and dispatch does not find it - the "  \
		"type is serialized as if the hook were not there. Make it static: " SIGNATURE \
		"\n  If it is a virtual method that only plays the part of a hook, give it "   \
		"another name - ser reserves serVisit, serWrite, serRead and serMake."         \
	);

	template<class T, class Ar>
	constexpr void checkHooks() {
		// A hook nothing can name comes first: every message below it describes a hook
		// that at least exists as far as the library is concerned.
		SER_DETAIL_ASSERT_HOOK_STATIC(
			VISIT,
			"this type's serVisit",
			"static ser::Errc serVisit(auto& ar, auto& self) { return ar(self.a, self.b); }"
		)
		SER_DETAIL_ASSERT_HOOK_STATIC(
			WRITE,
			"this type's serWrite",
			"static ser::Errc serWrite(ser::writer auto& ar, const T& x) { return ar(x.a); }"
		)
		SER_DETAIL_ASSERT_HOOK_STATIC(
			READ,
			"this type's serRead",
			"static ser::Errc serRead(ser::reader auto& ar, T& x) { return ar(x.a); }"
		)
		SER_DETAIL_ASSERT_HOOK_STATIC(
			MAKE,
			"this type's serMake",
			"static T serMake(ser::reader auto& ar) { return T{ ser::readField<A>(ar) }; }"
			"\n  There is no object yet when serMake runs, so a non-static one could never "
			"be called at all."
		)

		// A hook the archive in use cannot call is a hook that silently does nothing;
		// everything below it is about hooks that are actually being used.
		static_assert(
			!NONGENERIC_VISIT_HOOK_V<T, Ar>,
			"ser: this type declares a visit hook that the archive in use cannot call, so "
			"it is being ignored. Make it a template on the archive:\n"
			"  static ser::Errc serVisit(auto& ar, auto& self) { return ar(self.a, self.b); }"
		);

		static_assert(
			!NONGENERIC_WRITE_HOOK_V<T, Ar>,
			"ser: this type declares a write hook that the archive in use cannot call, so "
			"it is being ignored. Make it a template on the archive:\n"
			"  static ser::Errc serWrite(ser::writer auto& ar, const T& x) { return ar(x.a, x.b); "
			"}"
		);

		static_assert(
			!NONGENERIC_READ_HOOK_V<T, Ar>,
			"ser: this type declares a read hook that the archive in use cannot call, so "
			"it is being ignored. Make it a template on the archive:\n"
			"  static ser::Errc serRead(ser::reader auto& ar, T& x) { return ar(x.a, x.b); }"
		);

		static_assert(
			!NONGENERIC_MAKE_HOOK_V<T, Ar>,
			"ser: this type declares a make hook that the archive in use cannot call, so "
			"it is being ignored. Make it a template on the archive:\n"
			"  static T serMake(ser::reader auto& ar) { return T{ ser::readField<A>(ar) }; }"
		);

		static_assert(
			!VISIT_WRONG_RETURN_V<T, Ar>,
			"ser: a serVisit hook for this type exists but does not return ser::Errc.\n"
			"  in the class:  static ser::Errc serVisit(auto& ar, auto& self) { return ar(self.a, "
			"self.b); }\n"
			"  by ADL:        ser::Errc serVisit(auto& ar, auto& self)\n"
			"  by trait:      ser::serializer<T>::visit, same signature"
		);

		static_assert(
			!WRITE_WRONG_RETURN_V<T, Ar>,
			"ser: a serWrite hook for this type exists but does not return ser::Errc.\n"
			"  static ser::Errc serWrite(ser::writer auto& ar, const T& x) { return ar(x.a, x.b); "
			"}"
		);

		static_assert(
			!READ_WRONG_RETURN_V<T, Ar>,
			"ser: a serRead hook for this type exists but does not return ser::Errc.\n"
			"  static ser::Errc serRead(ser::reader auto& ar, T& x) { return ar(x.a, x.b); }"
		);

		static_assert(
			!MAKE_WRONG_RETURN_V<T, Ar>,
			"ser: a serMake hook for this type exists but does not return T by value.\n"
			"  static T serMake(ser::reader auto& ar) { return T{ ser::readField<A>(ar), "
			"ser::readField<B>(ar) }; }\n"
			"  Braces, not parentheses - the order in which constructor arguments are "
			"evaluated is unspecified, and that would make the byte order compiler-dependent."
		);

		SER_DETAIL_ASSERT_READ_FILLS(
			TRAIT_READ, "serializer<T>::read", "static ser::Errc read(ser::reader auto& ar, T& x)."
		)
		SER_DETAIL_ASSERT_READ_FILLS(
			MEMBER_READ, "T::serRead", "static ser::Errc serRead(ser::reader auto& ar, T& x)."
		)
		SER_DETAIL_ASSERT_READ_FILLS(
			ADL_READ,
			"the ADL serRead for this type",
			"ser::Errc serRead(ser::reader auto& ar, T& x)."
		)
		SER_DETAIL_ASSERT_VISIT_TAKES_CONST(
			TRAIT, "serializer<T>::visit", "static ser::Errc visit(auto& ar, auto& self)."
		)
		SER_DETAIL_ASSERT_VISIT_TAKES_CONST(
			MEMBER, "T::serVisit", "static ser::Errc serVisit(auto& ar, auto& self)."
		)
		SER_DETAIL_ASSERT_VISIT_TAKES_CONST(
			ADL, "the ADL serVisit for this type", "ser::Errc serVisit(auto& ar, auto& self)."
		)

		SER_DETAIL_ASSERT_VISIT_NOT_PAIRED(
			TRAIT,
			"serializer<T> declares both visit and write/read",
			"visit for a symmetric format, or the write/read pair for an asymmetric one."
		)
		SER_DETAIL_ASSERT_VISIT_NOT_PAIRED(
			MEMBER,
			"this type declares both serVisit and serWrite/serRead",
			"serVisit for a symmetric format, or the serWrite/serRead pair for an "
			"asymmetric one."
		)
		SER_DETAIL_ASSERT_VISIT_NOT_PAIRED(
			ADL,
			"this type has both an ADL serVisit and an ADL serWrite/serRead",
			"serVisit for a symmetric format, or the pair for an asymmetric one."
		)

		// Both pairing rules go quiet when a hook is being ignored: with the hook out of
		// sight the type looks half-serializable, and saying so would send the reader
		// looking for a missing hook instead of at the one they wrote.
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

}  // namespace ser::detail
