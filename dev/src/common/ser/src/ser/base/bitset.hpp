#pragma once

/*
 * base::DynamicBitset
 * The bit count, then ceil(count / 64) words of 64 bits, low word first and each word
 * through dispatch - so the stream is the same on a big-endian machine as on a little-
 * endian one, and a bitset written by one reads back on the other.
 *
 * The capacity is in the stream because it is not recoverable from the words: a bitset of 3
 * bits and one of 64 both occupy exactly one word, and they are not the same object - the
 * type is not resizable, so its size is data.
 *
 * The words are rebuilt through set() rather than written into the vector directly, because
 * `words` is private. A stream whose last word has bits above the capacity is refused
 * rather than loaded: loading it would leave count(), any() and forEachSet() disagreeing
 * with size() forever after.
 */

#include <base/collections/dynamic_bitset.hpp>
#include <base/numeric/numeric_utils.hpp>
#include <base/numeric/overflow.hpp>

#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/internal/container.hpp>
#include <ser/internal/dispatch_fwd.hpp>
#include <ser/serializer.hpp>
#include <ser/traits.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace ser {

	namespace internal {

		inline constexpr ::std::size_t BITSET_BITS_PER_WORD = 64;

		[[nodiscard]] constexpr ::std::size_t bitsetWordCount(::std::size_t bits) noexcept {
			return (bits + BITSET_BITS_PER_WORD - 1) / BITSET_BITS_PER_WORD;
		}

		/**
		 * @brief The checks readLength makes, redone for a count of BITS. readLength cannot do this
		 * one: its bound is n * MIN_SERIALIZED_SIZE_V<E> bytes, and a bit costs an eighth of a
		 * byte, so it would refuse every bitset wider than the stream holding it. The order matters
		 * for the same reason as there - each check makes the next one safe to perform.
		 */
		constexpr Errc readBitCount(Reader auto& ar, ::std::size_t& out) {
			LengthType bits = 0;
			if (const auto c = dispatchRead<LengthType>(ar, bits); c != Errc::Ok) return c;
			if (!::base::fitsIn<::std::size_t>(bits)) return Errc::SizeOverflow;

			/*
			 * bitsetWordCount rounds UP, so a count within a word of the maximum would
			 * overflow on the way to the byte figure that is the real bound.
			 */
			const auto bit_count = static_cast<::std::size_t>(bits);
			if (bit_count > ::std::numeric_limits<::std::size_t>::max() - (BITSET_BITS_PER_WORD - 1))
				return Errc::SizeOverflow;

			::std::size_t bytes = 0;
			if (::base::mulOvf(bitsetWordCount(bit_count), sizeof(::u64), bytes))
				return Errc::SizeOverflow;
			if (bytes > ar.avail()) return Errc::Truncated;

			out = bit_count;
			return Errc::Ok;
		}

	} /* namespace internal */

	template<>
	struct MinSerializedSize<::base::DynamicBitset> {
		static constexpr ::std::size_t VALUE = sizeof(internal::LengthType);
	};

	template<>
	struct Schema<::base::DynamicBitset> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return internal::schemaOf<::u64, Mode, Seen>(
				internal::schemaText(h, "base.dynamic_bitset")
			);
		}
	};

	template<>
	struct Serializer<::base::DynamicBitset> {
		static constexpr Errc write(Writer auto& ar, const ::base::DynamicBitset& b) {
			const ::std::size_t bits = b.size();
			if (const auto c = internal::dispatchWrite<internal::LengthType>(
					ar, static_cast<internal::LengthType>(bits)
				);
			    c != Errc::Ok)
				return c;

			for (::std::size_t w = 0; w < internal::bitsetWordCount(bits); ++w) {
				::u64 word = 0;
				for (::std::size_t i = 0; i < internal::BITSET_BITS_PER_WORD; ++i) {
					const ::std::size_t bit = w * internal::BITSET_BITS_PER_WORD + i;
					if (bit < bits && b.test(bit)) word |= (::u64{ 1 } << i);
				}
				if (const auto c = internal::dispatchWrite<::u64>(ar, word); c != Errc::Ok)
					return c;
			}
			return Errc::Ok;
		}

		static constexpr Errc read(Reader auto& ar, ::base::DynamicBitset& b) {
			::std::size_t bits = 0;
			if (const auto c = internal::readBitCount(ar, bits); c != Errc::Ok) return c;

			/*
			 * Assigned rather than filled: the capacity is fixed at construction, so there
			 * is no way to resize the one that is already here.
			 */
			b = ::base::DynamicBitset(bits);

			for (::std::size_t w = 0; w < internal::bitsetWordCount(bits); ++w) {
				::u64 word = 0;
				if (const auto c = internal::dispatchRead<::u64>(ar, word); c != Errc::Ok) return c;

				for (::std::size_t i = 0; i < internal::BITSET_BITS_PER_WORD; ++i) {
					if ((word & (::u64{ 1 } << i)) == 0) continue;

					const ::std::size_t bit = w * internal::BITSET_BITS_PER_WORD + i;
					if (bit >= bits) return Errc::InvalidValue; /* set above the capacity */
					b.set(bit);
				}
			}
			return Errc::Ok;
		}
	};

} /* namespace ser */
