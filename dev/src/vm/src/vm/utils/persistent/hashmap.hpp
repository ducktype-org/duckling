#pragma once

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/types/ints.hpp>

#include <vm/utils/bijective_map.hpp>
#include <vm/utils/persistent/memory.hpp>

#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>

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
	template<typename KeyT, typename ValT, typename KeyH, typename ValH>
	class HashMapStateView;

	template<
		typename KeyT,
		typename ValT,
		typename KeyH = std::hash<KeyT>,
		typename ValH = std::hash<ValT>>
	class HashMap final {
		friend class HashMapStateView<KeyT, ValT, KeyH, ValH>;

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

		/**
		 * @brief Validates a vector state.
		 * @return The corresponding memory state.
		 */
		MemoryStateID validateState(HashMapStateID map_state) const {
			const auto mem_state = toMemState(map_state);
			if (!inner.knows(mem_state)) throw std::invalid_argument("unknown hashmap state");
			return mem_state;
		}


	public:
		// Public state representing an empty hash map.
		static constexpr auto EMPTY = HashMapStateID(u64(Memory::EMPTY));

		/**
		 * @brief Returns the number of entries in a hash map state.
		 */
		[[nodiscard]]
		usize size(HashMapStateID state_id) const {
			const auto mem_state = validateState(state_id);
			return inner.size(mem_state);
		}

		/**
		 * @brief Returns the hash map represented by a state.
		 */
		[[nodiscard]]
		base::HashMap<KeyT, ValT, KeyH> toMap(HashMapStateID state_id) const {
			if (state_id == EMPTY) return {};

			const auto mem_state = validateState(state_id);
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
		[[nodiscard]]
		bool contains(HashMapStateID state_id, const KeyT& key) const {
			const auto mem_state = validateState(state_id);
			if_opt_some(held_keys.atLeftOpt(key), key_id) {
				return inner.active(mem_state, key_id);
			}

			return false;
		}

		/**
		 * @brief Returns the value associated with a key, if present.
		 */
		[[nodiscard]]
		base::Optional<ValT> atMaybe(HashMapStateID state_id, const KeyT& key) const {
			try {
				return at(state_id, key);
			} catch (...) {}
			return std::nullopt;
		}

		[[nodiscard]]
		const ValT& at(HashMapStateID state_id, const KeyT& key) const {
			const auto mem_state = validateState(state_id);
			if_opt_some(held_keys.atLeftOpt(key), key_id) {
				if_opt_some(inner.access(mem_state, key_id), val_id) {
					return held_values.atRight(val_id);
				}

				throw std::out_of_range("key not present at current state");
			}
			throw std::out_of_range("key not found");
		}

		/**
		 * @brief Returns a state with a key-value pair inserted or replaced.
		 */
		[[nodiscard]]
		HashMapStateID insert(HashMapStateID state_id, const KeyT& key, const ValT& var) {
			const auto mem_state = validateState(state_id);
			const auto key_id    = emplaceNewKey(key);
			const auto val_id    = emplaceNewVal(var);
			return toMapState(inner.set(mem_state, key_id, val_id));
		}

		/**
		 * @brief Returns a state with the entry for a key removed.
		 * @note If the key is not present, the state is unchanged.
		 */
		[[nodiscard]]
		HashMapStateID erase(HashMapStateID state_id, const KeyT& key) {
			const auto mem_state = validateState(state_id);
			if_opt_some(held_keys.atLeftOpt(key), key_id) {
				return toMapState(inner.erase(mem_state, key_id));
			}
			return state_id;
		}

		/**
		 * @brief Returns whether a key-value pair was inserted and the resulting state.
		 * @note If the key is already present, the state is unchanged.
		 */
		[[nodiscard]]
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

	template<
		typename KeyT,
		typename ValT,
		typename KeyH = std::hash<KeyT>,
		typename ValH = std::hash<ValT>>
	class HashMapStateView final {
		HashMapStateID                         id;
		const HashMap<KeyT, ValT, KeyH, ValH>& map;
		MemoryStateView                        mem_view;

	public:
		class HashMapIterator {
			MemoryStateView::MemoryIterator        mem_it;
			const HashMap<KeyT, ValT, KeyH, ValH>* map = nullptr;

		public:
			using value_type = std::pair<const KeyT&, const ValT&>;

			std::pair<const KeyT&, const ValT&> operator*() const {
				auto [key_id, val_id] = *mem_it;
				return { map->held_keys.atRight(key_id), map->held_values.atRight(val_id) };
			}

			HashMapIterator& operator++() {
				++mem_it;
				return *this;
			}

			HashMapIterator operator++(int) {
				auto copy = *this;
				++(*this);
				return copy;
			}

			HashMapIterator& operator--() {
				--mem_it;
				return *this;
			}

			HashMapIterator operator--(int) {
				auto copy = *this;
				--(*this);
				return copy;
			}

			bool operator==(const HashMapIterator& oth) const { return mem_it == oth.mem_it; }

			bool operator!=(const HashMapIterator& oth) const { return !(*this == oth); }

			HashMapIterator() = default;

			HashMapIterator(
				const HashMap<KeyT, ValT, KeyH, ValH>& map, MemoryStateView::MemoryIterator mem_it
			):
				  mem_it(std::move(mem_it)),
				  map(&map) {}
		};

		HashMapStateView(const HashMap<KeyT, ValT, KeyH, ValH>& map, HashMapStateID id):
			  id(id),
			  map(map),
			  mem_view(map.inner, HashMap<KeyT, ValT, KeyH, ValH>::toMemState(id)) {}

		HashMapIterator begin() const { return HashMapIterator{ map, mem_view.begin() }; }

		HashMapIterator end() const { return HashMapIterator{ map, mem_view.end() }; }

		const ValT& at(const KeyT& key) const { return map.at(id, key); }

		bool contains(const KeyT& key) const { return map.contains(id, key); }

		[[nodiscard]]
		usize size() const {
			return map.size(id);
		}
	};
}
