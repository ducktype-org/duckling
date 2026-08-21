#pragma once

#include <ser/archive/in.hpp>
#include <ser/archive/out.hpp>
#include <ser/concepts.hpp>
#include <ser/detail/adl.hpp>
#include <ser/detail/meta.hpp>
#include <ser/errc.hpp>
#include <ser/serializer.hpp>

#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>

namespace ser {

	// ── ser::members<N> ───────────────────────────────────────────────────────
	// The opt-in field count. It skips the counting phase and, together with
	// `friend ser::access`, is what lets the structured-bindings ladder in block C see
	// private fields. A wrong N is a compile error, so the format cannot drift silently.
	template<::std::size_t N>
	struct members {
		static constexpr ::std::size_t COUNT = N;
	};

	// ── ser::access ───────────────────────────────────────────────────────────
	// Every in-class hook detector lives HERE rather than in ser::detail, and that is not
	// a matter of taste: access control is part of SFINAE, so a detector written in
	// ser::detail reports false for a private serVisit that ser::access can call
	// perfectly well. Detection and call must happen in the same class the type befriends.
	//
	// Trait-level detectors (ser::serializer<T>) have no access problem, but they live
	// here too so that the whole priority table can be read in one place.
	//
	// T carries the cv-qualification of the object for the symmetric `visit` form:
	// dispatchWrite asks for `const U`, dispatchRead for `U`. A hook that only binds a
	// non-const object is then invisible on the write side, which detail::checkHooks
	// reports instead of letting the two directions use different formats.
	struct access {
		// ── level 1: ser::serializer<T> ───────────────────────────────────────
		template<class T, class Ar>
		static constexpr bool HAS_TRAIT_VISIT_V = requires(Ar& ar, T& x) {
			{ serializer<::std::remove_cvref_t<T>>::visit(ar, x) } -> ::std::same_as<Errc>;
		};

		template<class T, class Ar>
		static constexpr bool HAS_TRAIT_WRITE_V
			= requires(Ar& ar, const ::std::remove_cvref_t<T>& x) {
				  { serializer<::std::remove_cvref_t<T>>::write(ar, x) } -> ::std::same_as<Errc>;
			  };

		template<class T, class Ar>
		static constexpr bool HAS_TRAIT_READ_V = requires(Ar& ar, ::std::remove_cvref_t<T>& x) {
			{ serializer<::std::remove_cvref_t<T>>::read(ar, x) } -> ::std::same_as<Errc>;
		};

		template<class T, class Ar>
		static constexpr bool HAS_TRAIT_MAKE_V = requires(Ar& ar) {
			{
				serializer<::std::remove_cvref_t<T>>::make(ar)
			} -> ::std::same_as<::std::remove_cvref_t<T>>;
		};

		// ── level 2: hooks declared in the class ──────────────────────────────
		template<class T, class Ar>
		static constexpr bool HAS_MEMBER_VISIT_V = requires(Ar& ar, T& x) {
			{ ::std::remove_cvref_t<T>::serVisit(ar, x) } -> ::std::same_as<Errc>;
		};

		template<class T, class Ar>
		static constexpr bool HAS_MEMBER_WRITE_V
			= requires(Ar& ar, const ::std::remove_cvref_t<T>& x) {
				  { ::std::remove_cvref_t<T>::serWrite(ar, x) } -> ::std::same_as<Errc>;
			  };

		template<class T, class Ar>
		static constexpr bool HAS_MEMBER_READ_V = requires(Ar& ar, ::std::remove_cvref_t<T>& x) {
			{ ::std::remove_cvref_t<T>::serRead(ar, x) } -> ::std::same_as<Errc>;
		};

		template<class T, class Ar>
		static constexpr bool HAS_MEMBER_MAKE_V = requires(Ar& ar) {
			{ ::std::remove_cvref_t<T>::serMake(ar) } -> ::std::same_as<::std::remove_cvref_t<T>>;
		};

		// ── the same eight without the return-type requirement ────────────────
		// "loose but not strict" means the hook is there and returns the wrong thing -
		// the single most common way to write one, and the one whose default diagnostic
		// ("no way to serialize this type") points nowhere near the cause.
		template<class T, class Ar>
		static constexpr bool HAS_TRAIT_VISIT_LOOSE_V
			= requires(Ar& ar, T& x) { serializer<::std::remove_cvref_t<T>>::visit(ar, x); };
		template<class T, class Ar>
		static constexpr bool HAS_TRAIT_WRITE_LOOSE_V
			= requires(Ar& ar, const ::std::remove_cvref_t<T>& x) {
				  serializer<::std::remove_cvref_t<T>>::write(ar, x);
			  };
		template<class T, class Ar>
		static constexpr bool HAS_TRAIT_READ_LOOSE_V
			= requires(Ar& ar, ::std::remove_cvref_t<T>& x) {
				  serializer<::std::remove_cvref_t<T>>::read(ar, x);
			  };
		template<class T, class Ar>
		static constexpr bool HAS_TRAIT_MAKE_LOOSE_V
			= requires(Ar& ar) { serializer<::std::remove_cvref_t<T>>::make(ar); };

		template<class T, class Ar>
		static constexpr bool HAS_MEMBER_VISIT_LOOSE_V
			= requires(Ar& ar, T& x) { ::std::remove_cvref_t<T>::serVisit(ar, x); };
		template<class T, class Ar>
		static constexpr bool HAS_MEMBER_WRITE_LOOSE_V
			= requires(Ar& ar, const ::std::remove_cvref_t<T>& x) {
				  ::std::remove_cvref_t<T>::serWrite(ar, x);
			  };
		template<class T, class Ar>
		static constexpr bool HAS_MEMBER_READ_LOOSE_V
			= requires(Ar& ar, ::std::remove_cvref_t<T>& x) {
				  ::std::remove_cvref_t<T>::serRead(ar, x);
			  };
		template<class T, class Ar>
		static constexpr bool HAS_MEMBER_MAKE_LOOSE_V
			= requires(Ar& ar) { ::std::remove_cvref_t<T>::serMake(ar); };

		// ── "the name is there, but this archive cannot call it" ──────────────
		// Naming a member without calling it is the one question that needs no archive.
		// An id-expression for a function TEMPLATE cannot be formed without arguments to
		// deduce from, so each of these is true for exactly a single NON-template
		// declaration - a hook pinned to one concrete archive type. Compared against the
		// archive in use, that is how detail::nongeneric_*_hook_v tells a hook that works
		// from a hook that is being silently ignored.
		template<class T>
		static constexpr bool NAMES_TRAIT_VISIT_V
			= requires { serializer<::std::remove_cvref_t<T>>::visit; };
		template<class T>
		static constexpr bool NAMES_TRAIT_WRITE_V
			= requires { serializer<::std::remove_cvref_t<T>>::write; };
		template<class T>
		static constexpr bool NAMES_TRAIT_READ_V
			= requires { serializer<::std::remove_cvref_t<T>>::read; };
		template<class T>
		static constexpr bool NAMES_TRAIT_MAKE_V
			= requires { serializer<::std::remove_cvref_t<T>>::make; };

		template<class T>
		static constexpr bool NAMES_MEMBER_VISIT_V
			= requires { ::std::remove_cvref_t<T>::serVisit; };
		template<class T>
		static constexpr bool NAMES_MEMBER_WRITE_V
			= requires { ::std::remove_cvref_t<T>::serWrite; };
		template<class T>
		static constexpr bool NAMES_MEMBER_READ_V = requires { ::std::remove_cvref_t<T>::serRead; };
		template<class T>
		static constexpr bool NAMES_MEMBER_MAKE_V = requires { ::std::remove_cvref_t<T>::serMake; };

		// ── the calls ─────────────────────────────────────────────────────────
		// These are the reason a private hook works at all: the call site is inside the
		// class the type befriended. dispatch never names T::ser_* directly.
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
		// `using ser_members = ser::members<2>;` inside the class. It may be private,
		// which is the whole reason the detector is here and not in ser::detail.
		//
		// It exists because COUNTING and DECOMPOSING have different requirements:
		// decomposition works on any class whose members are accessible here, but the
		// count is probed with aggregate initialization, which a non-aggregate does not
		// support. So a class with private fields, or with a constructor, tells us the
		// number and the ladder does the rest.
		template<class T>
		static constexpr bool NAMES_MEMBER_COUNT_V
			= requires { ::std::remove_cvref_t<T>::ser_members::COUNT; };

		template<class T>
		static constexpr ::std::size_t declaredMemberCount() {
			return ::std::remove_cvref_t<T>::ser_members::COUNT;
		}

		// ── what SER_DESCRIBE left behind ─────────────────────────────────────
		// Same reason as everything else in this struct, and it bites in a way that is
		// easy to miss: SER_DESCRIBE is usually written in a private section, next to the
		// fields it names. A detector in ser::detail reports false for it - access control
		// is part of substitution - and the caller then falls back to comparing EVERY
		// member. A description exists precisely to leave some out, and the ones left out
		// come back default-constructed, so the fallback reports a field that came back
		// "different" for a field the author never asked to be written.
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

		// ── the structured-bindings ladder ────────────────────────────────────
		// Calls f with every member of obj as an lvalue. The arity is a template
		// parameter rather than something computed here, so this stays free of
		// describe_bind.hpp and the include graph has no cycle: counting needs the
		// ladder (member_type_t), the ladder needs nothing.
		//
		// A wrong N is a compile error inside the branch it selects - the arity of a
		// structured binding is checked, not deduced - so an N that does not match the
		// type cannot silently write the wrong bytes.
		template<::std::size_t N, class T, class F>
		// The ladder binds obj and calls f in place; forwarding either would change
		// what the members bind as, which is the whole contract of the walk.
		// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
		static constexpr decltype(auto) visitMembersN(T&& obj, F&& f) {
#include <ser/detail/ladder.inc>
		}

		// "Can a non-const lvalue reference bind to this?" Never defined, and used only
		// inside a requires-expression, so nothing is ever called. One thing answers no:
		// a bit-field. Measured SFINAE-friendly on MSVC 14.51, GCC 13.3 and Clang 22 -
		// the candidate is simply not viable, which is a substitution failure and not a
		// hard error.
		template<class X>
		static void bindProbe(X&);  // NOT defined, on purpose

		// The declared type of every field, which only a structured binding can still
		// see. Never called either: callers ask for decltype of it, exactly as
		// tieMembers does. Same N-as-a-parameter arrangement as visitMembersN, for the
		// same reason - counting needs the ladder, so the ladder needs nothing.
		template<::std::size_t N, class T>
		// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
		static constexpr auto fieldDeclsN(T&& obj) {
#include <ser/detail/ladder_decls.inc>
		}
	};

// A type opts in with one line. `friend ser::access;` works just as well; the macro
// exists so the spelling does not have to be remembered.
#define SER_FRIEND friend struct ::ser::access;

}  // namespace ser

namespace ser::detail {

	// ── the other direction of an archive ─────────────────────────────────────
	// Half the questions checkHooks asks are about the direction it is NOT in: from
	// dispatchWrite it has to know whether the type can be read back. Answering that
	// needs a reader, and the only honest one is a REAL archive carrying the same Ctx -
	// so a hook reaching for ar.pool<P>() compiles during detection, and a hook usable
	// only with one archive is judged against the archive actually in use.
	//
	// ser::in<Ctx> is exact: there is one reader shape and the context is the whole of
	// it. The writer is exact only in its context - the buffer is the caller's choice and
	// nothing in a reader remembers it. That imprecision can only matter for a hook
	// pinned to one concrete out<Buf, Ctx>, which nongeneric_*_hook_v reports anyway.
	template<class Ar>
	using reader_for = ::std::conditional_t<reader<Ar>, Ar, in<typename Ar::context_type>>;

	template<class Ar>
	using writer_for
		= ::std::conditional_t<writer<Ar>, Ar, out<::std::span<::std::byte>, typename Ar::context_type>>;

	// ── per-level summaries ───────────────────────────────────────────────────
	// Every one of them asks with a real archive, so "does this type declare a hook" and
	// "which rung will the ladder take" are the same question asked twice.

	template<class T, class Ar>
	inline constexpr bool TRAIT_VISIT_WRITE_V = access::HAS_TRAIT_VISIT_V<const T, writer_for<Ar>>;
	template<class T, class Ar>
	inline constexpr bool TRAIT_VISIT_READ_V = access::HAS_TRAIT_VISIT_V<T, reader_for<Ar>>;
	template<class T, class Ar>
	inline constexpr bool TRAIT_WRITE_V = access::HAS_TRAIT_WRITE_V<T, writer_for<Ar>>;
	template<class T, class Ar>
	inline constexpr bool TRAIT_READ_V = access::HAS_TRAIT_READ_V<T, reader_for<Ar>>;
	template<class T, class Ar>
	inline constexpr bool TRAIT_MAKE_V = access::HAS_TRAIT_MAKE_V<T, reader_for<Ar>>;

	template<class T, class Ar>
	inline constexpr bool MEMBER_VISIT_WRITE_V
		= access::HAS_MEMBER_VISIT_V<const T, writer_for<Ar>>;
	template<class T, class Ar>
	inline constexpr bool MEMBER_VISIT_READ_V = access::HAS_MEMBER_VISIT_V<T, reader_for<Ar>>;
	template<class T, class Ar>
	inline constexpr bool MEMBER_WRITE_V = access::HAS_MEMBER_WRITE_V<T, writer_for<Ar>>;
	template<class T, class Ar>
	inline constexpr bool MEMBER_READ_V = access::HAS_MEMBER_READ_V<T, reader_for<Ar>>;
	template<class T, class Ar>
	inline constexpr bool MEMBER_MAKE_V = access::HAS_MEMBER_MAKE_V<T, reader_for<Ar>>;

	template<class T, class Ar>
	inline constexpr bool ADL_VISIT_WRITE_V = HAS_ADL_VISIT_V<const T, writer_for<Ar>>;
	template<class T, class Ar>
	inline constexpr bool ADL_VISIT_READ_V = HAS_ADL_VISIT_V<T, reader_for<Ar>>;
	template<class T, class Ar>
	inline constexpr bool ADL_WRITE_V = HAS_ADL_WRITE_V<T, writer_for<Ar>>;
	template<class T, class Ar>
	inline constexpr bool ADL_READ_V = HAS_ADL_READ_V<T, reader_for<Ar>>;
	template<class T, class Ar>
	inline constexpr bool ADL_MAKE_V = HAS_ADL_MAKE_V<T, reader_for<Ar>>;

	// ── HAS_CUSTOM_SERIALIZER_V ───────────────────────────────────────────────
	// Must count all three levels and all four forms. This is the trait that keeps a
	// type with its own serializer from ever being decomposed field by field (block C)
	// or bulk-copied (M2's is_flat_v) - and the types that need saving from both are
	// exactly the ones that look harmless: trivially copyable, no padding, remapped
	// through a pool. Missing one form here writes raw addresses to the stream.
	//
	// It takes Ar for the same reason is_flat_v does (see CLAUDE.md): the answer only
	// means anything relative to the archive that will do the work.
	template<class T, class Ar>
	inline constexpr bool HAS_CUSTOM_SERIALIZER_V
		= TRAIT_VISIT_WRITE_V<T, Ar> || TRAIT_VISIT_READ_V<T, Ar> || TRAIT_WRITE_V<T, Ar>
	   || TRAIT_READ_V<T, Ar> || TRAIT_MAKE_V<T, Ar> || MEMBER_VISIT_WRITE_V<T, Ar>
	   || MEMBER_VISIT_READ_V<T, Ar> || MEMBER_WRITE_V<T, Ar> || MEMBER_READ_V<T, Ar>
	   || MEMBER_MAKE_V<T, Ar> || ADL_VISIT_WRITE_V<T, Ar> || ADL_VISIT_READ_V<T, Ar>
	   || ADL_WRITE_V<T, Ar> || ADL_READ_V<T, Ar> || ADL_MAKE_V<T, Ar>;

	// Either direction, any level. The pairing rule below is about the type being
	// serializable at all, not about which level answers.
	template<class T, class Ar>
	inline constexpr bool HAS_ANY_WRITE_HOOK_V
		= TRAIT_WRITE_V<T, Ar> || MEMBER_WRITE_V<T, Ar> || ADL_WRITE_V<T, Ar>
	   || TRAIT_VISIT_WRITE_V<T, Ar> || MEMBER_VISIT_WRITE_V<T, Ar> || ADL_VISIT_WRITE_V<T, Ar>;

	template<class T, class Ar>
	inline constexpr bool HAS_ANY_READ_HOOK_V
		= TRAIT_READ_V<T, Ar> || MEMBER_READ_V<T, Ar> || ADL_READ_V<T, Ar> || TRAIT_MAKE_V<T, Ar>
	   || MEMBER_MAKE_V<T, Ar> || ADL_MAKE_V<T, Ar> || TRAIT_VISIT_READ_V<T, Ar>
	   || MEMBER_VISIT_READ_V<T, Ar> || ADL_VISIT_READ_V<T, Ar>;

	template<class T, class Ar>
	inline constexpr bool HAS_ANY_MAKE_HOOK_V
		= TRAIT_MAKE_V<T, Ar> || MEMBER_MAKE_V<T, Ar> || ADL_MAKE_V<T, Ar>;

	// ── wrong return type ─────────────────────────────────────────────────────
	// Fires only when NO level got the form right, so a correct trait hook still wins
	// over a broken in-class one instead of blocking the build.
	template<class T, class Ar>
	inline constexpr bool VISIT_WRONG_RETURN_V
		= (access::HAS_TRAIT_VISIT_LOOSE_V<T, reader_for<Ar>>
	       || access::HAS_TRAIT_VISIT_LOOSE_V<const T, writer_for<Ar>>
	       || access::HAS_MEMBER_VISIT_LOOSE_V<T, reader_for<Ar>>
	       || access::HAS_MEMBER_VISIT_LOOSE_V<const T, writer_for<Ar>>
	       || HAS_ADL_VISIT_LOOSE_V<T, reader_for<Ar>>
	       || HAS_ADL_VISIT_LOOSE_V<const T, writer_for<Ar>>)
	   && !(TRAIT_VISIT_READ_V<T, Ar> || TRAIT_VISIT_WRITE_V<T, Ar> || MEMBER_VISIT_READ_V<T, Ar>
	        || MEMBER_VISIT_WRITE_V<T, Ar> || ADL_VISIT_READ_V<T, Ar> || ADL_VISIT_WRITE_V<T, Ar>);

	template<class T, class Ar>
	inline constexpr bool WRITE_WRONG_RETURN_V
		= (access::HAS_TRAIT_WRITE_LOOSE_V<T, writer_for<Ar>>
	       || access::HAS_MEMBER_WRITE_LOOSE_V<T, writer_for<Ar>>
	       || HAS_ADL_WRITE_LOOSE_V<T, writer_for<Ar>>)
	   && !(TRAIT_WRITE_V<T, Ar> || MEMBER_WRITE_V<T, Ar> || ADL_WRITE_V<T, Ar>);

	template<class T, class Ar>
	inline constexpr bool READ_WRONG_RETURN_V
		= (access::HAS_TRAIT_READ_LOOSE_V<T, reader_for<Ar>>
	       || access::HAS_MEMBER_READ_LOOSE_V<T, reader_for<Ar>>
	       || HAS_ADL_READ_LOOSE_V<T, reader_for<Ar>>)
	   && !(TRAIT_READ_V<T, Ar> || MEMBER_READ_V<T, Ar> || ADL_READ_V<T, Ar>);

	template<class T, class Ar>
	inline constexpr bool MAKE_WRONG_RETURN_V
		= (access::HAS_TRAIT_MAKE_LOOSE_V<T, reader_for<Ar>>
	       || access::HAS_MEMBER_MAKE_LOOSE_V<T, reader_for<Ar>>
	       || HAS_ADL_MAKE_LOOSE_V<T, reader_for<Ar>>)
	   && !(TRAIT_MAKE_V<T, Ar> || MEMBER_MAKE_V<T, Ar> || ADL_MAKE_V<T, Ar>);

	// ── a hook this archive cannot reach ──────────────────────────────────────
	// True when the name is declared but the archive in use cannot call it: the hook is
	// there, and it is being ignored. For a member or trait hook that means one thing -
	// it is a plain function pinned to some other concrete archive type, so the ladder
	// walks straight past it into the builtin or automatic path and the bytes are not
	// the ones the author wrote.
	//
	// Naming a member without calling it is the one question that needs no archive. An
	// id-expression for a function TEMPLATE cannot be formed without arguments to deduce
	// from, so each names_* is true for exactly a single NON-template declaration. That
	// is also the whole reach of this check: it cannot see an overload set, a template
	// constrained to one archive, or an ADL free function.
	template<class T, class Ar>
	inline constexpr bool NONGENERIC_VISIT_HOOK_V
		= (access::NAMES_TRAIT_VISIT_V<T>
	       && !(TRAIT_VISIT_WRITE_V<T, Ar> || TRAIT_VISIT_READ_V<T, Ar>) )
	   || (access::NAMES_MEMBER_VISIT_V<T>
	       && !(MEMBER_VISIT_WRITE_V<T, Ar> || MEMBER_VISIT_READ_V<T, Ar>) );

	template<class T, class Ar>
	inline constexpr bool NONGENERIC_WRITE_HOOK_V
		= (access::NAMES_TRAIT_WRITE_V<T> && !TRAIT_WRITE_V<T, Ar>)
	   || (access::NAMES_MEMBER_WRITE_V<T> && !MEMBER_WRITE_V<T, Ar>);

	template<class T, class Ar>
	inline constexpr bool NONGENERIC_READ_HOOK_V
		= (access::NAMES_TRAIT_READ_V<T> && !TRAIT_READ_V<T, Ar>)
	   || (access::NAMES_MEMBER_READ_V<T> && !MEMBER_READ_V<T, Ar>);

	template<class T, class Ar>
	inline constexpr bool NONGENERIC_MAKE_HOOK_V
		= (access::NAMES_TRAIT_MAKE_V<T> && !TRAIT_MAKE_V<T, Ar>)
	   || (access::NAMES_MEMBER_MAKE_V<T> && !MEMBER_MAKE_V<T, Ar>);

	// Any of the four. The ladder uses it to stay quiet: once checkHooks has said "the
	// hook you wrote is being ignored", a second complaint that the type cannot be
	// serialized at all is noise pointing away from the cause.
	template<class T, class Ar>
	inline constexpr bool NONGENERIC_ANY_HOOK_V
		= NONGENERIC_VISIT_HOOK_V<T, Ar> || NONGENERIC_WRITE_HOOK_V<T, Ar>
	   || NONGENERIC_READ_HOOK_V<T, Ar> || NONGENERIC_MAKE_HOOK_V<T, Ar>;

	// ── checkHooks<T, Ar>() ──────────────────────────────────────────────────
	// Called at the top of all three dispatch contexts, with the archive they hold.
	// Everything here is a compile-time diagnostic; it produces no code. The order
	// matters: the most specific message about a hook that exists comes before the
	// vaguer ones about hooks that do not.
	//
	// `serMake` together with `serRead` is deliberately allowed - they answer different
	// questions ("build me one" vs "fill this one in") and a type may usefully offer both.
	// `serVisit` together with `serWrite` is not: both answer "write this one", and
	// nothing in the type says which format was meant.
	template<class T, class Ar>
	constexpr void checkHooks() {
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

		// A visit hook that binds only a non-const object is found when reading and
		// missed when writing, so writing would silently fall through to the builtin or
		// automatic path and the two directions would disagree about the format.
		static_assert(
			!(TRAIT_VISIT_READ_V<T, Ar> && !TRAIT_VISIT_WRITE_V<T, Ar>),
			"ser: serializer<T>::visit accepts a non-const object only, so it is invisible "
			"when writing and the two directions would use different formats. Take the "
			"object as a deduced reference: static ser::Errc visit(auto& ar, auto& self)."
		);

		static_assert(
			!(MEMBER_VISIT_READ_V<T, Ar> && !MEMBER_VISIT_WRITE_V<T, Ar>),
			"ser: T::serVisit accepts a non-const object only, so it is invisible when "
			"writing and the two directions would use different formats. Take the object "
			"as a deduced reference: static ser::Errc serVisit(auto& ar, auto& self)."
		);

		static_assert(
			!(ADL_VISIT_READ_V<T, Ar> && !ADL_VISIT_WRITE_V<T, Ar>),
			"ser: the ADL serVisit for this type accepts a non-const object only, so it "
			"is invisible when writing and the two directions would use different formats. "
			"Take the object as a deduced reference: ser::Errc serVisit(auto& ar, auto& self)."
		);

		static_assert(
			!((TRAIT_VISIT_WRITE_V<T, Ar> || TRAIT_VISIT_READ_V<T, Ar>)
		      && (TRAIT_WRITE_V<T, Ar> || TRAIT_READ_V<T, Ar>) ),
			"ser: serializer<T> declares both visit and write/read. They answer the same "
			"question and nothing says which format was meant. Keep visit for a symmetric "
			"format, or the write/read pair for an asymmetric one."
		);

		static_assert(
			!((MEMBER_VISIT_WRITE_V<T, Ar> || MEMBER_VISIT_READ_V<T, Ar>)
		      && (MEMBER_WRITE_V<T, Ar> || MEMBER_READ_V<T, Ar>) ),
			"ser: this type declares both serVisit and serWrite/serRead. They answer "
			"the same question and nothing says which format was meant. Keep serVisit for "
			"a symmetric format, or the serWrite/serRead pair for an asymmetric one."
		);

		static_assert(
			!((ADL_VISIT_WRITE_V<T, Ar> || ADL_VISIT_READ_V<T, Ar>)
		      && (ADL_WRITE_V<T, Ar> || ADL_READ_V<T, Ar>) ),
			"ser: this type has both an ADL serVisit and an ADL serWrite/serRead. They "
			"answer the same question and nothing says which format was meant. Keep "
			"serVisit for a symmetric format, or the pair for an asymmetric one."
		);

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
