/**
 * @file stable_hashmap.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <base/collections/maps.hpp>
#include <base/memory/single_type_memory_pool_allocator.hpp>
#include <base/pointers/box.hpp>

#include <iterator>
#include <type_traits>
#include <utility>

namespace base {

	/**
	 * Custom, Stable hash map implementation.
	 * Its performance is similar or better then std::unordered_map, while keeping references always
	 * stable.
	 *
	 * Pointer to the stored data will never be invalidated until the data is erased from the map.
	 * Iterators can be invalidated when elements are added or removed from the map.
	 */
	template<
		typename KEY_T,
		typename DATA_T,
		typename HASH_T          = std::hash<KEY_T>,
		u64 ALLOCATOR_BLOCK_SIZE = 4'096>
	class StableHashMap final {
	public:
		/**
		 * Key Value pair stored in the map.
		 */
		struct KeyValuePair final {
			const KEY_T key;
			DATA_T      value;

			template<class K = KEY_T, class D = DATA_T>
			KeyValuePair(K&& key, D&& value) noexcept:
				  key(std::forward<K>(key)),
				  value(std::forward<D>(value)) {}
		};

	private:
		static constexpr usize  INITIAL_BUCKETS = 64;
		static constexpr double MAX_LOAD_FACTOR = 0.7;

		using KeyHash = u64;

		struct Node final {
			MRef<Node> next;

			KeyValuePair key_value;

			template<class K = KEY_T, class D = DATA_T>
			Node(MRef<Node> next, K&& key, D&& value) noexcept:
				  next(next),
				  key_value(std::forward<K>(key), std::forward<D>(value)) {}
		};

		[[nodiscard]]
		static auto keyHash(const KEY_T& key) {
			return HASH_T{}(key);
		}

		[[nodiscard]]
		u64 keyToBucket(const KEY_T& key) const RELEASE_NOEXCEPT {
			CORE_ASSERT(!buckets.empty(), "No buckets in StableHashMap");

			u64  hash = keyHash(key);
			auto res  = hash % buckets.size();
			CORE_ASSERT(0 <= res and res < buckets.size(), "Bucket index out of bounds");
			return res;
		}

		void rehash() RELEASE_NOEXCEPT {
			usize new_bucket_count = buckets.size() * 2;

			std::vector<Ref<Node>> all_nodes;
			all_nodes.reserve(element_count);

			for (const auto& bucket: buckets) {
				MRef<Node> current_node = bucket;
				while (current_node) {
					all_nodes.emplace_back(current_node.toOpt().value());
					current_node           = current_node.toOpt().value()->next;
					all_nodes.back()->next = nullptr;
				}
			}

			CORE_ASSERT(
				all_nodes.size() == element_count,
				"Node count mismatch during rehash: ",
				all_nodes.size(),
				" vs ",
				element_count
			);

			buckets.clear();
			buckets.resize(new_bucket_count);

			for (const Ref<Node>& node: all_nodes)
				addToBucket(keyToBucket(node->key_value.key), node);
		}

		/**
		 * Links new node to the bucket.
		 * Does not perform any links correctness checks.
		 */
		void addToBucket(u64 bucket_index, Ref<Node> new_node) RELEASE_NOEXCEPT {
			CORE_ASSERT(new_node->next == nullptr, "New node must be ending node");
			CORE_ASSERT(bucket_index < buckets.size(), "Bucket index out of bounds");

			new_node->next        = buckets[bucket_index];
			buckets[bucket_index] = new_node;
		}

		void maybeRehash() RELEASE_NOEXCEPT {
			if (double(element_count) > MAX_LOAD_FACTOR * double(buckets.size())) [[unlikely]]
				rehash();
		}

	public:
		StableHashMap(): buckets(INITIAL_BUCKETS) {}

		StableHashMap(const StableHashMap&) = delete;

		StableHashMap(StableHashMap&& other) noexcept:
			  buckets(std::move(other.buckets)),
			  node_allocator(std::move(other.node_allocator)),
			  element_count(other.element_count) {
			other.element_count = 0;
			other.buckets.resize(1, nullptr);
		}

		~StableHashMap() {
			for (auto& bucket: buckets) {
				MRef<Node> current_node = bucket;
				while (current_node) {
					MRef<Node> next_node = current_node->next;
					node_allocator.justDestroy(current_node.toOpt().value());
					current_node = next_node;
				}
			}
		}

		/**
		 * Forward iterator over the key-value pairs in the map.
		 * This is a template, so we can can have const and non-const versions.
		 */
		template<class ValueT>
		class Iterator final {
			// end is represented by bucket_index == buckets.size(), and current_node == nullptr

			u64 bucket_index = 0;

			// mref, since we have to represent "end" iterator.
			MRef<Node> current_node;

			const MRef<Node>* buckets_array_ptr;

			u64 buckets_size;

			Iterator(
				u64               bucket_index,
				MRef<Node>        current_node,
				const MRef<Node>* buckets_array_ptr,
				u64               buckets_size
			) noexcept:
				  bucket_index(bucket_index),
				  current_node(current_node),
				  buckets_array_ptr(buckets_array_ptr),
				  buckets_size(buckets_size) {}


			friend class StableHashMap;

		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type   = std::ptrdiff_t;
			using value_type        = ValueT;
			using pointer           = value_type*;
			using reference         = value_type&;

			Iterator():
				  bucket_index(u64(-1)),
				  current_node(nullptr),
				  buckets_array_ptr(nullptr),
				  buckets_size(0) {}

			Iterator(const Iterator&) = default;
			// Iterator(Iterator&&) = default;
			Iterator& operator=(const Iterator&) = default;

			// Iterator& operator=(Iterator&&) = default;

			reference operator*() const { return current_node->key_value; }

			pointer operator->() const { return &current_node->key_value; }

			Iterator& operator++() {
				if (current_node->next != nullptr) {
					current_node = current_node->next.toOpt().value();
				} else {
					bucket_index++;
					current_node = nullptr;

					while (bucket_index < buckets_size
					       and buckets_array_ptr[bucket_index] == nullptr) {
						bucket_index++;
					}

					if (bucket_index < buckets_size)
						current_node = buckets_array_ptr[bucket_index].toOpt().value();
				}
				return *this;
			}

			Iterator operator++(int) {
				Iterator temp = *this;
				++(*this);
				return temp;
			}

			friend bool operator==(const Iterator& a, const Iterator& b) {
				return std::tie(a.bucket_index, a.current_node, a.buckets_array_ptr, a.buckets_size)
				    == std::tie(b.bucket_index, b.current_node, b.buckets_array_ptr, b.buckets_size);
			}

			friend bool operator!=(const Iterator& a, const Iterator& b) { return !(a == b); }
		};

		using IteratorT      = Iterator<KeyValuePair>;
		using ConstIteratorT = Iterator<const KeyValuePair>;

		static_assert(std::forward_iterator<IteratorT>, "IteratorT must be a forward iterator");
		static_assert(
			std::forward_iterator<ConstIteratorT>, "ConstIteratorT must be a forward iterator"
		);

		/**
		 * Inserts key->value into the container.
		 * Panics if key already exists.
		 * @param key Data key
		 * @param value The data
		 * @returns A reference to the inserted key-value pair.
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		Ref<KeyValuePair> put(K&& key, D&& value) RELEASE_NOEXCEPT {
			auto new_node = node_allocator.allocateEmplace(Node{
				nullptr, std::forward<K>(key), std::forward<D>(value) });

			// Note that this can in theory have some observable side effects:
			CORE_ASSERT(
				not this->contains(new_node->key_value.key), "Key already exists in StableHashMap"
			);

			addToBucket(keyToBucket(new_node->key_value.key), new_node);

			element_count++;
			maybeRehash();

			return &new_node->key_value;
		}

		/**
		 * If key is not in the container, inserts key->value into the container.
		 * @param key Data key
		 * @param value The data
		 * @returns Optional reference to the inserted key-value pair. Reference is empty if key
		 * already existed.
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		MRef<KeyValuePair> maybePut(K&& key, D&& value) RELEASE_NOEXCEPT {
			auto new_node = node_allocator.allocateEmplace(Node{
				nullptr, std::forward<K>(key), std::forward<D>(value) });

			// @OPT: make this more efficient, by direct, one-pass implementation
			if (this->contains(new_node->key_value.key)) {
				node_allocator.deallocateDestroy(new_node);
				return nullptr;
			}

			addToBucket(keyToBucket(new_node->key_value.key), new_node);

			element_count++;
			maybeRehash();

			return &new_node->key_value;
		}

		[[nodiscard]]
		base::Optional<CRef<DATA_T>> atMaybe(const KEY_T& key) const RELEASE_NOEXCEPT {
			auto current_node = buckets.at(keyToBucket(key));
			while (current_node) {
				if (current_node->key_value.key == key) return &current_node->key_value.value;
				current_node = current_node->next;
			}
			return {};
		}

		[[nodiscard]]
		base::Optional<Ref<DATA_T>> atMaybe(const KEY_T& key) RELEASE_NOEXCEPT {
			auto current_node = buckets.at(keyToBucket(key));
			while (current_node) {
				if (current_node->key_value.key == key) return &current_node->key_value.value;
				current_node = current_node->next;
			}
			return {};
		}

		[[nodiscard]]
		base::Optional<DATA_T> atMaybeCopy(const KEY_T& key) const RELEASE_NOEXCEPT {
			auto current_node = buckets.at(keyToBucket(key));
			while (current_node) {
				if (current_node->key_value.key == key) return current_node->key_value.value;
				current_node = current_node->next;
			}
			return {};
		}

		DATA_T& operator[](const KEY_T& key) { return **atMaybe(key); }

		const DATA_T& operator[](const KEY_T& key) const { return **atMaybe(key); }

		[[nodiscard]]
		bool contains(const KEY_T& key) const RELEASE_NOEXCEPT {
			return atMaybe(key).has_value();
		}

		/**
		 * @brief Erases value at @p key position if it exists.
		 * The references to the erased value are invalidated.
		 * @returns Whether a value was erased.
		 */
		bool erase(const KEY_T& key) RELEASE_NOEXCEPT {
			u64        bucket_index  = keyToBucket(key);
			MRef<Node> current_node  = buckets.at(bucket_index);
			MRef<Node> previous_node = nullptr;

			// iterate over the bucket:
			while (current_node) {
				if (current_node->key_value.key == key) {
					// found the node to erase

					// relink the pointers in the bucket:
					if (previous_node) {
						previous_node->next = current_node->next;
					} else {
						// erasing first node in bucket
						buckets.at(bucket_index) = current_node->next;
					}

					node_allocator.deallocateDestroy(current_node.toOpt().value());
					element_count--;
					return true;
				}
				previous_node = current_node;
				current_node  = current_node->next;
			}
			return false;
		}

		/**
		 * Clears the map, destroying all stored elements.
		 * @note does not free the memory used to store the elements.
		 */
		void clear() RELEASE_NOEXCEPT {
			for (auto& bucket: buckets) {
				MRef<Node> current_node = bucket;
				while (current_node) {
					node_allocator.justDestroy(current_node.toOpt().value());
					current_node = current_node->next;
				}
				bucket = nullptr;
			}
			element_count = 0;
		}

		/**
		 * Query the number of pairs stored in the container.
		 * @return Number of pairs
		 */
		[[nodiscard]]
		usize size() const {
			return element_count;
		}

		IteratorT begin() RELEASE_NOEXCEPT {
			u64        bucket_index = 0;
			MRef<Node> current_node = nullptr;

			while (bucket_index < buckets.size()) {
				current_node = buckets[bucket_index];
				if (current_node != nullptr) break;
				bucket_index++;
			}

			return IteratorT(bucket_index, current_node, buckets.data(), buckets.size());
		}

		IteratorT end() RELEASE_NOEXCEPT {
			return IteratorT(buckets.size(), nullptr, buckets.data(), buckets.size());
		}

		ConstIteratorT begin() const RELEASE_NOEXCEPT {
			u64        bucket_index = 0;
			MRef<Node> current_node = nullptr;

			while (bucket_index < buckets.size()) {
				current_node = buckets[bucket_index];
				if (current_node != nullptr) break;
				bucket_index++;
			}

			return ConstIteratorT(bucket_index, current_node, buckets.data(), buckets.size());
		}

		ConstIteratorT end() const RELEASE_NOEXCEPT {
			return ConstIteratorT(buckets.size(), nullptr, buckets.data(), buckets.size());
		}

	private:
		/**
		 * Array of bucket beginnings.
		 */
		std::vector<MRef<Node>> buckets;

		/**
		 * Memory pool allocator for node storage.
		 */
		SingleTypeMemoryPoolAllocator<Node, ALLOCATOR_BLOCK_SIZE> node_allocator;

		/**
		 * Number of elements stored in the map.
		 */
		u64 element_count = 0;
	};

}
