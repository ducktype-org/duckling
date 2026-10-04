#pragma once

/**
 * @file
 * @brief base::Box, base::MBox, base::SharedBox, base::BoxOrCRef
 * @details Two of these serialize and two are refused, and the line between them is ownership.
 *
 *   Box<T>       the sole owner of one T. The pointer is storage, not format: a Box<T>
 *                writes exactly what a T writes, and nothing says "this was behind a
 *                pointer". Round-trips to a fresh allocation.
 *   MBox<T>      the same, plus "may be null" - so exactly the format of an optional:
 *                one presence byte, then the value if there is one.
 *   SharedBox<T> refused. Sharing IS the type's meaning and it is not expressible: two
 *                SharedBoxes pointing at one object would be written twice and read back
 *                as two objects, which is a silent change of behaviour, not a slow path.
 *   BoxOrCRef<T>  refused. Whether it owns the pointee is a runtime property, so the
 *                reader could not know whether to allocate.
 *
 * Box<T> and T hash the SAME, because the bytes really are identical: a stream written from
 * a struct with a T field reads back into one with a Box<T> field. A POLYMORPHIC Box -
 * Box<Base> actually holding a Derived - is not handled and cannot be, since the stream
 * would have to say which type to allocate; write a tag and a serializer that switches on
 * it.
 */

#include <base/comptime/type_list.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/box_or_ref.hpp>
#include <base/pointers/default_deleter.hpp>
#include <base/pointers/shared_box.hpp>

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

	namespace internal {

		/**
		 * @brief Keyed to the archive, not to T: see the note on denyNonOwningRef in refs.hpp for
		 * why the difference decides whether the refusal waits for a real call.
		 */
		template<class Ar>
		constexpr Errc denySharedBox() {
			static_assert(
				::base::DEPENDENT_FALSE_V<Ar>,
				"ser: cannot serialize base::SharedBox - what the type provides is SHARING, "
				"and sharing is not in the stream. Two SharedBoxes onto one object would be "
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
		constexpr Errc denyCustomBoxDeleter() {
			static_assert(
				::base::DEPENDENT_FALSE_V<Ar>,
				"ser: cannot serialize a base::Box / base::MBox with a custom deleter - "
				"reading one ALLOCATES, and the only allocation ser has is `new T` with a "
				"default-constructed deleter. A custom deleter says the pointee came from "
				"somewhere else - an arena, a pool, malloc, a C API - so ser would hand "
				"`new`-ed memory to something that frees it another way, and the state the "
				"deleter needs to find its way home (WHICH arena) is not in the stream.\n"
				"  The default deleter?  base::Box<T> / base::MBox<T>, nothing to do\n"
				"  An arena or a pool?   serialize the VALUE, and put it back where it "
				"belongs yourself after reading\n"
				"  Stateless, and really plain `delete`?  opt in with one line:\n"
				"    template<> struct ser::BoxDeleterIsNewDelete<MyDeleter> { static "
				"constexpr bool VALUE = true; };"
			);
			return Errc::InvalidValue;
		}

		template<class Ar>
		constexpr Errc denyBoxOrCRef() {
			static_assert(
				::base::DEPENDENT_FALSE_V<Ar>,
				"ser: cannot serialize base::BoxOrCRef - whether it owns the pointee is a "
				"runtime property, so nothing in the stream could tell the reader whether "
				"to allocate. Decide at the field: base::Box<T> when the payload owns it, "
				"an index into the owning container when it does not."
			);
			return Errc::InvalidValue;
		}

	}  // namespace internal

	// which deleters a read may allocate for

	/**
	 * @brief Does D free its pointee with plain `delete`, and carry no state?
	 * Those are the two promises, and reading a Box needs both: it allocates with `new` and
	 * default-constructs the deleter. True only for base::DefaultBoxPtrDeleter; anything
	 * else is refused unless you opt in with one line:
	 *
	 *     template<>
	 *     struct ser::BoxDeleterIsNewDelete<MyDeleter> {
	 *         static constexpr bool VALUE = true;
	 *     };
	 */
	template<class D>
	struct BoxDeleterIsNewDelete final {
		static constexpr bool VALUE = false;
	};

	/**
	 * @brief Any U: DefaultBoxPtrDeleter is plain `delete ptr` whatever it is spelled over, which
	 * is exactly what makeBox's `new` pairs with.
	 */
	template<class U>
	struct BoxDeleterIsNewDelete<::base::DefaultBoxPtrDeleter<U>> {
		static constexpr bool VALUE = true;
	};

	template<class D>
	inline constexpr bool BOX_DELETER_IS_NEW_DELETE_V = BoxDeleterIsNewDelete<D>::VALUE;

	/** @brief Box */

	template<class T, class D>
	struct MinSerializedSize<::base::Box<T, D>> {
		static constexpr ::std::size_t VALUE = MinSerializedSize<T>::VALUE;
	};

	/** @brief Transparent on purpose - the pointer is storage, not format. See the note above. */
	template<class T, class D>
	struct Schema<::base::Box<T, D>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return internal::schemaOf<T, Mode, Seen>(h);
		}
	};

	template<class T, class D>
	struct Serializer<::base::Box<T, D>> {
		using BoxType = ::base::Box<T, D>;

		/**
		 * @brief Writing needs nothing from the deleter, so this could have let a custom one
		 * through - but a type that writes and cannot be read back is what the pairing rule
		 * in checkHooks exists to refuse, and the refusal reads better at the write than as
		 * a surprise at the first read.
		 */
		static constexpr Errc write(Writer auto& ar, const BoxType& b) {
			if constexpr (!BOX_DELETER_IS_NEW_DELETE_V<D>)
				return internal::denyCustomBoxDeleter<decltype(ar)>();
			else
				// operator* is const and hands back T&, and a Box is never null - the class
				// panics before it can be - so there is no presence byte and no check here.
				return internal::dispatchWrite<T>(ar, *b);
		}

		static constexpr Errc read(Reader auto& ar, BoxType& b)
			requires(internal::READABLE_ELEMENT_V<T>) {
			if constexpr (!BOX_DELETER_IS_NEW_DELETE_V<D>)
				return internal::denyCustomBoxDeleter<decltype(ar)>();
			// The Box already owns an object, so reading fills THAT object rather than
			// allocating a second one and throwing the first away.
			else if constexpr (internal::FILL_IN_PLACE_V<T>)
				return internal::dispatchRead<T>(ar, *b);
			else {
				b = ::base::makeBox<T, D>(internal::dispatchMake<T>(ar));
				return Errc::Ok;
			}
		}

		static BoxType make(Reader auto& ar) requires(internal::READABLE_ELEMENT_V<T>) {
			// throwError only so this compiles once the static_assert has had its say - a
			// make has a box to return and a refused deleter has none.
			if constexpr (!BOX_DELETER_IS_NEW_DELETE_V<D>)
				throwError(internal::denyCustomBoxDeleter<decltype(ar)>(), ar.position());
			else if constexpr (internal::FILL_IN_PLACE_V<T>) {
				BoxType b = ::base::makeBox<T, D>();
				if (const auto c = internal::dispatchRead<T>(ar, *b); c != Errc::Ok)
					throwError(c, ar.position());
				return b;
			} else
				return ::base::makeBox<T, D>(internal::dispatchMake<T>(ar));
		}
	};

	/** @brief MBox */

	template<class T, class D>
	struct MinSerializedSize<::base::MBox<T, D>> {
		static constexpr ::std::size_t VALUE = MinSerializedSize<::std::optional<T>>::VALUE;
	};

	/**
	 * @brief The same bytes as std::optional<T>, so the same hash: a stream written from an
	 * optional field reads into an MBox field and back.
	 */
	template<class T, class D>
	struct Schema<::base::MBox<T, D>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return internal::schemaOf<::std::optional<T>, Mode, Seen>(h);
		}
	};

	template<class T, class D>
	struct Serializer<::base::MBox<T, D>> {
		using BoxType = ::base::MBox<T, D>;

		static constexpr Errc write(Writer auto& ar, const BoxType& b) {
			if constexpr (!BOX_DELETER_IS_NEW_DELETE_V<D>)
				return internal::denyCustomBoxDeleter<decltype(ar)>();
			else {
				const ::std::uint8_t present = b ? 1u : 0u;
				if (const auto c = internal::dispatchWrite<::std::uint8_t>(ar, present);
				    c != Errc::Ok)
					return c;
				if (!present) return Errc::Ok;
				return internal::dispatchWrite<T>(ar, *b);
			}
		}

		/**
		 * @brief An MBox is worse off than a Box even on the fill path: reading a null assigns a
		 * fresh BoxType, and reading a value into one that is null allocates - so whether
		 * the deleter survives would depend on the BYTES, which is the same runtime-property
		 * objection that refuses BoxOrCRef above.
		 */
		static constexpr Errc read(Reader auto& ar, BoxType& b)
			requires(internal::READABLE_ELEMENT_V<T>) {
			if constexpr (!BOX_DELETER_IS_NEW_DELETE_V<D>)
				return internal::denyCustomBoxDeleter<decltype(ar)>();
			else {
				::std::uint8_t present = 0;
				if (const auto c = internal::dispatchRead<::std::uint8_t>(ar, present);
				    c != Errc::Ok)
					return c;
				// A byte that is neither 0 nor 1 is corrupt input, not a value to interpret -
				// the same rule the optional and bool adapters follow.
				if (present > 1) return Errc::InvalidValue;

				if (present == 0) {
					b = BoxType{};  // drops whatever was there, which is the point of a read
					return Errc::Ok;
				}

				if constexpr (internal::FILL_IN_PLACE_V<T>) {
					// Reuse the allocation when there is one; allocate only for a null MBox.
					if (!b) b = ::base::makeBox<T, D>();
					return internal::dispatchRead<T>(ar, *b);
				} else {
					b = ::base::makeBox<T, D>(internal::dispatchMake<T>(ar));
					return Errc::Ok;
				}
			}
		}
	};

	/** @brief the two refusals */

	template<class T>
	struct Serializer<::base::SharedBox<T>> {
		static constexpr Errc write(Writer auto& ar, const ::base::SharedBox<T>&) {
			return internal::denySharedBox<decltype(ar)>();
		}

		static constexpr Errc read(Reader auto& ar, ::base::SharedBox<T>&) {
			return internal::denySharedBox<decltype(ar)>();
		}
	};

	template<class T>
	struct Serializer<::base::BoxOrCRef<T>> {
		static constexpr Errc write(Writer auto& ar, const ::base::BoxOrCRef<T>&) {
			return internal::denyBoxOrCRef<decltype(ar)>();
		}

		static constexpr Errc read(Reader auto& ar, ::base::BoxOrCRef<T>&) {
			return internal::denyBoxOrCRef<decltype(ar)>();
		}
	};

}  // namespace ser
