/**
 * @file dynamic_bitset.hpp
 *
 * @brief A fixed-capacity, heap-backed set of bits indexed by `usize`, with cheap word-parallel
 * set operations (union, intersection, difference), equality and set-bit iteration.
 *
 * It is meant for dense integer-keyed sets — e.g. dataflow analyses over contiguously-numbered ids
 * — where every operation is O(capacity / 64). It is NOT resizable after construction: all bitsets
 * combined together must share the same capacity.
 */
#pragma once

#include <base/except/exceptions.hpp>
#include <base/types/ints.hpp>  // IWYU pragma: export

#include <algorithm>
#include <bit>
#include <vector>

namespace base {
	/**
	 * @brief A set of bits with a fixed capacity, backed by an array of 64-bit words.
	 *
	 * Bit @p i is stored in bit `i % 64` of word `i / 64`. Bits at indices `>= capacity` (the
	 * unused high bits of the last word) are kept zero at all times, so `operator==` and the
	 * word-parallel set operations can work word-by-word without masking on every call.
	 */
	class DynamicBitset final {
		std::vector<u64> words;
		usize            bit_count = 0;

		static constexpr usize BITS_PER_WORD = 64;

		[[nodiscard]] static constexpr usize wordCount(usize bits) {
			return (bits + BITS_PER_WORD - 1) / BITS_PER_WORD;
		}

	public:
		/**
		 * @brief Constructs an empty bitset (no capacity). Present so the type can be
		 * default-stored in maps; assign a sized bitset before use.
		 */
		DynamicBitset() = default;

		/**
		 * @brief Constructs a bitset with @p bit_count bits, all cleared.
		 */
		explicit DynamicBitset(usize bit_count):
			  words(wordCount(bit_count), 0),
			  bit_count(bit_count) {}

		/**
		 * @brief The number of bits (the capacity), i.e. the highest valid index plus one.
		 */
		[[nodiscard]] usize size() const { return bit_count; }

		/**
		 * @brief Sets bit @p i to one.
		 */
		void set(usize i) {
			CORE_ASSERT(i < bit_count, "DynamicBitset::set index out of range");
			words[i / BITS_PER_WORD] |= (u64{ 1 } << (i % BITS_PER_WORD));
		}

		/**
		 * @brief Clears bit @p i to zero.
		 */
		void reset(usize i) {
			CORE_ASSERT(i < bit_count, "DynamicBitset::reset index out of range");
			words[i / BITS_PER_WORD] &= ~(u64{ 1 } << (i % BITS_PER_WORD));
		}

		/**
		 * @brief Whether bit @p i is set.
		 */
		[[nodiscard]] bool test(usize i) const {
			CORE_ASSERT(i < bit_count, "DynamicBitset::test index out of range");
			return (words[i / BITS_PER_WORD] >> (i % BITS_PER_WORD)) & u64{ 1 };
		}

		/**
		 * @brief Clears every bit.
		 */
		void clearAll() { std::ranges::fill(words, 0); }

		/**
		 * @brief In-place union: this |= other. Both must have the same capacity.
		 */
		DynamicBitset& operator|=(const DynamicBitset& other) {
			CORE_ASSERT(bit_count == other.bit_count, "DynamicBitset union size mismatch");
			for (usize i = 0; i < words.size(); i++) words[i] |= other.words[i];
			return *this;
		}

		/**
		 * @brief In-place intersection: this &= other. Both must have the same capacity.
		 */
		DynamicBitset& operator&=(const DynamicBitset& other) {
			CORE_ASSERT(bit_count == other.bit_count, "DynamicBitset intersection size mismatch");
			for (usize i = 0; i < words.size(); i++) words[i] &= other.words[i];
			return *this;
		}

		/**
		 * @brief In-place set difference: removes from this every bit set in @p other (this AND-NOT
		 * other). Both must have the same capacity.
		 */
		DynamicBitset& subtract(const DynamicBitset& other) {
			CORE_ASSERT(bit_count == other.bit_count, "DynamicBitset difference size mismatch");
			for (usize i = 0; i < words.size(); i++) words[i] &= ~other.words[i];
			return *this;
		}

		/**
		 * @brief Whether any bit is set.
		 */
		[[nodiscard]] bool any() const {
			return std::ranges::any_of(words, [](u64 w) { return w != 0; });
		}

		/**
		 * @brief The number of set bits.
		 */
		[[nodiscard]] usize count() const {
			usize total = 0;
			for (u64 w: words) total += static_cast<usize>(std::popcount(w));
			return total;
		}

		/**
		 * @brief Two bitsets are equal when they have the same capacity and the same set bits.
		 */
		bool operator==(const DynamicBitset& other) const {
			return bit_count == other.bit_count && words == other.words;
		}

		/**
		 * @brief Calls @p fn with the index of each set bit, in ascending order.
		 */
		template<class Fn>
		void forEachSet(Fn fn) const {
			for (usize wi = 0; wi < words.size(); wi++) {
				u64 w = words[wi];
				while (w != 0) {
					auto bit = static_cast<usize>(std::countr_zero(w));
					fn(wi * BITS_PER_WORD + bit);
					w &= w - 1;  // clear the lowest set bit
				}
			}
		}
	};
}
