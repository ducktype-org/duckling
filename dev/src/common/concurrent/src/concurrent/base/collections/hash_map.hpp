#pragma once

#include <concurrent/base/locks/atomic_flag_spinlock.hpp>
#include <concurrent/module_flags/worker_count.hpp>

#include <base/collections/stable_hashmap.hpp>

#include <atomic>

namespace concurrent {

	/**
	 * A sharded concurrent StableHashMap implementation.
	 * It is implemented as a simple wrapper around base::StableHashMap.
	 * It does not forward a full interface of a hash map, in particular it does not implement
	 * iteration.
	 *
	 * For more info on sharding see (for example): https://le.qun.ch/en/blog/sharding/
	 *
	 * Concurrency:
	 * - Methods of this class are thread-safe.
	 * - Sequential consistency is guaranteed only per key.
	 * - Internally the map is sharded into multiple sub-maps, each protected by
	 *    its own lock.
	 */
	template<
		typename KEY_T,
		typename DATA_T,
		typename HASH_T          = std::hash<KEY_T>,
		u64 ALLOCATOR_BLOCK_SIZE = 4'096>
	class ConHashMap final {
		using HashMapType = base::StableHashMap<KEY_T, DATA_T, HASH_T, ALLOCATOR_BLOCK_SIZE>;

		using KeyHash = u64;

		[[nodiscard]]
		constexpr u64 keyToShard(const KEY_T& key) const
			noexcept(::base::IS_BUILD_TYPE_RELEASE && noexcept(HASH_T{}(key))) {
			u64 hash = HASH_T{}(key);

			CORE_ASSERT(
				SHARD_COUNT == shard_mutexes.size() and SHARD_COUNT == shards.size(),
				"Shard count mismatch"
			);
			CORE_ASSERT(SHARD_COUNT > 0, "Shard count must be greater than zero");

			u64 result = hash % SHARD_COUNT;
			CORE_ASSERT(result < SHARD_COUNT, "Shard index out of bounds");

			return result;
		}

		using KeyValuePair = typename HashMapType::KeyValuePair;

		/**
		 * RAII lock for a given shard.
		 *
		 * @note This can be changed to a reader-writer lock if needed in the future.
		 * Each method must then specify whether it needs a read or write lock.
		 */
		struct WithShardLock final {
			u64               shard_index;
			const ConHashMap& self;

			WithShardLock(const ConHashMap& self, u64 shard_index) noexcept:
				  shard_index(shard_index),
				  self(self) {
				self.shard_mutexes[shard_index]->lock();
			}

			~WithShardLock() noexcept { self.shard_mutexes[shard_index]->unlock(); }
		};

		/**
		 * RAII guard that locks ALL shards for the lifetime of the object.
		 * Used by begin()/end() to provide safe iteration over the entire map.
		 *
		 * @warning Holding this lock blocks all concurrent access to the map.
		 *          Keep the locked scope as short as possible.
		 */
		class WithAllShardsLock final {
			const ConHashMap& self;

		public:
			explicit WithAllShardsLock(const ConHashMap& self) noexcept: self(self) {
				for (u64 i = 0; i < SHARD_COUNT; i++) self.shard_mutexes[i]->lock();
			}

			~WithAllShardsLock() noexcept {
				for (u64 i = 0; i < SHARD_COUNT; i++) self.shard_mutexes[i]->unlock();
			}

			WithAllShardsLock(const WithAllShardsLock&) = delete;
			WithAllShardsLock(WithAllShardsLock&&)      = delete;
		};


	public:
		ConHashMap(): shards(SHARD_COUNT) {
			for (u64 i = 0; i < SHARD_COUNT; i++)
				shard_mutexes.emplace_back(makeBox<concurrent::AtomicFlagSpinlock>());
		}

		ConHashMap(const ConHashMap&) = delete;
		ConHashMap(ConHashMap&&)      = delete;

		~ConHashMap() = default;

		/**
		 * Inserts key->value into the container.
		 * Panics if key already exists.
		 * @param key Data key
		 * @param value The data
		 * @returns A reference to the inserted key-value pair.
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		auto put(K&& key, D&& value) RELEASE_NOEXCEPT -> decltype(auto) {
			WithShardLock lock(*this, keyToShard(key));
			auto          result
				= shards[lock.shard_index].put(std::forward<K>(key), std::forward<D>(value));
			elements_count.fetch_add(1, std::memory_order_relaxed);
			return result;
		}

		/**
		 * Inserts key->value into the container.
		 * Does nothing if key already exists.
		 * @param key Data key
		 * @param value The data
		 * @returns Optional reference to the inserted key-value pair. Reference is empty if key
		 * already existed.
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		MRef<KeyValuePair> maybePut(K&& key, D&& value) RELEASE_NOEXCEPT {
			WithShardLock lock(*this, keyToShard(key));

			auto result
				= shards.at(lock.shard_index).maybePut(std::forward<K>(key), std::forward<D>(value));
			if (result != nullptr) elements_count.fetch_add(1, std::memory_order_relaxed);
			return result;
		}

		/**
		 * Performs atomically a following sequence:
		 * 1. Inserts key->value into the container if key does not exist.
		 * 2. Calls f with reference to the value associated with the key.
		 */
		template<typename K = KEY_T, typename D = DATA_T, typename Func>
		void maybePutAndUpdate(const K& key, const D& value, Func f) RELEASE_NOEXCEPT {
			WithShardLock lock(*this, keyToShard(key));

			auto inserted = shards[lock.shard_index].maybePut(key, value);
			if (inserted != nullptr) elements_count.fetch_add(1, std::memory_order_relaxed);
			f(shards[lock.shard_index][key]);
		}

		/**
		 * Atomically retrieves a copy of the value associated with the given key.
		 */
		[[nodiscard]]
		DATA_T getCopy(const KEY_T& key) const RELEASE_NOEXCEPT {
			WithShardLock lock(*this, keyToShard(key));
			DATA_T        value = shards[lock.shard_index][key];
			return value;
		}

		/**
		 * Atomically retrieves a copy of the value associated with the given key.
		 * Return empty optional, if the key is not present in the map.
		 */
		[[nodiscard]]
		base::Optional<DATA_T> atMaybeCopy(const KEY_T& key) const RELEASE_NOEXCEPT {
			WithShardLock lock(*this, keyToShard(key));
			return shards[lock.shard_index].atMaybeCopy(key);
		}

		/**
		 * Atomically retrieves a reference to the value associated with the given key.
		 *
		 * @important Usage of the reference must be synchronized externally.
		 * For example `map.at(key) = ...` may lead to data races on `=` operator.
		 */
		[[nodiscard]]
		auto atMaybe(const KEY_T& key) RELEASE_NOEXCEPT -> decltype(auto) {
			WithShardLock lock(*this, keyToShard(key));
			return shards[lock.shard_index].atMaybe(key);
		}

		/**
		 * Atomically updates the value associated with the given key.
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		void update(const KEY_T& key, const DATA_T& value) RELEASE_NOEXCEPT {
			WithShardLock lock(*this, keyToShard(key));
			shards[lock.shard_index][key] = value;
		}

		[[nodiscard]]
		auto contains(const KEY_T& key) const RELEASE_NOEXCEPT -> decltype(auto) {
			WithShardLock lock(*this, keyToShard(key));
			return shards[lock.shard_index].contains(key);
		}

		/**
		 * Atomically erases the given key->value pair from the map.
		 */
		auto erase(const KEY_T& key) RELEASE_NOEXCEPT -> decltype(auto) {
			WithShardLock lock(*this, keyToShard(key));
			bool          erased = shards[lock.shard_index].erase(key);
			if (erased) elements_count.fetch_sub(1, std::memory_order_relaxed);
			return erased;
		}

		/**
		 * Returns the number of elements currently stored in the map.
		 *
		 * @note The value is approximate under concurrent modifications –
		 *       other operations uses relaxed memory ordering and this one is not synchronized with
		 *       any particular shard lock.
		 */
		[[nodiscard]]
		u64 size() const noexcept {
			return elements_count.load(std::memory_order_seq_cst);
		}

		/**
		 * Iterator over the entire ConHashMap.
		 *
		 * On construction it receives a shared_ptr to an WithAllShardsLock that keeps
		 * every shard locked for as long as any live iterator references it.
		 * Dereferencing, advancing and comparing are delegated to the underlying
		 * StableHashMap iterators.
		 *
		 * @tparam ValueT  KeyValuePair or const KeyValuePair – controls mutability.
		 *
		 * @warning All shards remain locked while any iterator obtained from the
		 *          same begin()/end() pair is alive.  Keep iteration scopes short.
		 */
		template<typename ValueT>
		class LockedIterator final {
			using InnerIterator = typename HashMapType::template Iterator<ValueT>;

			std::shared_ptr<WithAllShardsLock> lock_guard;

			/// Pointer to the shards vector (const or mutable depending on ValueT).
			/// This would have to change when number of shards is not compile-time constant
			using ShardsPtr = std::conditional_t<
				std::is_const_v<ValueT>,
				const std::vector<HashMapType>*,
				std::vector<HashMapType>*>;

			ShardsPtr shards_ptr;

			/// This would have to change when number of shards is not compile-time constant
			u64           shard_index;
			InnerIterator inner;

			/**
			 * If the current inner iterator has reached the end of its shard,
			 * advance to the next non-empty shard.
			 */
			void advanceToValid() {
				while (shard_index < SHARD_COUNT && inner == (*shards_ptr)[shard_index].end()) {
					++shard_index;
					if (shard_index < SHARD_COUNT) inner = (*shards_ptr)[shard_index].begin();
				}
			}

		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type   = std::ptrdiff_t;
			using value_type        = ValueT;
			using pointer           = value_type*;
			using reference         = value_type&;

			/**
			 * Constructs an iterator starting at the given shard.
			 *
			 * @param lock_guard  Shared lock keeping all shards locked.
			 * @param shards_ptr  Pointer to the shards vector.
			 * @param shard_index Starting shard index (SHARD_COUNT for end()).
			 */
			LockedIterator(
				std::shared_ptr<WithAllShardsLock> lock_guard, ShardsPtr shards_ptr, u64 shard_index
			) noexcept:
				  lock_guard(std::move(lock_guard)),
				  shards_ptr(shards_ptr),
				  shard_index(shard_index) {
				if (shard_index < SHARD_COUNT) {
					inner = (*this->shards_ptr)[shard_index].begin();
					advanceToValid();
				}
			}

			LockedIterator() noexcept: shards_ptr(nullptr), shard_index(SHARD_COUNT) {}

			LockedIterator(const LockedIterator&)            = default;
			LockedIterator& operator=(const LockedIterator&) = default;

			reference operator*() const { return *inner; }

			pointer operator->() const { return &(*inner); }

			LockedIterator& operator++() {
				++inner;
				advanceToValid();
				return *this;
			}

			LockedIterator operator++(int) {
				LockedIterator tmp = *this;
				++(*this);
				return tmp;
			}

			bool operator==(const LockedIterator& other) const noexcept {
				if (shard_index == SHARD_COUNT && other.shard_index == SHARD_COUNT) return true;
				return shard_index == other.shard_index && inner == other.inner;
			}

			bool operator!=(const LockedIterator& other) const noexcept {
				return !(*this == other);
			}
		};

		using Iterator      = LockedIterator<KeyValuePair>;
		using ConstIterator = LockedIterator<const KeyValuePair>;


		static_assert(std::forward_iterator<Iterator>, "Iterator must be a forward iterator");
		static_assert(
			std::forward_iterator<ConstIterator>, "ConstIterator must be a forward iterator"
		);

		/**
		 * Returns an iterator-pair spanning all elements across every shard.
		 *
		 * All shards are locked for the lifetime of the returned iterators
		 * (they share ownership of the lock via shared_ptr).
		 *
		 * @code
		 *   for (auto it = map.begin(); it != map.end(); ++it) { ... }
		 * @endcode
		 *
		 * @warning Do NOT store the iterators beyond the scope where you need
		 *          them – the entire map is blocked while any iterator is alive.
		 */
		Iterator begin() RELEASE_NOEXCEPT {
			auto guard = std::make_shared<WithAllShardsLock>(*this);
			return Iterator(guard, &shards, 0);
		}

		Iterator end() RELEASE_NOEXCEPT { return Iterator(nullptr, &shards, SHARD_COUNT); }

		ConstIterator begin() const RELEASE_NOEXCEPT {
			auto guard = std::make_shared<WithAllShardsLock>(*this);
			return ConstIterator(guard, &shards, 0);
		}

		ConstIterator end() const RELEASE_NOEXCEPT {
			return ConstIterator(nullptr, &shards, SHARD_COUNT);
		}

	private:
		/**
		 * Number of shards used in the map.
		 * @important In the current implementation GCD(SHARD_COUNT, 2) must be 1.
		 * Otherwise, the distribution of keys over buckets in individual shards is highly
		 * non-uniform, as the modulus used to select the buckets are the powers of two.
		 *
		 * @note In the future we might want to make SHARD_COUNT configurable, so it can be smaller
		 * for "small" use cases and larger where it might matter (e.g. cache of highly concurrent
		 * queries).
		 */
		constexpr static u64 SHARD_COUNT = 129;

		/**
		 * The shards of the map.
		 */
		std::vector<HashMapType> shards;

		/**
		 * The locks protecting each shard.
		 */
		mutable std::vector<Box<concurrent::AtomicFlagSpinlock>> shard_mutexes;

		/**
		 * Atomic counter tracking the number of elements in the map.
		 */
		std::atomic<u64> elements_count{ 0 };
	};

}
