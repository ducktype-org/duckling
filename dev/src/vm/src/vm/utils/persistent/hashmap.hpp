#pragma once

#include "base/collections/optional.hpp"
#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/types/ints.hpp>

#include <vm/utils/bijective_map.hpp>
#include <vm/utils/persistent/memory.hpp>

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
	class HashMap final: private Memory {
		detail::BijectiveMap<ValT, usize, ValH> held_values{};
		detail::BijectiveMap<KeyT, usize, KeyH> held_keys{};

		usize next_val_id = 0;
		usize next_key_id = 0;

		usize emplaceKey(const KeyT& var) {
			auto [_, idx] = held_keys.emplaceByLeft(var, held_keys.size());
			return idx;
		}

		constexpr static HashMapStateID toMapState(MemoryStateID state) {
			return HashMapStateID{ u64(state) };
		}

		constexpr static MemoryStateID toMemState(HashMapStateID state) {
			return MemoryStateID{ u64(state) };
		}

		usize emplaceNewVal(const ValT& var) {
			auto [is_new, var_id] = held_values.emplaceByLeft(var, next_val_id);
			next_val_id += (is_new ? 1 : 0);
			return var_id;
		}

		usize emplaceNewKey(const KeyT& key) {
			auto [is_new, var_id] = held_values.emplaceByLeft(key, next_key_id);
			next_key_id += (is_new ? 1 : 0);
			return var_id;
		}


	public:
		static constexpr auto EMPTY = HashMapStateID(u64(Memory::EMPTY));

		bool contains(HashMapStateID state_id, const KeyT& key) const {
			if_opt_some(held_keys.atLeftOpt(key), key_id) {
				auto state = toMemState(state_id);
				return Memory::active(state, key_id);
			}

			return false;
		}

		const ValT& access(HashMapStateID state_id, const KeyT& key) const {
			auto key_id = *held_keys.atLeftOpt(key);
			auto state  = toMemState(state_id);
			return Memory::active(state, key_id);
		}

		HashMapStateID insert(HashMapStateID state_id, const KeyT& key, const ValT& var) {
			auto state  = toMemState(state_id);
			auto key_id = emplaceKey(key);
			auto val_id = emplaceNewVal(var);
			return toMapState(Memory::set(state, key_id, val_id));
		}

		HashMapStateID erase(HashMapStateID state_id, const KeyT& key) {
			auto state  = toMemState(state_id);
			auto key_id = emplaceKey(key);
			return toMapState(Memory::erase(state, key_id));
		}

		std::pair<bool, HashMapStateID> emplace(
			HashMapStateID state_id, const KeyT& key, const ValT& var
		) {
			if (contains(state_id, key)) return { false, state_id };

			auto ans = insert(state_id, key, var);
			return { true, ans };
		}
	};
}
