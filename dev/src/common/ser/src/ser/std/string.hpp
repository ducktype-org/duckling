#pragma once

// ── std::string and friends ───────────────────────────────────────────────────
// Length prefix, then the characters. Nothing about the encoding is on the wire and no
// terminator is written: a string is `n` and `n` characters, so an embedded '\0' is
// ordinary data and survives the round-trip.
//
// The adapters are ser::serializer<T> specializations - level 1, "the extension point for
// types you do not own". That is what lets them stay an opt-in include: the ladder already
// consults the trait first, so <ser/ser.hpp> never has to know these types exist, and a
// user who wants a different format for their own std::vector<Thing> specializes the same
// trait more specifically and wins by partial ordering.
//
// Characters go through dispatch one at a time rather than as a block. A bulk copy is the
// is_flat_v optimization, which is out of M1 on purpose - it has to prove byte identity
// against this loop first, and this loop is also what makes a string writable in a
// constexpr context.
//
// std::string_view is NOT here. It cannot own what it points at, so reading one means
// pointing into the input buffer - zero-copy, out of scope for M1 - and a write-only
// adapter is refused by the library's own pairing rule, which is the correct answer: a
// format you can write and never read back is a bug with a use case.

#include <ser/concepts.hpp>
#include <ser/errc.hpp>
#include <ser/serializer.hpp>
#include <ser/hash.hpp>
#include <ser/traits.hpp>

#include <ser/detail/container.hpp>
#include <ser/detail/dispatch_fwd.hpp>

#include <cstddef>
#include <string>

namespace ser {

    template <class Ch, class Tr, class Al>
    struct min_wire_size<::std::basic_string<Ch, Tr, Al>> {
        static constexpr ::std::size_t value = sizeof(detail::wire_size_type);
    };

    // The character type is the format; the traits and the allocator are not on the wire
    // and are not hashed. See the note on ser::schema in hash.hpp for why an adapter says
    // this itself instead of letting a sizeof stand in for it.
    template <class Ch, class Tr, class Al>
    struct schema<::std::basic_string<Ch, Tr, Al>> {
        template <class Mode, class Seen>
        static consteval ::std::uint64_t mix(::std::uint64_t h) {
            return detail::schema_of<Ch, Mode, Seen>(detail::schema_text(h, "string"));
        }
    };

    template <class Ch, class Tr, class Al>
    struct serializer<::std::basic_string<Ch, Tr, Al>> {
        using string_type = ::std::basic_string<Ch, Tr, Al>;

        static constexpr errc write(writer auto& ar, const string_type& s) {
            if (const auto c = detail::write_length(ar, s.size()); c != errc::ok) return c;
            for (const Ch ch : s)
                if (const auto c = detail::dispatch_write<Ch>(ar, ch); c != errc::ok) return c;
            return errc::ok;
        }

        static constexpr errc read(reader auto& ar, string_type& s) {
            ::std::size_t n = 0;
            if (const auto c = detail::read_length<Ch>(ar, n); c != errc::ok) return c;

            // resize AFTER the checks in read_length, never before - that is the whole
            // point of them. Cleared first so a partial read cannot leave the tail of a
            // previous value behind.
            s.clear();
            s.resize(n);
            for (::std::size_t i = 0; i < n; ++i)
                if (const auto c = detail::dispatch_read<Ch>(ar, s[i]); c != errc::ok) return c;
            return errc::ok;
        }
    };

} // namespace ser
