#pragma once

#include "base/types/ints.hpp"
#include <base/extend_cpp/strongly_typed_int.hpp>

#include "vm/loader/compiler/persistent_vector.hpp"
#include <vm/loader/compiler/bijective_map.hpp>
#include <vm/loader/compiler/persistent_array.hpp>

#include <ranges>
#include <unordered_set>

namespace persistent {
	STRONG_TYPEDEF_INT(HashMapID, u64);

	template<
		typename KeyT,
		typename ValT,
		typename KeyH = std::hash<KeyT>,
		typename ValH = std::hash<ValT>>
	class HashMap {
		using NodeID = u64;

		Array<ValT>                       buffer;
		detail::BijectiveMap<KeyT, usize> key_binding;

		NodeID getIdxOfkey(const KeyT& var) {
			auto [_, idx] = key_binding.emplaceByLeft(var, key_binding.size());
			return idx;
		}

	public:
		const ValT& access(HashMapID state_id, usize idx) const {}

		HashMapID insert(HashMapID state_id, const KeyT& key, const ValT& var) {
			auto inner     = VectorStateID{ u64(state_id) };
			auto idx       = getIdxOfkey(key);
			auto new_state = buffer.insert(inner, idx, var);

			return HashMapID{ u64{ new_state } };
		}

		HashMapID erase(HashMapID state_id, const KeyT& key, const ValT& var) {
			auto inner     = VectorStateID{ u64(state_id) };
			auto idx       = getIdxOfkey(key);
			auto new_state = buffer.erase(inner, idx, var);

			return HashMapID{ u64{ new_state } };
		}

		[[nodiscard]]
		HashMapID getEmpty() const { return HashMapID{ 1 }; }

		HashMap(usize buffer_init) : buffer{buffer_init} {}
	};
}
