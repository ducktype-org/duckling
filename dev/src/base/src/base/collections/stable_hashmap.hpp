/**
 * @file stable_hashmap.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <base/collections/maps.hpp>
#include <base/memory/single_type_memory_pool_allocator.hpp>
#include <base/pointers/box.hpp>

#include <iterator>
#include <utility>

namespace concurrent {
	template<class, class, class, u64>
	class ConHashMap;
}

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
	class StableHashMap final {
	public:
		/**
		 * Key Value pair stored in the map.
		 */
		struct KeyValuePair final {
			const KEY_T key;
			DATA_T      value;
		};

	private:

		// concurrent::ConHashMap uses private components of this map
		// for efficiency reasons, so we just friend it.
		// This is not very good code design, but it is acceptable
		// in this case.
		template<class, class, class, u64>
		friend class concurrent::ConHashMap;

		/** 
		 * Initial number of buckets in the map.
		 * This must be a power of two.	
		 */
		static constexpr usize  INITIAL_BUCKETS = 16;

		static constexpr double MAX_LOAD_FACTOR = 0.7;

		/**
		 * Type used to represent the hash of the key.
		 */
		using KeyHash = u64;

		struct Node final {
			MRef<Node> next;

			KeyHash cached_hash;

			KeyValuePair key_value;

			template<class K = KEY_T, class D = DATA_T>
			Node(MRef<Node> next, KeyHash hash, K&& key, D&& value) noexcept:
				  next(next),
				  cached_hash(hash),
				  key_value(std::forward<K>(key), std::forward<D>(value)) {}
		};

		[[nodiscard]]
		static KeyHash keyHash(const KEY_T& key
		) noexcept(::base::IS_BUILD_TYPE_RELEASE && noexcept(HASH_T{}(key))) {
			return HASH_T{}(key);
		}

		[[nodiscard]]
		u64 hashToBucket(KeyHash hash) const RELEASE_NOEXCEPT {
			CORE_ASSERT(!buckets.empty(), "No buckets in StableHashMap");
			CORE_ASSERT(bucket_mask + 1 == buckets.size(), "Bucket size mismatch");

			auto res = hash & bucket_mask;

			CORE_ASSERT(res < buckets.size(), "Bucket index out of bounds");
			return res;
		}


		void rehash() RELEASE_NOEXCEPT {
			usize new_bucket_count = buckets.size() * 2;

			std::vector<MRef<Node>> previous_buckets = std::move(buckets);

			buckets.clear();
			buckets.resize(new_bucket_count);
			bucket_mask = new_bucket_count - 1;

			CORE_ASSERT(
				(new_bucket_count & bucket_mask) == 0,
				"Bucket count must be a power of two"
			);

			u64 considered_nodes = 0;

			for (const auto& bucket: previous_buckets) {
				MRef<Node> current_node = bucket;
				while (current_node) {
					auto next_node = current_node->next;
					
					current_node->next = nullptr;
					addToBucket(hashToBucket(current_node->cached_hash), current_node.toOpt().value());

					current_node = next_node;
					considered_nodes++;
				}
			}

			CORE_ASSERT(
				considered_nodes == element_count,
				"Node count mismatch during rehash: ",
				considered_nodes,
				" vs ",
				element_count
			);
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

		/*****************************************************************************************\
		|  Below is the map interface methods that take calculated hash of a key as a parameter. |
		|  It is used to ensure that hash is only calculated once when needed.                    |
		|  For doc comments explaining the interface, see the corresponding public functions.     |
		\*****************************************************************************************/


		/**
		 * See docs of put() method for for info.
		 * @param key_hash Precomputed hash of the key.
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		Ref<KeyValuePair> putAssumingHash(K&& key, D&& value, KeyHash key_hash) RELEASE_NOEXCEPT {
			auto new_node = node_allocator.allocateEmplace(
				nullptr, key_hash, std::forward<K>(key), std::forward<D>(value)
			);

			// Note that this can in theory have some observable side effects:
			CORE_ASSERT(
				not this->containsAssumingHash(new_node->key_value.key, key_hash),
				"Key already exists in StableHashMap"
			);

			addToBucket(hashToBucket(key_hash), new_node);

			element_count++;
			maybeRehash();

			return &new_node->key_value;
		}

		/**
		 * See docs of maybePut() method for for info.
		 * @param key_hash Precomputed hash of the key.
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		MRef<KeyValuePair> maybePutAssumingHash(K&& key, D&& value, KeyHash key_hash)
			RELEASE_NOEXCEPT {
			auto new_node = node_allocator.allocateEmplace(
				nullptr, key_hash, std::forward<K>(key), std::forward<D>(value)
			);

			if (this->containsAssumingHash(new_node->key_value.key, key_hash)) {
				node_allocator.deallocateDestroy(new_node);
				return nullptr;
			}

			addToBucket(hashToBucket(key_hash), new_node);

			element_count++;
			maybeRehash();

			return &new_node->key_value;
		}

		/**
		 * See docs of atMaybe() method for for info.
		 * @param key_hash Precomputed hash of the key.
		 */
		[[nodiscard]]
		base::Optional<CRef<DATA_T>> atMaybeAssumingHash(const KEY_T& key, KeyHash key_hash) const
			RELEASE_NOEXCEPT {
			auto current_node = buckets.at(hashToBucket(key_hash));
			while (current_node) {
				if (current_node->key_value.key == key) return &current_node->key_value.value;
				current_node = current_node->next;
			}
			return {};
		}

		/**
		 * See docs of atMaybe() method for for info.
		 * @param key_hash Precomputed hash of the key.
		 */
		[[nodiscard]]
		base::Optional<Ref<DATA_T>> atMaybeAssumingHash(const KEY_T& key, KeyHash key_hash)
			RELEASE_NOEXCEPT {
			auto current_node = buckets.at(hashToBucket(key_hash));
			while (current_node) {
				if (current_node->key_value.key == key) return &current_node->key_value.value;
				current_node = current_node->next;
			}
			return {};
		}

		/**
		 * See docs of atMaybeCopy() method for for info.
		 * @param key_hash Precomputed hash of the key.
		 */
		[[nodiscard]]
		base::Optional<DATA_T> atMaybeCopyAssumingHash(const KEY_T& key, KeyHash key_hash) const
			RELEASE_NOEXCEPT {
			auto current_node = buckets.at(hashToBucket(key_hash));
			while (current_node) {
				if (current_node->key_value.key == key) return current_node->key_value.value;
				current_node = current_node->next;
			}
			return {};
		}

		/**
		 * See docs of contains() method for for info.
		 * @param key_hash Precomputed hash of the key.
		 */
		[[nodiscard]]
		bool containsAssumingHash(const KEY_T& key, KeyHash key_hash) const RELEASE_NOEXCEPT {
			return atMaybeAssumingHash(key, key_hash).has_value();
		}

		bool eraseAssumingHash(const KEY_T& key, KeyHash key_hash) RELEASE_NOEXCEPT {
			u64        bucket_index  = hashToBucket(key_hash);

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


	public:
		StableHashMap(): bucket_mask(INITIAL_BUCKETS - 1), buckets(INITIAL_BUCKETS) {
			CORE_ASSERT(
				(INITIAL_BUCKETS & bucket_mask) == 0,
				"INITIAL_BUCKETS must be a power of two"
			);
		}

		StableHashMap(const StableHashMap&) = delete;

		StableHashMap(StableHashMap&& other) noexcept:
			  bucket_mask(other.bucket_mask),
			  buckets(std::move(other.buckets)),
			  node_allocator(std::move(other.node_allocator)),
			  element_count(other.element_count) {
			other.element_count = 0;
			other.buckets.resize(1, nullptr);
			other.bucket_mask = 0;
			CORE_ASSERT(bucket_mask + 1 == buckets.size(), "Bucket size mismatch after move");
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
		 * Inserts key->value into the container.
		 * Panics if key already exists.
		 * @param key Data key
		 * @param value The data
		 * @returns A reference to the inserted key-value pair.
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		Ref<KeyValuePair> put(K&& key, D&& value) RELEASE_NOEXCEPT {
			auto hash = keyHash(key);
			return putAssumingHash(std::forward<K>(key), std::forward<D>(value), hash);
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
			auto hash = keyHash(key);
			return maybePutAssumingHash(std::forward<K>(key), std::forward<D>(value), hash);
		}

		[[nodiscard]]
		base::Optional<CRef<DATA_T>> atMaybe(const KEY_T& key) const RELEASE_NOEXCEPT {
			auto hash = keyHash(key);
			return atMaybeAssumingHash(key, hash);
		}

		[[nodiscard]]
		base::Optional<Ref<DATA_T>> atMaybe(const KEY_T& key) RELEASE_NOEXCEPT {
			auto hash = keyHash(key);
			return atMaybeAssumingHash(key, hash);
		}

		[[nodiscard]]
		base::Optional<DATA_T> atMaybeCopy(const KEY_T& key) const RELEASE_NOEXCEPT {
			auto hash = keyHash(key);
			return atMaybeCopyAssumingHash(key, hash);
		}

		DATA_T& operator[](const KEY_T& key) { return **atMaybe(key); }

		const DATA_T& operator[](const KEY_T& key) const { return **atMaybe(key); }

		[[nodiscard]]
		bool contains(const KEY_T& key) const RELEASE_NOEXCEPT {
			auto hash = keyHash(key);
			return containsAssumingHash(key, hash);
		}

		/**
		 * @brief Erases value at @p key position if it exists.
		 * The references to the erased value are invalidated.
		 * @returns Whether a value was erased.
		 */
		bool erase(const KEY_T& key) RELEASE_NOEXCEPT {
			u64 key_hash = keyHash(key);
			return eraseAssumingHash(key, key_hash);
		}

		/**
		 * Clears the map, destroying all stored elements.
		 * @note does not free the memory used to store the elements.
		 */
		void clear() RELEASE_NOEXCEPT {
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
		 * Mask used to quickly calculate the bucket index from the hash.
		 * It is always equal to (number_of_buckets - 1).
		 */
		u64 bucket_mask;

		/**
		 * Array of bucket beginnings.
		 */
		std::vector<MRef<Node>> buckets;

		/**
		 * Memory pool allocator for node storage.
		 */
		// SingleTypeMemoryPoolAllocator<Node, ALLOCATOR_BLOCK_SIZE> node_allocator;
		// for now, to remove any allocator influence on performance:
		SingleTypeNewDeleteAllocator<Node> node_allocator;

		/**
		 * Number of elements stored in the map.
		 */
		u64 element_count = 0;
	};

}
