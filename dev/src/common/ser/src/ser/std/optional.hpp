#pragma once

/*
 * std::optional
 * One byte of presence, written as an explicit 0 or 1 and validated on read, then the
 * value if there is one. Same rule as bool and for the same reason: a byte that is
 * neither is corrupt input, not a value to be interpreted.
 *
 * Two read paths, mirroring the vector adapter, and the choice is again about T:
 *
 *   fill  - T can be default-constructed and assigned: emplace an empty T, then read into
 *           it in place. Zero moves.
 *   build - it cannot: emplace(dispatchMake<T>(ar)). One move, and it is unavoidable.
 *
 * Never `opt = dispatchMake<T>(ar)`: assignment demands move-ASSIGNABLE, which is strictly
 * more than move-constructible, and the difference is exactly the shape serMake exists for.
 * A type whose move constructor is DELETED cannot go inside an optional at all - emplace
 * fails on it inside std::construct_at, as a hard error - so no adapter can rescue it.
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
#include <optional>
#include <type_traits>

namespace ser {

	template<class T>
	struct min_wire_size<::std::optional<T>> {
		static constexpr ::std::size_t VALUE = 1; /* the presence byte, always there */
	};

	template<class T>
	struct schema<::std::optional<T>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return internal::schemaOf<T, Mode, Seen>(internal::schemaText(h, "optional"));
		}
	};

	template<class T>
	struct serializer<::std::optional<T>> {
		static constexpr bool FILLABLE = internal::FILL_IN_PLACE_V<T>;

		static constexpr Errc write(writer auto& ar, const ::std::optional<T>& o) {
			const ::std::uint8_t present = o.has_value() ? 1u : 0u;
			if (const auto c = internal::dispatchWrite<::std::uint8_t>(ar, present); c != Errc::Ok)
				return c;
			if (!o.has_value()) return Errc::Ok;
			return internal::dispatchWrite<T>(ar, *o);
		}

		static constexpr Errc read(reader auto& ar, ::std::optional<T>& o)
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
