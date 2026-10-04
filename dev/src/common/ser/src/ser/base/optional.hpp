#pragma once

/*
 * base::Optional
 * Byte for byte the format of std::optional, and hashed as one: a presence byte written as
 * an explicit 0 or 1 and validated on read, then the value if there is one. A byte that is
 * neither is corrupt input, not a value to be interpreted - the same rule as bool, and for
 * the same reason.
 *
 * Written out here rather than round-tripped through a temporary std::optional, which is
 * the obvious shortcut and costs a COPY of T on the write side - so a base::Optional<Box<T>>
 * or any other non-copyable payload would fail to compile in the direction that only reads.
 *
 * Two read paths, mirroring the std adapter, and the choice is about T:
 *
 *   fill  - T can be default-constructed and assigned: emplace an empty T, then read into
 *           it in place. Zero moves.
 *   build - it cannot: emplace(dispatchMake<T>(ar)). One move, and it is unavoidable.
 *
 * Never `o = dispatchMake<T>(ar)`: assignment demands move-ASSIGNABLE, which is strictly
 * more than move-constructible, and the difference is exactly the shape serMake exists for.
 */

#include <base/collections/optional.hpp>

#include <ser/concepts.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/internal/dispatch_fwd.hpp>
#include <ser/internal/fillable.hpp>
#include <ser/serializer.hpp>
#include <ser/std/optional.hpp>
#include <ser/traits.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace ser {

	template<class T>
	struct MinWireSize<::base::Optional<T>> {
		static constexpr ::std::size_t VALUE = 1; /* the presence byte, always there */
	};

	/**
	 * @brief Delegated to std::optional<T> because the bytes are identical. Without this the type
	 * would hash as an opaque hook - sizeof and alignof only - and base::Optional<u32> and
	 * base::Optional<f32> share both, so the envelope would read one as the other.
	 */
	template<class T>
	struct Schema<::base::Optional<T>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return internal::schemaOf<::std::optional<T>, Mode, Seen>(h);
		}
	};

	template<class T>
	struct Serializer<::base::Optional<T>> {
		static constexpr bool FILLABLE = internal::FILL_IN_PLACE_V<T>;

		static constexpr Errc write(Writer auto& ar, const ::base::Optional<T>& o) {
			const ::std::uint8_t present = o.has_value() ? 1u : 0u;
			if (const auto c = internal::dispatchWrite<::std::uint8_t>(ar, present); c != Errc::Ok)
				return c;
			if (!o.has_value()) return Errc::Ok;
			return internal::dispatchWrite<T>(ar, *o);
		}

		static constexpr Errc read(Reader auto& ar, ::base::Optional<T>& o)
			requires(internal::READABLE_ELEMENT_V<T>) {
			::std::uint8_t present = 0;
			if (const auto c = internal::dispatchRead<::std::uint8_t>(ar, present); c != Errc::Ok)
				return c;
			if (present > 1) return Errc::InvalidValue;

			if (present == 0) {
				o.reset();
				return Errc::Ok;
			}

			if constexpr (FILLABLE) {
				if (!o.has_value()) o.emplace();
				return internal::dispatchRead<T>(ar, *o);
			} else {
				o.emplace(internal::dispatchMake<T>(ar));
				return Errc::Ok;
			}
		}
	};

} /* namespace ser */
