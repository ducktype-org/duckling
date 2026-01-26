#pragma once

#include <concurrent/locks/atomic_flag_spinlock.hpp>
#include <concurrent/module_flags/worker_count.hpp>

#include <base/collections/stable_hashmap.hpp>

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
			return shards[lock.shard_index].put(std::forward<K>(key), std::forward<D>(value));
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

			return shards.at(lock.shard_index).maybePut(std::forward<K>(key), std::forward<D>(value));
		}

		/**
		 * Performs atomically a following sequence:
		 * 1. Inserts key->value into the container if key does not exist.
		 * 2. Calls f with reference to the value associated with the key.
		 */
		template<typename K = KEY_T, typename D = DATA_T, typename Func>
		void maybePutAndUpdate(const K& key, const D& value, Func f) RELEASE_NOEXCEPT {
			WithShardLock lock(*this, keyToShard(key));

			shards[lock.shard_index].maybePut(key, value);
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

		auto erase(const KEY_T& key) RELEASE_NOEXCEPT -> decltype(auto) {
			WithShardLock lock(*this, keyToShard(key));
			return shards[lock.shard_index].erase(key);
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
	};

}
