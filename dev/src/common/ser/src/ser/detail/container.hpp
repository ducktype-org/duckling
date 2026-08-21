#pragma once

// ── the length prefix, and the three checks that must precede any allocation ───
// Every variable-length adapter starts the same way, and getting the ORDER wrong is the
// difference between an error code and an out-of-memory: the prefix is attacker-controlled
// data, so `n` is not a count until it has been checked against something real.
//
//   1. n > max_container_elements            -> message_size   (a policy ceiling)
//   2. n * min_wire_size_v<E> overflows      -> size_overflow   (the multiply itself)
//   3. that many bytes are not there         -> truncated       (what the stream can hold)
//
// Only then may a caller reserve or resize. Invariant 9 in CLAUDE.md is this function.
//
// An element whose minimum is zero - an empty type - skips steps 2 and 3, because no
// number of them implies any bytes at all. The policy ceiling is what bounds that case,
// and it is the only thing that can.

#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/errc.hpp>
#include <ser/traits.hpp>

#include <ser/detail/dispatch_fwd.hpp>
#include <ser/detail/ovf.hpp>

#include <cstddef>

namespace ser::detail {

    using wire_size_type = config_global::size_type;

    // size_t -> u64 is the identity on every platform ser supports, which is why
    // config_global::size_type is u64 and no adapter needs an overflow check on write.
    template <writer Ar>
    constexpr errc write_length(Ar& ar, ::std::size_t n) {
        return dispatch_write<wire_size_type>(ar, static_cast<wire_size_type>(n));
    }

    template <class E, reader Ar>
    constexpr errc read_length(Ar& ar, ::std::size_t& out) {
        wire_size_type n = 0;
        if (const auto c = dispatch_read<wire_size_type>(ar, n); c != errc::ok) return c;

        if (n > config_global::max_container_elements) return errc::message_size;

        if constexpr (min_wire_size_v<E> > 0) {
            ::std::size_t lower_bound = 0;
            if (mul_ovf(static_cast<::std::size_t>(n), min_wire_size_v<E>, lower_bound))
                return errc::size_overflow;
            if (lower_bound > ar.avail()) return errc::truncated;
        }

        out = static_cast<::std::size_t>(n);
        return errc::ok;
    }

} // namespace ser::detail
