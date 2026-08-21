#pragma once

// ── SER_TEST_ROUNDTRIP ────────────────────────────────────────────────────────
// The check that C++23 makes necessary. SER_MAKE_FROM verifies the argument types
// against the constructor, which catches two swapped fields of DIFFERENT types at
// compile time - and cannot catch two swapped fields of the same type, because that
// permutation compiles perfectly and simply writes the wrong bytes.
//
// So this round-trips a sample and compares field by field, and names the field that
// came back wrong. That is a condition of using SER_MAKE_FROM_MEMBERS, not a suggestion:
// that variant has no field list to check anything against.
//
// A separate header because it is the one place in the library that needs a heap buffer,
// and <vector> has no business in the umbrella for something only tests call.

#include <ser/errc.hpp>
#include <ser/macros.hpp>
#include <ser/ser.hpp>

#include <cstddef>
#include <cstdio>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace ser {

    inline constexpr ::std::size_t no_field = static_cast<::std::size_t>(-1);

    struct roundtrip_report {
        errc          code  = errc::ok;         // the round-trip itself
        ::std::size_t field = no_field;         // first field that came back different
        const char*   name  = nullptr;          // its name, when SER_DESCRIBE gave one

        [[nodiscard]] constexpr bool ok() const noexcept {
            return code == errc::ok && field == no_field;
        }
    };

    namespace detail {

        // The described fields when the type says which they are, every field otherwise.
        // The distinction matters: SER_DESCRIBE deliberately leaves fields out, and those
        // come back default-constructed, which is not a failure.
        //
        // Asked through ser::access, because SER_DESCRIBE is normally written in a
        // private section: the question and the fields have the same access, and a
        // detector here would answer false and quietly compare the skipped fields too.
        template <class T>
        constexpr auto roundtrip_tie(const T& x) {
            if constexpr (access::has_described_v<T>) {
                return access::described(x);
            } else {
                // tie_members goes straight to the ladder with member_count_v, past the
                // check that visit_members makes - so for a type nobody can enumerate it
                // does not fail, it yields an EMPTY tuple, and comparing no fields reports
                // every round-trip as faithful. A test that cannot fail is worse than no
                // test, so the condition is stated here rather than assumed.
                static_assert(can_enumerate_members_v<T>,
                    "ser: SER_TEST_ROUNDTRIP cannot see this type's fields, so it would "
                    "compare nothing and pass. Add SER_DESCRIBE(a, b) to the class - it "
                    "names the fields to compare and gives the report their names - or "
                    "declare using ser_members = ser::members<N>; with SER_FRIEND.");
                return tie_members(x);
            }
        }

        template <class Tuple, ::std::size_t... I>
        constexpr ::std::size_t first_difference(const Tuple& a, const Tuple& b,
                                                 ::std::index_sequence<I...>) {
            ::std::size_t bad = no_field;
            ((bad == no_field && !(::std::get<I>(a) == ::std::get<I>(b)) ? (bad = I) : 0u), ...);
            return bad;
        }

        template <class T>
        constexpr ::std::size_t compare_fields(const T& a, const T& b) {
            const auto ta = roundtrip_tie(a);
            const auto tb = roundtrip_tie(b);
            return first_difference(ta, tb,
                ::std::make_index_sequence<::std::tuple_size_v<decltype(ta)>>{});
        }

        template <class T>
        constexpr const char* field_name(::std::size_t i) {
            if constexpr (access::has_field_names_v<T>) return access::field_name<T>(i);
            else                                       return nullptr;
        }

        inline bool report_roundtrip(const char* what, const roundtrip_report& r) {
            if (r.ok()) return true;
            if (r.code != errc::ok) {
                ::std::printf("    SER_TEST_ROUNDTRIP(%s): %s\n", what, to_string(r.code));
            } else if (r.name != nullptr) {
                ::std::printf("    SER_TEST_ROUNDTRIP(%s): field %zu (%s) came back different\n",
                              what, r.field, r.name);
            } else {
                ::std::printf("    SER_TEST_ROUNDTRIP(%s): field %zu came back different "
                              "(no field names - add SER_DESCRIBE to get one)\n", what, r.field);
            }
            return false;
        }

    } // namespace detail

    // Writes the sample, reads it back and compares. read_or_throw rather than read, so
    // that a type which cannot be moved is testable too - that is exactly the kind of
    // type SER_MAKE_FROM exists for.
    template <class T>
    [[nodiscard]] roundtrip_report test_roundtrip(const T& sample) {
        ::std::vector<::std::byte> buf;
        if (const auto w = write(buf, sample); !w.has_value())
            return roundtrip_report{w.code()};

        try {
            const auto back = read_or_throw<T>(::std::span<const ::std::byte>{buf.data(), buf.size()});
            const auto bad  = detail::compare_fields(sample, *back);
            return roundtrip_report{errc::ok, bad,
                                    bad == no_field ? nullptr : detail::field_name<T>(bad)};
        } catch (const exception& e) {
            return roundtrip_report{e.err().code};
        }
    }

} // namespace ser

// Yields true when the round-trip is faithful, and prints which field is not otherwise.
// Meant to sit inside whatever the project's check macro is:  CHECK(SER_TEST_ROUNDTRIP(x));
// Variadic because only PARENTHESES hide a comma from the preprocessor - braces do not -
// and the samples worth testing are braced: SER_TEST_ROUNDTRIP(Vec3{1, 2, 3}).
#define SER_TEST_ROUNDTRIP(...) \
    (::ser::detail::report_roundtrip(#__VA_ARGS__, ::ser::test_roundtrip(__VA_ARGS__)))
