#pragma once

#include <base/comptime/type_traits.hpp>
#include <base/types/ints.hpp>
#include <base/types/monostate.hpp>

#include <array>
#include <bit>
#include <concepts>
#include <ranges>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

namespace hashing {


	namespace internal {

		/**
		 * checks if the type is a span of const or non-const bytes.
		 * @note Implementation is weird to handle the length argument of span correctly.
		 */
		template<typename T>
		concept span_of_bytes
			= base::IsInstantiationOfTypeValue<T, std::span>
		   && std::same_as<std::remove_const_t<typename T::value_type>, std::byte>;

		/**
		 * @brief Checks if the type accepts a span of bytes through `update()`.
		 *
		 * Deliberately a named method and not `operator()`: an algorithm is a byte sink, and
		 * the only thing allowed to feed it is `addBytes()` below. A call spelled `h(x)` reads
		 * like "hash this object" and invited hook authors to hand over unframed bytes,
		 * bypassing the length prefix.
		 */
		template<typename T>
		concept accepts_byte_span
			= requires(T t) { t.update(std::declval<std::span<const std::byte>>()); };

		/**
		 * Checks if the type has a finalize() method that returns a result_type
		 */
		template<typename T>
		concept has_finalize = requires(T t) {
			{ t.finalize() } -> std::same_as<typename T::result_type>;
		};

		/**
		 * Implementation of the hash_algorithm concept
		 */
		template<typename T>
		concept hash_algorithm_impl
			= std::is_object_v<T> && std::is_constructible_v<T> && std::is_destructible_v<T>
		   && requires { typename T::result_type; }
		   && accepts_byte_span<T> && has_finalize<T>;

	}  // namespace internal

	/**
	 * Checks if the type is a hash algorithm
	 */
	template<typename T>
	concept hash_algorithm = internal::hash_algorithm_impl<std::remove_cvref_t<T>>;

	/**
	 * Returns a reference to one of the Bases of Derived
	 *
	 * @tparam Base - the base class to get a reference to
	 * @param derived - object for which we want to get a reference to the base
	 * @return reference to (one of) the base(s) of derived
	 */
	template<typename Base, std::derived_from<std::remove_cvref_t<Base>> Derived>
	requires(not std::is_same_v<std::remove_cvref_t<Base>, std::remove_cvref_t<Derived>>)
	constexpr const Base& getBase(const Derived& derived) noexcept {
		return static_cast<const Base&>(derived);
	}

	namespace internal {
		/**
		 * Checks if the type can be hashed by just hashing its representation.
		 *
		 * @note See HashByAddress for hashing pointers by their address.
		 */
		template<typename T>
		concept can_hash_by_representation
			= std::has_unique_object_representations_v<T> && (not std::is_pointer_v<T>)
		   && (std::is_integral_v<T> || std::is_enum_v<T> ||

		       // the const Monostate& here is needed, as this is
		       // simply how it works with static constexpr members.
		       requires {
				   {
					   T::HASHING_CAN_HASH_BY_REPRESENTATION
				   } -> std::same_as<const base::Monostate&>;
			   });

		/**
		 * Checks if the type is a tuple of references
		 */
		template<typename T>
		concept tuple_of_refs = requires(T t) {
			[]<typename... Args>(std::tuple<Args...>)
				requires std::is_same_v<std::tuple<Args...>, T>
			          && (std::is_reference_v<Args> && ...) {}(t);
		};

		/**
		 * Checks if hashDecompose() can be called on the type and if it returns a tuple of
		 * references
		 */
		template<typename T>
		concept can_hashDecompose = requires(const T& t) {
			{ hashDecompose(t) } -> tuple_of_refs;
		};

		/**
		 * @brief Hands bytes to the algorithm with no framing at all.
		 *
		 * The framing layer only - `writeLengthPrefix()` and `addBytes()`. Nothing else in the
		 * library, and nothing outside it, may call this: bytes that reach the algorithm
		 * without a length in front are what made ("ab", "c") collide with ("a", "bc").
		 */
		template<hash_algorithm HashAlgorithm>
		constexpr void writeRaw(HashAlgorithm& h, std::span<const std::byte> bytes) {
			h.update(bytes);
		}

		/**
		 * @brief Writes a count as a canonical variable-width integer.
		 *
		 * One byte for a count up to 0xFE, otherwise the escape byte 0xFF followed by a
		 * little-endian `u64`. Canonical because the rule leaves no choice: below 0xFF the
		 * short form is required and the escape is forbidden, so every count has exactly one
		 * encoding. A scheme that allowed two spellings of the same count would put the
		 * ambiguity straight back into the prefix that is supposed to remove it.
		 *
		 * `u64` and not `std::size_t` in the wide form, so the hash does not depend on the
		 * platform's pointer width.
		 *
		 * Measured on a 63k line package: against a fixed 8-byte prefix this is worth 2.35% of
		 * total compile time. The compiler hashes mostly identifiers - five to fifteen
		 * characters - where an eight byte length was about as large as the data it described,
		 * and SHA-256 charges per 64 byte block, so shaving the prefix drops a great many
		 * streams from two blocks to one.
		 *
		 * This one write is necessarily unframed: a length prefix cannot itself carry a length
		 * prefix. Its width is recoverable from its own first byte instead.
		 */
		template<hash_algorithm HashAlgorithm>
		constexpr void writeLengthPrefix(HashAlgorithm& h, std::size_t count) {
			constexpr std::size_t ESCAPE_THRESHOLD = 0xFE;

			if (count <= ESCAPE_THRESHOLD) {
				const auto small = static_cast<std::byte>(count);
				writeRaw(h, std::span<const std::byte>{ &small, 1 });
				return;
			}

			const auto escape = std::byte{ 0xFF };
			writeRaw(h, std::span<const std::byte>{ &escape, 1 });

			const auto wide
				= std::bit_cast<std::array<const std::byte, sizeof(u64)>>(static_cast<u64>(count));
			writeRaw(h, std::span<const std::byte>{ wide.data(), wide.size() });
		}

		/**
		 * @brief The one entry point through which every payload byte reaches the algorithm.
		 *
		 * Writes the length of `bytes`, then `bytes`. Every field is therefore
		 * self-delimiting: reading the stream you know where one ends without knowing its
		 * type, so no two distinct values can flatten to the same bytes. Two variable-length
		 * fields in a row, a `{u32, u32}` against a `{u64}` - all of it is separated by the
		 * length in front.
		 *
		 * Every automatic path and every hand-written hook funnels through here, so the
		 * guarantee does not depend on anybody remembering it. See the readme for what the
		 * stream deliberately does not separate (types, and same-shaped composites).
		 */
		template<hash_algorithm HashAlgorithm>
		constexpr void addBytes(HashAlgorithm& h, std::span<const std::byte> bytes) {
			writeLengthPrefix(h, bytes.size());
			writeRaw(h, bytes);
		}

		/**
		 * Hashes an object as a sequence of bytes
		 */
		template<hash_algorithm HashAlgorithm, typename T>
		constexpr void hashAsBytes(HashAlgorithm& h, const T& t) {
			const auto arr = std::bit_cast<std::array<const std::byte, sizeof(T)>, T>(t);
			addBytes(h, std::span<const std::byte>{ arr.data(), arr.size() });
		}

		/**
		 * Checks if a range can be hashed as a contiguous sequence of memory
		 * (i.e. its elements are in a contiguous memory block, individual elements meet
		 * can_hash_by_representation and range size is known)
		 */
		template<typename HashAlgorithm, typename R>
		concept can_hash_range_as_bytes
			= hash_algorithm<HashAlgorithm> && std::ranges::contiguous_range<R>
		   && can_hash_by_representation<std::ranges::range_value_t<R>>
		   && requires(const R& r) { std::ranges::size(r); };

		/**
		 * @brief Adds a composite's member count to the hash.
		 *
		 * The range prefix below makes a variable-length field self-delimiting, but a
		 * composite still is not: {int, int, float} and {float, std::string} both flatten to
		 * twelve zero bytes when default-constructed, so two unrelated types would share a
		 * hash. The member count in front separates them. It is a compile-time constant, so
		 * within one type it contributes nothing but a fixed offset.
		 *
		 * It does not make the hash type-aware - u32(1) and i32(1) still agree, and so do two
		 * structs with the same arity and the same field widths. Only a real type tag fixes
		 * that.
		 */
		template<std::size_t MEMBERS, hash_algorithm HashAlgorithm>
		constexpr void hashCompositeArity(HashAlgorithm& h) {
			// A framing field, so it goes out through writeLengthPrefix() rather than
			// addBytes(): the grammar says a composite opens with its arity, so the reader
			// knows to expect it and it needs no length of its own. The same reasoning as for
			// the length prefix itself.
			writeLengthPrefix(h, MEMBERS);
		}

		/**
		 * @brief Adds a range's element count to the hash.
		 *
		 * Without it the byte stream is not self-delimiting: two variable-length fields
		 * hashed one after another cannot be told from the same bytes split differently, so
		 * ("ab", "c") and ("a", "bc") collide. A fixed-width count in front removes the
		 * ambiguity.
		 */
		template<hash_algorithm HashAlgorithm>
		constexpr void hashRangeLengthPrefix(HashAlgorithm& h, std::size_t size) {
			// Framing, like the arity above - the reader expects a count here and each element
			// carries its own length, so the count needs no length of its own.
			writeLengthPrefix(h, size);
		}

		/**
		 * Hashes a range as a contiguous sequence of memory
		 * (requires that its elements are in a contiguous memory block, have unique object
		 * representations and size is known)
		 */
		template<hash_algorithm HashAlgorithm, std::ranges::contiguous_range R>
		requires can_hash_range_as_bytes<HashAlgorithm, R>
		constexpr void hashRangeAsBytes(HashAlgorithm& h, const R& r) {
			constexpr std::size_t ELEM_SIZE   = sizeof(std::ranges::range_value_t<R>);
			const std::size_t     r_size      = std::ranges::size(r);
			const std::size_t     buffer_size = r_size * ELEM_SIZE;

			// No separate element count here: the byte length written by addBytes() divided by
			// the (compile-time constant) element size is the count, so writing both would say
			// the same thing twice.
			if consteval {
				// In a constant expression the only way to get at an object's memory
				// representation is std::bit_cast, so the elements are copied one by one into
				// a buffer. Since C++20 an allocation freed in the same expression it was
				// allocated in is allowed in a constant expression.
				auto* const buffer = ::new std::byte[buffer_size];

				for (u64 i = 0, j = 0; i < r_size; ++i, j += ELEM_SIZE) {
					// Ranges may have both singed and unsigned index types and there is not
					// good trait that can always tell which one the range expects. To suppress
					// warnings we get the elements using std::next with range's difference_type
					const auto& elem = *std::next(
						std::ranges::begin(r), static_cast<std::ranges::range_difference_t<R>>(i)
					);
					const auto arr = std::bit_cast<std::array<const std::byte, ELEM_SIZE>>(elem);
					std::copy(arr.begin(), arr.end(), buffer + j);
				}

				addBytes(h, std::span<const std::byte>{ buffer, buffer_size });

				delete[] buffer;
			} else {
				// At run time the buffer is pure overhead - the concept above already requires
				// a contiguous range, so the bytes can be handed over in place. Measured: the
				// buffer cost one heap allocation and one full copy per call, at -O0 and -O2
				// alike, despite the copy elision the old comment here hoped for.
				addBytes(h, std::as_bytes(std::span{ std::ranges::data(r), r_size }));
			}
		}

		/**
		 * Checks if the type is tuple-like i.e. supports std::tuple_size and std::get
		 */
		template<typename T>
		concept supports_std_get = requires {
			// Don't remove this line and don't change to std::tuple_size_v.
			// If std::tuple_size_v is ill-formed, the fail may happen not in the immediate context
			// of the concept check which may omit SFINAE and cause a hard error
			std::tuple_size<T>::value;

			[]<std::size_t... Is>(std::index_sequence<Is...>) requires requires {
				(std::get<Is>(std::declval<T>()), ...);
			} {}(std::make_index_sequence<
				 /* don't change to std::tuple_size_v */ std::tuple_size<T>::value>{});
		};

	}  // namespace internal


}  // namespace hashing
