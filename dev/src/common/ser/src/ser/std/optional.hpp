#pragma once

// ── std::optional ─────────────────────────────────────────────────────────────
// One byte of presence, written as an explicit 0 or 1 and validated on read, then the
// value if there is one. Same rule as bool and for the same reason: a byte that is
// neither is corrupt input, not a value to be interpreted.
//
// Two read paths, mirroring the vector adapter, and the choice is again about T:
//
//   fill  - T can be default-constructed and assigned: emplace an empty T, then read into
//           it in place. Zero moves.
//   build - it cannot: emplace(dispatchMake<T>(ar)). One move, and it is unavoidable.
//
// Never `opt = dispatchMake<T>(ar)`. Assignment demands that T be move-ASSIGNABLE, which
// is strictly more than move-constructible, and the difference is exactly the shapes
// serMake exists for: a type with a const field is move-constructible and not
// move-assignable, so `=` does not compile and emplace does.
//
// The limit worth knowing: a type whose move constructor is DELETED cannot go inside an
// optional at all - emplace fails on it inside std::construct_at, as a hard error rather
// than a substitution failure - so no adapter can rescue it.

#include <ser/concepts.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
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
		static constexpr ::std::size_t VALUE = 1;  // the presence byte, always there
	};

	template<class T>
	struct schema<::std::optional<T>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return detail::schemaOf<T, Mode, Seen>(detail::schemaText(h, "optional"));
		}
	};

	template<class T>
	struct serializer<::std::optional<T>> {
		static constexpr bool FILLABLE
			= ::std::default_initializable<T> && ::std::is_move_assignable_v<T>;

		static constexpr Errc write(writer auto& ar, const ::std::optional<T>& o) {
			const ::std::uint8_t present = o.has_value() ? 1u : 0u;
			if (const auto c = detail::dispatchWrite<::std::uint8_t>(ar, present); c != Errc::Ok)
				return c;
			if (!o.has_value()) return Errc::Ok;
			return detail::dispatchWrite<T>(ar, *o);
		}

		static constexpr Errc read(reader auto& ar, ::std::optional<T>& o)
			requires(FILLABLE || ::std::is_move_constructible_v<T>) {
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
