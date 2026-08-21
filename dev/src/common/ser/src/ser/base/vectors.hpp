#pragma once

// ── base::StableVector ────────────────────────────────────────────────────────
// A length prefix and then the elements - byte for byte a std::vector<Data>, and hashed as
// one, because the stability the type provides is a property of its storage and not of the
// stream. So a std::vector stream reads into a StableVector and back.
//
// StableVector<const Data> - the "locked" form handed out by toConstData() - goes through
// the SAME adapter. The const is about what the accessors hand out, not about the
// container: pushBack and emplaceBack are exposed on both forms, and what is stored is
// Data either way. So the only difference is the read path, and it is the same difference
// std::vector already has:
//
//   fill   Data can be default-constructed and assigned, and the container is not the
//          locked form: emplaceBack an empty element, then read into it in place. No moves.
//   build  otherwise: emplaceBack(dispatchMake<Data>(ar)). One move per element, which is
//          also the only way in for the locked form - last() there hands back a CRef, and
//          nothing can be read through a const reference.
//
// No make hook, for the same reason the std::vector adapter has none: the type is
// default-constructible and movable, so dispatchMake already builds one by reading into a
// fresh object. A make hook would be a second copy of the format with nothing to add.

#include <base/collections/stable_container.hpp>

#include <ser/concepts.hpp>
#include <ser/detail/container.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/serializer.hpp>
#include <ser/std/vector.hpp>
#include <ser/traits.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace ser {

	template<class Data>
	struct min_wire_size<::base::StableVector<Data>> {
		static constexpr ::std::size_t VALUE = sizeof(detail::wire_size_type);
	};

	// remove_const, and not only because std::vector<const T> is ill-formed: the locked and
	// unlocked forms write the same bytes, so they are one format and one hash.
	template<class Data>
	struct schema<::base::StableVector<Data>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return detail::schemaOf<::std::vector<::std::remove_const_t<Data>>, Mode, Seen>(h);
		}
	};

	template<class Data>
	struct serializer<::base::StableVector<Data>> {
		using vector_type  = ::base::StableVector<Data>;
		using element_type = ::std::remove_const_t<Data>;

		static constexpr bool FILLABLE = !::std::is_const_v<Data>
		                              && ::std::default_initializable<element_type>
		                              && ::std::is_move_assignable_v<element_type>;

		static constexpr Errc write(writer auto& ar, const vector_type& v) {
			if (const auto c = detail::writeLength(ar, v.size()); c != Errc::Ok) return c;
			// Indexed rather than iterated: operator[] hands back a CRef, and the element
			// type is what dispatch has to see - not whatever the iterator dereferences to.
			for (::std::size_t i = 0; i < v.size(); ++i)
				if (const auto c = detail::dispatchWrite<element_type>(ar, *v[i]); c != Errc::Ok)
					return c;
			return Errc::Ok;
		}

		static constexpr Errc read(reader auto& ar, vector_type& v) {
			::std::size_t n = 0;
			// Before clear(), and before any element exists: readLength is what refuses a
			// prefix claiming more elements than the stream could possibly hold.
			if (const auto c = detail::readLength<element_type>(ar, n); c != Errc::Ok) return c;

			v.clear();
			for (::std::size_t i = 0; i < n; ++i) {
				if constexpr (FILLABLE) {
					v.emplaceBack();
					if (const auto c = detail::dispatchRead<element_type>(ar, *v.last());
					    c != Errc::Ok)
						return c;
				} else
					v.emplaceBack(detail::dispatchMake<element_type>(ar));
			}
			return Errc::Ok;
		}
	};

}  // namespace ser
