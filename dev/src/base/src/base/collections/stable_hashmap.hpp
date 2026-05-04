/**
 * @file stable_hashmap.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <base/collections/maps.hpp>
#include <base/memory/single_type_memory_pool_allocator.hpp>
#include <base/pointers/box.hpp>

#include <concepts>
#include <iterator>
#include <type_traits>
#include <utility>
#include <iostream>

namespace concurrent {
	// forward declaration of ConHashMap
	template<
		typename KEY_T,
		typename DATA_T,
		typename HASH_T,
		u64 ALLOCATOR_BLOCK_SIZE>
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


		friend class ::concurrent::ConHashMap<KEY_T, DATA_T, HASH_T, ALLOCATOR_BLOCK_SIZE>;

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

		/**
		 * Type used to represent the hash of the key.
		 */
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
		static KeyHash keyHash(const KEY_T& key
		) noexcept(::base::IS_BUILD_TYPE_RELEASE && noexcept(HASH_T{}(key))) {
			return HASH_T{}(key);
		}

		[[nodiscard]]
		u64 hashToBucket(KeyHash hash) const RELEASE_NOEXCEPT {
			CORE_ASSERT(!buckets.empty(), "No buckets in StableHashMap");
			auto res = hash % buckets.size();
			CORE_ASSERT(0 <= res and res < buckets.size(), "Bucket index out of bounds");
			return res;
		}

		[[nodiscard]]
		u64 keyToBucket(const KEY_T& key) const {
			return hashToBucket(keyHash(key));
		}

		void rehash() RELEASE_NOEXCEPT {
			usize new_bucket_count = buckets.size() * 8;

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
			// NO DIFFERENCE IN PERFORMANCE:
			if (element_count * 10 > buckets.size() * 7) [[unlikely]]
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
				nullptr, std::forward<K>(key), std::forward<D>(value)
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
		template<typename K, typename D = DATA_T>
		requires std::same_as<std::remove_cvref_t<K>, KEY_T>
		MRef<KeyValuePair> maybePutAssumingHash(K&& key, D&& value, KeyHash key_hash)
			RELEASE_NOEXCEPT {
			u64        bucket_index = hashToBucket(key_hash);
			MRef<Node> current_node = buckets.at(bucket_index);
			while (current_node) {
				if (current_node->key_value.key == key) return nullptr;
				current_node = current_node->next;
			}

			auto new_node = node_allocator.allocateEmplace(
				nullptr, std::forward<K>(key), std::forward<D>(value)
			);
			addToBucket(bucket_index, new_node);

			element_count++;
			maybeRehash();

			return &new_node->key_value;
		}

		/**
		 * See docs of maybePutAndUpdate() method for info.
		 * @param key_hash Precomputed hash of the key.
		 */
		template<typename K, typename D = DATA_T, typename Func>
		requires std::same_as<std::remove_cvref_t<K>, KEY_T>
		MRef<KeyValuePair> maybePutAndUpdateAssumingHash(
			K&& key, D&& value, Func&& f, KeyHash key_hash
		) RELEASE_NOEXCEPT {
			u64        bucket_index = hashToBucket(key_hash);
			MRef<Node> current_node = buckets.at(bucket_index);

			while (current_node) {
				if (current_node->key_value.key == key) {
					std::forward<Func>(f)(Ref<DATA_T>(&current_node->key_value.value));
					return nullptr;
				}
				current_node = current_node->next;
			}

			auto new_node = node_allocator.allocateEmplace(
				nullptr, std::forward<K>(key), std::forward<D>(value)
			);

			addToBucket(bucket_index, new_node);

			element_count++;
			maybeRehash();

			std::forward<Func>(f)(Ref<DATA_T>(&new_node->key_value.value));
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
		 * See docs of atMaybe() method for for info.
		 * @param key_hash Precomputed hash of the key.
		 */
		[[nodiscard]]
		CRef<DATA_T> atAssumingHash(const KEY_T& key, KeyHash key_hash) const
			RELEASE_NOEXCEPT {
			auto current_node = buckets.at(hashToBucket(key_hash));
			while (current_node) {
				if (current_node->next == nullptr or current_node->key_value.key == key) return &current_node->key_value.value;
				current_node = current_node->next;
			}
			CORE_UNREACHABLE();
		}

		/**
		 * See docs of atMaybe() method for for info.
		 * @param key_hash Precomputed hash of the key.
		 */
		[[nodiscard]]
		Ref<DATA_T> atAssumingHash(const KEY_T& key, KeyHash key_hash)
			RELEASE_NOEXCEPT {
			auto current_node = buckets.at(hashToBucket(key_hash));
			while (current_node) {
				if (current_node->next == nullptr or current_node->key_value.key == key) return &current_node->key_value.value;
				current_node = current_node->next;
			}
			CORE_UNREACHABLE();
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
			auto bucket_count = buckets.size();
			u64 more_then_one_element = 0;
			u64 zero_element_buckets = 0;

			for (auto& bucket: buckets) {
				MRef<Node> current_node = bucket;

				u64 node_count = 0;
				while (current_node) {
					node_count++;
					MRef<Node> next_node = current_node->next;
					node_allocator.justDestroy(current_node.toOpt().value());
					current_node = next_node;
				}
				if (node_count > 1) more_then_one_element++;
				if (node_count == 0) zero_element_buckets++;
			}

			if (element_count != 0 and more_then_one_element > 0) {
				// std::cerr << typeid(KEY_T).name() << " -> " << typeid(DATA_T).name() << ": ";
				// std::cerr << "StableHashMap destroyed. Final bucket count: " << bucket_count
			    //       << ", element count: " << element_count
			    //       << ", buckets with more than 1 element: " << more_then_one_element
			    //       << ", buckets with 0 elements: " << zero_element_buckets << std::endl;

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
		template<typename K, typename D = DATA_T>
		requires std::same_as<std::remove_cvref_t<K>, KEY_T>
		MRef<KeyValuePair> maybePut(K&& key, D&& value) RELEASE_NOEXCEPT {
			auto hash = keyHash(key);
			return maybePutAssumingHash(std::forward<K>(key), std::forward<D>(value), hash);
		}

		/**
		 * Performs atomically a following sequence:
		 * 1. Inserts key->value into the container if key does not exist.
		 * 2. Calls f with reference to the value associated with the key.
		 *
		 * This method performs only one lookup in the target bucket.
		 *
		 * @returns Optional reference to the inserted key-value pair. Reference is empty if key
		 * already existed.
		 */
		template<typename K, typename D = DATA_T, typename Func>
		requires std::same_as<std::remove_cvref_t<K>, KEY_T>
		MRef<KeyValuePair> maybePutAndUpdate(K&& key, D&& value, Func&& f) RELEASE_NOEXCEPT {
			auto hash = keyHash(key);
			return maybePutAndUpdateAssumingHash(
				std::forward<K>(key), std::forward<D>(value), std::forward<Func>(f), hash
			);
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
		CRef<DATA_T> at(const KEY_T& key) const RELEASE_NOEXCEPT {
			auto hash = keyHash(key);
			return atAssumingHash(key, hash);
		}

		[[nodiscard]]
		Ref<DATA_T> at(const KEY_T& key) RELEASE_NOEXCEPT {
			auto hash = keyHash(key);
			return atAssumingHash(key, hash);
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
