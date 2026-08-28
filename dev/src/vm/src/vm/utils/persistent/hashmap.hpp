#pragma once

#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/types/ints.hpp>

#include <vm/utils/bijective_map.hpp>
#include <vm/utils/persistent/memory.hpp>

namespace vm::persistent {
	STRONG_TYPEDEF_INT(HashMapStateID, u64);

	/**
	 * @brief A persistent hash map whose previous states remain available after updates.
	 *
	 * Each operation returns a `HashMapStateID` identifying the resulting state. Existing states
	 * remain unchanged and can be used for subsequent operations.
	 *
	 * @note Two state IDs are equal if and only if they represent equal hash maps.
	 * @note Keys and values are stored once and referenced by ID in the persistent memory.
	 *
	 * @tparam KeyT key type
	 * @tparam ValT value type
	 * @tparam KeyH hash function for keys
	 * @tparam ValH hash function for values
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

		// Converts a memory state ID to a hash map state ID.
		constexpr static HashMapStateID toMapState(MemoryStateID state) {
			return HashMapStateID{ u64(state) };
		}

		// Converts a hash map state ID to a memory state ID.
		constexpr static MemoryStateID toMemState(HashMapStateID state) {
			return MemoryStateID{ u64(state) };
		}

		/**
		 * @brief Returns the ID associated with a value.
		 * @note A new ID is assigned if the value has not been stored before.
		 */
		usize emplaceNewVal(const ValT& var) {
			const auto [is_new, var_id] = held_values.emplaceByLeft(var, next_val_id);
			next_val_id += (is_new ? 1 : 0);
			return var_id;
		}

		/**
		 * @brief Returns the ID associated with a key.
		 * @note A new ID is assigned if the key has not been stored before.
		 */
		usize emplaceNewKey(const KeyT& key) {
			const auto [is_new, var_id] = held_keys.emplaceByLeft(key, next_key_id);
			next_key_id += (is_new ? 1 : 0);
			return var_id;
		}


	public:
		// Public state representing an empty hash map.
		static constexpr auto EMPTY = HashMapStateID(u64(Memory::EMPTY));

		/**
		 * @brief Returns the number of entries in a hash map state.
		 */
		usize size(HashMapStateID state_id) const {
			const auto mem_state = toMemState(state_id);
			return inner.size(mem_state);
		}

		/**
		 * @brief Returns the hash map represented by a state.
		 */
		base::HashMap<KeyT, ValT, KeyH> toMap(HashMapStateID state_id) const {
			if (state_id == EMPTY) return {};

			const auto mem_state = toMemState(state_id);
			const auto [l, r]    = inner.getRangeOf(mem_state);
			auto iter            = *inner.getPathTo(mem_state, l);

			base::HashMap<KeyT, ValT, KeyH> ans = {};

			bool keep_going = true;

			while (keep_going) {
				const auto val_id = iter.getValue();
				const auto idx    = iter.getIdx();

				const auto [_, success]
					= ans.emplace(held_keys.atRight(idx), held_values.atRight(val_id));

				CORE_ASSERT(success, "I need the value to be successfully emplaced");

				keep_going &= iter.moveToValid(Memory::Dir::Rght);
			}

			return ans;
		}

		/**
		 * @brief Returns whether a key is present in a hash map state.
		 */
		bool contains(HashMapStateID state_id, const KeyT& key) const {
			if_opt_some(held_keys.atLeftOpt(key), key_id) {
				const auto state = toMemState(state_id);
				return inner.active(state, key_id);
			}

			return false;
		}

		/**
		 * @brief Returns the value associated with a key, if present.
		 */
		base::Optional<ValT> atMaybe(HashMapStateID state_id, const KeyT& key) const {
			try {
				return at(state_id, key);
			} catch (...) {}
			return std::nullopt;
		}

		const ValT& at(HashMapStateID state_id, const KeyT& key) const {
			const auto state = toMemState(state_id);
			if_opt_some(held_keys.atLeftOpt(key), key_id) {
				if_opt_some(inner.access(state, key_id), val_id) {
					return held_values.atRight(val_id);
				}
			}
			throw std::out_of_range("key not found");
		}

		/**
		 * @brief Returns a state with a key-value pair inserted or replaced.
		 */
		HashMapStateID insert(HashMapStateID state_id, const KeyT& key, const ValT& var) {
			const auto state  = toMemState(state_id);
			const auto key_id = emplaceNewKey(key);
			const auto val_id = emplaceNewVal(var);
			return toMapState(inner.set(state, key_id, val_id));
		}

		/**
		 * @brief Returns a state with the entry for a key removed.
		 * @note If the key is not present, the state is unchanged.
		 */
		HashMapStateID erase(HashMapStateID state_id, const KeyT& key) {
			const auto state = toMemState(state_id);
			if_opt_some(held_keys.atLeftOpt(key), key_id) {
				return toMapState(inner.erase(state, key_id));
			}
			return state_id;
		}

		/**
		 * @brief Returns whether a key-value pair was inserted and the resulting state.
		 * @note If the key is already present, the state is unchanged.
		 */
		std::pair<bool, HashMapStateID> emplace(
			HashMapStateID state_id, const KeyT& key, const ValT& var
		) {
			if (contains(state_id, key)) return { false, state_id };

			const auto ans = insert(state_id, key, var);
			return { true, ans };
		}

		/**
		 * @brief Returns whether two hash map states are equal.
		 */
		[[nodiscard]]
		bool eq(HashMapStateID state_1, HashMapStateID state_2) const {
			return state_1 == state_2;
		}
	};
}
