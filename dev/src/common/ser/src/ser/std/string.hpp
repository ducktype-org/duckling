#pragma once

// ── std::string and friends ───────────────────────────────────────────────────
// Length prefix, then the characters. Nothing about the encoding is on the wire and no
// terminator is written: a string is `n` and `n` characters, so an embedded '\0' is
// ordinary data and survives the round-trip.
//
// The adapters are ser::serializer<T> specializations - level 1, "the extension point for
// types you do not own". That is what lets them stay an opt-in include: dispatch consults
// the trait first, so <ser/ser.hpp> never has to know these types exist, and a user who
// wants a different format for their own std::vector<Thing> specializes the same trait
// more specifically and wins by partial ordering.
//
// A ONE-BYTE character type is copied in bulk, because there the two are the same stream:
// a single byte has no representation to swap, so a memcpy and a dispatch loop cannot
// disagree. char16_t and char32_t go one at a time and must - a bulk copy of those would
// write the host's byte order into the payload, which is the one thing this format does not
// do. The bulk path is skipped under constant evaluation as well: getting from Ch* to
// std::byte* needs a reinterpret_cast, which a constant expression may not contain.
//
// std::string_view is NOT here. It cannot own what it points at, so reading one would mean
// pointing into the input buffer, and a write-only adapter is refused by the library's own
// pairing rule - correctly: a format you can write and never read back is a bug with a use
// case.

#include <ser/concepts.hpp>
#include <ser/detail/bytes.hpp>
#include <ser/detail/container.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/serializer.hpp>
#include <ser/traits.hpp>

#include <cstddef>
#include <string>
#include <type_traits>

namespace ser {

	template<class Ch, class Tr, class Al>
	struct min_wire_size<::std::basic_string<Ch, Tr, Al>> {
		static constexpr ::std::size_t VALUE = sizeof(detail::wire_size_type);
	};

	// The character type is the format; the traits and the allocator are not on the wire
	// and are not hashed. See the note on ser::schema in hash.hpp for why an adapter says
	// this itself instead of letting a sizeof stand in for it.
	template<class Ch, class Tr, class Al>
	struct schema<::std::basic_string<Ch, Tr, Al>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return detail::schemaOf<Ch, Mode, Seen>(detail::schemaText(h, "string"));
		}
	};

	template<class Ch, class Tr, class Al>
	struct serializer<::std::basic_string<Ch, Tr, Al>> {
		using string_type = ::std::basic_string<Ch, Tr, Al>;

		static constexpr bool BULK = sizeof(Ch) == 1 && ::std::is_trivially_copyable_v<Ch>;

		static constexpr Errc write(writer auto& ar, const string_type& s) {
			if (const auto c = detail::writeLength(ar, s.size()); c != Errc::Ok) return c;

			if constexpr (BULK)
				if !consteval {
					return ar.rawWrite({ reinterpret_cast<const ::std::byte*>(s.data()), s.size() });
				}

			for (const Ch ch: s)
				if (const auto c = detail::dispatchWrite<Ch>(ar, ch); c != Errc::Ok) return c;
			return Errc::Ok;
		}

		static constexpr Errc read(reader auto& ar, string_type& s) {
			::std::size_t n = 0;
			if (const auto c = detail::readLength<Ch>(ar, n); c != Errc::Ok) return c;

			// resize AFTER the checks in readLength, never before - that is the whole
			// point of them. Cleared first so a partial read cannot leave the tail of a
			// previous value behind.
			s.clear();
			s.resize(n);

			if constexpr (BULK)
				if !consteval {
					// ensure() before take(), which has it as a precondition. readLength
					// has already bounded n, so this cannot fail - it is what makes that
					// guarantee local instead of remote.
					if (const auto c = ar.ensure(n); c != Errc::Ok) return c;
					detail::copyBytes(
						reinterpret_cast<::std::byte*>(s.data()), ar.take(n).data(), n
					);
					return Errc::Ok;
				}

			for (::std::size_t i = 0; i < n; ++i)
				if (const auto c = detail::dispatchRead<Ch>(ar, s[i]); c != Errc::Ok) return c;
			return Errc::Ok;
		}
	};

}  // namespace ser
