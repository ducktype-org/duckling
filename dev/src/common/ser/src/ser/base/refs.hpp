#pragma once

/*
 * base::Ref, base::CRef, base::MRef, base::MCRef
 * Refused, in both directions. A Ref is an address that does not own what it points at,
 * and neither half of that survives the trip: the address means nothing in another
 * process, and nothing in the type says who keeps the pointee alive. There is no honest
 * thing to write, and on read there is nothing to rebind - a Ref cannot be made to point
 * somewhere new once it exists.
 *
 * The same refusal ser already gives a raw pointer (builtin/pointer_deny.hpp), spelled out
 * for the base types on purpose: without it they fall to the "looks pointer-like" heuristic
 * at the bottom of dispatch, whose message sends the reader looking for a header that would
 * fix it. There is no such header - this one IS the answer.
 *
 * CRef<T> is Ref<const T> and MCRef<T> is MRef<const T>, both plain aliases, so the two
 * specializations below cover all four names.
 */

#include <base/comptime/type_list.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/pointers/ref.hpp>

#include <ser/concepts.hpp>
#include <ser/errc.hpp>
#include <ser/serializer.hpp>

namespace ser {

	namespace internal {

		/**
		 * @brief A function rather than a static_assert in each hook, so the message cannot drift
		 * between the two. The parameter is the ARCHIVE and that is not cosmetic: a condition
		 * not depending on the enclosing template's own parameters may be diagnosed where the
		 * template is DEFINED rather than instantiated - gcc waits, clang does not - and
		 * keying it to the archive is what keeps declaring the specialization free.
		 */
		template<class Ar>
		constexpr Errc denyNonOwningRef() {
			static_assert(
				::base::DEPENDENT_FALSE_V<Ar>,
				"ser: cannot serialize base::Ref / CRef / MRef / MCRef - it is an address "
				"that does not own what it points at, so the pointee does not travel with "
				"it and reading cannot rebind it.\n"
				"  The only owner?     serialize the pointed-to VALUE instead\n"
				"  One of many?        store an index into the container that owns it, and "
				"serialize that container once\n"
				"  Standing in for \"may be absent\"?  base::Optional<T> "
				"(<ser/base/optional.hpp>)"
			);
			return Errc::InvalidValue;
		}

	} /* namespace internal */

	template<class T>
	struct serializer<::base::Ref<T>> {
		static constexpr Errc write(writer auto& ar, const ::base::Ref<T>&) {
			return internal::denyNonOwningRef<decltype(ar)>();
		}

		/**
		 * @brief Present so the refusal is the message the user gets. With only a write hook the
		 * pairing rule in checkHooks fires first and complains about a missing read hook,
		 * which is true and useless.
		 */
		static constexpr Errc read(reader auto& ar, ::base::Ref<T>&) {
			return internal::denyNonOwningRef<decltype(ar)>();
		}
	};

	template<class T>
	struct serializer<::base::MRef<T>> {
		static constexpr Errc write(writer auto& ar, const ::base::MRef<T>&) {
			return internal::denyNonOwningRef<decltype(ar)>();
		}

		static constexpr Errc read(reader auto& ar, ::base::MRef<T>&) {
			return internal::denyNonOwningRef<decltype(ar)>();
		}
	};

} /* namespace ser */
