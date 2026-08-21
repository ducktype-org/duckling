#pragma once

// ── std::map, std::unordered_map, std::set, std::unordered_set ────────────────
// Length prefix, then the elements: for a map each key followed by its value, for a set
// each key. The container's own structure - buckets, tree shape, load factor - is not on
// the wire and is rebuilt by the reader, which is why a map written by one implementation
// reads back on another.
//
// THE KEY AND THE VALUE ARE READ AS TWO STATEMENTS, never as two arguments of one call.
// `m.emplace(dispatch_make<K>(ar), dispatch_make<V>(ar))` looks obvious and is invariant 3
// all over again: those are function arguments, their evaluation order is unspecified, and
// MSVC evaluates right to left - a stream that is key-first on one compiler and value-first
// on another, passing every round-trip test on both while agreeing with neither. Two
// declarations are ordered by the language, so that is what this uses. The cost is one move
// per element into the node, and value_type being pair<const K, V> means the node has to be
// constructed rather than assigned anyway.
//
// A repeated key is corrupt input, not a merge: insert reports it and the read fails with
// errc::invalid_value. Silently keeping the first one would turn a damaged stream into a
// container that is quietly smaller than the one that was written.
//
// On the unordered containers, iteration order is unspecified, so the STREAM IS NOT A
// CANONICAL FORM of the object: one map written twice gives the same bytes, but two EQUAL
// maps need not - a different insertion history is enough, at the same bucket count. Two
// consequences: compare a round-trip element by element and never byte for byte, and reach
// for std::map when a payload is going to be signed or content-addressed.

#include <ser/concepts.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/serializer.hpp>
#include <ser/std/tuple.hpp>
#include <ser/traits.hpp>

#include <ser/detail/container.hpp>
#include <ser/detail/dispatch_fwd.hpp>

#include <cstddef>
#include <map>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace ser {

    namespace detail {

        // Only the hash-based containers have it, and asking the container rather than
        // listing the types keeps this working for one with a custom allocator.
        template <class C>
        constexpr void reserve_if_possible(C& c, ::std::size_t n) {
            if constexpr (requires { c.reserve(n); }) c.reserve(n);
        }

        template <class M, class K, class V>
        struct map_adapter {
            static constexpr errc write(writer auto& ar, const M& m) {
                if (const auto c = write_length(ar, m.size()); c != errc::ok) return c;
                for (const auto& [key, value] : m) {
                    if (const auto c = dispatch_write<K>(ar, key);   c != errc::ok) return c;
                    if (const auto c = dispatch_write<V>(ar, value); c != errc::ok) return c;
                }
                return errc::ok;
            }

            static constexpr errc read(reader auto& ar, M& m) {
                ::std::size_t n = 0;
                if (const auto c = read_length<::std::pair<K, V>>(ar, n); c != errc::ok) return c;

                m.clear();
                reserve_if_possible(m, n);
                for (::std::size_t i = 0; i < n; ++i) {
                    auto key   = dispatch_make<K>(ar);      // two statements, and that is
                    auto value = dispatch_make<V>(ar);      // the whole point - see above
                    if (!m.emplace(::std::move(key), ::std::move(value)).second)
                        return errc::invalid_value;         // the same key twice
                }
                return errc::ok;
            }
        };

        template <class S, class K>
        struct set_adapter {
            static constexpr errc write(writer auto& ar, const S& s) {
                if (const auto c = write_length(ar, s.size()); c != errc::ok) return c;
                for (const auto& key : s)
                    if (const auto c = dispatch_write<K>(ar, key); c != errc::ok) return c;
                return errc::ok;
            }

            static constexpr errc read(reader auto& ar, S& s) {
                ::std::size_t n = 0;
                if (const auto c = read_length<K>(ar, n); c != errc::ok) return c;

                s.clear();
                reserve_if_possible(s, n);
                for (::std::size_t i = 0; i < n; ++i)
                    if (!s.insert(dispatch_make<K>(ar)).second) return errc::invalid_value;
                return errc::ok;
            }
        };

    } // namespace detail

    // ── the schema of a keyed container ───────────────────────────────────────
    // std::map and std::unordered_map hash IDENTICALLY, and so do the two sets. They have
    // to: the wire format is the same length prefix followed by the same elements, and a
    // stream written from one really does read back into the other - the comparator, the
    // hash and the bucket count are not on the wire. Hashing them apart would refuse a
    // stream that is perfectly readable.
    //
    // What is NOT the same is ordering: a std::map stream is sorted, an unordered one is
    // in whatever order the buckets were walked. That is a property of the bytes, not of
    // the schema, and the note at the top of this file is where it belongs.
    namespace detail {

        template <class Mode, class Seen, class K, class V>
        [[nodiscard]] consteval ::std::uint64_t schema_map(::std::uint64_t h) {
            h = schema_text(h, "map");
            h = schema_of<K, Mode, Seen>(h);
            return schema_of<V, Mode, Seen>(h);
        }

        template <class Mode, class Seen, class K>
        [[nodiscard]] consteval ::std::uint64_t schema_set(::std::uint64_t h) {
            return schema_of<K, Mode, Seen>(schema_text(h, "set"));
        }

    } // namespace detail

    template <class K, class V, class C, class Al>
    struct schema<::std::map<K, V, C, Al>> {
        template <class Mode, class Seen>
        static consteval ::std::uint64_t mix(::std::uint64_t h) {
            return detail::schema_map<Mode, Seen, K, V>(h);
        }
    };

    template <class K, class V, class H, class E, class Al>
    struct schema<::std::unordered_map<K, V, H, E, Al>> {
        template <class Mode, class Seen>
        static consteval ::std::uint64_t mix(::std::uint64_t h) {
            return detail::schema_map<Mode, Seen, K, V>(h);
        }
    };

    template <class K, class C, class Al>
    struct schema<::std::set<K, C, Al>> {
        template <class Mode, class Seen>
        static consteval ::std::uint64_t mix(::std::uint64_t h) {
            return detail::schema_set<Mode, Seen, K>(h);
        }
    };

    template <class K, class H, class E, class Al>
    struct schema<::std::unordered_set<K, H, E, Al>> {
        template <class Mode, class Seen>
        static consteval ::std::uint64_t mix(::std::uint64_t h) {
            return detail::schema_set<Mode, Seen, K>(h);
        }
    };

    // ── the four specializations ──────────────────────────────────────────────
    template <class K, class V, class C, class Al>
    struct min_wire_size<::std::map<K, V, C, Al>> {
        static constexpr ::std::size_t value = sizeof(detail::wire_size_type);
    };
    template <class K, class V, class H, class E, class Al>
    struct min_wire_size<::std::unordered_map<K, V, H, E, Al>> {
        static constexpr ::std::size_t value = sizeof(detail::wire_size_type);
    };
    template <class K, class C, class Al>
    struct min_wire_size<::std::set<K, C, Al>> {
        static constexpr ::std::size_t value = sizeof(detail::wire_size_type);
    };
    template <class K, class H, class E, class Al>
    struct min_wire_size<::std::unordered_set<K, H, E, Al>> {
        static constexpr ::std::size_t value = sizeof(detail::wire_size_type);
    };

    template <class K, class V, class C, class Al>
    struct serializer<::std::map<K, V, C, Al>>
        : detail::map_adapter<::std::map<K, V, C, Al>, K, V> {};

    template <class K, class V, class H, class E, class Al>
    struct serializer<::std::unordered_map<K, V, H, E, Al>>
        : detail::map_adapter<::std::unordered_map<K, V, H, E, Al>, K, V> {};

    template <class K, class C, class Al>
    struct serializer<::std::set<K, C, Al>>
        : detail::set_adapter<::std::set<K, C, Al>, K> {};

    template <class K, class H, class E, class Al>
    struct serializer<::std::unordered_set<K, H, E, Al>>
        : detail::set_adapter<::std::unordered_set<K, H, E, Al>, K> {};

} // namespace ser
