#pragma once

// ── std::pair and std::tuple ──────────────────────────────────────────────────
// The elements in order, nothing else: no count, because the type carries it, and no
// padding. A pair is its first element followed by its second, which is what makes
// pair<K, V> and a two-field aggregate the same bytes.
//
// Both a read and a make, and they answer different questions. `read` fills an existing
// tuple element by element and is constrained on the elements being assignable, so a
// tuple with a const element simply does not have it and the ladder moves on to `make`.
// `make` builds one - IN BRACES, because that is the only way to order the reads: a
// braced-init-list is evaluated left to right by [dcl.init.list]/4, while the arguments
// of a constructor call are not ordered at all and MSVC evaluates them right to left.
// That is invariant 3, and it is the difference between a format and a coin flip.

#include <ser/concepts.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/serializer.hpp>
#include <ser/traits.hpp>

#include <ser/detail/dispatch_fwd.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ser {

    template <class A, class B>
    struct min_wire_size<::std::pair<A, B>> {
        static constexpr ::std::size_t value = min_wire_size_v<A> + min_wire_size_v<B>;
    };

    template <class... Es>
    struct min_wire_size<::std::tuple<Es...>> {
        static constexpr ::std::size_t value = (::std::size_t{0} + ... + min_wire_size_v<Es>);
    };

    namespace detail {

        // Shared by both, so pair and tuple cannot drift apart in a way the format would
        // notice. Es are the element types with cv stripped - a const element is written
        // and read as its underlying type.
        template <class T, class... Es>
        constexpr errc write_elements(writer auto& ar, const T& t) {
            errc code = errc::ok;
            [&]<::std::size_t... I>(::std::index_sequence<I...>) {
                (void)((code = dispatch_write<Es>(ar, ::std::get<I>(t)), code == errc::ok) && ...);
            }(::std::index_sequence_for<Es...>{});
            return code;
        }

        template <class T, class... Es>
        constexpr errc read_elements(reader auto& ar, T& t) {
            errc code = errc::ok;
            [&]<::std::size_t... I>(::std::index_sequence<I...>) {
                (void)((code = dispatch_read<Es>(ar, ::std::get<I>(t)), code == errc::ok) && ...);
            }(::std::index_sequence_for<Es...>{});
            return code;
        }

        template <class T>
        inline constexpr bool tuple_elements_fillable = false;

        template <class A, class B>
        inline constexpr bool tuple_elements_fillable<::std::pair<A, B>> =
            ::std::is_move_assignable_v<A> && ::std::is_move_assignable_v<B>;

        template <class... Es>
        inline constexpr bool tuple_elements_fillable<::std::tuple<Es...>> =
            (::std::is_move_assignable_v<Es> && ... && true);

    } // namespace detail

    // A pair is its first element followed by its second, which is the same bytes as a
    // two-field aggregate - and it hashes DIFFERENTLY, because "struct" and "pair" are
    // different tokens. That is the conservative direction: a hash that says "changed"
    // when the bytes did not is a rebuilt cache, while the reverse is a misread stream.
    template <class A, class B>
    struct schema<::std::pair<A, B>> {
        template <class Mode, class Seen>
        static consteval ::std::uint64_t mix(::std::uint64_t h) {
            h = detail::schema_text(h, "pair");
            h = detail::schema_of<::std::remove_cv_t<A>, Mode, Seen>(h);
            return detail::schema_of<::std::remove_cv_t<B>, Mode, Seen>(h);
        }
    };

    template <class... Es>
    struct schema<::std::tuple<Es...>> {
        template <class Mode, class Seen>
        static consteval ::std::uint64_t mix(::std::uint64_t h) {
            h = detail::schema_number(detail::schema_text(h, "tuple"), sizeof...(Es));
            ((h = detail::schema_of<::std::remove_cv_t<Es>, Mode, Seen>(h)), ...);
            return h;
        }
    };

    template <class A, class B>
    struct serializer<::std::pair<A, B>> {
        using pair_type = ::std::pair<A, B>;
        using first     = ::std::remove_cv_t<A>;
        using second    = ::std::remove_cv_t<B>;

        static constexpr errc write(writer auto& ar, const pair_type& p) {
            return detail::write_elements<pair_type, first, second>(ar, p);
        }

        static constexpr errc read(reader auto& ar, pair_type& p)
            requires (detail::tuple_elements_fillable<pair_type>)
        {
            return detail::read_elements<pair_type, first, second>(ar, p);
        }

        static constexpr pair_type make(reader auto& ar) {
            return pair_type{ detail::dispatch_make<first>(ar), detail::dispatch_make<second>(ar) };
        }
    };

    template <class... Es>
    struct serializer<::std::tuple<Es...>> {
        using tuple_type = ::std::tuple<Es...>;

        static constexpr errc write(writer auto& ar, const tuple_type& t) {
            return detail::write_elements<tuple_type, ::std::remove_cv_t<Es>...>(ar, t);
        }

        static constexpr errc read(reader auto& ar, tuple_type& t)
            requires (detail::tuple_elements_fillable<tuple_type>)
        {
            return detail::read_elements<tuple_type, ::std::remove_cv_t<Es>...>(ar, t);
        }

        static constexpr tuple_type make(reader auto& ar) {
            return tuple_type{ detail::dispatch_make<::std::remove_cv_t<Es>>(ar)... };
        }
    };

} // namespace ser
