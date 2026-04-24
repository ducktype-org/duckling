#pragma once

#include <concurrent/base/locks/atomic_flag_spinlock.hpp>
#include <concurrent/module_flags/worker_count.hpp>

#include <base/collections/stable_hashmap.hpp>

#include <atomic>
#include <concepts>
#include <type_traits>

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
		u64 ALLOCATOR_BLOCK_SIZE = 4'096, 
		u64 SHARD_COUNT          = 129,
		usize  INITIAL_BUCKETS   = 32>
	class ConHashMap final {
		using HashMapType = base::StableHashMap<KEY_T, DATA_T, HASH_T, ALLOCATOR_BLOCK_SIZE, INITIAL_BUCKETS>;

		using KeyHash = u64;

		[[nodiscard]]
		constexpr u64 keyToShard(const KEY_T& key) const
			noexcept(::base::IS_BUILD_TYPE_RELEASE && noexcept(HASH_T{}(key))) {
			u64 hash = HASH_T{}(key);

			u64 result = hash % SHARD_COUNT;
			CORE_ASSERT(result < SHARD_COUNT, "Shard index out of bounds");

			return result;
		}

		/**
		 * Asserts that the number of shards and their corresponding mutexes match.
		 * This fails when someone accidentally pushes a new shard to the shards vector.
		 */
		void assertCorrectShardsSize() const {
			CORE_ASSERT(
				SHARD_COUNT == shard_mutexes.size() and SHARD_COUNT == shards.size(),
				"Shard count mismatch"
			);
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
				self.assertCorrectShardsSize();
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
				self.assertCorrectShardsSize();
			}

			~WithAllShardsLock() noexcept {
				for (u64 i = 0; i < SHARD_COUNT; i++) self.shard_mutexes[i]->unlock();
			}

			WithAllShardsLock(const WithAllShardsLock&) = delete;
			WithAllShardsLock(WithAllShardsLock&&)      = delete;
		};


	public:
		using KeyValuePair = typename HashMapType::KeyValuePair;

		ConHashMap(): shards(SHARD_COUNT) {
			for (u64 i = 0; i < SHARD_COUNT; i++)
				shard_mutexes.emplace_back(makeBox<concurrent::AtomicFlagSpinlock>());
		}

		ConHashMap(const ConHashMap&) = delete;

		/**
		 * Move constructor.
		 * After this operation, the @p source map is left in an empty but valid state.
		 *
		 * Underneath this performs the following:
		 * * It moves the content of each shard by transferring the ownership,
		 *   i.e. the shards remain in the same place in memory, and their constructors are
		 *   not called.
		 * * It generates new locks for the new map. This way if other threads try to access the
		 *   source map during the move, they will be properly synchronized and will not
		 *   cause data races or access to invalid memory.
		 *
		 * @note This operation is thread safe, but beware that moves perform a large lock on the
		 * map, and may in general be bug prone when done accidentally.
		 */
		ConHashMap(ConHashMap&& source) noexcept: ConHashMap() {
			// No one should access the source map during this operation
			WithAllShardsLock lock(source);

			// Transfer ownership of each shard's content to the new map.
			// Note that locks are initialized in the constructor initializer list.
			// Linter wants the following line to be placed in init-list. We can't do that, since
			// we need to lock the source map first.
			shards = std::move(source.shards);  // NOLINT
			// elements_count.store(source.elements_count.load());

			// Leave the source map in an empty but valid state
			source.shards.clear();
			source.shards.resize(SHARD_COUNT);
			// source.elements_count.store(0);
		}

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
			WithShardLock lock(*this, keyToShard(key));
			auto          result
				= shards[lock.shard_index].put(std::forward<K>(key), std::forward<D>(value));
			// elements_count.fetch_add(1, std::memory_order_relaxed);
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
		template<typename K, typename D = DATA_T>
		requires std::same_as<std::remove_cvref_t<K>, KEY_T>
		MRef<KeyValuePair> maybePut(K&& key, D&& value) RELEASE_NOEXCEPT {
			WithShardLock lock(*this, keyToShard(key));

			auto result
				= shards.at(lock.shard_index).maybePut(std::forward<K>(key), std::forward<D>(value));
			// if (result != nullptr) elements_count.fetch_add(1, std::memory_order_relaxed);
			return result;
		}

		/**
		 * Inserts key->value into the container if key does not exist.
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		void putOrAssign(const K& key, D&& value) RELEASE_NOEXCEPT {
			WithShardLock lock(*this, keyToShard(key));

			if (shards[lock.shard_index].contains(key)) {
				shards[lock.shard_index][key] = std::forward<D>(value);
				return;
			}
			shards[lock.shard_index].maybePut(key, std::forward<D>(value));
			// elements_count.fetch_add(1, std::memory_order_relaxed);
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
			WithShardLock lock(*this, keyToShard(key));

			auto inserted = shards[lock.shard_index].maybePutAndUpdate(
				std::forward<K>(key), std::forward<D>(value), std::forward<Func>(f)
			);
			// if (inserted != nullptr) elements_count.fetch_add(1, std::memory_order_relaxed);
			return inserted;
		}

		/**
		 * Calls f with reference to the value associated with the key if the key exists.
		 */
		template<typename K = KEY_T, typename Func>
		void maybeCallOn(const K& key, Func&& f) RELEASE_NOEXCEPT {
			WithShardLock lock(*this, keyToShard(key));

			auto data = shards[lock.shard_index].atMaybe(key);
			if (data.has_value()) std::forward<Func>(f)(Ref<DATA_T>(data.value()));
		}

		/**
		 * Calls f with const reference to the value associated with the key if the key exists.
		 */
		template<typename K = KEY_T, typename Func>
		void maybeCallOn(const K& key, Func&& f) const RELEASE_NOEXCEPT {
			WithShardLock lock(*this, keyToShard(key));

			auto data = shards[lock.shard_index].atMaybe(key);
			if (data.has_value()) std::forward<Func>(f)(CRef<DATA_T>(data.value()));
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
		auto atMaybe(const KEY_T& key) RELEASE_NOEXCEPT -> base::Optional<Ref<DATA_T>> {
			WithShardLock lock(*this, keyToShard(key));
			return shards[lock.shard_index].atMaybe(key);
		}

		auto atMaybe(const KEY_T& key) const RELEASE_NOEXCEPT -> base::Optional<CRef<DATA_T>> {
			WithShardLock lock(*this, keyToShard(key));
			return shards[lock.shard_index].atMaybe(key);
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
			WithShardLock lock(*this, keyToShard(key));
			shards[lock.shard_index][std::forward<K>(key)] = std::forward<D>(value);
		}

		[[nodiscard]]
		auto contains(const KEY_T& key) const RELEASE_NOEXCEPT {
			WithShardLock lock(*this, keyToShard(key));
			return shards[lock.shard_index].contains(key);
		}

		/**
		 * Atomically erases the given key->value pair from the map.
		 */
		auto erase(const KEY_T& key) RELEASE_NOEXCEPT {
			WithShardLock lock(*this, keyToShard(key));
			bool          erased = shards[lock.shard_index].erase(key);
			// if (erased) elements_count.fetch_sub(1, std::memory_order_relaxed);
			return erased;
		}

		/**
		 * Atomically erases the key-value pair if the key exists and the predicate returns true.
		 */
		template<typename Predicate>
		bool eraseIf(const KEY_T& key, Predicate&& pred) RELEASE_NOEXCEPT {
			WithShardLock lock(*this, keyToShard(key));
			auto&         shard = shards[lock.shard_index];

			auto opt_ref = shard.atMaybe(key);
			if (opt_ref.has_value()) {
				if (std::forward<Predicate>(pred)(opt_ref.value())) {
					if (shard.erase(key)) {
						// elements_count.fetch_sub(1, std::memory_order_relaxed);
						return true;
					}
				}
			}
			return false;
		}

		/**
		 * Atomically extracts the given value from the map, that is:
		 * 1. Moves out the value associated with the key and returns it.
		 * 2. Erases the key->value pair from the map.
		 */
		base::Optional<DATA_T> extract(const KEY_T& key) RELEASE_NOEXCEPT {
			WithShardLock lock(*this, keyToShard(key));

			auto at_maybe = shards[lock.shard_index].atMaybe(key);
			if (!at_maybe.has_value()) {
				// data was already not present
				return base::Optional<DATA_T>{};
			} else {
				DATA_T value = std::move(*at_maybe.value());
				shards[lock.shard_index].erase(key);
				// elements_count.fetch_sub(1, std::memory_order_relaxed);
				return base::Optional<DATA_T>{ std::move(value) };
			}
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
			// return elements_count.load(std::memory_order_seq_cst);
			u64 total = 0;
			for (u64 i = 0; i < SHARD_COUNT; i++) {
				WithShardLock lock(*this, i);
				total += shards[i].size();
			}
			return total;
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

		/**
		 * Retrieves all key-value pairs from the map.
		 * Locks WithAllShardsLock underneath to ensure thread safety,
		 * but locks each shard only for the time needed to copy its elements,
		 * so it can see elements added during the call, but not necessarily all of them.
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
		 * @important In the current implementation GCD(SHARD_COUNT, 2) must be 1.
		 * Otherwise, the distribution of keys over buckets in individual shards is highly
		 * non-uniform, as the modulus used to select the buckets are the powers of two.
		 *
		 * @note In the future we might want to make SHARD_COUNT configurable, so it can be smaller
		 * for "small" use cases and larger where it might matter (e.g. cache of highly concurrent
		 * queries).
		 */
		// constexpr static u64 SHARD_COUNT = 129;

		static_assert(SHARD_COUNT > 0, "Shard count must be positive");
		static_assert(SHARD_COUNT % 2 == 1, "Shard count must be odd to ensure uniform distribution of keys");

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
		// std::atomic<u64> elements_count{ 0 };
	};

}
