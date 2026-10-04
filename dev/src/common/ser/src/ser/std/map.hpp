#pragma once

/*
 * std::map, std::unordered_map, std::set, std::unordered_set
 * Length prefix, then the elements: for a map each key followed by its value, for a set
 * each key. The container's own structure - buckets, tree shape, load factor - is not on
 * the wire and is rebuilt by the reader, which is why a map written by one implementation
 * reads back on another.
 *
 * THE KEY AND THE VALUE ARE READ AS TWO STATEMENTS, never as two arguments of one call.
 * `m.emplace(dispatchMake<K>(ar), dispatchMake<V>(ar))` is wrong: argument evaluation order
 * is unspecified, so the stream would be key-first on one compiler and value-first on
 * another, passing every round-trip test on both. Two declarations are ordered by the
 * language, and the cost is one move per element into the node.
 *
 * A repeated key is corrupt input, not a merge: the read fails with Errc::InvalidValue
 * rather than quietly producing a container smaller than the one that was written.
 *
 * On the unordered containers iteration order is unspecified, so the STREAM IS NOT A
 * CANONICAL FORM: one map written twice gives the same bytes, but two EQUAL maps need not.
 * So compare a round-trip element by element and never byte for byte, and reach for
 * std::map when a payload is going to be signed or content-addressed.
 */

#include <ser/concepts.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/internal/container.hpp>
#include <ser/internal/dispatch_fwd.hpp>
#include <ser/internal/fillable.hpp>
#include <ser/serializer.hpp>
#include <ser/std/tuple.hpp>
#include <ser/traits.hpp>

#include <cstddef>
#include <map>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace ser {

	namespace internal {

		/**
		 * @brief Only the hash-based containers have it, and asking the container rather than
		 * listing the types keeps this working for one with a custom allocator.
		 */
		template<class C>
		constexpr void reserveIfPossible(C& c, ::std::size_t n) {
			if constexpr (requires { c.reserve(n); }) c.reserve(n);
		}

		/** @brief NOT final: ser::Serializer<std::map/unordered_map> derives from it below. */
		template<class M, class K, class V>
		struct MapAdapter {
			static constexpr Errc write(Writer auto& ar, const M& m) {
				if (const auto c = writeLength<::std::pair<K, V>>(ar, m.size()); c != Errc::Ok)
					return c;
				for (const auto& [key, value]: m) {
					if (const auto c = dispatchWrite<K>(ar, key); c != Errc::Ok) return c;
					if (const auto c = dispatchWrite<V>(ar, value); c != Errc::Ok) return c;
				}
				return Errc::Ok;
			}

			static constexpr Errc read(Reader auto& ar, M& m)
				requires(BUILDABLE_V<K> && BUILDABLE_V<V>) {
				::std::size_t n = 0;
				if (const auto c = readLength<::std::pair<K, V>>(ar, n); c != Errc::Ok) return c;

				m.clear();
				reserveIfPossible(m, n);
				for (::std::size_t i = 0; i < n; ++i) {
					auto key   = dispatchMake<K>(ar); /* two statements, and that is */
					auto value = dispatchMake<V>(ar); /* the whole point - see above */
					if (!m.emplace(::std::move(key), ::std::move(value)).second)
						return Errc::InvalidValue;    /* the same key twice */
				}
				return Errc::Ok;
			}
		};

		/** @brief NOT final: ser::Serializer<std::set/unordered_set> derives from it below. */
		template<class S, class K>
		struct SetAdapter {
			static constexpr Errc write(Writer auto& ar, const S& s) {
				if (const auto c = writeLength<K>(ar, s.size()); c != Errc::Ok) return c;
				for (const auto& key: s)
					if (const auto c = dispatchWrite<K>(ar, key); c != Errc::Ok) return c;
				return Errc::Ok;
			}

			static constexpr Errc read(Reader auto& ar, S& s) requires(BUILDABLE_V<K>) {
				::std::size_t n = 0;
				if (const auto c = readLength<K>(ar, n); c != Errc::Ok) return c;

				s.clear();
				reserveIfPossible(s, n);
				for (::std::size_t i = 0; i < n; ++i)
					if (!s.insert(dispatchMake<K>(ar)).second) return Errc::InvalidValue;
				return Errc::Ok;
			}
		};

	} /* namespace internal */

	/*
	 * the schema of a keyed container
	 * std::map and std::unordered_map hash IDENTICALLY, and so do the two sets. They have
	 * to: the wire format is the same length prefix followed by the same elements, and a
	 * stream written from one really does read back into the other - the comparator, the
	 * hash and the bucket count are not on the wire. Hashing them apart would refuse a
	 * stream that is perfectly readable.
	 *
	 * What is NOT the same is ordering: a std::map stream is sorted, an unordered one is
	 * in whatever order the buckets were walked. That is a property of the bytes, not of
	 * the schema, and the note at the top of this file is where it belongs.
	 */
	namespace internal {

		template<class Mode, class Seen, class K, class V>
		[[nodiscard]] consteval ::std::uint64_t schemaMap(::std::uint64_t h) {
			h = schemaText(h, "map");
			h = schemaOf<K, Mode, Seen>(h);
			return schemaOf<V, Mode, Seen>(h);
		}

		template<class Mode, class Seen, class K>
		[[nodiscard]] consteval ::std::uint64_t schemaSet(::std::uint64_t h) {
			return schemaOf<K, Mode, Seen>(schemaText(h, "set"));
		}

	} /* namespace internal */

	template<class K, class V, class C, class Al>
	struct Schema<::std::map<K, V, C, Al>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return internal::schemaMap<Mode, Seen, K, V>(h);
		}
	};

	template<class K, class V, class H, class E, class Al>
	struct Schema<::std::unordered_map<K, V, H, E, Al>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return internal::schemaMap<Mode, Seen, K, V>(h);
		}
	};

	template<class K, class C, class Al>
	struct Schema<::std::set<K, C, Al>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return internal::schemaSet<Mode, Seen, K>(h);
		}
	};

	template<class K, class H, class E, class Al>
	struct Schema<::std::unordered_set<K, H, E, Al>> {
		template<class Mode, class Seen>
		static consteval ::std::uint64_t mix(::std::uint64_t h) {
			return internal::schemaSet<Mode, Seen, K>(h);
		}
	};

	/**
	 * @brief The smallest serialized map or set is an empty one, which is just its length
	 * prefix. Reading a container of maps uses this to reject a corrupt length before
	 * allocating.
	 */
	template<class K, class V, class C, class Al>
	struct MinWireSize<::std::map<K, V, C, Al>> {
		static constexpr ::std::size_t VALUE = sizeof(internal::WireSizeType);
	};

	template<class K, class V, class H, class E, class Al>
	struct MinWireSize<::std::unordered_map<K, V, H, E, Al>> {
		static constexpr ::std::size_t VALUE = sizeof(internal::WireSizeType);
	};

	template<class K, class C, class Al>
	struct MinWireSize<::std::set<K, C, Al>> {
		static constexpr ::std::size_t VALUE = sizeof(internal::WireSizeType);
	};

	template<class K, class H, class E, class Al>
	struct MinWireSize<::std::unordered_set<K, H, E, Al>> {
		static constexpr ::std::size_t VALUE = sizeof(internal::WireSizeType);
	};

	template<class K, class V, class C, class Al>
	struct Serializer<::std::map<K, V, C, Al>>:
		  internal::MapAdapter<::std::map<K, V, C, Al>, K, V> {};

	template<class K, class V, class H, class E, class Al>
	struct Serializer<::std::unordered_map<K, V, H, E, Al>>:
		  internal::MapAdapter<::std::unordered_map<K, V, H, E, Al>, K, V> {};

	template<class K, class C, class Al>
	struct Serializer<::std::set<K, C, Al>>: internal::SetAdapter<::std::set<K, C, Al>, K> {};

	template<class K, class H, class E, class Al>
	struct Serializer<::std::unordered_set<K, H, E, Al>>:
		  internal::SetAdapter<::std::unordered_set<K, H, E, Al>, K> {};

} /* namespace ser */
