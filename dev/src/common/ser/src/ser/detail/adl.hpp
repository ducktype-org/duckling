#pragma once

#include <ser/errc.hpp>
#include <ser/tags.hpp>

#include <concepts>
#include <type_traits>

namespace ser::detail::adl_barrier {

    // ── the barrier ───────────────────────────────────────────────────────────
    // Unqualified lookup stops at the first enclosing scope that declares the name, so
    // these four declarations keep the calls below from ever reaching ser::detail:: or
    // ser:: - argument-dependent lookup is then the only way a hook can be found.
    // Without them an unqualified ser_write(ar, x) inside the library would happily bind
    // to a library symbol of that name and the user's hook would never be consulted.
    //
    // They take no arguments, so they are never viable candidates themselves; = delete
    // makes a stray zero-argument call an error rather than a link failure.
    void ser_visit() = delete;
    void ser_write() = delete;
    void ser_read()  = delete;
    void ser_make()  = delete;

    // Trailing return types on purpose. The substitution then happens in the DECLARATION,
    // where a missing hook is a plain substitution failure the detectors below can see.
    // With a fixed `errc` return type the declaration would always be well-formed and
    // every detector would report true for every type.
    template <class Ar, class T>
    constexpr auto call_visit(Ar& ar, T& x) -> decltype(ser_visit(ar, x)) {
        return ser_visit(ar, x);
    }

    template <class Ar, class T>
    constexpr auto call_write(Ar& ar, const T& x) -> decltype(ser_write(ar, x)) {
        return ser_write(ar, x);
    }

    template <class Ar, class T>
    constexpr auto call_read(Ar& ar, T& x) -> decltype(ser_read(ar, x)) {
        return ser_read(ar, x);
    }

    // The tag is a PARAMETER so that both template parameters are deduced. Making T
    // explicit here works everywhere except GCC - see has_adl_make_v below.
    template <class Ar, class T>
    constexpr auto call_make(Ar& ar, ::ser::tag<T> t) -> decltype(ser_make(ar, t)) {
        return ser_make(ar, t);
    }

} // namespace ser::detail::adl_barrier

namespace ser::detail {

    // ── ADL hook detectors ────────────────────────────────────────────────────
    // T carries the cv-qualification of the object being visited: dispatch_write asks
    // has_adl_visit_v<const U, Ar>, dispatch_read asks has_adl_visit_v<U, Ar>. A hook
    // that only accepts a non-const object is therefore invisible on the write side,
    // and check_hooks turns that asymmetry into a message instead of two formats.

    template <class T, class Ar>
    inline constexpr bool has_adl_visit_v = requires(Ar& ar, T& x) {
        { adl_barrier::call_visit(ar, x) } -> ::std::same_as<errc>;
    };

    template <class T, class Ar>
    inline constexpr bool has_adl_write_v = requires(Ar& ar, const ::std::remove_cvref_t<T>& x) {
        { adl_barrier::call_write(ar, x) } -> ::std::same_as<errc>;
    };

    template <class T, class Ar>
    inline constexpr bool has_adl_read_v = requires(Ar& ar, ::std::remove_cvref_t<T>& x) {
        { adl_barrier::call_read(ar, x) } -> ::std::same_as<errc>;
    };

    // The tag travels as a function argument, not as an explicit template argument, to
    // work around a GCC bug (13 and 14; clang 19 and MSVC accept the code below).
    // Substituting the explicit T while Ar is still unknown leaves the call
    // type-dependent, so it must be resolved in the instantiation context, where ADL
    // would find the hook; GCC instead resolves it against the poisoned declaration
    // right away. It is not overload resolution getting this wrong - a barrier declared
    // `void ser_make(int) = delete`, never viable for a two-argument call, fails the
    // same way. Standalone reproducer, g++ -std=c++20:
    //
    //     namespace user { struct X {}; template <class Ar> int ser_make(Ar&, X) { return 0; } }
    //     namespace bar {
    //         void ser_make() = delete;
    //         template <class T, class Ar> auto call(Ar& ar) -> decltype(ser_make(ar, T{}));
    //     }
    //     struct Ar {};
    //     static_assert(requires(Ar& a) { bar::call<user::X>(a); });
    //     int main() {}
    //
    //     error: no matching function for call to 'call<user::X>(Ar&)'
    //     In substitution of 'decltype (bar::ser_make(ar, T{})) bar::call(Ar&)
    //                         [with T = user::X; Ar = <missing>]'
    //     error: use of deleted function 'void bar::ser_make()'
    template <class T, class Ar>
    inline constexpr bool has_adl_make_v = requires(Ar& ar) {
        { adl_barrier::call_make(ar, ::ser::tag<::std::remove_cvref_t<T>>{}) }
            -> ::std::same_as<::std::remove_cvref_t<T>>;
    };

    // ── the same four, without the return-type requirement ────────────────────
    // The difference between "loose but not strict" is exactly "the hook is there and
    // returns the wrong thing", which is the single most common way to write one.
    template <class T, class Ar>
    inline constexpr bool has_adl_visit_loose_v = requires(Ar& ar, T& x) {
        adl_barrier::call_visit(ar, x);
    };

    template <class T, class Ar>
    inline constexpr bool has_adl_write_loose_v = requires(Ar& ar, const ::std::remove_cvref_t<T>& x) {
        adl_barrier::call_write(ar, x);
    };

    template <class T, class Ar>
    inline constexpr bool has_adl_read_loose_v = requires(Ar& ar, ::std::remove_cvref_t<T>& x) {
        adl_barrier::call_read(ar, x);
    };

    template <class T, class Ar>
    inline constexpr bool has_adl_make_loose_v = requires(Ar& ar) {
        adl_barrier::call_make(ar, ::ser::tag<::std::remove_cvref_t<T>>{});
    };

} // namespace ser::detail
