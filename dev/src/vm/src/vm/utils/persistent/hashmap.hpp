#pragma once

#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/types/ints.hpp>

#include <vm/utils/bijective_map.hpp>
#include <vm/utils/persistent/memory.hpp>

namespace vm::persistent {
	STRONG_TYPEDEF_INT(HashMapStateID, u64);

	/**
	 * @brief Class implementing a STL hashmap with time-persistency aka control version. You can
	 * modify any of the previous instances of the vector, by using `HashMapStateID`.
	 *
	 * @note Implementation based of persistent memory.
	 * @note two HashMapStateID's are equal if and only if corresponding hashmaps are the same (same
	 size and same values on same keys)
	 * @note Held values & keys are constructed only once.
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
	class HashMap final {
		detail::BijectiveMap<ValT, usize, ValH> held_values{};
		detail::BijectiveMap<KeyT, usize, KeyH> held_keys{};

		usize next_val_id = 0;
		usize next_key_id = 0;

		Memory inner;

		// casting memory state to hashmap state
		constexpr static HashMapStateID toMapState(MemoryStateID state) {
			return HashMapStateID{ u64(state) };
		}

		// casting hashmap state to memory state
		constexpr static MemoryStateID toMemState(HashMapStateID state) {
			return MemoryStateID{ u64(state) };
		}

		/**
		 * @brief returns an ID of a ValT value
		 * @note if the value wasn't previously used, it is assigned a new one
		 */
		usize emplaceNewVal(const ValT& var) {
			auto [is_new, var_id] = held_values.emplaceByLeft(var, next_val_id);
			next_val_id += (is_new ? 1 : 0);
			return var_id;
		}

		/**
		 * @brief returns an ID of a KeyT key
		 * @note if the key wasn't previously used, it is assigned a new one
		 */
		usize emplaceNewKey(const KeyT& key) {
			auto [is_new, var_id] = held_keys.emplaceByLeft(key, next_key_id);
			next_key_id += (is_new ? 1 : 0);
			return var_id;
		}


	public:
		// public state representing empty vector
		static constexpr auto EMPTY = HashMapStateID(u64(Memory::EMPTY));

		/**
		 * @brief return size of hashmap at given instance
		 */
		usize size(HashMapStateID state_id) const {
			auto mem_state = toMemState(state_id);
			return inner.size(mem_state);
		}

		/**
		 * @brief transforms given state to actual hashmap
		 */
		base::HashMap<KeyT, ValT, KeyH> toMap(HashMapStateID state_id) const {
			if (state_id == EMPTY) return {};

			auto mem_state = toMemState(state_id);
			auto [l, r]    = inner.getRangeOf(mem_state);
			auto iter      = *inner.getPathTo(mem_state, l);

			base::HashMap<KeyT, ValT, KeyH> ans = {};

			bool keep_going = true;

			while (keep_going) {
				auto val_id = iter.getValue();
				auto idx    = iter.getIdx();

				auto [_, success]
					= ans.emplace(held_keys.atRight(idx), held_values.atRight(val_id));

				CORE_ASSERT(success, "I need the value to be successfully emplaced");

				keep_going &= iter.moveToValid(Memory::Dir::Rght);
			}

			return ans;
		}

		/**
		 * @brief checks if the key is present in given instance of hashmap
		 */
		bool contains(HashMapStateID state_id, const KeyT& key) const {
			if_opt_some(held_keys.atLeftOpt(key), key_id) {
				auto state = toMemState(state_id);
				return inner.active(state, key_id);
			}

			return false;
		}

		/**
		 * @brief method for accessing element for given key at given instance.
		 */
		const ValT& at(HashMapStateID state_id, const KeyT& key) const {
			auto state = toMemState(state_id);
			if_opt_some(held_keys.atLeftOpt(key), key_id) {
				if_opt_some(inner.access(state, key_id), val_id) {
					return held_values.atRight(val_id);
				}
			}
			CORE_UNREACHABLE();
		}

		/**
		 * @brief method for inserting [key, value] to given instance.
		 * @note if key was present at given instance, it will be overriden
		 */
		HashMapStateID insert(HashMapStateID state_id, const KeyT& key, const ValT& var) {
			auto state  = toMemState(state_id);
			auto key_id = emplaceNewKey(key);
			auto val_id = emplaceNewVal(var);
			return toMapState(inner.set(state, key_id, val_id));
		}

		/**
		 * @brief method for erasing entry with given key at given instance
		 * @note if given instance doesn't contain the key, no effect take place
		 */
		HashMapStateID erase(HashMapStateID state_id, const KeyT& key) {
			auto state  = toMemState(state_id);
			auto key_id = emplaceNewKey(key);
			return toMapState(inner.erase(state, key_id));
		}

		/**
		 * @brief method for inserting [key, value] to given instance.
		 * @note if key was present at given instance, it won't be overriden
		 */
		std::pair<bool, HashMapStateID> emplace(
			HashMapStateID state_id, const KeyT& key, const ValT& var
		) {
			if (contains(state_id, key)) return { false, state_id };

			auto ans = insert(state_id, key, var);
			return { true, ans };
		}

		/**
		 * @brief comapre two states of the hashmap
		 * @return true if the instances are equal
		 */
		[[nodiscard]]
		bool eq(HashMapStateID state_1, HashMapStateID state_2) const {
			return state_1 == state_2;
		}
	};
}
