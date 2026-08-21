#pragma once

// ── std::vector ───────────────────────────────────────────────────────────────
// Length prefix, then the elements through full dispatch, so an element with its own
// hook is written by that hook.
//
// Two read paths, and which one is taken is about the ELEMENT, not the container:
//
//   fill  - the element can be default-constructed and assigned: resize once, then read
//           into each element in place. Zero moves, zero temporaries.
//   build - it cannot: reserve, then emplace_back(dispatchMake<T>(ar)). One move per
//           element, which is the price of a type that has to be built rather than filled.
//
// The fill path is not just faster: for a large vector it is the difference between one
// allocation and one allocation plus n constructions of a temporary.

#include <ser/concepts.hpp>
#include <ser/detail/container.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/detail/meta.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/serializer.hpp>
#include <ser/traits.hpp>

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace ser {

	template<class T, class Al>
	struct min_wire_size<::std::vector<T, Al>> {
		static constexpr ::std::size_t VALUE = sizeof(detail::wire_size_type);
	};

	// The hash is STRUCTURAL, not a sizeof: sizeof(std::vector) is a property of the
	// standard library and has nothing to do with the bytes this adapter writes, while the
	// ELEMENT type is the whole of the format after the length prefix. Without this
	// specialization a vector would hash as an opaque hooked type and vector<int> would be
	// indistinguishable from vector<float>. The allocator is deliberately absent - it
	// never reaches the wire.
	template<class T, class Al>
	struct schema<::std::vector<T, Al>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return detail::schemaOf<T, Mode, Seen>(detail::schemaText(h, "vector"));
		}
	};

	template<class T, class Al>
	struct serializer<::std::vector<T, Al>> {
		using vector_type = ::std::vector<T, Al>;

		// "Can this element be filled in rather than built?" Asked of the element type
		// alone, because that is what decides it.
		static constexpr bool FILLABLE
			= ::std::default_initializable<T> && ::std::is_move_assignable_v<T>;

		static constexpr Errc write(writer auto& ar, const vector_type& v) {
			if (const auto c = detail::writeLength(ar, v.size()); c != Errc::Ok) return c;
			for (const T& e: v)
				if (const auto c = detail::dispatchWrite<T>(ar, e); c != Errc::Ok) return c;
			return Errc::Ok;
		}

		static constexpr Errc read(reader auto& ar, vector_type& v) {
			::std::size_t n = 0;
			if (const auto c = detail::readLength<T>(ar, n); c != Errc::Ok) return c;

			v.clear();
			if constexpr (FILLABLE) {
				v.resize(n);
				for (::std::size_t i = 0; i < n; ++i)
					if (const auto c = detail::dispatchRead<T>(ar, v[i]); c != Errc::Ok) return c;
			} else {
				v.reserve(n);
				for (::std::size_t i = 0; i < n; ++i) v.emplace_back(detail::dispatchMake<T>(ar));
			}
			return Errc::Ok;
		}
	};

	// ── std::vector<bool> ─────────────────────────────────────────────────────
	// Refused by name. It is not a container of bool: operator[] hands back a proxy, so
	// `dispatchRead<bool>(ar, v[i])` would read into a temporary and throw it away, and
	// the element loop that works for every other T silently reads nothing. Saying so
	// beats a page of errors about a proxy reference nobody asked for.
	template<class Al>
	struct serializer<::std::vector<bool, Al>> {
		static constexpr Errc write(writer auto& ar, const ::std::vector<bool, Al>& v) {
			static_assert(
				detail::DEPENDENT_FALSE<Al>,
				"ser: std::vector<bool> is a bit-packed proxy container, not a container of "
				"bool, so the element loop cannot read into it. Use std::vector<std::uint8_t> "
				"for a byte per flag, or std::bitset<N> when the count is fixed - and note "
				"that neither has vector<bool>'s packing, which is a format decision either "
				"way. Specialize ser::serializer<std::vector<bool>> to make that decision."
			);
			(void) ar;
			(void) v;
			return Errc::InvalidValue;
		}

		static constexpr Errc read(reader auto& ar, ::std::vector<bool, Al>& v) {
			static_assert(
				detail::DEPENDENT_FALSE<Al>,
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
