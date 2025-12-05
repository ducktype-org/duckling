#pragma once

#include <base/collections/stable_hashmap.hpp>
#include <concurrent/module_flags/worker_count.hpp>
#include <concurrent/locks/atomic_flag_mutex.hpp>

namespace concurrent {

    /**
     * A shared concurrent StableHashMap implementation.
     * It is implemented as a simple wrapper around base::StableHashMap.
     * It does not forward full interface, in particular it does not implement iteration.
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
            CORE_ASSERT(shard_counts == shard_mutexes.size() and shard_counts == shards.size(), "Shard count mismatch");
            CORE_ASSERT(shard_counts > 0, "Shard count must be greater than zero");
            return hash % shard_counts;
        }

        using KeyValuePair = typename HashMapType::KeyValuePair;

        /**
         * RAII lock for a given shard.
         */
        struct WithLock final {
            u64 shard_index;
            const HashMap& self;

            WithLock(HashMap& self, u64 shard_index) noexcept: shard_index(shard_index), self(self) {
                self.shard_mutexes[shard_index].lock();
            }

            ~WithLock() noexcept {
                self.shard_mutexes[shard_index].unlock();
            }
        };


    public:
        HashMap():
            shards(shard_counts),
            shard_mutexes(shard_counts) {}

        HashMap(const HashMap&) = delete;
        HashMap(HashMap&&) = delete;

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
            WithLock lock(*this, keyToShard(key));
            return shards[lock.shard_index].put(std::forward<K>(key), std::forward<D>(value));
        }

        [[nodiscard]]
		auto atMaybe(const KEY_T& key) const RELEASE_NOEXCEPT -> decltype(auto) {
            WithLock lock(*this, keyToShard(key));
            return shards[lock.shard_index].atMaybe(key);
        }

        [[nodiscard]]
		auto atMaybe(const KEY_T& key) RELEASE_NOEXCEPT -> decltype(auto) {
            WithLock lock(*this, keyToShard(key));
            return shards[lock.shard_index].atMaybe(key);
        }

    private:
        const u64 worker_count = concurrent::getWorkerCount();
        const u64 shard_counts = worker_count * 4;

        std::vector<HashMapType> shards;
        mutable std::vector<AtomicFlagMutex> shard_mutexes;
    };

}
