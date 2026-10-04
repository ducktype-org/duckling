#pragma once

/**
 * @file
 * @brief base::StableVector
 * @details A length prefix and then the elements - byte for byte a std::vector<Data>, and hashed as
 * one, because the stability the type provides is a property of its storage and not of the
 * stream. So a std::vector stream reads into a StableVector and back.
 *
 * StableVector<const Data> - the "locked" form from toConstData() - goes through the SAME
 * adapter: the const is about what the accessors hand out, not about the container. So the
 * only difference is the read path, and it is the one std::vector already has:
 *
 *   fill   Data can be default-constructed and assigned, and the container is not the
 *          locked form: emplaceBack an empty element, then read into it in place. No moves.
 *   build  otherwise: emplaceBack(dispatchMake<Data>(ar)). One move per element, which is
 *          also the only way in for the locked form - last() there hands back a CRef, and
 *          nothing can be read through a const reference.
 *
 * No make hook, for the same reason the std::vector adapter has none: the type is
 * default-constructible and movable, so dispatchMake already builds one by reading into a
 * fresh object.
 */

#include <base/collections/stable_container.hpp>
#include <base/except/exceptions.hpp>

#include <ser/concepts.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/internal/container.hpp>
#include <ser/internal/dispatch_fwd.hpp>
#include <ser/internal/fillable.hpp>
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
	struct MinSerializedSize<::base::StableVector<Data>> {
		static constexpr ::std::size_t VALUE = sizeof(internal::LengthType);
	};

	/**
	 * @brief remove_const, and not only because std::vector<const T> is ill-formed: the locked and
	 * unlocked forms write the same bytes, so they are one format and one hash.
	 */
	template<class Data>
	struct Schema<::base::StableVector<Data>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return internal::schemaOf<::std::vector<::std::remove_const_t<Data>>, Mode, Seen>(h);
		}
	};

	template<class Data>
	struct Serializer<::base::StableVector<Data>> {
		using VectorType  = ::base::StableVector<Data>;
		using ElementType = ::std::remove_const_t<Data>;

		static constexpr bool FILLABLE
			= !::std::is_const_v<Data> && internal::FILL_IN_PLACE_V<ElementType>;

		static constexpr Errc write(Writer auto& ar, const VectorType& v) {
			if (const auto c = internal::writeLength<ElementType>(ar, v.size()); c != Errc::Ok)
				return c;
			// Indexed rather than iterated: operator[] hands back a CRef, and the element
			// type is what dispatch has to see - not whatever the iterator dereferences to.
			for (::std::size_t i = 0; i < v.size(); ++i)
				if (const auto c = internal::dispatchWrite<ElementType>(ar, *v[i]); c != Errc::Ok)
					return c;
			return Errc::Ok;
		}

		static constexpr Errc read(Reader auto& ar, VectorType& v)
			requires(FILLABLE || internal::BUILDABLE_V<ElementType>) {
			::std::size_t n = 0;
			// Before clear(), and before any element exists: readLength is what refuses a
			// prefix claiming more elements than the stream could possibly hold.
			if (const auto c = internal::readLength<ElementType>(ar, n); c != Errc::Ok) return c;

			CORE_ASSERT(v.empty(), "ser: reading into a non-empty base::StableVector");
			for (::std::size_t i = 0; i < n; ++i) {
				if constexpr (FILLABLE) {
					v.emplaceBack();
					if (const auto c = internal::dispatchRead<ElementType>(ar, *v.last());
					    c != Errc::Ok)
						return c;
				} else
					v.emplaceBack(internal::dispatchMake<ElementType>(ar));
			}
			return Errc::Ok;
		}
	};

}  // namespace ser
