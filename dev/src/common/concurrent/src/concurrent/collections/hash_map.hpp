#pragma once

#include <concurrent/locks/atomic_flag_spinlock.hpp>
#include <concurrent/module_flags/worker_count.hpp>

#include <base/collections/stable_hashmap.hpp>

namespace concurrent {

	/**
	 * A shared concurrent StableHashMap implementation.
	 * It is implemented as a simple wrapper around base::StableHashMap.
	 * It does not forward a full interface of a hash map, in particular it does not implement
	 * iteration.
	 *
	 * Concurrency:
	 * - Methods of this class are thread-safe.
	 * - Execution sequential consistency is guaranteed only per key.
	 * - Internally the map is sharded into multiple sub-maps, each protected by
	 *    its own AtomicFlagSpinlock.
	 */
	template<
		typename KEY_T,
		typename DATA_T,
		typename HASH_T          = std::hash<KEY_T>,
		u64 ALLOCATOR_BLOCK_SIZE = 4'096>
	class HashMap final {
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
		 */
		struct WithWriterLock final {
			u64            shard_index;
			const HashMap& self;

			WithWriterLock(const HashMap& self, u64 shard_index) noexcept:
				  shard_index(shard_index),
				  self(self) {
				self.shard_mutexes[shard_index]->lock();
			}

			~WithWriterLock() noexcept { self.shard_mutexes[shard_index]->unlock(); }
		};

		/**
		 * RAII lock for a given shard.
		 */
		struct WithReaderLock final {
			u64            shard_index;
			const HashMap& self;

			WithReaderLock(const HashMap& self, u64 shard_index) noexcept:
				  shard_index(shard_index),
				  self(self) {
				self.shard_mutexes[shard_index]->lock();
			}

			~WithReaderLock() noexcept { self.shard_mutexes[shard_index]->unlock(); }
		};


	public:
		HashMap(): shards(SHARD_COUNT) {
			for (u64 i = 0; i < SHARD_COUNT; i++)
				shard_mutexes.emplace_back(makeBox<concurrent::AtomicFlagSpinlock>());
		}

		HashMap(const HashMap&) = delete;
		HashMap(HashMap&&)      = delete;

		~HashMap() = default;

		/**
		 * Inserts key->value into the container.
		 * Panics if key already exists.
		 * @param key Data key
		 * @param value The data
		 * @returns A reference to the inserted key-value pair.
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		auto put(K&& key, D&& value) RELEASE_NOEXCEPT -> decltype(auto) {
			WithWriterLock lock(*this, keyToShard(key));
			return shards[lock.shard_index].put(std::forward<K>(key), std::forward<D>(value));
		}

		/**
		 * Inserts key->value into the container.
		 * Does nothing if key already exists.
		 * @param key Data key
		 * @param value The data
		 * @returns A reference to the inserted key-value pair.
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		void tryPut(const K& key, const D& value) RELEASE_NOEXCEPT {
			WithWriterLock lock(*this, keyToShard(key));

			if (shards.at(lock.shard_index).contains(key)) return;
			shards.at(lock.shard_index).put(key, value);
		}

		template<typename K = KEY_T, typename D = DATA_T, typename Func>
		void tryPutAndUpdate(const K& key, const D& value, Func f) RELEASE_NOEXCEPT {
			WithWriterLock lock(*this, keyToShard(key));

			shards[lock.shard_index].maybePut(key, value);
			f(shards[lock.shard_index][key]);
		}

		/**
		 * Atomically retrieves a copy of the value associated with the given key.
		 */
		[[nodiscard]]
		DATA_T getCopy(const KEY_T& key) const RELEASE_NOEXCEPT {
			WithReaderLock lock(*this, keyToShard(key));
			DATA_T         value = shards[lock.shard_index][key];
			return value;
		}

		// PR: this does not work well with concurrent map, since
		// there can be races on reference returned.
		// [[nodiscard]]
		// auto atMaybe(const KEY_T& key) RELEASE_NOEXCEPT -> decltype(auto) {
		//     WithLock lock(*this, keyToShard(key));
		//     return shards[lock.shard_index].atMaybe(key);
		// }


		// operators[] don't work well with concurrent map, since
		// DATA_T& operator[](const KEY_T& key) {
		// }
		// const DATA_T& operator[](const KEY_T& key) const { return **atMaybe(key); }

		template<typename K = KEY_T, typename D = DATA_T>
		void update(const KEY_T& key, const DATA_T& value) RELEASE_NOEXCEPT {
			WithWriterLock lock(*this, keyToShard(key));
			shards[lock.shard_index][key] = value;
		}

		[[nodiscard]]
		auto contains(const KEY_T& key) const RELEASE_NOEXCEPT -> decltype(auto) {
			WithReaderLock lock(*this, keyToShard(key));
			return shards[lock.shard_index].contains(key);
		}

	private:
		/**
		 * Number of shards used in the map.
		 */
		constexpr static u64 SHARD_COUNT = 1'024;

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
