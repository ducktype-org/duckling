#pragma once

// ── base::Optional ────────────────────────────────────────────────────────────
// Byte for byte the format of std::optional, and hashed as one: a presence byte written as
// an explicit 0 or 1 and validated on read, then the value if there is one. A byte that is
// neither is corrupt input, not a value to be interpreted - the same rule as bool, and for
// the same reason.
//
// Written out here rather than round-tripped through a temporary std::optional, which is
// the obvious shortcut and costs a COPY of T on the write side - so a base::Optional<Box<T>>
// or any other non-copyable payload would fail to compile in the direction that only reads.
//
// Two read paths, mirroring the std adapter, and the choice is about T:
//
//   fill  - T can be default-constructed and assigned: emplace an empty T, then read into
//           it in place. Zero moves.
//   build - it cannot: emplace(dispatchMake<T>(ar)). One move, and it is unavoidable.
//
// Never `o = dispatchMake<T>(ar)`: assignment demands move-ASSIGNABLE, which is strictly
// more than move-constructible, and the difference is exactly the shape serMake exists for.

#include <base/collections/optional.hpp>

#include <ser/concepts.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/detail/fillable.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
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
	struct min_wire_size<::base::Optional<T>> {
		static constexpr ::std::size_t VALUE = 1;  // the presence byte, always there
	};

	// Delegated to std::optional<T> because the bytes are identical. Without this the type
	// would hash as an opaque hook - sizeof and alignof only - and base::Optional<u32> and
	// base::Optional<f32> share both, so the envelope would read one as the other.
	template<class T>
	struct schema<::base::Optional<T>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return detail::schemaOf<::std::optional<T>, Mode, Seen>(h);
		}
	};

	template<class T>
	struct serializer<::base::Optional<T>> {
		static constexpr bool FILLABLE = detail::FILL_IN_PLACE_V<T>;

		static constexpr Errc write(writer auto& ar, const ::base::Optional<T>& o) {
			const ::std::uint8_t present = o.has_value() ? 1u : 0u;
			if (const auto c = detail::dispatchWrite<::std::uint8_t>(ar, present); c != Errc::Ok)
				return c;
			if (!o.has_value()) return Errc::Ok;
			return detail::dispatchWrite<T>(ar, *o);
		}

		static constexpr Errc read(reader auto& ar, ::base::Optional<T>& o)
			requires(detail::READABLE_ELEMENT_V<T>) {
			::std::uint8_t present = 0;
			if (const auto c = detail::dispatchRead<::std::uint8_t>(ar, present); c != Errc::Ok)
				return c;
			if (present > 1) return Errc::InvalidValue;

			if (present == 0) {
				o.reset();
				return Errc::Ok;
			}

			if constexpr (FILLABLE) {
				if (!o.has_value()) o.emplace();
				return detail::dispatchRead<T>(ar, *o);
			} else {
				o.emplace(detail::dispatchMake<T>(ar));
				return Errc::Ok;
			}
		}
	};

}  // namespace ser
