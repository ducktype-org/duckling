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
		// @TODO: #1747 For some reason StableHashMap here
		// is much slower than std::unordered_map in
		// concurrent scenarios. Investigate and fix, preferably by improving StableHashMap.

		// Internal hash map types and checks:
		
		using HashMapType = base::StableHashMap<KEY_T, DATA_T, HASH_T, ALLOCATOR_BLOCK_SIZE>;

		using KeyHash = u64;

		using KeyValuePair = typename HashMapType::KeyValuePair;

		static_assert(std::is_same_v<decltype(HashMapType::keyHash(std::declval<KEY_T>())), u64>,
			"keyHash must return u64, if this breaks please update the implementation of this hash map accordingly"
		);
		static_assert(
			std::is_same_v<KeyHash, typename HashMapType::KeyHash>,
			"KeyHash of ConHashMap must be the same as KeyHash of underlying StableHashMap, please update the implementation of this hash map accordingly"
		);

		// Helper methods:


		/**
		 * This computes the hash of the given key using the hash function of the underlying
		 * StableHashMap.
		 * 
		 * @important: The computed hash must be the same as used internally by the
		 *             StableHashMap, otherwise the map will not work correctly.
		 *             This is needed, because ConHashMap computes the hash to determine
		 *             the shard for a given key, and then uses private methods of StableHashMap
		 *             that take precomputed hash as a parameter.
		 */
		static KeyHash keyHash(const KEY_T& key) {
			return HashMapType::keyHash(key);
		}

	
		[[nodiscard]]
		constexpr u64 hashToShard(KeyHash hash) const {
			CORE_ASSERT(
				SHARD_COUNT == shard_mutexes.size() and SHARD_COUNT == shards.size(),
				"Shard count mismatch"
			);
			CORE_ASSERT(SHARD_COUNT > 0, "Shard count must be greater than zero");

			u64 result = hash % SHARD_COUNT;
			CORE_ASSERT(result < SHARD_COUNT, "Shard index out of bounds");

			return result;
		}



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
			auto hash = keyHash(key);

			WithShardLock lock(*this, hashToShard(hash));

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
			auto hash = keyHash(key);

			WithShardLock lock(*this, hashToShard(hash));

			return shards.at(lock.shard_index).maybePutAssumingHash(
				std::forward<K>(key),
				std::forward<D>(value),
				hash
			);
		}

		/**
		 * Performs atomically a following sequence:
		 * 1. Inserts key->value into the container if key does not exist.
		 * 2. Calls f with reference to the value associated with the key.
		 */
		template<typename K = KEY_T, typename D = DATA_T, typename Func>
		void maybePutAndUpdate(const K& key, const D& value, Func f) RELEASE_NOEXCEPT {
			auto hash = keyHash(key);
			
			WithShardLock lock(*this, hashToShard(hash));

			shards[lock.shard_index].maybePut(key, value);
			f(shards[lock.shard_index][key]);
		}

		/**
		 * Atomically retrieves a copy of the value associated with the given key.
		 */
		[[nodiscard]]
		DATA_T getCopy(const KEY_T& key) const RELEASE_NOEXCEPT {
			auto hash = keyHash(key);
			
			WithShardLock lock(*this, hashToShard(hash));

			DATA_T        value = shards[lock.shard_index][key];
			return value;
		}

		/**
		 * Atomically retrieves a reference to the value associated with the given key.
		 *
		 * @important Usage of the reference must be synchronized externally.
		 * For example `map.at(key) = ...` may lead to data races on `=` operator.
		 */
		[[nodiscard]]
		auto atMaybe(const KEY_T& key) RELEASE_NOEXCEPT -> decltype(auto) {
			auto hash = keyHash(key);
			
			WithShardLock lock(*this, hashToShard(hash));

			return shards[lock.shard_index].atMaybeAssumingHash(key, hash);
		}

		template<typename K = KEY_T, typename D = DATA_T>
		void update(const KEY_T& key, const DATA_T& value) RELEASE_NOEXCEPT {
			auto hash = keyHash(key);
			
			WithShardLock lock(*this, hashToShard(hash));

			shards[lock.shard_index][key] = value;
		}

		[[nodiscard]]
		auto contains(const KEY_T& key) const RELEASE_NOEXCEPT -> decltype(auto) {
			auto hash = keyHash(key);
			
			WithShardLock lock(*this, hashToShard(hash));

			return shards[lock.shard_index].contains(key);
		}

		[[nodiscard]]
		auto erase(const KEY_T& key) RELEASE_NOEXCEPT -> decltype(auto) {
			auto hash = keyHash(key);
			
			WithShardLock lock(*this, hashToShard(hash));

			return shards[lock.shard_index].eraseAssumingHash(key, hash);
		}

	private:
		/**
		 * Number of shards used in the map.
		 */
		constexpr static u64 SHARD_COUNT = 128;

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
