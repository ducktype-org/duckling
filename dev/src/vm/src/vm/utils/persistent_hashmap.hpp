#pragma once

#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/types/ints.hpp>

#include <vm/utils/bijective_map.hpp>
#include <vm/utils/persistent_array.hpp>

namespace persistent {
	STRONG_TYPEDEF_INT(HashMapStateID, u64);

	/**
	 * @brief Persistent data structure which simulates STL hashmap
	 *
	 * @note currently a wrapper for persistant arrau and bijective map value~idx
	 *
	 * @tparam KeyT
	 * @tparam ValT
	 * @tparam KeyH
	 * @tparam ValH
	 */
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
		const ValT& access(HashMapStateID state_id, const KeyT& key) const {
			auto inner     = ArrayStateID{ u64(state_id) };
			auto idx       = getIdxOfkey(key);
			auto new_state = buffer.access(inner, idx);

			return HashMapStateID{ u64{ new_state } };
		}

		HashMapStateID insert(HashMapStateID state_id, const KeyT& key, const ValT& var) {
			auto inner     = ArrayStateID{ u64(state_id) };
			auto idx       = getIdxOfkey(key);
			auto new_state = buffer.insert(inner, idx, var);

			return HashMapStateID{ u64{ new_state } };
		}

		HashMapStateID erase(HashMapStateID state_id, const KeyT& key, const ValT& var) {
			auto inner     = ArrayStateID{ u64(state_id) };
			auto idx       = getIdxOfkey(key);
			auto new_state = buffer.erase(inner, idx, var);

			return HashMapStateID{ u64{ new_state } };
		}

		std::pair<bool, HashMapStateID> emplace(
			HashMapStateID state_id, const KeyT& key, const ValT& var
		) {
			auto inner               = ArrayStateID{ u64(state_id) };
			auto idx                 = getIdxOfkey(key);
			auto [is_new, new_state] = buffer.emplace(inner, idx, var);

			return { is_new, HashMapStateID{ u64{ new_state } } };
		}

		[[nodiscard]]
		HashMapStateID getEmpty() const {
			return HashMapStateID{ 1 };
		}

		HashMap(usize buffer_init): buffer{ buffer_init } {}
	};
}
