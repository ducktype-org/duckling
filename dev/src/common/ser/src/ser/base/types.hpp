#pragma once

// ── base::Bit256, base::CheckedOkBad ──────────────────────────────────────────
// Bit256 is four u64s in base 2^64 with the lowest word first, and that is exactly what
// goes on the wire - low word first, each through dispatch, so the stream is the same on a
// big-endian machine as on a little-endian one. A SHA-256 written on one reads back equal
// on the other.
//
// It needs an adapter at all only because it has constructors: `data` is public, but a
// class with a user-provided constructor is not an aggregate, so the member walk cannot
// see it.
//
// CheckedOkBad is refused - it is a check obligation, not a value.
//
// Two types in base that are deliberately NOT here, because they already work:
//
//   base::OkBad      an aggregate with one field, an enum class over bool. The member walk
//                    takes it, and the bool goes on the wire as a validated 0/1 byte, so a
//                    corrupt stream gives Errc::InvalidValue rather than a poisoned branch.
//   base::Monostate  an empty aggregate: zero fields, zero bytes, min_wire_size 0.
//
// Adding specializations for those two would only be a second place for the format to
// drift away from the first.

#include <base/types/bit256.hpp>
#include <base/types/checked_okbad.hpp>

#include <ser/concepts.hpp>
#include <ser/detail/meta.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/serializer.hpp>
#include <ser/traits.hpp>

#include <cstddef>
#include <cstdint>

namespace ser {

	template<>
	struct min_wire_size<::base::Bit256> {
		static constexpr ::std::size_t VALUE = 4 * sizeof(::u64);
	};

	template<>
	struct schema<::base::Bit256> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return detail::schemaOf<::u64, Mode, Seen>(detail::schemaText(h, "base.bit256"));
		}
	};

	template<>
	struct serializer<::base::Bit256> {
		template<class Ar, class Self>
		static constexpr Errc visit(Ar& ar, Self& self) {
			for (auto& word: self.data)
				if (const auto c = ar(word); c != Errc::Ok) return c;
			return Errc::Ok;
		}
	};

	namespace detail {

		// Keyed to the ARCHIVE type, and here that is load-bearing rather than defensive:
		// serializer<CheckedOkBad> is a full specialization, so the type is not a template
		// parameter of anything, and an assert naming it fires where the header is parsed
		// instead of where a write is attempted. gcc waits, clang does not. Same reason as
		// denyNonOwningRef in refs.hpp.
		template<class Ar>
		constexpr Errc denyCheckedOkBad() {
			static_assert(
				DEPENDENT_FALSE<Ar>,
				"ser: cannot serialize base::CheckedOkBad - what it carries is an obligation "
				"to call status(), and an obligation does not travel in a stream. It is also "
				"neither copyable nor movable, so it could not be read back into anyway. "
				"Serialize the base::OkBad it wraps: that is one validated byte, and the "
				"caller of the read decides who has to check it."
			);
			return Errc::InvalidValue;
		}

	}  // namespace detail

	template<>
	struct serializer<::base::CheckedOkBad> {
		static constexpr Errc write(writer auto& ar, const ::base::CheckedOkBad&) {
			return detail::denyCheckedOkBad<decltype(ar)>();
		}

		static constexpr Errc read(reader auto& ar, ::base::CheckedOkBad&) {
			return detail::denyCheckedOkBad<decltype(ar)>();
		}
	};

}  // namespace ser
