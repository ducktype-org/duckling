#pragma once

// ── base::DynamicBitset ───────────────────────────────────────────────────────
// The bit count, then ceil(count / 64) words of 64 bits, low word first and each word
// through dispatch - so the stream is the same on a big-endian machine as on a little-
// endian one, and a bitset written by one reads back on the other.
//
// The capacity is on the wire because it is not recoverable from the words: a bitset of 3
// bits and one of 64 both occupy exactly one word, and they are not the same object - the
// type is not resizable, so its size is data.
//
// The words are rebuilt through set() rather than written into the vector directly, because
// `words` is private. A stream whose last word has bits above the capacity is refused
// rather than loaded: loading it would leave count(), any() and forEachSet() disagreeing
// with size() forever after.

#include <base/collections/dynamic_bitset.hpp>

#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/detail/container.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/detail/ovf.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/serializer.hpp>
#include <ser/traits.hpp>

#include <cstddef>
#include <cstdint>

namespace ser {

	namespace detail {

		inline constexpr ::std::size_t BITSET_BITS_PER_WORD = 64;

		[[nodiscard]] constexpr ::std::size_t bitsetWordCount(::std::size_t bits) noexcept {
			return (bits + BITSET_BITS_PER_WORD - 1) / BITSET_BITS_PER_WORD;
		}

		// The three checks readLength makes, redone for a count of BITS. readLength cannot do
		// this one: its bound is n * MIN_WIRE_SIZE_V<E> bytes, and a bit costs an eighth of a
		// byte, so it would refuse every bitset wider than the stream holding it. The order
		// matters for the same reason as there - policy ceiling first, so the word arithmetic
		// below cannot overflow on the way to finding out that it would have.
		constexpr Errc readBitCount(reader auto& ar, ::std::size_t& out) {
			wire_size_type bits = 0;
			if (const auto c = dispatchRead<wire_size_type>(ar, bits); c != Errc::Ok) return c;
			if (bits > config_global::MAX_CONTAINER_ELEMENTS) return Errc::MessageSize;

			::std::size_t bytes = 0;
			if (mulOvf(bitsetWordCount(static_cast<::std::size_t>(bits)), sizeof(::u64), bytes))
				return Errc::SizeOverflow;
			if (bytes > ar.avail()) return Errc::Truncated;

			out = static_cast<::std::size_t>(bits);
			return Errc::Ok;
		}

	}  // namespace detail

	template<>
	struct min_wire_size<::base::DynamicBitset> {
		static constexpr ::std::size_t VALUE = sizeof(detail::wire_size_type);
	};

	template<>
	struct schema<::base::DynamicBitset> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return detail::schemaOf<::u64, Mode, Seen>(detail::schemaText(h, "base.dynamic_bitset"));
		}
	};

	template<>
	struct serializer<::base::DynamicBitset> {
		static constexpr Errc write(writer auto& ar, const ::base::DynamicBitset& b) {
			const ::std::size_t bits = b.size();
			if (const auto c = detail::writeLength(ar, bits); c != Errc::Ok) return c;

			for (::std::size_t w = 0; w < detail::bitsetWordCount(bits); ++w) {
				::u64 word = 0;
				for (::std::size_t i = 0; i < detail::BITSET_BITS_PER_WORD; ++i) {
					const ::std::size_t bit = w * detail::BITSET_BITS_PER_WORD + i;
					if (bit < bits && b.test(bit)) word |= (::u64{ 1 } << i);
				}
				if (const auto c = detail::dispatchWrite<::u64>(ar, word); c != Errc::Ok) return c;
			}
			return Errc::Ok;
		}

		static constexpr Errc read(reader auto& ar, ::base::DynamicBitset& b) {
			::std::size_t bits = 0;
			if (const auto c = detail::readBitCount(ar, bits); c != Errc::Ok) return c;

			// Assigned rather than filled: the capacity is fixed at construction, so there
			// is no way to resize the one that is already here.
			b = ::base::DynamicBitset(bits);

			for (::std::size_t w = 0; w < detail::bitsetWordCount(bits); ++w) {
				::u64 word = 0;
				if (const auto c = detail::dispatchRead<::u64>(ar, word); c != Errc::Ok) return c;

				for (::std::size_t i = 0; i < detail::BITSET_BITS_PER_WORD; ++i) {
					if ((word & (::u64{ 1 } << i)) == 0) continue;

					const ::std::size_t bit = w * detail::BITSET_BITS_PER_WORD + i;
					if (bit >= bits) return Errc::InvalidValue;  // set above the capacity
					b.set(bit);
				}
			}
			return Errc::Ok;
		}
	};

}  // namespace ser
