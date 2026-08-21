#pragma once

// ── the length prefix, and the three checks that must precede any allocation ───
// Every variable-length adapter starts the same way, and getting the ORDER wrong is the
// difference between an error code and an out-of-memory: the prefix is attacker-controlled
// data, so `n` is not a count until it has been checked against something real.
//
//   1. n > MAX_CONTAINER_ELEMENTS            -> MessageSize   (a policy ceiling)
//   2. n * MIN_WIRE_SIZE_V<E> overflows      -> SizeOverflow   (the multiply itself)
//   3. that many bytes are not there         -> Truncated       (what the stream can hold)
//
// Only then may a caller reserve or resize. Invariant 9 in CLAUDE.md is this function.
//
// An element whose minimum is zero - an empty type - skips steps 2 and 3, because no
// number of them implies any bytes at all. The policy ceiling is what bounds that case,
// and it is the only thing that can.

#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/detail/ovf.hpp>
#include <ser/errc.hpp>
#include <ser/traits.hpp>

#include <cstddef>

namespace ser::detail {

	using wire_size_type = config_global::size_type;

	// size_t -> u64 is the identity on every platform ser supports, which is why
	// config_global::size_type is u64 and no adapter needs an overflow check on write.
	template<writer Ar>
	constexpr Errc writeLength(Ar& ar, ::std::size_t n) {
		return dispatchWrite<wire_size_type>(ar, static_cast<wire_size_type>(n));
	}

	template<class E, reader Ar>
	constexpr Errc readLength(Ar& ar, ::std::size_t& out) {
		wire_size_type n = 0;
		if (const auto c = dispatchRead<wire_size_type>(ar, n); c != Errc::Ok) return c;

		if (n > config_global::MAX_CONTAINER_ELEMENTS) return Errc::MessageSize;

		if constexpr (MIN_WIRE_SIZE_V<E> > 0) {
			::std::size_t lower_bound = 0;
			if (mulOvf(static_cast<::std::size_t>(n), MIN_WIRE_SIZE_V<E>, lower_bound))
				return Errc::SizeOverflow;
			if (lower_bound > ar.avail()) return Errc::Truncated;
		}

		out = static_cast<::std::size_t>(n);
		return Errc::Ok;
	}

}  // namespace ser::detail
