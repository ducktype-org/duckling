#pragma once

/**
 * @file
 * @brief std::vector
 * @details Length prefix, then the elements through full dispatch, so an element with its own
 * hook is written by that hook.
 *
 * Two read paths, and which one is taken is about the ELEMENT, not the container:
 *
 *   fill  - the element can be default-constructed and assigned: resize once, then read
 *           into each element in place. Zero moves, zero temporaries.
 *   build - it cannot: reserve, then emplace_back(dispatchMake<T>(ar)). One move per
 *           element, which is the price of a type that has to be built rather than filled.
 *
 * `read` is constrained on the element having one of the two paths, so an element with
 * neither takes the vector out of the read direction rather than failing somewhere inside
 * emplace_back. See internal/fillable.hpp.
 */

#include <base/comptime/type_list.hpp>
#include <base/comptime/type_traits.hpp>

#include <ser/concepts.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/internal/container.hpp>
#include <ser/internal/dispatch_fwd.hpp>
#include <ser/internal/fillable.hpp>
#include <ser/serializer.hpp>
#include <ser/traits.hpp>

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace ser {

	template<class T, class Al>
	struct MinSerializedSize<::std::vector<T, Al>> {
		static constexpr ::std::size_t VALUE = sizeof(internal::LengthType);
	};

	/**
	 * @brief The hash is STRUCTURAL, not a sizeof: the ELEMENT type is the whole of the format
	 * after the length prefix, and without this specialization vector<int> would be
	 * indistinguishable from vector<float>. The allocator never reaches the stream.
	 */
	template<class T, class Al>
	struct Schema<::std::vector<T, Al>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return internal::schemaOf<T, Mode, Seen>(internal::schemaText(h, "vector"));
		}
	};

	template<class T, class Al>
	struct Serializer<::std::vector<T, Al>> {
		using VectorType = ::std::vector<T, Al>;

		/**
		 * @brief "Can this element be filled in rather than built?" Asked of the element type
		 * alone, because that is what decides it.
		 */
		static constexpr bool FILLABLE = internal::FILL_IN_PLACE_V<T>;

		static constexpr Errc write(Writer auto& ar, const VectorType& v) {
			if (const auto c = internal::writeLength<T>(ar, v.size()); c != Errc::Ok) return c;
			for (const T& e: v)
				if (const auto c = internal::dispatchWrite<T>(ar, e); c != Errc::Ok) return c;
			return Errc::Ok;
		}

		static constexpr Errc read(Reader auto& ar, VectorType& v)
			requires(internal::READABLE_ELEMENT_V<T>) {
			::std::size_t n = 0;
			if (const auto c = internal::readLength<T>(ar, n); c != Errc::Ok) return c;

			v.clear();
			// resize + fill in place is cheaper than emplace_back per element, the else branch
			// is for elements that cannot be default-constructed and have to be built
			if constexpr (FILLABLE) {
				v.resize(n);
				for (::std::size_t i = 0; i < n; ++i)
					if (const auto c = internal::dispatchRead<T>(ar, v[i]); c != Errc::Ok) return c;
			} else {
				v.reserve(n);
				for (::std::size_t i = 0; i < n; ++i) v.emplace_back(internal::dispatchMake<T>(ar));
			}
			return Errc::Ok;
		}
	};

	/**
	 * @brief std::vector<bool>
	 * @details Refused by name. operator[] hands back a proxy, so `dispatchRead<bool>(ar, v[i])`
	 * would read into a temporary and throw it away, and the element loop that works for
	 * every other T would silently read nothing.
	 */
	template<class Al>
	struct Serializer<::std::vector<bool, Al>> {
		static constexpr Errc write(Writer auto& ar, const ::std::vector<bool, Al>& v) {
			static_assert(
				::base::DEPENDENT_FALSE_V<Al>,
				"ser: std::vector<bool> is a bit-packed proxy container, not a container of "
				"bool, so the element loop cannot read into it. Use std::vector<std::uint8_t> "
				"for a byte per flag, or std::bitset<N> when the count is fixed - and note "
				"that neither has vector<bool>'s packing, which is a format decision either "
				"way. Specialize ser::Serializer<std::vector<bool>> to make that decision."
			);
			(void) ar;
			(void) v;
			return Errc::InvalidValue;
		}

		static constexpr Errc read(Reader auto& ar, ::std::vector<bool, Al>& v) {
			static_assert(
				::base::DEPENDENT_FALSE_V<Al>,
				"ser: std::vector<bool> is a bit-packed proxy container, not a container of "
				"bool, so the element loop cannot read into it. Use std::vector<std::uint8_t> "
				"for a byte per flag, or std::bitset<N> when the count is fixed."
			);
			(void) ar;
			(void) v;
			return Errc::InvalidValue;
		}
	};

}  // namespace ser
