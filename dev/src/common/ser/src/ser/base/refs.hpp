#pragma once

// ── base::Ref, base::CRef, base::MRef, base::MCRef ────────────────────────────
// Refused, in both directions. A Ref is an address that does not own what it points at,
// and neither half of that survives the trip: the address means nothing in another
// process, and nothing in the type says who keeps the pointee alive. There is no honest
// thing to write, and on read there is nothing to rebind - a Ref cannot be made to point
// somewhere new once it exists.
//
// This is the refusal ser already gives a raw pointer and a std::reference_wrapper
// (builtin/pointer_deny.hpp), spelled out for the base types on purpose. Without it these
// fall to the bottom of dispatch and get the "looks pointer-like" heuristic, whose message
// says "base::Box / base::Ref? not supported yet" and sends the reader looking for a header
// that would fix it. There is no such header: this one IS the answer.
//
// CRef<T> is Ref<const T> and MCRef<T> is MRef<const T>, both plain aliases, so the two
// specializations below cover all four names.

#include <base/pointers/ref.hpp>

#include <ser/concepts.hpp>
#include <ser/detail/meta.hpp>
#include <ser/errc.hpp>
#include <ser/serializer.hpp>

namespace ser {

	namespace detail {

		// A function rather than a static_assert in each hook: the message is the whole
		// user interface of a refusal, and one copy of it cannot drift from the other.
		//
		// The parameter is the ARCHIVE, and that is not cosmetic. A condition that does not
		// depend on the enclosing template's own parameters may be diagnosed where the
		// template is defined rather than where it is instantiated - gcc waits, clang does
		// not - so keying the assert to the archive type is what makes "declaring this
		// specialization is free, only a real write or read fires it" true on both.
		template<class Ar>
		constexpr Errc denyNonOwningRef() {
			static_assert(
				DEPENDENT_FALSE<Ar>,
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

	}  // namespace detail

	template<class T>
	struct serializer<::base::Ref<T>> {
		static constexpr Errc write(writer auto& ar, const ::base::Ref<T>&) {
			return detail::denyNonOwningRef<decltype(ar)>();
		}

		// Present so the refusal is the message the user gets. With only a write hook the
		// pairing rule in checkHooks fires first and complains about a missing read hook,
		// which is true and useless.
		static constexpr Errc read(reader auto& ar, ::base::Ref<T>&) {
			return detail::denyNonOwningRef<decltype(ar)>();
		}
	};

	template<class T>
	struct serializer<::base::MRef<T>> {
		static constexpr Errc write(writer auto& ar, const ::base::MRef<T>&) {
			return detail::denyNonOwningRef<decltype(ar)>();
		}

		static constexpr Errc read(reader auto& ar, ::base::MRef<T>&) {
			return detail::denyNonOwningRef<decltype(ar)>();
		}
	};

}  // namespace ser
