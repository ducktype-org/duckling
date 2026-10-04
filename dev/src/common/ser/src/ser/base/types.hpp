#pragma once

/*
 * base::Bit256, base::CheckedOkBad
 * Bit256 is four u64s in base 2^64 with the lowest word first, and that is exactly what
 * goes on the wire - low word first, each through dispatch, so the stream is the same on a
 * big-endian machine as on a little-endian one. A SHA-256 written on one reads back equal
 * on the other.
 *
 * It needs an adapter at all only because it has constructors: `data` is public, but a
 * class with a user-provided constructor is not an aggregate, so the member walk cannot
 * see it.
 *
 * CheckedOkBad is refused - it is a check obligation, not a value.
 *
 * base::OkBad and base::Monostate are deliberately NOT here: both are aggregates the member
 * walk already handles, and a specialization would only be a second place for the format to
 * drift.
 */

#include <base/comptime/type_list.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/types/bit256.hpp>
#include <base/types/checked_okbad.hpp>

#include <ser/concepts.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/serializer.hpp>
#include <ser/traits.hpp>

#include <cstddef>
#include <cstdint>

namespace ser {

	template<>
	struct MinWireSize<::base::Bit256> {
		static constexpr ::std::size_t VALUE = 4 * sizeof(::u64);
	};

	template<>
	struct Schema<::base::Bit256> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return internal::schemaOf<::u64, Mode, Seen>(internal::schemaText(h, "base.bit256"));
		}
	};

	template<>
	struct Serializer<::base::Bit256> {
		template<class Ar, class Self>
		static constexpr Errc visit(Ar& ar, Self& self) {
			for (auto& word: self.data)
				if (const auto c = ar(word); c != Errc::Ok) return c;
			return Errc::Ok;
		}
	};

	namespace internal {

		/**
		 * @brief Keyed to the ARCHIVE type, and that is load-bearing: serializer<CheckedOkBad> is a
		 * full specialization, so an assert naming the type would fire where the header is
		 * parsed rather than where a write is attempted. See denyNonOwningRef in refs.hpp.
		 */
		template<class Ar>
		constexpr Errc denyCheckedOkBad() {
			static_assert(
				::base::DEPENDENT_FALSE_V<Ar>,
				"ser: cannot serialize base::CheckedOkBad - what it carries is an obligation "
				"to call status(), and an obligation does not travel in a stream. It is also "
				"neither copyable nor movable, so it could not be read back into anyway. "
				"Serialize the base::OkBad it wraps: that is one validated byte, and the "
				"caller of the read decides who has to check it."
			);
			return Errc::InvalidValue;
		}

	} /* namespace internal */

	template<>
	struct Serializer<::base::CheckedOkBad> {
		static constexpr Errc write(Writer auto& ar, const ::base::CheckedOkBad&) {
			return internal::denyCheckedOkBad<decltype(ar)>();
		}

		static constexpr Errc read(Reader auto& ar, ::base::CheckedOkBad&) {
			return internal::denyCheckedOkBad<decltype(ar)>();
		}
	};

} /* namespace ser */
