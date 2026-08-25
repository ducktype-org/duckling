#pragma once

// ── base::Box, base::MBox, base::SharedBox, base::BoxOrCRef ────────────────────
// Two of these serialize and two are refused, and the line between them is ownership.
//
//   Box<T>       the sole owner of one T. The pointer is storage, not format: a Box<T>
//                writes exactly what a T writes, and nothing says "this was behind a
//                pointer". Round-trips to a fresh allocation.
//   MBox<T>      the same, plus "may be null" - so exactly the format of an optional:
//                one presence byte, then the value if there is one.
//   SharedBox<T> refused. Sharing IS the type's meaning and it is not expressible: two
//                SharedBoxes pointing at one object would be written twice and read back
//                as two objects, which is a silent change of behaviour, not a slow path.
//   BoxOrCRef<T>  refused. Whether it owns the pointee is a runtime property, so the
//                reader could not know whether to allocate.
//
// Box<T> and T hash the SAME, because the bytes really are identical: a stream written from
// a struct with a T field reads back into one with a Box<T> field. A POLYMORPHIC Box -
// Box<Base> actually holding a Derived - is not handled and cannot be, since the stream
// would have to say which type to allocate; write a tag and a serializer that switches on
// it.

#include <base/pointers/box.hpp>
#include <base/pointers/box_or_ref.hpp>
#include <base/pointers/shared_box.hpp>

#include <ser/concepts.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/detail/fillable.hpp>
#include <ser/detail/meta.hpp>
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

	namespace detail {

		// Keyed to the archive, not to T: see the note on denyNonOwningRef in refs.hpp for
		// why the difference decides whether the refusal waits for a real call.
		template<class Ar>
		constexpr Errc denySharedBox() {
			static_assert(
				DEPENDENT_FALSE<Ar>,
				"ser: cannot serialize base::SharedBox - what the type provides is SHARING, "
				"and sharing is not on the wire. Two SharedBoxes onto one object would be "
				"written as two objects and read back as two objects, so the reader would "
				"silently get a different object graph rather than an error.\n"
				"  Only one holder in the payload?  serialize the VALUE (or a Box<T>)\n"
				"  Several holders?                 serialize the objects once, in a "
				"container, and store an index per holder - then rebuild the sharing after "
				"reading"
			);
			return Errc::InvalidValue;
		}

		template<class Ar>
		constexpr Errc denyBoxOrCRef() {
			static_assert(
				DEPENDENT_FALSE<Ar>,
				"ser: cannot serialize base::BoxOrCRef - whether it owns the pointee is a "
				"runtime property, so nothing in the stream could tell the reader whether "
				"to allocate. Decide at the field: base::Box<T> when the payload owns it, "
				"an index into the owning container when it does not."
			);
			return Errc::InvalidValue;
		}

	}  // namespace detail

	// ── Box ───────────────────────────────────────────────────────────────────

	template<class T, class D>
	struct min_wire_size<::base::Box<T, D>> {
		static constexpr ::std::size_t VALUE = min_wire_size<T>::VALUE;
	};

	// Transparent on purpose - the pointer is storage, not format. See the note above.
	template<class T, class D>
	struct schema<::base::Box<T, D>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return detail::schemaOf<T, Mode, Seen>(h);
		}
	};

	template<class T, class D>
	struct serializer<::base::Box<T, D>> {
		using box_type = ::base::Box<T, D>;

		static constexpr Errc write(writer auto& ar, const box_type& b) {
			// operator* is const and hands back T&, and a Box is never null - the class
			// panics before it can be - so there is no presence byte and no check here.
			return detail::dispatchWrite<T>(ar, *b);
		}

		static constexpr Errc read(reader auto& ar, box_type& b)
			requires(detail::READABLE_ELEMENT_V<T>) {
			// The Box already owns an object, so reading fills THAT object rather than
			// allocating a second one and throwing the first away.
			if constexpr (detail::FILL_IN_PLACE_V<T>)
				return detail::dispatchRead<T>(ar, *b);
			else {
				b = ::base::makeBox<T, D>(detail::dispatchMake<T>(ar));
				return Errc::Ok;
			}
		}

		static box_type make(reader auto& ar) requires(detail::READABLE_ELEMENT_V<T>) {
			if constexpr (detail::FILL_IN_PLACE_V<T>) {
				box_type b = ::base::makeBox<T, D>();
				if (const auto c = detail::dispatchRead<T>(ar, *b); c != Errc::Ok)
					throwError(c, ar.position());
				return b;
			} else
				return ::base::makeBox<T, D>(detail::dispatchMake<T>(ar));
		}
	};

	// ── MBox ──────────────────────────────────────────────────────────────────

	template<class T, class D>
	struct min_wire_size<::base::MBox<T, D>> {
		static constexpr ::std::size_t VALUE = min_wire_size<::std::optional<T>>::VALUE;
	};

	// The same bytes as std::optional<T>, so the same hash: a stream written from an
	// optional field reads into an MBox field and back.
	template<class T, class D>
	struct schema<::base::MBox<T, D>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return detail::schemaOf<::std::optional<T>, Mode, Seen>(h);
		}
	};

	template<class T, class D>
	struct serializer<::base::MBox<T, D>> {
		using box_type = ::base::MBox<T, D>;

		static constexpr Errc write(writer auto& ar, const box_type& b) {
			const ::std::uint8_t present = b ? 1u : 0u;
			if (const auto c = detail::dispatchWrite<::std::uint8_t>(ar, present); c != Errc::Ok)
				return c;
			if (!present) return Errc::Ok;
			return detail::dispatchWrite<T>(ar, *b);
		}

		static constexpr Errc read(reader auto& ar, box_type& b)
			requires(detail::READABLE_ELEMENT_V<T>) {
			::std::uint8_t present = 0;
			if (const auto c = detail::dispatchRead<::std::uint8_t>(ar, present); c != Errc::Ok)
				return c;
			// A byte that is neither 0 nor 1 is corrupt input, not a value to interpret -
			// the same rule the optional and bool adapters follow.
			if (present > 1) return Errc::InvalidValue;

			if (present == 0) {
				b = box_type{};  // drops whatever was there, which is the point of a read
				return Errc::Ok;
			}

			if constexpr (detail::FILL_IN_PLACE_V<T>) {
				// Reuse the allocation when there is one; allocate only for a null MBox.
				if (!b) b = ::base::makeBox<T, D>();
				return detail::dispatchRead<T>(ar, *b);
			} else {
				b = ::base::makeBox<T, D>(detail::dispatchMake<T>(ar));
				return Errc::Ok;
			}
		}
	};

	// ── the two refusals ──────────────────────────────────────────────────────

	template<class T>
	struct serializer<::base::SharedBox<T>> {
		static constexpr Errc write(writer auto& ar, const ::base::SharedBox<T>&) {
			return detail::denySharedBox<decltype(ar)>();
		}

		static constexpr Errc read(reader auto& ar, ::base::SharedBox<T>&) {
			return detail::denySharedBox<decltype(ar)>();
		}
	};

	template<class T>
	struct serializer<::base::BoxOrCRef<T>> {
		static constexpr Errc write(writer auto& ar, const ::base::BoxOrCRef<T>&) {
			return detail::denyBoxOrCRef<decltype(ar)>();
		}

		static constexpr Errc read(reader auto& ar, ::base::BoxOrCRef<T>&) {
			return detail::denyBoxOrCRef<decltype(ar)>();
		}
	};

}  // namespace ser
