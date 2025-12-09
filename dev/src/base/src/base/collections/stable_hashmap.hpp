/**
 * @file stable_hashmap.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <base/collections/optional.hpp>
#include <base/collections/maps.hpp>
#include <base/memory/single_type_memory_pool_allocator.hpp>
#include <base/pointers/box.hpp>

#include <iterator>
#include <utility>

namespace base {

	/**
	 * Custom, Stable hash map implementation.
	 * Its performance is similar or better than std::unordered_map, while keeping references always
	 * stable (what std::unordered_map does as well).
	 *
	 * Pointers to the stored data are never invalidated until the data is erased from the map.
	 * Usage of iterators after elements are added or removed from the map is undefined.
	 */
	template<
		typename KEY_T,
		typename DATA_T,
		typename HASH_T          = std::hash<KEY_T>,
		u64 ALLOCATOR_BLOCK_SIZE = 4'096>
	class HashMap final {
	public:
		/**
		 * Key Value pair stored in the map.
		 */
		struct KeyValuePair final {
			const KEY_T key;
			DATA_T      value;
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
		static auto keyHash(const KEY_T& key
		) noexcept(::base::IS_BUILD_TYPE_RELEASE && noexcept(HASH_T{}(key))) {
			return HASH_T{}(key);
		}

		[[nodiscard]]
		u64 keyToBucket(const KEY_T& key) const
			noexcept(::base::IS_BUILD_TYPE_RELEASE && noexcept(keyHash(std::declval<KEY_T>()))) {
			CORE_ASSERT(!buckets.empty(), "No buckets in HashMap");

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
		 * Does not perform any link correctness checks.
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
	void assertElementCountAllocatorConsistency() const {
			[[maybe_unused]]
			auto allocated_nodes = node_allocator.getAllocatedCount();
			CORE_ASSERT_NOEXCEPT(
				element_count == allocated_nodes,
				"Element count and allocated nodes count mismatch: ",
				element_count,
				" vs ",
				allocated_nodes
			);
			

			// more detailed check (O(n)):
			u64 counted_elements = 0;
			for (const auto& bucket: buckets) {
				MRef<Node> current_node = bucket;
				while (current_node) {
					counted_elements++;
					current_node = current_node->next;
				}
			}
			CORE_ASSERT_NOEXCEPT(
				element_count == counted_elements,
				"Element count and counted elements mismatch: ",
				element_count,
				" vs ",
				counted_elements
			);
		}

		
		HashMap(): buckets(INITIAL_BUCKETS) { assertElementCountAllocatorConsistency(); }

		HashMap(const HashMap&) = delete;

		HashMap(HashMap&& other) noexcept:
			  buckets(std::move(other.buckets)),
			  node_allocator(std::move(other.node_allocator)),
			  element_count(other.element_count) {
			other.element_count = 0;
			other.buckets.resize(1, nullptr);

			// assertElementCountAllocatorConsistency();
			// other.assertElementCountAllocatorConsistency();
		}

		HashMap& operator=(const HashMap& other) noexcept {
			auto other_size = other.size();
			clear();
			for (auto& kv_pair: other) put(kv_pair.key, kv_pair.value);
			
			CORE_ASSERT_NOEXCEPT(size() == other_size, "Size mismatch after copy assignment");

			// assertElementCountAllocatorConsistency();
			// other.assertElementCountAllocatorConsistency();

			return *this;
		}

		// TODO PR: change to move like 
		HashMap& operator=(HashMap&& other) noexcept {
			auto other_size = other.size();
			clear();
			for (auto& kv_pair: other) put(kv_pair.key, kv_pair.value);
			other.clear();
			
			CORE_ASSERT_NOEXCEPT(size() == other_size, "Size mismatch after move assignment");
	
			// assertElementCountAllocatorConsistency();
			// other.assertElementCountAllocatorConsistency();
			
			return *this;
		}

		~HashMap() {
			assertElementCountAllocatorConsistency();
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
		 * @future: Support sentinel iterators that are never invalidated.
		 * .end() functions should return them.
		 */
		template<class ValueT>
		class Iterator final {
			// end is represented by bucket_index == buckets.size(), and current_node == nullptr

			u64 bucket_index = 0;

			// mref, since we have to represent "end" iterator.
			MRef<Node> current_node;

			/**
			 * This stores the pointer to data stored in the `buckets` member.
			 * We need it to here to be able to iterate to the next bucket when needed.
			 *
			 * We can't just store reference to buckets themselves, since the iterator
			 * has to remain valid even if the map is moved.
			 */
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


			friend class HashMap;

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

			Iterator(const Iterator&)            = default;
			Iterator& operator=(const Iterator&) = default;

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
		 * Naively copies the hashmap.
		 * Not a constructor, to make this call explicit.
		 * @returns A copy of the hashmap.
		 */
		// template<typename K = KEY_T, typename D = DATA_T>
		HashMap copy() const {
			assertElementCountAllocatorConsistency();

			HashMap result;
			for (auto& kv_pair: (*this)) result.put(kv_pair.key, kv_pair.value);
			
			// assertElementCountAllocatorConsistency();
			// result.assertElementCountAllocatorConsistency();
			
			return result;
		}

		/**
		 * Inserts key->value into the container.
		 * Panics if key already exists.
		 * @param key Data key
		 * @param value The data
		 * @returns A reference to the inserted key-value pair.
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		Ref<KeyValuePair> put(K&& key, D&& value) RELEASE_NOEXCEPT {
			// assertElementCountAllocatorConsistency();

			auto new_node = node_allocator.allocateEmplace(
				nullptr, std::forward<K>(key), std::forward<D>(value)
			);
			// we increment element count here, to keep consistency in case of panic in addToBucket/contains
			element_count++;

			// Note that this can in theory have some observable side effects:
			CORE_ASSERT(
				not this->contains(new_node->key_value.key), "Key already exists in HashMap"
			);

			addToBucket(keyToBucket(new_node->key_value.key), new_node);

			maybeRehash();

			// assertElementCountAllocatorConsistency();

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
			// assertElementCountAllocatorConsistency();

			auto new_node = node_allocator.allocateEmplace(
				nullptr, std::forward<K>(key), std::forward<D>(value)
			);
			element_count++;

			// @OPT: make this more efficient, by direct, one-pass implementation
			if (this->contains(new_node->key_value.key)) {
				node_allocator.deallocateDestroy(new_node);
				
				// assertElementCountAllocatorConsistency();
				return nullptr;
			}

			addToBucket(keyToBucket(new_node->key_value.key), new_node);

			maybeRehash();

			// assertElementCountAllocatorConsistency();
			return &new_node->key_value;
		}

		/**
		 * If key is not in the container, inserts key->value into the container.
		 * Otherwise substitutes the value assigned to key.
		 * @param key Data key
		 * @param value The data
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		bool putOrAssign(K&& key, D&& value) RELEASE_NOEXCEPT {
			auto new_node = node_allocator.allocateEmplace(
				nullptr, std::forward<K>(key), std::forward<D>(value)
			);
			element_count++;


			// @OPT: make this more efficient, by direct, one-pass implementation
			if (this->contains(new_node->key_value.key)) {
				
				node_allocator.deallocateDestroy(new_node);
				element_count--;

				(*this)[key] = value;
	
				// assertElementCountAllocatorConsistency();
				return false;
			}

			addToBucket(keyToBucket(new_node->key_value.key), new_node);

			maybeRehash();

			// assertElementCountAllocatorConsistency();
			return true;
		}

		/**
		 * Inserts empty value at a given key.
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		requires std::is_default_constructible_v<D> void putEmpty(K&& key) RELEASE_NOEXCEPT {
			put(std::forward<K>(key), D());
		}

		/**
		 * Inserts empty value at a given key.
		 * If the value exists, does nothing.
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		requires std::is_default_constructible_v<D> void maybePutEmpty(K&& key) RELEASE_NOEXCEPT {
			maybePut(std::forward<K>(key), D());
		}

		[[nodiscard]]
		base::Optional<CRef<DATA_T>> atMaybe(const KEY_T& key) const RELEASE_NOEXCEPT {
			auto current_node = buckets.at(keyToBucket(key));
			while (current_node) {
				if (current_node->key_value.key == key) {
					// assertElementCountAllocatorConsistency();
					return &current_node->key_value.value;
				}
				current_node = current_node->next;
			}

			// assertElementCountAllocatorConsistency();
			return {};
		}

		[[nodiscard]]
		base::Optional<Ref<DATA_T>> atMaybe(const KEY_T& key) RELEASE_NOEXCEPT {
			auto current_node = buckets.at(keyToBucket(key));
			while (current_node) {
				if (current_node->key_value.key == key) {
					// assertElementCountAllocatorConsistency();
					return &current_node->key_value.value;
				}
				current_node = current_node->next;
			}
			// assertElementCountAllocatorConsistency();
			return {};
		}

		[[nodiscard]]
		base::Optional<DATA_T> atMaybeCopy(const KEY_T& key) const RELEASE_NOEXCEPT {
			// assertElementCountAllocatorConsistency();

			auto current_node = buckets.at(keyToBucket(key));
			while (current_node) {
				if (current_node->key_value.key == key) return current_node->key_value.value;
				current_node = current_node->next;
			}
			
			return {};
		}

		DATA_T& operator[](const KEY_T& key) { 
			// assertElementCountAllocatorConsistency(); 
			return **atMaybe(key); }

		const DATA_T& operator[](const KEY_T& key) const { 
			// assertElementCountAllocatorConsistency(); 
			return **atMaybe(key); }

		[[nodiscard]]
		bool contains(const KEY_T& key) const RELEASE_NOEXCEPT {
			// assertElementCountAllocatorConsistency();
			return atMaybe(key).has_value();
		}

		/**
		 * @brief Erases value at @p key position if it exists.
		 * The references to the erased value are invalidated.
		 * @returns Whether a value was erased.
		 */
		bool erase(const KEY_T& key) RELEASE_NOEXCEPT {
			// assertElementCountAllocatorConsistency();

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

					// assertElementCountAllocatorConsistency();
					return true;
				}
				previous_node = current_node;
				current_node  = current_node->next;
			}

			// assertElementCountAllocatorConsistency();
			return false;
		}

		/**
		 * Clears the map, destroying all stored elements.
		 * @note does not free the memory used to store the elements.
		 */
		void clear() RELEASE_NOEXCEPT {
			// assertElementCountAllocatorConsistency();
			for (auto& bucket: buckets) {
				MRef<Node> current_node = bucket;
				while (current_node) {
					MRef next_node = current_node->next;
					node_allocator.justDestroy(current_node.toOpt().value());
					current_node = next_node;
				}
				bucket = nullptr;
			}
			element_count = 0;
			// assertElementCountAllocatorConsistency();
		}

		/**
		 * Clears the map, destroying all stored elements.
		 * @note Frees the memory used to store the elements.
		 * @TODO PR: unify this and move 
		 */
		void clearAndFree() RELEASE_NOEXCEPT {
			// assertElementCountAllocatorConsistency();
			for (u64 i = 0; i < buckets.size(); i++) {
				MRef<Node> current_node = buckets[i];
				while (current_node) {
					MRef next_node = current_node->next;
					// HMM:
					node_allocator.justDestroy(current_node.toOpt().value());
					current_node = next_node;
				}
				buckets[i] = nullptr;
			}
			element_count = 0;
			// assertElementCountAllocatorConsistency();
		}

		/**
		 * Query the number of pairs stored in the container.
		 * @return Number of pairs
		 */
		[[nodiscard]]
		usize size() const {
			return element_count;
		}

		/**
		 * Query whether there are any pairs stored in the container.
		 * @return Whether container is empty
		 */
		[[nodiscard]]
		bool empty() const {
			return (element_count == 0);
		}

		IteratorT begin() RELEASE_NOEXCEPT {
			// assertElementCountAllocatorConsistency();

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
			// assertElementCountAllocatorConsistency();

			return IteratorT(buckets.size(), nullptr, buckets.data(), buckets.size());
		}

		ConstIteratorT begin() const RELEASE_NOEXCEPT {
			// assertElementCountAllocatorConsistency();

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
			// assertElementCountAllocatorConsistency();

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
