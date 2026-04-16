#pragma once

#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/types/ints.hpp>

#include <vm/utils/bijective_map.hpp>
#include <vm/utils/persistent_array.hpp>

namespace vm::persistent {
	STRONG_TYPEDEF_INT(HashMapStateID, u64);

	/**
	 * @brief Persistent data structure which simulates STL hashmap
	 *
	 * @note currently a wrapper for persistant array and bijective map value~idx.
	 * @note Allows for (==) comparison of two instances with ArrayStateID in O(1)
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

		NodeID emplaceKey(const KeyT& var) {
			auto [_, idx] = key_binding.emplaceByLeft(var, key_binding.size());
			return idx;
		}

	public:
		bool contains(HashMapStateID state_id, const KeyT& key) const {
			auto maybe_idx = key_binding.atLeftOpt(key);
			if (!maybe_idx) return false;

			auto inner = ArrayStateID{ u64(state_id) };

			return buffer.active(inner, *maybe_idx);
		}

		const ValT& access(HashMapStateID state_id, const KeyT& key) const {
			auto inner     = ArrayStateID{ u64(state_id) };
			auto maybe_idx = key_binding.atLeftOpt(key);

			if (!maybe_idx) throw std::invalid_argument("no such key in map");

			return buffer.access(inner, maybe_idx);
		}

		HashMapStateID insert(HashMapStateID state_id, const KeyT& key, const ValT& var) {
			auto inner     = ArrayStateID{ u64(state_id) };
			auto idx       = emplaceKey(key);
			auto new_state = buffer.change(inner, idx, var);

			return HashMapStateID{ u64{ new_state } };
		}

		HashMapStateID erase(HashMapStateID state_id, const KeyT& key) {
			auto inner     = ArrayStateID{ u64(state_id) };
			auto maybe_idx = key_binding.atLeftOpt(key);

			if (!maybe_idx) return state_id;

			auto new_state = buffer.erase(inner, *maybe_idx);

			return HashMapStateID{ u64{ new_state } };
		}

		std::pair<bool, HashMapStateID> emplace(
			HashMapStateID state_id, const KeyT& key, const ValT& var
		) {
			auto inner               = ArrayStateID{ u64(state_id) };
			auto idx                 = emplaceKey(key);
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
