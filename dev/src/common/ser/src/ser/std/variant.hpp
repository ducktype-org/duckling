#pragma once

// ── std::variant and std::monostate ───────────────────────────────────────────
// A tag, then the alternative the tag names. The tag is a plain 64-bit integer - the same
// width as a container's length prefix - because a variant is written once and read on a
// machine whose ::std::size_t need not be the one that wrote it, and a tag that is eight
// bytes on both sides is one fewer platform question. It costs seven bytes over a byte
// tag; nothing on this wire is packed, and paying them buys a format that never has to
// decide how many alternatives are "few enough".
//
// THE INDEX IS A RUNTIME VALUE AND EVERY ALTERNATIVE NEEDS A COMPILE-TIME ONE. That gap
// is the whole implementation. `v.index()` is an ordinary call on an ordinary object, so
// neither ::std::get<I> nor ::std::variant_alternative_t accepts it - not even inside a
// constexpr function, because constexpr says when a function MAY be evaluated and says
// nothing about its arguments being constants. `applyByIndex` is the bridge, and both
// directions cross it: the write side has exactly the same problem as the read side.
//
// Three shapes, and the reason there are three:
//
//   write - tag, then the alternative. A valueless variant is refused rather than written:
//           ::std::variant_npos is not an alternative, and putting it on the wire would
//           produce a stream that no reader can honour.
//   read  - fills an EXISTING variant. All-or-nothing, like the tuple adapter: which
//           alternative arrives is a property of the bytes, so a `read` that exists for
//           some tags and not others would be a format whose readability depends on the
//           payload. Every alternative therefore has to be reachable by one of two paths,
//           and per alternative it takes the cheaper one - emplace<I>() and fill in place
//           when the alternative is default-constructible and assignable, otherwise
//           emplace<I>(dispatchMake<T>(ar)).
//   make  - builds one. What dispatch reaches for at the top level of ser::read, and the
//           only path for a variant that has no `read`.
//
// `read` does NOT rescue an alternative with a const FIELD from the move: emplace
// move-constructs, so that move happens either way. What it buys is the VARIANT not having
// to be move-assignable, which is what dispatch demands of a make-only type being filled
// in - and a variant with such an alternative is exactly the variant that is not
// move-assignable, so this is the difference between working and a static_assert.
//
// ALWAYS in_place_index<I>, never the converting constructor. `VType{ value }` picks its
// alternative by overload resolution, which is a different question from "which index did
// the tag say" and answers it differently for variant<int, long> or variant<bool, string>.
// The tag is the answer; in_place_index is how it is obeyed.
//
// A failed `read` leaves the variant holding a partially filled alternative, exactly as a
// failed container read leaves a partly filled container. The rule is the same: a target
// whose read returned an error is not a value.
//
// ::std::monostate is here because that is where it is needed - variant<monostate, T...>
// is the common shape - and it writes zero bytes. An empty type has nothing to write and
// its tag has already said everything the reader needs.

#include <ser/concepts.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/serializer.hpp>
#include <ser/traits.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>
#include <variant>

namespace ser {

	namespace detail {

		template<class V, class R, ::std::size_t I = 0, class F>
		constexpr R applyByIndex(::std::size_t i, F&& f) {
			if constexpr (I < ::std::variant_size_v<V>) {
				if (i == I) return ::std::forward<F>(f).template operator()<I>();
				return applyByIndex<V, R, I + 1>(i, ::std::forward<F>(f));
			} else if constexpr (::std::same_as<R, Errc>) {
				return Errc::InvalidValue;
			} else {
				throwError(Errc::InvalidValue);
			}
		}

		// The minimum of the alternatives, because exactly one of them is on the wire.
		// An empty variant has no alternative and therefore no lower bound to add.
		template<class... Ts>
		[[nodiscard]] consteval ::std::size_t minAlternativeWire() {
			::std::size_t smallest = 0;
			bool          first    = true;
			((first ? (smallest = MIN_WIRE_SIZE_V<Ts>, first = false)
			        : (smallest = MIN_WIRE_SIZE_V<Ts> < smallest ? MIN_WIRE_SIZE_V<Ts> : smallest,
			           false)),
			 ...);
			return smallest;
		}

	}  // namespace detail

	template<class... Ts>
	struct min_wire_size<::std::variant<Ts...>> {
		static constexpr ::std::size_t VALUE
			= MIN_WIRE_SIZE_V<::std::uint64_t>
		    + detail::minAlternativeWire<::std::remove_cv_t<Ts>...>();
	};

	template<>
	struct min_wire_size<::std::monostate> {
		static constexpr ::std::size_t VALUE = 0;  // an empty type writes nothing
	};

	// The alternative COUNT is mixed, not just the alternatives: it is what a tag means.
	// Appending an alternative leaves every existing tag reading the same bytes and still
	// changes the format, because the set of tags a reader will accept is now larger.
	template<class... Ts>
	struct schema<::std::variant<Ts...>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			h = detail::schemaNumber(detail::schemaText(h, "variant"), sizeof...(Ts));
			((h = detail::schemaOf<::std::remove_cv_t<Ts>, Mode, Seen>(h)), ...);
			return h;
		}
	};

	template<>
	struct schema<::std::monostate> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return detail::schemaText(h, "monostate");
		}
	};

	template<class... Ts>
	struct serializer<::std::variant<Ts...>> {
		using variant_type = ::std::variant<Ts...>;
		using tag_type     = ::std::uint64_t;

		static constexpr ::std::size_t ALTERNATIVE_COUNT = sizeof...(Ts);

		template<class T>
		static constexpr bool FILLS_IN_PLACE
			= ::std::default_initializable<T> && ::std::is_move_assignable_v<T>;

		static constexpr bool FILLABLE
			= ((FILLS_IN_PLACE<Ts> || ::std::is_move_constructible_v<Ts>) && ...);

		static constexpr Errc write(writer auto& ar, const variant_type& o) {
			// variant_npos is the only index a valueless variant has, and it names no
			// alternative. Refuse it here rather than write a tag nothing can read.
			if (o.valueless_by_exception()) return Errc::InvalidValue;

			const ::std::size_t index = o.index();
			if (const auto c = detail::dispatchWrite<tag_type>(ar, static_cast<tag_type>(index));
			    c != Errc::Ok)
				return c;

			const auto write_alternative = [&ar, &o]<::std::size_t I>() {
				using T = ::std::remove_cv_t<::std::variant_alternative_t<I, variant_type>>;
				return detail::dispatchWrite<T>(ar, ::std::get<I>(o));
			};
			return detail::applyByIndex<variant_type, Errc>(index, write_alternative);
		}

		static constexpr Errc read(reader auto& ar, variant_type& o) requires(FILLABLE) {
			::std::size_t index = 0;
			if (const auto c = readTag(ar, index); c != Errc::Ok) return c;

			const auto fill_alternative = [&ar, &o]<::std::size_t I>() {
				using alternative = ::std::variant_alternative_t<I, variant_type>;
				using T           = ::std::remove_cv_t<alternative>;

				// A const alternative never takes the first branch - const is exactly what
				// makes it unassignable - so ::std::get<I> below is never a const ref.
				if constexpr (FILLS_IN_PLACE<alternative>) {
					o.template emplace<I>();
					return detail::dispatchRead<T>(ar, ::std::get<I>(o));
				} else {
					o.template emplace<I>(detail::dispatchMake<T>(ar));
					return Errc::Ok;
				}
			};
			return detail::applyByIndex<variant_type, Errc>(index, fill_alternative);
		}

		static constexpr variant_type make(reader auto& ar)
			requires((::std::is_move_constructible_v<Ts> && ...)) {
			::std::size_t index = 0;
			if (const auto c = readTag(ar, index); c != Errc::Ok) throwError(c, ar.position());

			const auto build_alternative = [&ar]<::std::size_t I>() {
				using T = ::std::remove_cv_t<::std::variant_alternative_t<I, variant_type>>;
				return variant_type{ ::std::in_place_index<I>, detail::dispatchMake<T>(ar) };
			};
			return detail::applyByIndex<variant_type, variant_type>(index, build_alternative);
		}

	private:
		/** @brief Reads the tag and turns it into an index, or fails. */
		static constexpr Errc readTag(reader auto& ar, ::std::size_t& out) {
			tag_type tag = 0;
			if (const auto c = detail::dispatchRead<tag_type>(ar, tag); c != Errc::Ok) return c;

			if constexpr (sizeof(tag_type) > sizeof(::std::size_t))
				if (tag > static_cast<tag_type>(::std::numeric_limits<::std::size_t>::max()))
					return Errc::InvalidValue;

			const auto index = static_cast<::std::size_t>(tag);
			if (index >= ALTERNATIVE_COUNT) return Errc::InvalidValue;

			out = index;
			return Errc::Ok;
		}
	};

	template<>
	struct serializer<::std::monostate> {
		static constexpr Errc write(writer auto&, const ::std::monostate&) { return Errc::Ok; }

		static constexpr Errc read(reader auto&, ::std::monostate&) { return Errc::Ok; }

		static constexpr ::std::monostate make(reader auto&) { return {}; }
	};

}  // namespace ser
