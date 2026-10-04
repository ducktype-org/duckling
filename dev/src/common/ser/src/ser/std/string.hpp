#pragma once

/*
 * std::string and friends
 * Length prefix, then the characters. Nothing about the encoding is on the wire and no
 * terminator is written: a string is `n` and `n` characters, so an embedded '\0' is
 * ordinary data and survives the round-trip.
 *
 * The adapters are ser::Serializer<T> specializations, which is what lets them stay an
 * opt-in include: dispatch consults the trait first, so <ser/ser.hpp> never has to know
 * these types exist.
 *
 * A ONE-BYTE character type is copied in bulk, because a single byte has no representation
 * to swap and a memcpy cannot disagree with a dispatch loop. char16_t and char32_t go one at
 * a time so that a custom serializer<Ch> is honoured - NOT for byte order: nothing in this
 * library swaps bytes, so the element path is byte-identical native-endian output too, and
 * endianness is the envelope's job through StreamHeader::flags and PlatformMismatch. The
 * bulk path is also skipped under constant evaluation, which may not contain a
 * reinterpret_cast.
 *
 * std::string_view is NOT here: it cannot own what it points at, so reading one could only
 * point into the input buffer, and a write-only adapter is refused by the library's own
 * pairing rule.
 */

#include <ser/concepts.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/internal/bytes.hpp>
#include <ser/internal/container.hpp>
#include <ser/internal/dispatch_fwd.hpp>
#include <ser/serializer.hpp>
#include <ser/traits.hpp>

#include <cstddef>
#include <string>
#include <type_traits>

namespace ser {

	template<class Ch, class Tr, class Al>
	struct MinWireSize<::std::basic_string<Ch, Tr, Al>> {
		static constexpr ::std::size_t VALUE = sizeof(internal::WireSizeType);
	};

	/**
	 * @brief The character type is the format; the traits and the allocator are not on the wire
	 * and are not hashed. See the note on ser::Schema in hash.hpp for why an adapter says
	 * this itself instead of letting a sizeof stand in for it.
	 */
	template<class Ch, class Tr, class Al>
	struct Schema<::std::basic_string<Ch, Tr, Al>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return internal::schemaOf<Ch, Mode, Seen>(internal::schemaText(h, "string"));
		}
	};

	template<class Ch, class Tr, class Al>
	struct Serializer<::std::basic_string<Ch, Tr, Al>> {
		using StringType = ::std::basic_string<Ch, Tr, Al>;

		static constexpr bool BULK = sizeof(Ch) == 1 && ::std::is_trivially_copyable_v<Ch>;

		static constexpr Errc write(Writer auto& ar, const StringType& s) {
			if (const auto c = internal::writeLength<Ch>(ar, s.size()); c != Errc::Ok) return c;

			if constexpr (BULK)
				if !consteval {
					return ar.rawWrite({ reinterpret_cast<const ::std::byte*>(s.data()), s.size() });
				}

			for (const Ch ch: s)
				if (const auto c = internal::dispatchWrite<Ch>(ar, ch); c != Errc::Ok) return c;
			return Errc::Ok;
		}

		static constexpr Errc read(Reader auto& ar, StringType& s) {
			::std::size_t n = 0;
			if (const auto c = internal::readLength<Ch>(ar, n); c != Errc::Ok) return c;

			/*
			 * resize AFTER the checks in readLength, never before - that is the whole
			 * point of them. Cleared first so a partial read cannot leave the tail of a
			 * previous value behind.
			 */
			s.clear();
			s.resize(n);

			if constexpr (BULK)
				if !consteval {
					/*
					 * ensure() before take(), which has it as a precondition. readLength
					 * has already bounded n, so this cannot fail - it is what makes that
					 * guarantee local instead of remote.
					 */
					if (const auto c = ar.ensure(n); c != Errc::Ok) return c;
					internal::copyBytes(
						reinterpret_cast<::std::byte*>(s.data()), ar.take(n).data(), n
					);
					return Errc::Ok;
				}

			for (::std::size_t i = 0; i < n; ++i)
				if (const auto c = internal::dispatchRead<Ch>(ar, s[i]); c != Errc::Ok) return c;
			return Errc::Ok;
		}
	};

} /* namespace ser */
