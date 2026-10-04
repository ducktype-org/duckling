#pragma once

/*
 * std::variant and std::monostate
 * A tag, then the alternative the tag names. The tag is a plain 64-bit integer - the same
 * width as a container's length prefix - so a stream stays readable on a machine whose
 * ::std::size_t is not the writer's. It costs seven bytes over a byte tag, and buys a
 * format that never has to decide how many alternatives are "few enough".
 *
 * THE INDEX IS A RUNTIME VALUE AND EVERY ALTERNATIVE NEEDS A COMPILE-TIME ONE. That gap is
 * the whole implementation: neither ::std::get<I> nor ::std::variant_alternative_t accepts
 * `v.index()`, so `applyByIndex` is the bridge and both directions cross it.
 *
 * Three shapes:
 *
 *   write - tag, then the alternative. A valueless variant is refused rather than written.
 *   read  - fills an EXISTING variant. All-or-nothing, like the tuple adapter: which
 *           alternative arrives is a property of the bytes, so a `read` available for only
 *           some tags would be a format whose readability depends on the payload. Per
 *           alternative it takes the cheaper path - emplace<I>() and fill in place, or
 *           emplace<I>(dispatchMake<T>(ar)).
 *   make  - builds one. What dispatch reaches for at the top level of ser::read.
 *
 * `read` does not save a move for an alternative with a const field - emplace
 * move-constructs either way. What it buys is the VARIANT not having to be move-assignable,
 * which is the difference between working and a static_assert.
 *
 * ALWAYS in_place_index<I>, never the converting constructor: `VType{ value }` picks by
 * overload resolution, which answers a different question than the tag for
 * variant<int, long> or variant<bool, string>. A failed `read` leaves a partially filled
 * alternative, exactly as a failed container read does. ::std::monostate is here because
 * variant<monostate, T...> is the common shape, and it writes zero bytes.
 */

#include <ser/concepts.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/internal/dispatch_fwd.hpp>
#include <ser/internal/fillable.hpp>
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

	namespace internal {

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

		/**
		 * @brief The minimum of the alternatives, because exactly one of them is on the wire.
		 * An empty variant has no alternative and therefore no lower bound to add.
		 */
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

	} /* namespace internal */

	template<class... Ts>
	struct MinWireSize<::std::variant<Ts...>> {
		static constexpr ::std::size_t VALUE
			= MIN_WIRE_SIZE_V<::std::uint64_t>
		    + internal::minAlternativeWire<::std::remove_cv_t<Ts>...>();
	};

	template<>
	struct MinWireSize<::std::monostate> {
		static constexpr ::std::size_t VALUE = 0; /* an empty type writes nothing */
	};

	/**
	 * @brief The alternative COUNT is mixed, not just the alternatives: it is what a tag means.
	 * Appending an alternative leaves every existing tag reading the same bytes and still
	 * changes the format, because the set of tags a reader will accept is now larger.
	 */
	template<class... Ts>
	struct Schema<::std::variant<Ts...>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			h = internal::schemaNumber(internal::schemaText(h, "variant"), sizeof...(Ts));
			((h = internal::schemaOf<::std::remove_cv_t<Ts>, Mode, Seen>(h)), ...);
			return h;
		}
	};

	template<>
	struct Schema<::std::monostate> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return internal::schemaText(h, "monostate");
		}
	};

	template<class... Ts>
	struct Serializer<::std::variant<Ts...>> {
		using VariantType = ::std::variant<Ts...>;
		using TagType     = ::std::uint64_t;

		static constexpr ::std::size_t ALTERNATIVE_COUNT = sizeof...(Ts);

		template<class T>
		static constexpr bool FILLS_IN_PLACE = internal::FILL_IN_PLACE_V<T>;

		static constexpr bool FILLABLE = (internal::READABLE_ELEMENT_V<Ts> && ...);

		static constexpr Errc write(Writer auto& ar, const VariantType& o) {
			/*
			 * variant_npos is the only index a valueless variant has, and it names no
			 * alternative. Refuse it here rather than write a tag nothing can read.
			 */
			if (o.valueless_by_exception()) return Errc::InvalidValue;

			const ::std::size_t index = o.index();
			if (const auto c = internal::dispatchWrite<TagType>(ar, static_cast<TagType>(index));
			    c != Errc::Ok)
				return c;

			const auto write_alternative = [&ar, &o]<::std::size_t I>() {
				using T = ::std::remove_cv_t<::std::variant_alternative_t<I, VariantType>>;
				return internal::dispatchWrite<T>(ar, ::std::get<I>(o));
			};
			return internal::applyByIndex<VariantType, Errc>(index, write_alternative);
		}

		static constexpr Errc read(Reader auto& ar, VariantType& o) requires(FILLABLE) {
			::std::size_t index = 0;
			if (const auto c = readTag(ar, index); c != Errc::Ok) return c;

			const auto fill_alternative = [&ar, &o]<::std::size_t I>() {
				using Alternative = ::std::variant_alternative_t<I, VariantType>;
				using T           = ::std::remove_cv_t<Alternative>;

				/**
				 * @brief A const alternative never takes the first branch - const is exactly what
				 * makes it unassignable - so ::std::get<I> below is never a const ref.
				 */
				if constexpr (FILLS_IN_PLACE<Alternative>) {
					o.template emplace<I>();
					return internal::dispatchRead<T>(ar, ::std::get<I>(o));
				} else {
					o.template emplace<I>(internal::dispatchMake<T>(ar));
					return Errc::Ok;
				}
			};
			return internal::applyByIndex<VariantType, Errc>(index, fill_alternative);
		}

		static constexpr VariantType make(Reader auto& ar)
			requires((internal::BUILDABLE_V<Ts> && ...)) {
			::std::size_t index = 0;
			if (const auto c = readTag(ar, index); c != Errc::Ok) throwError(c, ar.position());

			const auto build_alternative = [&ar]<::std::size_t I>() {
				using T = ::std::remove_cv_t<::std::variant_alternative_t<I, VariantType>>;
				return VariantType{ ::std::in_place_index<I>, internal::dispatchMake<T>(ar) };
			};
			return internal::applyByIndex<VariantType, VariantType>(index, build_alternative);
		}

	private:
		/** @brief Reads the tag and turns it into an index, or fails. */
		static constexpr Errc readTag(Reader auto& ar, ::std::size_t& out) {
			TagType tag = 0;
			if (const auto c = internal::dispatchRead<TagType>(ar, tag); c != Errc::Ok) return c;

			if constexpr (sizeof(TagType) > sizeof(::std::size_t))
				if (tag > static_cast<TagType>(::std::numeric_limits<::std::size_t>::max()))
					return Errc::InvalidValue;

			const auto index = static_cast<::std::size_t>(tag);
			if (index >= ALTERNATIVE_COUNT) return Errc::InvalidValue;

			out = index;
			return Errc::Ok;
		}
	};

	template<>
	struct Serializer<::std::monostate> {
		static constexpr Errc write(Writer auto&, const ::std::monostate&) { return Errc::Ok; }

		static constexpr Errc read(Reader auto&, ::std::monostate&) { return Errc::Ok; }

		static constexpr ::std::monostate make(Reader auto&) { return {}; }
	};

} /* namespace ser */
