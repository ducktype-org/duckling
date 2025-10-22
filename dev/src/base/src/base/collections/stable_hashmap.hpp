/**
 * @file stable_hashmap.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <base/collections/maps.hpp>
#include <base/pointers/box.hpp>
#include <base/memory/single_type_memory_pool_allocator.hpp>


#include <type_traits>

namespace base {
	/**
	 * A wrapper around base::HashMap, that keeps references (memory addresses) valid.
	 * @tparam DATA_T The datatype to store
	 * @tparam KEY_T Indentifies data
	 * @tparam HASH_T Hash functor for hashing keys
	 */
	template<typename KEY_T, typename DATA_T, typename HASH_T = std::hash<KEY_T>>
	class StableHashMap final {
	public:
		StableHashMap() = default;

		/**
		 * Returns the data identified by the key.
		 * @note throws `std::out_of_range` if key is not present
		 * @param key Data key
		 * @return A reference to the data.
		 */
		DATA_T& operator[](const KEY_T& key) { return *data[key]; }

		/**
		 * Returns the data identified by the key.
		 * @note throws `std::out_of_range` if key is not present
		 * @param key Data key
		 * @return A reference to the data.
		 */
		const DATA_T& operator[](const KEY_T& key) const { return *data[key]; }

		/**
		 * Returns a reference to the data inside an optional. If the data identified by the key
		 * does not exist returns an empty optional.
		 * @param key Data key
		 * @return An optional with a reference to the data.
		 */
		Optional<Ref<DATA_T>> atMaybe(const KEY_T& key) {
			if_opt_some(data.atMaybe(key), ptr) { return (*ptr).refMut(); }
			return {};
		}

		/**
		 * Returns a const reference to the data inside an optional. If the data identified by the
		 * key does not exist returns an empty optional.
		 * @param key Data key
		 * @return An optional with a const reference to the data.
		 */
		Optional<CRef<DATA_T>> atMaybe(const KEY_T& key) const {
			if_opt_some(data.atMaybe(key), ptr) { return (*ptr).ref(); }
			return {};
		}

		/**
		 * Returns a copy of a data inside an optional. If the data identified by the
		 * key does not exist returns an empty optional.
		 * @param key Data key
		 * @return An optional with a copy of the data.
		 */
		Optional<DATA_T> atMaybeCopy(const KEY_T& key) const
			requires std::is_copy_constructible_v<DATA_T> {
			if_opt_some(data.atMaybe(key), ptr) { return *(*ptr); }
			return {};
		}

		/**
		 * If the container doesn't store the key yet, then inserts value identified by the key.
		 * @param key Data key
		 * @param value The data
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		auto put(K&& key, D&& value) {
			return data.put(std::forward<K>(key), ::base::makeBox<DATA_T>(std::forward<D>(value)));
		}

		/**
		 * @brief Erases value at @p key position if it exists.
		 * The references to the erased value are invalidated.
		 * @returns Whether a value was erased.
		 */
		bool erase(const KEY_T& key) { return data.erase(key); }

		/**
		 * Clears all data from the data structure.
		 */
		void clear() { data.clear(); }

		/**
		 * Check if key is stored in the container.
		 * @param key The key to query
		 * @return True if containers already stores the key, false otherwise.
		 */
		bool contains(const KEY_T& key) const { return data.contains(key); }

		/**
		 * Query the number of pairs stored in the container.
		 * @return Number of pairs
		 */
		[[nodiscard]]
		usize size() const {
			return data.size();
		}

		/**
		 * @brief Data iterator -- begin.
		 */
		auto begin() { return data.begin(); }

		/**
		 * @brief Data iterator -- end.
		 */
		auto end() { return data.end(); }

		/**
		 * @brief Const data iterator -- begin.
		 */
		auto begin() const { return data.begin(); }

		/**
		 * @brief Const data iterator -- end.
		 */
		auto end() const { return data.end(); }

	private:
		HashMap<KEY_T, Box<DATA_T>, HASH_T> data;
	};


	template<typename KEY_T, typename DATA_T, typename HASH_T = std::hash<KEY_T>>
	class StableHashMap20 final {

		static constexpr usize  INITIAL_BUCKETS = 256;
		static constexpr double MAX_LOAD_FACTOR = 0.7;

		using KeyHash = u64;

		struct Node final {
			MRef<Node> next;
			KEY_T      key;
			DATA_T     value;

			Node(MRef<Node> next, KEY_T&& key, DATA_T&& value) noexcept
				: next(next), key(std::move(key)), value(std::move(value)) {}
		};

		std::vector<MRef<Node>>              buckets;
		SingleTypeMemoryPoolAllocator<Node>  node_allocator;
		u64                                  element_count = 0;

		[[nodiscard]]
		static auto keyHash(const KEY_T& key) {
			return HASH_T{}(key);
		}

		[[nodiscard]]
		u64 keyToBucket(const KEY_T& key) const RELEASE_NOEXCEPT {
			u64 hash = keyHash(key);
			auto res = hash % buckets.size();
			CORE_ASSERT(0 <= res and res < buckets.size(), "Bucket index out of bounds");
			return res;
		}

		void rehash() RELEASE_NOEXCEPT {
			usize                  new_bucket_count = buckets.size() * 2;
			
			std::vector<Ref<Node>> all_nodes;
			all_nodes.reserve(element_count);

			for (const auto& bucket: buckets) {
				MRef<Node> current_node = bucket;
				while (current_node) {
					all_nodes.emplace_back(current_node.toOpt().value());
					current_node = current_node.toOpt().value()->next;
					all_nodes.back()->next = nullptr;
				}
			}

			CORE_ASSERT(all_nodes.size() == element_count, "Node count mismatch during rehash: ", all_nodes.size(), " vs ", element_count);

			buckets.clear();
			buckets.resize(new_bucket_count);

			for (const Ref<Node>& node: all_nodes) {
				addToBucket(keyToBucket(node->key), node);
			}
		}

		/**
		* Appends new node to the bucket identified by node reference.
		* Panics if node with the same key already exists.
		*/
		void addToBucketNode(Ref<Node> bucket, Ref<Node> new_node) RELEASE_NOEXCEPT {
			CORE_ASSERT(new_node->next == nullptr, "New node must be ending node");

			Ref current_node = bucket;
			const KEY_T& key = new_node->key;

			while (true) {
				if (current_node->key == key) {
					// this can be changed to an assertion:
					CORE_PANIC("Duplicate key insertion in MyCustomHashMap");
				}
				if (current_node->next)
					current_node = current_node->next.toOpt().value();
				else
					break;
			}
			CORE_ASSERT(current_node->next == nullptr, "this must be the last node in the bucket");
			current_node->next = new_node;
		}

		void addToBucket(u64 bucket_index, Ref<Node> new_node) RELEASE_NOEXCEPT {
			MRef maybe_initial_node = buckets.at(bucket_index);
			if (maybe_initial_node)
				addToBucketNode(maybe_initial_node.toOpt().value(), new_node);
			else
				buckets.at(bucket_index) = new_node;
		}

		void maybeRehash() RELEASE_NOEXCEPT {
			if (double(element_count) > MAX_LOAD_FACTOR * double(buckets.size())) rehash();
		}

	public:
		StableHashMap20(): buckets(INITIAL_BUCKETS) {}

		void put(const KEY_T& key, const DATA_T& value) RELEASE_NOEXCEPT {
			auto new_node
				= node_allocator.allocateEmplace(Node{ nullptr, key, value });

			addToBucket(keyToBucket(key), new_node);

			element_count++;
			maybeRehash();
		}

		[[nodiscard]]
		base::Optional<Ref<DATA_T>> atMaybe(const KEY_T& key) const RELEASE_NOEXCEPT {
			auto current_node = buckets.at(keyToBucket(key));
			while (current_node) {
				if (current_node->key == key) return &current_node->value;
				current_node = current_node->next;
			}
			return {};
		}

		[[nodiscard]]
		bool contains(const KEY_T& key) const RELEASE_NOEXCEPT {
			return atMaybe(key).has_value();
		}
	};
}
