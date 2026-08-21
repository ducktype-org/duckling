#pragma once

#include <ser/concepts.hpp>
#include <ser/errc.hpp>
#include <ser/serializer.hpp>

#include <ser/archive/in.hpp>
#include <ser/archive/out.hpp>

#include <ser/detail/adl.hpp>
#include <ser/detail/meta.hpp>

#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>

namespace ser {

    // ── ser::members<N> ───────────────────────────────────────────────────────
    // The opt-in field count. It skips the counting phase and, together with
    // `friend ser::access`, is what lets the structured-bindings ladder in block C see
    // private fields. A wrong N is a compile error, so the format cannot drift silently.
    template <::std::size_t N>
    struct members { static constexpr ::std::size_t count = N; };

    // ── ser::access ───────────────────────────────────────────────────────────
    // Every in-class hook detector lives HERE rather than in ser::detail, and that is not
    // a matter of taste: access control is part of SFINAE, so a detector written in
    // ser::detail reports false for a private ser_visit that ser::access can call
    // perfectly well. Detection and call must happen in the same class the type befriends.
    //
    // Trait-level detectors (ser::serializer<T>) have no access problem, but they live
    // here too so that the whole priority table can be read in one place.
    //
    // T carries the cv-qualification of the object for the symmetric `visit` form:
    // dispatch_write asks for `const U`, dispatch_read for `U`. A hook that only binds a
    // non-const object is then invisible on the write side, which detail::check_hooks
    // reports instead of letting the two directions use different formats.
    struct access {

        // ── level 1: ser::serializer<T> ───────────────────────────────────────
        template <class T, class Ar>
        static constexpr bool has_trait_visit_v = requires(Ar& ar, T& x) {
            { serializer<::std::remove_cvref_t<T>>::visit(ar, x) } -> ::std::same_as<errc>;
        };

        template <class T, class Ar>
        static constexpr bool has_trait_write_v = requires(Ar& ar, const ::std::remove_cvref_t<T>& x) {
            { serializer<::std::remove_cvref_t<T>>::write(ar, x) } -> ::std::same_as<errc>;
        };

        template <class T, class Ar>
        static constexpr bool has_trait_read_v = requires(Ar& ar, ::std::remove_cvref_t<T>& x) {
            { serializer<::std::remove_cvref_t<T>>::read(ar, x) } -> ::std::same_as<errc>;
        };

        template <class T, class Ar>
        static constexpr bool has_trait_make_v = requires(Ar& ar) {
            { serializer<::std::remove_cvref_t<T>>::make(ar) }
                -> ::std::same_as<::std::remove_cvref_t<T>>;
        };

        // ── level 2: hooks declared in the class ──────────────────────────────
        template <class T, class Ar>
        static constexpr bool has_member_visit_v = requires(Ar& ar, T& x) {
            { ::std::remove_cvref_t<T>::ser_visit(ar, x) } -> ::std::same_as<errc>;
        };

        template <class T, class Ar>
        static constexpr bool has_member_write_v = requires(Ar& ar, const ::std::remove_cvref_t<T>& x) {
            { ::std::remove_cvref_t<T>::ser_write(ar, x) } -> ::std::same_as<errc>;
        };

        template <class T, class Ar>
        static constexpr bool has_member_read_v = requires(Ar& ar, ::std::remove_cvref_t<T>& x) {
            { ::std::remove_cvref_t<T>::ser_read(ar, x) } -> ::std::same_as<errc>;
        };

        template <class T, class Ar>
        static constexpr bool has_member_make_v = requires(Ar& ar) {
            { ::std::remove_cvref_t<T>::ser_make(ar) }
                -> ::std::same_as<::std::remove_cvref_t<T>>;
        };

        // ── the same eight without the return-type requirement ────────────────
        // "loose but not strict" means the hook is there and returns the wrong thing -
        // the single most common way to write one, and the one whose default diagnostic
        // ("no way to serialize this type") points nowhere near the cause.
        template <class T, class Ar>
        static constexpr bool has_trait_visit_loose_v = requires(Ar& ar, T& x) {
            serializer<::std::remove_cvref_t<T>>::visit(ar, x);
        };
        template <class T, class Ar>
        static constexpr bool has_trait_write_loose_v = requires(Ar& ar, const ::std::remove_cvref_t<T>& x) {
            serializer<::std::remove_cvref_t<T>>::write(ar, x);
        };
        template <class T, class Ar>
        static constexpr bool has_trait_read_loose_v = requires(Ar& ar, ::std::remove_cvref_t<T>& x) {
            serializer<::std::remove_cvref_t<T>>::read(ar, x);
        };
        template <class T, class Ar>
        static constexpr bool has_trait_make_loose_v = requires(Ar& ar) {
            serializer<::std::remove_cvref_t<T>>::make(ar);
        };

        template <class T, class Ar>
        static constexpr bool has_member_visit_loose_v = requires(Ar& ar, T& x) {
            ::std::remove_cvref_t<T>::ser_visit(ar, x);
        };
        template <class T, class Ar>
        static constexpr bool has_member_write_loose_v = requires(Ar& ar, const ::std::remove_cvref_t<T>& x) {
            ::std::remove_cvref_t<T>::ser_write(ar, x);
        };
        template <class T, class Ar>
        static constexpr bool has_member_read_loose_v = requires(Ar& ar, ::std::remove_cvref_t<T>& x) {
            ::std::remove_cvref_t<T>::ser_read(ar, x);
        };
        template <class T, class Ar>
        static constexpr bool has_member_make_loose_v = requires(Ar& ar) {
            ::std::remove_cvref_t<T>::ser_make(ar);
        };

        // ── "the name is there, but this archive cannot call it" ──────────────
        // Naming a member without calling it is the one question that needs no archive.
        // An id-expression for a function TEMPLATE cannot be formed without arguments to
        // deduce from, so each of these is true for exactly a single NON-template
        // declaration - a hook pinned to one concrete archive type. Compared against the
        // archive in use, that is how detail::nongeneric_*_hook_v tells a hook that works
        // from a hook that is being silently ignored.
        template <class T>
        static constexpr bool names_trait_visit_v = requires { serializer<::std::remove_cvref_t<T>>::visit; };
        template <class T>
        static constexpr bool names_trait_write_v = requires { serializer<::std::remove_cvref_t<T>>::write; };
        template <class T>
        static constexpr bool names_trait_read_v  = requires { serializer<::std::remove_cvref_t<T>>::read; };
        template <class T>
        static constexpr bool names_trait_make_v  = requires { serializer<::std::remove_cvref_t<T>>::make; };

        template <class T>
        static constexpr bool names_member_visit_v = requires { ::std::remove_cvref_t<T>::ser_visit; };
        template <class T>
        static constexpr bool names_member_write_v = requires { ::std::remove_cvref_t<T>::ser_write; };
        template <class T>
        static constexpr bool names_member_read_v  = requires { ::std::remove_cvref_t<T>::ser_read; };
        template <class T>
        static constexpr bool names_member_make_v  = requires { ::std::remove_cvref_t<T>::ser_make; };

        // ── the calls ─────────────────────────────────────────────────────────
        // These are the reason a private hook works at all: the call site is inside the
        // class the type befriended. dispatch never names T::ser_* directly.
        template <class T, class Ar>
        static constexpr errc call_visit(Ar& ar, T& x) {
            return ::std::remove_cvref_t<T>::ser_visit(ar, x);
        }

        template <class T, class Ar>
        static constexpr errc call_write(Ar& ar, const T& x) {
            return T::ser_write(ar, x);
        }

        template <class T, class Ar>
        static constexpr errc call_read(Ar& ar, T& x) {
            return T::ser_read(ar, x);
        }

        // T first: there is no argument to deduce it from.
        template <class T, class Ar>
        static constexpr T call_make(Ar& ar) {
            return T::ser_make(ar);
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
        template <class T>
        static constexpr bool names_member_count_v =
            requires { ::std::remove_cvref_t<T>::ser_members::count; };

        template <class T>
        static constexpr ::std::size_t declared_member_count() {
            return ::std::remove_cvref_t<T>::ser_members::count;
        }

        // ── what SER_DESCRIBE left behind ─────────────────────────────────────
        // Same reason as everything else in this struct, and it bites in a way that is
        // easy to miss: SER_DESCRIBE is usually written in a private section, next to the
        // fields it names. A detector in ser::detail reports false for it - access control
        // is part of substitution - and the caller then falls back to comparing EVERY
        // member. A description exists precisely to leave some out, and the ones left out
        // come back default-constructed, so the fallback reports a field that came back
        // "different" for a field the author never asked to be written.
        template <class T>
        static constexpr bool has_described_v =
            requires(const ::std::remove_cvref_t<T>& x) {
                ::std::remove_cvref_t<T>::ser_described(x);
            };

        template <class T>
        static constexpr auto described(const T& x) {
            return ::std::remove_cvref_t<T>::ser_described(x);
        }

        template <class T>
        static constexpr bool has_field_names_v =
            requires { ::std::remove_cvref_t<T>::ser_field_names; };

        template <class T>
        static constexpr const char* field_name(::std::size_t i) {
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
        template <::std::size_t N, class T, class F>
        static constexpr decltype(auto) visit_members_n(T&& obj, F&& f) {
            #include <ser/detail/ladder.inc>
        }

        // "Can a non-const lvalue reference bind to this?" Never defined, and used only
        // inside a requires-expression, so nothing is ever called. One thing answers no:
        // a bit-field. Measured SFINAE-friendly on MSVC 14.51, GCC 13.3 and Clang 22 -
        // the candidate is simply not viable, which is a substitution failure and not a
        // hard error.
        template <class X>
        static void bind_probe(X&);                         // NOT defined, on purpose

        // The declared type of every field, which only a structured binding can still
        // see. Never called either: callers ask for decltype of it, exactly as
        // tie_members does. Same N-as-a-parameter arrangement as visit_members_n, for the
        // same reason - counting needs the ladder, so the ladder needs nothing.
        template <::std::size_t N, class T>
        static constexpr auto field_decls_n(T&& obj) {
            #include <ser/detail/ladder_decls.inc>
        }
    };

    // A type opts in with one line. `friend ser::access;` works just as well; the macro
    // exists so the spelling does not have to be remembered.
    #define SER_FRIEND friend struct ::ser::access;

} // namespace ser

namespace ser::detail {

    // ── the other direction of an archive ─────────────────────────────────────
    // Half the questions check_hooks asks are about the direction it is NOT in: from
    // dispatch_write it has to know whether the type can be read back. Answering that
    // needs a reader, and the only honest one is a REAL archive carrying the same Ctx -
    // so a hook reaching for ar.pool<P>() compiles during detection, and a hook usable
    // only with one archive is judged against the archive actually in use.
    //
    // ser::in<Ctx> is exact: there is one reader shape and the context is the whole of
    // it. The writer is exact only in its context - the buffer is the caller's choice and
    // nothing in a reader remembers it. That imprecision can only matter for a hook
    // pinned to one concrete out<Buf, Ctx>, which nongeneric_*_hook_v reports anyway.
    template <class Ar>
    using reader_for = ::std::conditional_t<reader<Ar>, Ar, in<typename Ar::context_type>>;

    template <class Ar>
    using writer_for = ::std::conditional_t<
        writer<Ar>, Ar, out<::std::span<::std::byte>, typename Ar::context_type>>;

    // ── per-level summaries ───────────────────────────────────────────────────
    // Every one of them asks with a real archive, so "does this type declare a hook" and
    // "which rung will the ladder take" are the same question asked twice.

    template <class T, class Ar> inline constexpr bool trait_visit_write_v = access::has_trait_visit_v<const T, writer_for<Ar>>;
    template <class T, class Ar> inline constexpr bool trait_visit_read_v  = access::has_trait_visit_v<T,       reader_for<Ar>>;
    template <class T, class Ar> inline constexpr bool trait_write_v       = access::has_trait_write_v<T,       writer_for<Ar>>;
    template <class T, class Ar> inline constexpr bool trait_read_v        = access::has_trait_read_v <T,       reader_for<Ar>>;
    template <class T, class Ar> inline constexpr bool trait_make_v        = access::has_trait_make_v <T,       reader_for<Ar>>;

    template <class T, class Ar> inline constexpr bool member_visit_write_v = access::has_member_visit_v<const T, writer_for<Ar>>;
    template <class T, class Ar> inline constexpr bool member_visit_read_v  = access::has_member_visit_v<T,       reader_for<Ar>>;
    template <class T, class Ar> inline constexpr bool member_write_v       = access::has_member_write_v<T,       writer_for<Ar>>;
    template <class T, class Ar> inline constexpr bool member_read_v        = access::has_member_read_v <T,       reader_for<Ar>>;
    template <class T, class Ar> inline constexpr bool member_make_v        = access::has_member_make_v <T,       reader_for<Ar>>;

    template <class T, class Ar> inline constexpr bool adl_visit_write_v = has_adl_visit_v<const T, writer_for<Ar>>;
    template <class T, class Ar> inline constexpr bool adl_visit_read_v  = has_adl_visit_v<T,       reader_for<Ar>>;
    template <class T, class Ar> inline constexpr bool adl_write_v       = has_adl_write_v<T,       writer_for<Ar>>;
    template <class T, class Ar> inline constexpr bool adl_read_v        = has_adl_read_v <T,       reader_for<Ar>>;
    template <class T, class Ar> inline constexpr bool adl_make_v        = has_adl_make_v <T,       reader_for<Ar>>;

    // ── has_custom_serializer_v ───────────────────────────────────────────────
    // Must count all three levels and all four forms. This is the trait that keeps a
    // type with its own serializer from ever being decomposed field by field (block C)
    // or bulk-copied (M2's is_flat_v) - and the types that need saving from both are
    // exactly the ones that look harmless: trivially copyable, no padding, remapped
    // through a pool. Missing one form here writes raw addresses to the stream.
    //
    // It takes Ar for the same reason is_flat_v does (see CLAUDE.md): the answer only
    // means anything relative to the archive that will do the work.
    template <class T, class Ar>
    inline constexpr bool has_custom_serializer_v =
           trait_visit_write_v <T, Ar> || trait_visit_read_v <T, Ar> || trait_write_v <T, Ar>
        || trait_read_v        <T, Ar> || trait_make_v       <T, Ar>
        || member_visit_write_v<T, Ar> || member_visit_read_v<T, Ar> || member_write_v<T, Ar>
        || member_read_v       <T, Ar> || member_make_v      <T, Ar>
        || adl_visit_write_v   <T, Ar> || adl_visit_read_v   <T, Ar> || adl_write_v   <T, Ar>
        || adl_read_v          <T, Ar> || adl_make_v         <T, Ar>;

    // Either direction, any level. The pairing rule below is about the type being
    // serializable at all, not about which level answers.
    template <class T, class Ar>
    inline constexpr bool has_any_write_hook_v =
        trait_write_v<T, Ar> || member_write_v<T, Ar> || adl_write_v<T, Ar>
        || trait_visit_write_v<T, Ar> || member_visit_write_v<T, Ar> || adl_visit_write_v<T, Ar>;

    template <class T, class Ar>
    inline constexpr bool has_any_read_hook_v =
        trait_read_v<T, Ar> || member_read_v<T, Ar> || adl_read_v<T, Ar>
        || trait_make_v<T, Ar> || member_make_v<T, Ar> || adl_make_v<T, Ar>
        || trait_visit_read_v<T, Ar> || member_visit_read_v<T, Ar> || adl_visit_read_v<T, Ar>;

    template <class T, class Ar>
    inline constexpr bool has_any_make_hook_v =
        trait_make_v<T, Ar> || member_make_v<T, Ar> || adl_make_v<T, Ar>;

    // ── wrong return type ─────────────────────────────────────────────────────
    // Fires only when NO level got the form right, so a correct trait hook still wins
    // over a broken in-class one instead of blocking the build.
    template <class T, class Ar>
    inline constexpr bool visit_wrong_return_v =
        ( access::has_trait_visit_loose_v <T, reader_for<Ar>> || access::has_trait_visit_loose_v <const T, writer_for<Ar>>
       || access::has_member_visit_loose_v<T, reader_for<Ar>> || access::has_member_visit_loose_v<const T, writer_for<Ar>>
       || has_adl_visit_loose_v<T, reader_for<Ar>>            || has_adl_visit_loose_v<const T, writer_for<Ar>> )
        && !(trait_visit_read_v<T, Ar>  || trait_visit_write_v<T, Ar>
          || member_visit_read_v<T, Ar> || member_visit_write_v<T, Ar>
          || adl_visit_read_v<T, Ar>    || adl_visit_write_v<T, Ar>);

    template <class T, class Ar>
    inline constexpr bool write_wrong_return_v =
        ( access::has_trait_write_loose_v<T, writer_for<Ar>> || access::has_member_write_loose_v<T, writer_for<Ar>>
       || has_adl_write_loose_v<T, writer_for<Ar>> )
        && !(trait_write_v<T, Ar> || member_write_v<T, Ar> || adl_write_v<T, Ar>);

    template <class T, class Ar>
    inline constexpr bool read_wrong_return_v =
        ( access::has_trait_read_loose_v<T, reader_for<Ar>> || access::has_member_read_loose_v<T, reader_for<Ar>>
       || has_adl_read_loose_v<T, reader_for<Ar>> )
        && !(trait_read_v<T, Ar> || member_read_v<T, Ar> || adl_read_v<T, Ar>);

    template <class T, class Ar>
    inline constexpr bool make_wrong_return_v =
        ( access::has_trait_make_loose_v<T, reader_for<Ar>> || access::has_member_make_loose_v<T, reader_for<Ar>>
       || has_adl_make_loose_v<T, reader_for<Ar>> )
        && !(trait_make_v<T, Ar> || member_make_v<T, Ar> || adl_make_v<T, Ar>);

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
    template <class T, class Ar>
    inline constexpr bool nongeneric_visit_hook_v =
           (access::names_trait_visit_v <T> && !(trait_visit_write_v <T, Ar> || trait_visit_read_v <T, Ar>))
        || (access::names_member_visit_v<T> && !(member_visit_write_v<T, Ar> || member_visit_read_v<T, Ar>));

    template <class T, class Ar>
    inline constexpr bool nongeneric_write_hook_v =
           (access::names_trait_write_v <T> && !trait_write_v <T, Ar>)
        || (access::names_member_write_v<T> && !member_write_v<T, Ar>);

    template <class T, class Ar>
    inline constexpr bool nongeneric_read_hook_v =
           (access::names_trait_read_v <T> && !trait_read_v <T, Ar>)
        || (access::names_member_read_v<T> && !member_read_v<T, Ar>);

    template <class T, class Ar>
    inline constexpr bool nongeneric_make_hook_v =
           (access::names_trait_make_v <T> && !trait_make_v <T, Ar>)
        || (access::names_member_make_v<T> && !member_make_v<T, Ar>);

    // Any of the four. The ladder uses it to stay quiet: once check_hooks has said "the
    // hook you wrote is being ignored", a second complaint that the type cannot be
    // serialized at all is noise pointing away from the cause.
    template <class T, class Ar>
    inline constexpr bool nongeneric_any_hook_v =
           nongeneric_visit_hook_v<T, Ar> || nongeneric_write_hook_v<T, Ar>
        || nongeneric_read_hook_v <T, Ar> || nongeneric_make_hook_v <T, Ar>;

    // ── check_hooks<T, Ar>() ──────────────────────────────────────────────────
    // Called at the top of all three dispatch contexts, with the archive they hold.
    // Everything here is a compile-time diagnostic; it produces no code. The order
    // matters: the most specific message about a hook that exists comes before the
    // vaguer ones about hooks that do not.
    //
    // `ser_make` together with `ser_read` is deliberately allowed - they answer different
    // questions ("build me one" vs "fill this one in") and a type may usefully offer both.
    // `ser_visit` together with `ser_write` is not: both answer "write this one", and
    // nothing in the type says which format was meant.
    template <class T, class Ar>
    constexpr void check_hooks() {
        // A hook the archive in use cannot call is a hook that silently does nothing;
        // everything below it is about hooks that are actually being used.
        static_assert(!nongeneric_visit_hook_v<T, Ar>,
            "ser: this type declares a visit hook that the archive in use cannot call, so "
            "it is being ignored. Make it a template on the archive:\n"
            "  static ser::errc ser_visit(auto& ar, auto& self) { return ar(self.a, self.b); }");

        static_assert(!nongeneric_write_hook_v<T, Ar>,
            "ser: this type declares a write hook that the archive in use cannot call, so "
            "it is being ignored. Make it a template on the archive:\n"
            "  static ser::errc ser_write(ser::writer auto& ar, const T& x) { return ar(x.a, x.b); }");

        static_assert(!nongeneric_read_hook_v<T, Ar>,
            "ser: this type declares a read hook that the archive in use cannot call, so "
            "it is being ignored. Make it a template on the archive:\n"
            "  static ser::errc ser_read(ser::reader auto& ar, T& x) { return ar(x.a, x.b); }");

        static_assert(!nongeneric_make_hook_v<T, Ar>,
            "ser: this type declares a make hook that the archive in use cannot call, so "
            "it is being ignored. Make it a template on the archive:\n"
            "  static T ser_make(ser::reader auto& ar) { return T{ ser::read_field<A>(ar) }; }");

        static_assert(!visit_wrong_return_v<T, Ar>,
            "ser: a ser_visit hook for this type exists but does not return ser::errc.\n"
            "  in the class:  static ser::errc ser_visit(auto& ar, auto& self) { return ar(self.a, self.b); }\n"
            "  by ADL:        ser::errc ser_visit(auto& ar, auto& self)\n"
            "  by trait:      ser::serializer<T>::visit, same signature");

        static_assert(!write_wrong_return_v<T, Ar>,
            "ser: a ser_write hook for this type exists but does not return ser::errc.\n"
            "  static ser::errc ser_write(ser::writer auto& ar, const T& x) { return ar(x.a, x.b); }");

        static_assert(!read_wrong_return_v<T, Ar>,
            "ser: a ser_read hook for this type exists but does not return ser::errc.\n"
            "  static ser::errc ser_read(ser::reader auto& ar, T& x) { return ar(x.a, x.b); }");

        static_assert(!make_wrong_return_v<T, Ar>,
            "ser: a ser_make hook for this type exists but does not return T by value.\n"
            "  static T ser_make(ser::reader auto& ar) { return T{ ser::read_field<A>(ar), "
            "ser::read_field<B>(ar) }; }\n"
            "  Braces, not parentheses - the order in which constructor arguments are "
            "evaluated is unspecified, and that would make the byte order compiler-dependent.");

        // A visit hook that binds only a non-const object is found when reading and
        // missed when writing, so writing would silently fall through to the builtin or
        // automatic path and the two directions would disagree about the format.
        static_assert(!(trait_visit_read_v<T, Ar> && !trait_visit_write_v<T, Ar>),
            "ser: serializer<T>::visit accepts a non-const object only, so it is invisible "
            "when writing and the two directions would use different formats. Take the "
            "object as a deduced reference: static ser::errc visit(auto& ar, auto& self).");

        static_assert(!(member_visit_read_v<T, Ar> && !member_visit_write_v<T, Ar>),
            "ser: T::ser_visit accepts a non-const object only, so it is invisible when "
            "writing and the two directions would use different formats. Take the object "
            "as a deduced reference: static ser::errc ser_visit(auto& ar, auto& self).");

        static_assert(!(adl_visit_read_v<T, Ar> && !adl_visit_write_v<T, Ar>),
            "ser: the ADL ser_visit for this type accepts a non-const object only, so it "
            "is invisible when writing and the two directions would use different formats. "
            "Take the object as a deduced reference: ser::errc ser_visit(auto& ar, auto& self).");

        static_assert(!((trait_visit_write_v<T, Ar> || trait_visit_read_v<T, Ar>)
                        && (trait_write_v<T, Ar> || trait_read_v<T, Ar>)),
            "ser: serializer<T> declares both visit and write/read. They answer the same "
            "question and nothing says which format was meant. Keep visit for a symmetric "
            "format, or the write/read pair for an asymmetric one.");

        static_assert(!((member_visit_write_v<T, Ar> || member_visit_read_v<T, Ar>)
                        && (member_write_v<T, Ar> || member_read_v<T, Ar>)),
            "ser: this type declares both ser_visit and ser_write/ser_read. They answer "
            "the same question and nothing says which format was meant. Keep ser_visit for "
            "a symmetric format, or the ser_write/ser_read pair for an asymmetric one.");

        static_assert(!((adl_visit_write_v<T, Ar> || adl_visit_read_v<T, Ar>)
                        && (adl_write_v<T, Ar> || adl_read_v<T, Ar>)),
            "ser: this type has both an ADL ser_visit and an ADL ser_write/ser_read. They "
            "answer the same question and nothing says which format was meant. Keep "
            "ser_visit for a symmetric format, or the pair for an asymmetric one.");

        // Both pairing rules go quiet when a hook is being ignored: with the hook out of
        // sight the type looks half-serializable, and saying so would send the reader
        // looking for a missing hook instead of at the one they wrote.
        if constexpr (!nongeneric_any_hook_v<T, Ar>) {
        static_assert(!(has_any_write_hook_v<T, Ar> && !has_any_read_hook_v<T, Ar>),
            "ser: this type can be written but never read back - it declares a write hook "
            "and no matching read hook. Add ser_read(ar, x) to fill an existing object, or "
            "ser_make(ar) to build a fresh one; a type with const fields or no default "
            "constructor needs ser_make.");

        static_assert(!(has_any_read_hook_v<T, Ar> && !has_any_write_hook_v<T, Ar>),
            "ser: this type can be read but never written - it declares a read hook and no "
            "matching write hook. Add ser_write(ar, x), or use ser_visit for both "
            "directions at once.");
        }
    }

} // namespace ser::detail
