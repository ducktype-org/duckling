// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <concurrent/base/locks/atomic_flag_spinlock.hpp>
#include <concurrent/module_flags/worker_count.hpp>

#include <base/collections/stable_hashmap.hpp>

#include <atomic>
#include <concepts>
#include <type_traits>
#include <utility>
#include <vector>

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
	class alignas(64) ConHashMap final {
		using HashMapType = base::StableHashMap<KEY_T, DATA_T, HASH_T, ALLOCATOR_BLOCK_SIZE>;

		using KeyHash = u64;

		/**
		 * A single shard: its lock and its map together, aligned to a cache line.
		 *
		 * The lock lives next to the data it protects (no extra indirection), and neighbouring
		 * shards never share a cache line, so locking or modifying one shard does not invalidate
		 * the lines of other shards on other cores.
		 */
		struct alignas(64) Shard final {
			mutable concurrent::AtomicFlagSpinlock lock;
			HashMapType                            map;

			Shard() = default;

			explicit Shard(HashMapType&& source_map) noexcept: map(std::move(source_map)) {}

			// Only needed to satisfy std::vector requirements; shards are never relocated (the
			// vector is created with its final size and never grows).
			Shard(Shard&& other) noexcept: map(std::move(other.map)) {}

			Shard(const Shard&)            = delete;
			Shard& operator=(const Shard&) = delete;
			Shard& operator=(Shard&&)      = delete;
		};

		/**
		 * The hash is computed once per operation (by the underlying map's mixed keyHash) and
		 * reused for both the shard and the bucket selection.
		 */
		[[nodiscard]]
		static KeyHash keyHash(const KEY_T& key
		) noexcept(::base::IS_BUILD_TYPE_RELEASE && noexcept(HASH_T{}(key))) {
			return HashMapType::keyHash(key);
		}

		/**
		 * Shard index from the high 32 bits of the hash (multiply-shift instead of a division).
		 * Buckets inside a shard use the low bits, so the two selections are independent.
		 */
		[[nodiscard]]
		static constexpr u64 shardOf(KeyHash hash) noexcept {
			u64 result = ((hash >> 32) * SHARD_COUNT) >> 32;
			return result;
		}

		/**
		 * Asserts that the number of shards is correct.
		 * This fails when someone accidentally pushes a new shard to the shards vector.
		 */
		void assertCorrectShardsSize() const {
			CORE_ASSERT(SHARD_COUNT == shards.size(), "Shard count mismatch");
		}

		static void assertShardIndex(u64 shard_index) {
			CORE_ASSERT(shard_index < SHARD_COUNT, "Shard index out of bounds");
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
				assertShardIndex(shard_index);
				self.shards[shard_index].lock.lock();
				self.assertCorrectShardsSize();
			}

			~WithShardLock() noexcept { self.shards[shard_index].lock.unlock(); }
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
				for (u64 i = 0; i < SHARD_COUNT; i++) self.shards[i].lock.lock();
				self.assertCorrectShardsSize();
			}

			~WithAllShardsLock() noexcept {
				for (u64 i = 0; i < SHARD_COUNT; i++) self.shards[i].lock.unlock();
			}

			WithAllShardsLock(const WithAllShardsLock&) = delete;
			WithAllShardsLock(WithAllShardsLock&&)      = delete;
		};

		/**
		 * Moves the content of all shards of @p source out, under a lock of all its shards.
		 * The source maps are left empty but valid.
		 */
		static std::vector<Shard> takeShardsFrom(ConHashMap& source) noexcept {
			WithAllShardsLock  lock(source);
			std::vector<Shard> result;
			result.reserve(SHARD_COUNT);
			for (u64 i = 0; i < SHARD_COUNT; i++) result.emplace_back(std::move(source.shards[i].map));
			return result;
		}

		/**
		 * Locks the shard of @p key and calls @p f(shard map, key hash).
		 */
		template<typename Func>
		decltype(auto) withShardOf(const KEY_T& key, Func&& f) RELEASE_NOEXCEPT {
			const KeyHash hash = keyHash(key);
			WithShardLock lock(*this, shardOf(hash));
			return std::forward<Func>(f)(shards[lock.shard_index].map, hash);
		}

		template<typename Func>
		decltype(auto) withShardOf(const KEY_T& key, Func&& f) const RELEASE_NOEXCEPT {
			const KeyHash hash = keyHash(key);
			WithShardLock lock(*this, shardOf(hash));
			return std::forward<Func>(f)(std::as_const(shards[lock.shard_index].map), hash);
		}

	public:
		using KeyValuePair = typename HashMapType::KeyValuePair;

		ConHashMap(): shards(SHARD_COUNT) {}

		ConHashMap(const ConHashMap&) = delete;

		/**
		 * Move constructor.
		 * After this operation, the @p source map is left in an empty but valid state.
		 *
		 * Underneath this performs the following:
		 * * It locks all shards of the source, moves the content of each shard map out (the
		 *   nodes stay in place in memory, their constructors are not called) and leaves the
		 *   source maps empty.
		 * * The new map has its own, fresh locks. This way if other threads try to access the
		 *   source map during the move, they will be properly synchronized and will not cause
		 *   data races or access to invalid memory.
		 *
		 * @note This operation is thread safe, but beware that moves perform a large lock on the
		 * map, and may in general be bug prone when done accidentally.
		 */
		ConHashMap(ConHashMap&& source) noexcept: shards(takeShardsFrom(source)) {}

		~ConHashMap() = default;



		/**
		 * Inserts key->value into the container.
		 * Panics if key already exists.
		 * @param key Data key
		 * @param value The data
		 * @returns A reference to the inserted key-value pair.
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		auto put(K&& key, D&& value) RELEASE_NOEXCEPT {
			const KeyHash hash = keyHash(key);
			WithShardLock lock(*this, shardOf(hash));
			return shards[lock.shard_index].map.putAssumingHash(
				std::forward<K>(key), std::forward<D>(value), hash
			);
		}

		/**
		 * Inserts key->value into the container.
		 * Does nothing if key already exists.
		 * @param key Data key
		 * @param value The data
		 * @returns Optional reference to the inserted key-value pair. Reference is empty if key
		 * already existed.
		 */
		template<typename K, typename D = DATA_T>
		requires std::same_as<std::remove_cvref_t<K>, KEY_T>
		MRef<KeyValuePair> maybePut(K&& key, D&& value) RELEASE_NOEXCEPT {
			const KeyHash hash = keyHash(key);
			WithShardLock lock(*this, shardOf(hash));
			return shards[lock.shard_index].map.maybePutAssumingHash(
				std::forward<K>(key), std::forward<D>(value), hash
			);
		}

		/**
		 * Inserts key->value into the container if key does not exist,
		 * otherwise assigns the new value to the existing key.
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		void putOrAssign(const K& key, D&& value) RELEASE_NOEXCEPT {
			const KeyHash hash = keyHash(key);
			WithShardLock lock(*this, shardOf(hash));
			auto&         map = shards[lock.shard_index].map;

			if (auto existing = map.atMaybeAssumingHash(key, hash); existing.has_value()) {
				*existing.value() = std::forward<D>(value);
				return;
			}
			map.maybePutAssumingHash(key, std::forward<D>(value), hash);
		}

		/**
		 * Performs atomically a following sequence:
		 * 1. Inserts key->value into the container if key does not exist.
		 * 2. Calls f with reference to the value associated with the key.
		 * @returns Optional reference to the inserted key-value pair. Reference is empty if key
		 * already existed.
		 */
		template<typename K, typename D = DATA_T, typename Func>
		requires std::same_as<std::remove_cvref_t<K>, KEY_T>
		MRef<KeyValuePair> maybePutAndUpdate(K&& key, D&& value, Func&& f) RELEASE_NOEXCEPT {
			const KeyHash hash = keyHash(key);
			WithShardLock lock(*this, shardOf(hash));
			return shards[lock.shard_index].map.maybePutAndUpdateAssumingHash(
				std::forward<K>(key), std::forward<D>(value), std::forward<Func>(f), hash
			);
		}

		/**
		 * Calls f with reference to the value associated with the key if the key exists.
		 */
		template<typename K = KEY_T, typename Func>
		void maybeCallOn(const K& key, Func&& f) RELEASE_NOEXCEPT {
			withShardOf(key, [&](auto& map, KeyHash hash) {
				auto data = map.atMaybeAssumingHash(key, hash);
				if (data.has_value()) std::forward<Func>(f)(Ref<DATA_T>(data.value()));
			});
		}

		/**
		 * Calls f with const reference to the value associated with the key if the key exists.
		 */
		template<typename K = KEY_T, typename Func>
		void maybeCallOn(const K& key, Func&& f) const RELEASE_NOEXCEPT {
			withShardOf(key, [&](const auto& map, KeyHash hash) {
				auto data = map.atMaybeAssumingHash(key, hash);
				if (data.has_value()) std::forward<Func>(f)(CRef<DATA_T>(data.value()));
			});
		}

		/**
		 * Calls f with reference to the value associated with the key.
		 * Panics if the key does not exist.
		 */
		template<typename K = KEY_T, typename Func>
		void callOn(const K& key, Func&& f) RELEASE_NOEXCEPT {
			withShardOf(key, [&](auto& map, KeyHash hash) {
				auto data = map.atMaybeAssumingHash(key, hash);
				std::forward<Func>(f)(Ref<DATA_T>(data.value()));
			});
		}

		/**
		 * Calls f with const reference to the value associated with the key.
		 * Panics if the key does not exist.
		 */
		template<typename K = KEY_T, typename Func>
		void callOn(const K& key, Func&& f) const RELEASE_NOEXCEPT {
			withShardOf(key, [&](const auto& map, KeyHash hash) {
				auto data = map.atMaybeAssumingHash(key, hash);
				std::forward<Func>(f)(CRef<DATA_T>(data.value()));
			});
		}

		/**
		 * Atomically retrieves a copy of the value associated with the given key.
		 */
		[[nodiscard]]
		DATA_T getCopy(const KEY_T& key) const RELEASE_NOEXCEPT {
			return withShardOf(key, [&](const auto& map, KeyHash hash) -> DATA_T {
				return **map.atMaybeAssumingHash(key, hash);
			});
		}

		/**
		 * Atomically retrieves a copy of the value associated with the given key.
		 * Return empty optional, if the key is not present in the map.
		 */
		[[nodiscard]]
		base::Optional<DATA_T> atMaybeCopy(const KEY_T& key) const RELEASE_NOEXCEPT {
			return withShardOf(key, [&](const auto& map, KeyHash hash) {
				return map.atMaybeCopyAssumingHash(key, hash);
			});
		}

		/**
		 * Atomically retrieves a reference to the value associated with the given key.
		 *
		 * @important Usage of the reference must be synchronized externally.
		 * For example `map.at(key) = ...` may lead to data races on `=` operator.
		 */
		[[nodiscard]]
		auto atMaybe(const KEY_T& key) RELEASE_NOEXCEPT -> base::Optional<Ref<DATA_T>> {
			return withShardOf(key, [&](auto& map, KeyHash hash) {
				return map.atMaybeAssumingHash(key, hash);
			});
		}

		auto atMaybe(const KEY_T& key) const RELEASE_NOEXCEPT -> base::Optional<CRef<DATA_T>> {
			return withShardOf(key, [&](const auto& map, KeyHash hash) {
				return map.atMaybeAssumingHash(key, hash);
			});
		}

		[[nodiscard]]
		auto at(const KEY_T& key) RELEASE_NOEXCEPT -> Ref<DATA_T> {
			return atMaybe(key).value();
		}

		/**
		 * Atomically updates the value associated with the given key.
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		void update(K&& key, D&& value) RELEASE_NOEXCEPT {
			withShardOf(key, [&](auto& map, KeyHash hash) {
				**map.atMaybeAssumingHash(key, hash) = std::forward<D>(value);
			});
		}

		[[nodiscard]]
		auto contains(const KEY_T& key) const RELEASE_NOEXCEPT {
			return withShardOf(key, [&](const auto& map, KeyHash hash) {
				return map.containsAssumingHash(key, hash);
			});
		}

		/**
		 * Atomically erases the given key->value pair from the map.
		 */
		auto erase(const KEY_T& key) RELEASE_NOEXCEPT {
			return withShardOf(key, [&](auto& map, KeyHash hash) {
				return map.eraseAssumingHash(key, hash);
			});
		}

		/**
		 * Atomically erases the key-value pair if the key exists and the predicate returns true.
		 */
		template<typename Predicate>
		bool eraseIf(const KEY_T& key, Predicate&& pred) RELEASE_NOEXCEPT {
			return withShardOf(key, [&](auto& map, KeyHash hash) -> bool {
				auto opt_ref = map.atMaybeAssumingHash(key, hash);
				if (opt_ref.has_value() && std::forward<Predicate>(pred)(opt_ref.value()))
					return map.eraseAssumingHash(key, hash);
				return false;
			});
		}

		/**
		 * Atomically extracts the given value from the map, that is:
		 * 1. Moves out the value associated with the key and returns it.
		 * 2. Erases the key->value pair from the map.
		 */
		base::Optional<DATA_T> extract(const KEY_T& key) RELEASE_NOEXCEPT {
			return withShardOf(key, [&](auto& map, KeyHash hash) -> base::Optional<DATA_T> {
				auto at_maybe = map.atMaybeAssumingHash(key, hash);
				if (!at_maybe.has_value()) {
					// data was already not present
					return base::Optional<DATA_T>{};
				}
				DATA_T value = std::move(*at_maybe.value());
				map.eraseAssumingHash(key, hash);
				return base::Optional<DATA_T>{ std::move(value) };
			});
		}

		/**
		 * Returns the number of elements currently stored in the map.
		 *
		 * @note The value is approximate under concurrent modifications: shards are counted one
		 * after another, each under its own lock.
		 * @note There is intentionally no global element counter: updating one shared atomic on
		 * every insert/erase made all cores fight over a single cache line. This is O(shards),
		 * so do not call it on hot paths.
		 */
		[[nodiscard]]
		u64 size() const noexcept {
			u64 result = 0;
			for (u64 i = 0; i < SHARD_COUNT; i++) {
				WithShardLock lock(*this, i);
				result += shards[i].map.size();
			}
			return result;
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
			using ShardsPtr = std::
				conditional_t<std::is_const_v<ValueT>, const std::vector<Shard>*, std::vector<Shard>*>;

			ShardsPtr shards_ptr;

			/// This would have to change when number of shards is not compile-time constant
			u64           shard_index;
			InnerIterator inner;

			/**
			 * If the current inner iterator has reached the end of its shard,
			 * advance to the next non-empty shard.
			 */
			void advanceToValid() {
				while (shard_index < SHARD_COUNT && inner == (*shards_ptr)[shard_index].map.end()) {
					++shard_index;
					if (shard_index < SHARD_COUNT) inner = (*shards_ptr)[shard_index].map.begin();
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
					inner = (*this->shards_ptr)[shard_index].map.begin();
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

		/**
		 * Retrieves references to all key-value pairs from the map.
		 * Locks ALL shards (WithAllShardsLock) for the duration of the copy.
		 *
		 * @warning The returned references are unguarded once this method returns, notably
		 * a concurrent erase()/extract() of a key leaves its reference dangling.
		 * Synchronize externally if needed.
		 */
		[[nodiscard]]
		std::vector<CRef<KeyValuePair>> getAllKeyValuePairs() const RELEASE_NOEXCEPT {
			std::vector<CRef<KeyValuePair>> result;
			result.reserve(size());
			std::transform(begin(), end(), std::back_inserter(result), [](const auto& pair) {
				return &pair;
			});
			return result;
		}

	private:
		/**
		 * Number of shards used in the map.
		 *
		 * @note In the future we might want to make SHARD_COUNT configurable, so it can be smaller
		 * for "small" use cases and larger where it might matter (e.g. cache of highly concurrent
		 * queries).
		 */
		constexpr static u64 SHARD_COUNT = 129;

		static_assert(SHARD_COUNT > 0, "Shard count must be positive");

		/**
		 * The shards of the map (lock + map each), created once with SHARD_COUNT elements.
		 */
		std::vector<Shard> shards;
	};
}
