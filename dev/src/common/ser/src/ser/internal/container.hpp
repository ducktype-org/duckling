#pragma once

// ── the length prefix, and the three checks that must precede any allocation ───
// The prefix is attacker-controlled data, so `n` is not a count until it has been checked
// against something real - and getting the ORDER wrong is the difference between an error
// code and an out-of-memory.
//
//   1. n does not fit in size_t                -> SizeOverflow  (the cast below)
//   2. n * MIN_WIRE_SIZE_V<E> overflows        -> SizeOverflow  (the multiply itself)
//   3. that many bytes are not there           -> Truncated     (what the stream can hold)
//
// Only then may a caller reserve or resize.

#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/detail/ovf.hpp>
#include <ser/errc.hpp>
#include <ser/traits.hpp>

#include <cstddef>
#include <utility>

namespace ser::detail {

	using wire_size_type = config_global::size_type;

	template<class E, writer Ar>
	constexpr Errc writeLength(Ar& ar, ::std::size_t n) {
		if constexpr (MIN_WIRE_SIZE_V<E> == 0) {
			if (n > config_global::MAX_ZERO_SIZE_ELEMENTS) return Errc::MessageSize;
		}
		return dispatchWrite<wire_size_type>(ar, static_cast<wire_size_type>(n));
	}

	template<class E, reader Ar>
	constexpr Errc readLength(Ar& ar, ::std::size_t& out) {
		wire_size_type n = 0;
		if (const auto c = dispatchRead<wire_size_type>(ar, n); c != Errc::Ok) return c;

		if (!::std::in_range<::std::size_t>(n)) return Errc::SizeOverflow;

		if constexpr (MIN_WIRE_SIZE_V<E> > 0) {
			::std::size_t lower_bound = 0;
			if (mulOvf(static_cast<::std::size_t>(n), MIN_WIRE_SIZE_V<E>, lower_bound))
				return Errc::SizeOverflow;
			if (lower_bound > ar.avail()) return Errc::Truncated;
		} else {
			if (n > config_global::MAX_ZERO_SIZE_ELEMENTS) return Errc::MessageSize;
		}

		out = static_cast<::std::size_t>(n);
		return Errc::Ok;
	}

}  // namespace ser::detail
