#pragma once

// ── base::Map, base::HashMap, base::VectorMap, base::StableHashMap ────────────
// Three shapes, three reasons.
//
//   MapWrapper<C>     base::Map and base::HashMap are this over std::map and
//                     std::unordered_map. It IS its container - public inheritance, no
//                     data of its own - so the adapter casts to the base and hands the
//                     work to the std adapter. Same bytes, same schema: a base::Map stream
//                     reads into a std::map and back.
//   VectorMap         a vector of Optional slots indexed by the key, so the key is never
//                     on the wire. Written densely, which keeps the format canonical:
//                     empty slots at the end survive the round-trip and equal maps give
//                     equal bytes.
//   StableHashMap     length prefix, then each key followed by its value - the same format
//                     as std::map, so the same schema, and a stream really does move
//                     between them. The bucket layout is rebuilt on read, which is what
//                     makes that true.
//
// The cast in MapWrapper's adapter is the point of it: MapWrapper hides operator[] and
// insert to force put(), and the cast puts the std API back in scope. Nothing that matters
// is bypassed - the wrapper has no invariant of its own, it only re-purposes operator[] to
// mean at().

#include <base/collections/maps.hpp>
#include <base/collections/stable_hashmap.hpp>

#include <ser/base/optional.hpp>
#include <ser/concepts.hpp>
#include <ser/detail/container.hpp>
#include <ser/detail/dispatch_fwd.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/serializer.hpp>
#include <ser/std/map.hpp>
#include <ser/std/vector.hpp>
#include <ser/traits.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

namespace ser {

	// ── MapWrapper: base::Map, base::HashMap ──────────────────────────────────

	template<class ContainerType>
	struct min_wire_size<::base::MapWrapper<ContainerType>> {
		static constexpr ::std::size_t VALUE = min_wire_size<ContainerType>::VALUE;
	};

	// Delegated rather than mixed here: the wrapper adds nothing to the format, so a hash
	// of its own would be a second description of the container's format, free to drift
	// from the first. Without this specialization the wrapper is hashed as an opaque hook -
	// sizeof and alignof only - and sizeof(std::map) does not depend on K or V, so every
	// base::Map in the program would share one schema.
	template<class ContainerType>
	struct schema<::base::MapWrapper<ContainerType>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return detail::schemaOf<ContainerType, Mode, Seen>(h);
		}
	};

	template<class ContainerType>
	struct serializer<::base::MapWrapper<ContainerType>> {
		// Self is a named template parameter rather than `auto&` so that is_const_v answers
		// about the OBJECT - decltype(self) would be a reference, and a reference is never
		// const on top.
		template<class Ar, class Self>
		static constexpr Errc visit(Ar& ar, Self& self) {
			using Base
				= ::std::conditional_t<::std::is_const_v<Self>, const ContainerType, ContainerType>;
			return ar(static_cast<Base&>(self));
		}
	};

	// ── VectorMap ─────────────────────────────────────────────────────────────

	template<class KEY_T, class DATA_T, bool is_move, bool is_copy>
	struct min_wire_size<::base::VectorMap<KEY_T, DATA_T, is_move, is_copy>> {
		static constexpr ::std::size_t VALUE
			= min_wire_size<::std::vector<::base::Optional<DATA_T>>>::VALUE;
	};

	template<class KEY_T, class DATA_T, bool is_move, bool is_copy>
	struct schema<::base::VectorMap<KEY_T, DATA_T, is_move, is_copy>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			// The key is not on the wire - it is the index - but it is in the hash on
			// purpose: two VectorMaps keyed by different strong id types produce
			// byte-identical streams, and without this the envelope could not tell them
			// apart. is_move and is_copy are left out for the mirror-image reason: they
			// change what the type lets you do, not what it writes.
			h = detail::schemaText(h, "base.VectorMap");
			h = detail::schemaOf<KEY_T, Mode, Seen>(h);
			return detail::schemaOf<DATA_T, Mode, Seen>(h);
		}
	};

	template<class KEY_T, class DATA_T, bool is_move, bool is_copy>
	struct serializer<::base::VectorMap<KEY_T, DATA_T, is_move, is_copy>> {
		using Vm = ::base::VectorMap<KEY_T, DATA_T, is_move, is_copy>;

		// Asymmetric, and the asymmetry is why this is not a visit: element_count is a
		// function of the slot vector, so it is recomputed on read rather than read.
		// Writing it would let a corrupt stream disagree with the vector, and then size()
		// lies, empty() lies with elements present, and erase() walks the counter below
		// zero.
		static constexpr Errc write(writer auto& ar, const Vm& m) { return ar(m.map); }

		static constexpr Errc read(reader auto& ar, Vm& m) {
			if (const auto c = ar(m.map); c != Errc::Ok) return c;

			m.element_count = 0;
			for (const auto& slot: m.map)
				if (slot.has_value()) ++m.element_count;
			return Errc::Ok;
		}
	};

	// ── StableHashMap ─────────────────────────────────────────────────────────

	template<class KEY_T, class DATA_T, class HASH_T, ::u64 BLOCK>
	struct min_wire_size<::base::StableHashMap<KEY_T, DATA_T, HASH_T, BLOCK>> {
		static constexpr ::std::size_t VALUE = sizeof(detail::wire_size_type);
	};

	// The same hash as std::map and std::unordered_map, because it is the same format. The
	// hash functor and the allocator block size are not on the wire and are not hashed.
	template<class KEY_T, class DATA_T, class HASH_T, ::u64 BLOCK>
	struct schema<::base::StableHashMap<KEY_T, DATA_T, HASH_T, BLOCK>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return detail::schemaMap<Mode, Seen, KEY_T, DATA_T>(h);
		}
	};

	template<class KEY_T, class DATA_T, class HASH_T, ::u64 BLOCK>
	struct serializer<::base::StableHashMap<KEY_T, DATA_T, HASH_T, BLOCK>> {
		using Shm = ::base::StableHashMap<KEY_T, DATA_T, HASH_T, BLOCK>;

		static constexpr Errc write(writer auto& ar, const Shm& m) {
			if (const auto c = detail::writeLength(ar, m.size()); c != Errc::Ok) return c;
			for (const auto& pair: m) {
				if (const auto c = detail::dispatchWrite<KEY_T>(ar, pair.key); c != Errc::Ok)
					return c;
				if (const auto c = detail::dispatchWrite<DATA_T>(ar, pair.value); c != Errc::Ok)
					return c;
			}
			return Errc::Ok;
		}

		static constexpr Errc read(reader auto& ar, Shm& m) {
			::std::size_t n = 0;
			if (const auto c = detail::readLength<::std::pair<KEY_T, DATA_T>>(ar, n); c != Errc::Ok)
				return c;

			m.clear();
			for (::std::size_t i = 0; i < n; ++i) {
				// Two statements, never two arguments of one call: argument evaluation
				// order is unspecified, so a key-first format would hold on one compiler
				// and not on another. The std map adapter says the same at length.
				auto key   = detail::dispatchMake<KEY_T>(ar);
				auto value = detail::dispatchMake<DATA_T>(ar);
				// maybePut rather than put: put PANICS on a repeated key, and a repeated
				// key is corrupt input - an error code, not a crash.
				if (!m.maybePut(::std::move(key), ::std::move(value))) return Errc::InvalidValue;
			}
			return Errc::Ok;
		}
	};

}  // namespace ser
