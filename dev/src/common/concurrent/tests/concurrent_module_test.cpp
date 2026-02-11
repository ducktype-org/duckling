#include <concurrent/base/collections/hash_map.hpp>

#include <tester/tester.hpp>

#include <algorithm>
#include <random>

template<usize Size>
struct BigObject final {
	u64 data[Size] = {};  // NOLINT

	BigObject(u64 a): data{ a } {}

	bool operator==(const BigObject& other) const {
		for (usize i = 0; i < Size; i++)
			if (data[i] != other.data[i]) return false;  // NOLINT
		return true;
	}
};

template<usize N>
struct std::hash<BigObject<N>> {
	size_t operator()(const BigObject<N>& obj) const noexcept { return obj.data[0]; }
};

class ConcurrentTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ConcurrentTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		concurrent::worker::setWorkerCount(4);

		TESTER_ADD_TEST(hashMapSingleThreadTest1);

		TESTER_ADD_TEST(singleThreadedRandomTest<0>);
		TESTER_ADD_TEST(singleThreadedRandomTest<2>);
		TESTER_ADD_TEST(singleThreadedRandomTest<10>);
		TESTER_ADD_TEST(singleThreadedRandomTest<100>);
		TESTER_ADD_TEST(singleThreadedRandomTest<10'000>);
		TESTER_ADD_TEST(singleThreadedRandomTest<100'000>);

		TESTER_ADD_TEST(multiThreadedSimpleTest1<1>);
		TESTER_ADD_TEST(multiThreadedSimpleTest1<2>);
		TESTER_ADD_TEST(multiThreadedSimpleTest1<4>);

		TESTER_ADD_TEST(multiThreadedSimpleTest2<1>);
		TESTER_ADD_TEST(multiThreadedSimpleTest2<2>);
		TESTER_ADD_TEST(multiThreadedSimpleTest2<4>);

		TESTER_ADD_TEST(multiThreadedSimpleTest3<1>);
		TESTER_ADD_TEST(multiThreadedSimpleTest3<2>);
		TESTER_ADD_TEST(multiThreadedSimpleTest3<4>);

		TESTER_ADD_TEST(multiThreadedEraseTest<1>);
		TESTER_ADD_TEST(multiThreadedEraseTest<2>);
		TESTER_ADD_TEST(multiThreadedEraseTest<4>);

		TESTER_ADD_TEST(sizeTrackingTest);
		TESTER_ADD_TEST(iteratorTest);
		TESTER_ADD_TEST(constIteratorTest);
		TESTER_ADD_TEST(multiThreadedSizeTest);
	}

private:
	/**
	 * Simple single-threaded test of concurrent::ConHashMap.
	 */
	void hashMapSingleThreadTest1() {
		concurrent::ConHashMap<int, int> map;

		map.put(1, 10);
		map.put(2, 20);
		map.put(3, 30);
		ASSERT_TRUE(map.getCopy(1) == 10);
		ASSERT_TRUE(map.getCopy(2) == 20);
		ASSERT_TRUE(map.getCopy(3) == 30);

		ASSERT_TRUE(map.atMaybeCopy(1).value() == 10);
		ASSERT_TRUE(map.atMaybeCopy(2).value() == 20);
		ASSERT_TRUE(map.atMaybeCopy(3).value() == 30);

		ASSERT_TRUE(map.contains(1));
		ASSERT_TRUE(map.contains(2));
		ASSERT_TRUE(map.contains(3));
		ASSERT_TRUE(!map.contains(4));

		ASSERT_TRUE(map.atMaybe(2).has_value());
		ASSERT_TRUE(map.atMaybe(4).empty());
		ASSERT_TRUE(map.atMaybe(5).empty());

		ASSERT_TRUE(map.atMaybeCopy(2).has_value());
		ASSERT_TRUE(map.atMaybeCopy(4).empty());
		ASSERT_TRUE(map.atMaybeCopy(5).empty());

		map.update(2, 25);
		ASSERT_TRUE(map.getCopy(2) == 25);

		auto val_ref = map.atMaybe(3).value();
		*val_ref     = 35;
		ASSERT_TRUE(map.getCopy(3) == 35);
		ASSERT_TRUE(*map.atMaybe(3).value() == 35);


		map.maybePutAndUpdate(4, 40, [](int& v) { v += 5; });
		ASSERT_TRUE(map.getCopy(4) == 45);
		map.maybePutAndUpdate(1, 100, [](int& v) { v += 5; });
		ASSERT_TRUE(map.getCopy(1) == 15);

		map.maybePut(5, 50);
		ASSERT_TRUE(map.getCopy(5) == 50);
		map.maybePut(5, 500);
		ASSERT_TRUE(map.getCopy(5) == 50);

		auto erased = map.erase(2);
		ASSERT_TRUE(erased);
		ASSERT_TRUE(!map.contains(2));

		erased = map.erase(2);
		ASSERT_TRUE(!erased);

		ASSERT_TRUE(map.contains(1));
		ASSERT_TRUE(!map.contains(2));
		ASSERT_TRUE(map.contains(3));
		ASSERT_TRUE(map.contains(4));
	}

	/**
	 * Single-threaded random test of concurrent::ConHashMap adapted from tests of maps from base.
	 * It tests correctness of maybePut and atMaybe methods by comparing certain aggregated results
	 * with the results obtained when using std::unordered_map.
	 */
	template<u64 count>
	void singleThreadedRandomTest() {
		u64 base_result = 0;
		{
			std::minstd_rand rng(42);

			concurrent::ConHashMap<BigObject<13>, BigObject<16>> map;

			for (u64 i = 0; i < count; i++) {
				auto v       = rng() % 1'000'000;
				auto put_res = map.maybePut(v, v * 10);

				if (put_res != nullptr) {
					ASSERT_EQUAL(put_res->key, v);
					ASSERT_EQUAL(put_res->value, v * 10);
				}

				for (int j = 0; j < 3; j++) {
					auto new_v     = rng() % 1'000'000;
					auto maybe_val = map.atMaybe(new_v);

					ASSERT_EQUAL(maybe_val.has_value(), map.contains(new_v));

					if (maybe_val.has_value()) {
						auto val = **maybe_val;
						base_result += val.data[0];
					}
				}
			}
		}

		u64 std_result = 0;
		{
			std::minstd_rand rng(42);

			std::unordered_map<BigObject<13>, BigObject<16>> map;

			for (u64 i = 0; i < count; i++) {
				auto v = rng() % 1'000'000;
				map.emplace(BigObject<13>(v), BigObject<16>(v * 10));

				for (int j = 0; j < 3; j++) {
					auto it = map.find(BigObject<13>(rng() % 1'000'000));
					if (it != map.end()) {
						auto val = it->second;
						std_result += val.data[0];
					}
				}
			}
		}

		ASSERT_EQUAL(base_result, std_result);
	}

	/**
	 * Tests multi-threaded writes to the concurrent::ConHashMap on different keys.
	 */
	template<u64 thread_count>
	void multiThreadedSimpleTest1() {
		constexpr u64 LOOP_OPS_PER_THREAD = 10'000;

		concurrent::ConHashMap<u64, u64> map;

		std::vector<std::jthread> threads;
		threads.reserve(thread_count);

		for (u64 i = 0; i < thread_count; i++) {
			threads.emplace_back([&map, i]() {
				for (u64 j = 0; j < LOOP_OPS_PER_THREAD; j++) {
					u64 key = j * thread_count + i;
					map.put(key, key * 10);
				}
			});
		}

		for (u64 i = 0; i < thread_count; i++) threads.at(i).join();

		for (u64 i = 0; i < thread_count; i++) {
			for (u64 j = 0; j < LOOP_OPS_PER_THREAD; j++) {
				u64 key = j * thread_count + i;
				ASSERT_TRUE(map.contains(key));
				ASSERT_EQUAL(map.getCopy(key), key * 10);
			}
		}
	}

	/**
	 * Tests multi-threaded writes to the concurrent::ConHashMap on the same key using
	 * maybePutAndUpdate.
	 */
	template<u64 thread_count>
	void multiThreadedSimpleTest2() {
		constexpr u64 OPS_PER_THREAD = 10'000;

		concurrent::ConHashMap<u64, u64> map;

		std::vector<std::jthread> threads;
		threads.reserve(thread_count);

		for (u64 i = 0; i < thread_count; i++) {
			threads.emplace_back([&map]() {
				for (u64 j = 0; j < OPS_PER_THREAD; j++)
					map.maybePutAndUpdate(1ULL, 0ULL, [](u64& v) { v += 10; });
			});
		}

		for (u64 i = 0; i < thread_count; i++) threads.at(i).join();

		ASSERT_TRUE(map.contains(1));
		ASSERT_EQUAL(map.getCopy(1), thread_count * OPS_PER_THREAD * 10);
	}

	/**
	 * Tests multi-threaded writes to the concurrent::ConHashMap on the same key using update method.
	 */
	template<u64 thread_count>
	void multiThreadedSimpleTest3() {
		constexpr u64 OPS_PER_THREAD = 10'000;

		concurrent::ConHashMap<u64, u64> map;
		map.put(u64(1), u64(0));

		std::vector<std::jthread> threads;
		threads.reserve(thread_count);

		for (u64 i = 0; i < thread_count; i++) {
			threads.emplace_back([&map, i]() {
				for (u64 j = 0; j < OPS_PER_THREAD; j++) map.update(1, j * thread_count + i);
			});
		}

		for (u64 i = 0; i < thread_count; i++) threads.at(i).join();

		ASSERT_TRUE(map.contains(1));

		// we should have the last update from one of the threads:
		ASSERT_TRUE(map.getCopy(1) >= (OPS_PER_THREAD - 1) * thread_count);
	}

	/**
	 * Tests multi-threaded erases to the concurrent::ConHashMap on random keys.
	 */
	template<u64 thread_count>
	void multiThreadedEraseTest() {
		constexpr u64 OPS_PER_THREAD = 10'000;
		constexpr u64 ELEMENTS_COUNT = thread_count * OPS_PER_THREAD;

		concurrent::ConHashMap<u64, u64> map;

		// prepopulate the map
		for (u64 i = 0; i < ELEMENTS_COUNT; i++) map.put(i, i * 10);

		std::vector<std::jthread> threads;
		std::atomic<u64>          erase_count = 0;

		threads.reserve(thread_count);
		for (u64 i = 0; i < thread_count; i++) {
			std::minstd_rand rng(42 * i);

			threads.emplace_back([&map, &erase_count, rng]() mutable {
				for (u64 j = 0; j < OPS_PER_THREAD; j++) {
					u64 key = rng() % ELEMENTS_COUNT;

					auto was_erased = map.erase(key);
					if (was_erased) erase_count++;
				}
			});
		}

		for (u64 i = 0; i < thread_count; i++) threads.at(i).join();

		u64 element_count_after_erase = 0;
		for (u64 i = 0; i < ELEMENTS_COUNT; i++)
			if (map.contains(i)) element_count_after_erase++;

		message(base::strConcat("Erased elements: ", erase_count.load(), " / ", ELEMENTS_COUNT, "\n")
		);

		ASSERT_TRUE(ELEMENTS_COUNT - erase_count == element_count_after_erase);
	}

	/**
	 * Tests that size() correctly tracks insertions and erasures in a single thread.
	 */
	void sizeTrackingTest() {
		concurrent::ConHashMap<int, int> map;

		ASSERT_EQUAL(map.size(), 0ULL);

		map.put(1, 10);
		map.put(2, 20);
		map.put(3, 30);
		ASSERT_EQUAL(map.size(), 3ULL);

		// maybePut on existing key should not change size
		map.maybePut(2, 999);
		ASSERT_EQUAL(map.size(), 3ULL);

		// maybePut on new key should increment size
		map.maybePut(4, 40);
		ASSERT_EQUAL(map.size(), 4ULL);

		// maybePutAndUpdate on existing key should not change size
		map.maybePutAndUpdate(1, 0, [](int& v) { v += 1; });
		ASSERT_EQUAL(map.size(), 4ULL);

		// maybePutAndUpdate on new key should increment size
		map.maybePutAndUpdate(5, 50, [](int& v) { v += 1; });
		ASSERT_EQUAL(map.size(), 5ULL);

		// erase existing key should decrement size
		map.erase(3);
		ASSERT_EQUAL(map.size(), 4ULL);

		// erase non-existing key should not change size
		map.erase(3);
		ASSERT_EQUAL(map.size(), 4ULL);

		map.erase(1);
		map.erase(2);
		map.erase(4);
		map.erase(5);
		ASSERT_EQUAL(map.size(), 0ULL);
	}

	/**
	 * Tests that 3 threads can concurrently iterate using mutable iterators
	 * while a 4th thread attempts to put() into the map.
	 * The put() calls will block on the spinlock until the iterators release all shards.
	 */
	void iteratorTest() {
		concurrent::ConHashMap<int, int> map;

		constexpr int N         = 200;
		constexpr int EXTRA_KEY = N + 1000;  // keys that won't collide with existing ones

		for (int i = 0; i < N; i++) map.put(i, i * 10);

		std::array<std::vector<int>, 3> per_thread_keys;
		std::atomic<bool>               writer_done = false;

		{
			std::jthread t0([&map, &per_thread_keys]() {
				for (auto it = map.begin(); it != map.end(); ++it)
					per_thread_keys[0].push_back(it->key);
			});
			std::jthread t1([&map, &per_thread_keys]() {
				for (auto it = map.begin(); it != map.end(); ++it)
					per_thread_keys[1].push_back(it->key);
			});
			std::jthread t2([&map, &per_thread_keys]() {
				for (auto it = map.begin(); it != map.end(); ++it)
					per_thread_keys[2].push_back(it->key);
			});
			// Writer thread: tries to put while iterators hold all shard locks
			std::jthread writer([&map, &writer_done]() {
				for (int i = 0; i < 50; i++) map.put(EXTRA_KEY + i, i);
				writer_done.store(true);
			});
		}

		// Writer must have completed (was blocked, then released)
		ASSERT_TRUE(writer_done.load());

		// All extra keys should be present
		for (int i = 0; i < 50; i++) {
			ASSERT_TRUE(map.contains(EXTRA_KEY + i));
			ASSERT_EQUAL(map.getCopy(EXTRA_KEY + i), i);
		}

		// Each iterator should have seen at least the original N elements
		for (auto& keys: per_thread_keys) {
			std::ranges::sort(keys);
			// The iterator might also see some of the writer's keys (if the writer
			// sneaked in between iterations), but it must see all original N keys.
			ASSERT_TRUE(keys.size() >= static_cast<usize>(N));
			// Verify all original keys [0, N) are present
			std::vector<int> original;
			for (int k: keys)
				if (k < N) original.push_back(k);
			ASSERT_EQUAL(original.size(), static_cast<usize>(N));
			for (int i = 0; i < N; i++) ASSERT_EQUAL(original[static_cast<usize>(i)], i);
		}
	}

	/**
	 * Tests that 3 threads can concurrently iterate using const iterators without crashing.
	 */
	void constIteratorTest() {
		concurrent::ConHashMap<int, int> map;

		constexpr int N = 200;
		for (int i = 0; i < N; i++) map.put(i, i * 10);

		const auto& cmap = map;

		std::array<std::atomic<int>, 3> sums = {};

		{
			std::jthread t0([&cmap, &sums]() {
				int s = 0;
				for (auto it = cmap.begin(); it != cmap.end(); ++it) s += it->value;
				sums[0].store(s);
			});
			std::jthread t1([&cmap, &sums]() {
				int s = 0;
				for (auto it = cmap.begin(); it != cmap.end(); ++it) s += it->value;
				sums[1].store(s);
			});
			std::jthread t2([&cmap, &sums]() {
				int s = 0;
				for (auto it = cmap.begin(); it != cmap.end(); ++it) s += it->value;
				sums[2].store(s);
			});
		}

		int expected = 0;
		for (int i = 0; i < N; i++) expected += i * 10;

		for (auto& s: sums) ASSERT_EQUAL(s.load(), expected);
	}

	/**
	 * Tests that size() remains consistent under concurrent put + erase.
	 */
	void multiThreadedSizeTest() {
		constexpr u64 ELEMENTS = 5'000;

		concurrent::ConHashMap<u64, u64> map;

		// Phase 1: concurrent puts on disjoint keys from 2 threads
		{
			std::jthread t1([&map]() {
				for (u64 i = 0; i < ELEMENTS; i++) map.put(i * 2, i);
			});
			std::jthread t2([&map]() {
				for (u64 i = 0; i < ELEMENTS; i++) map.put(i * 2 + 1, i);
			});
		}
		ASSERT_EQUAL(map.size(), ELEMENTS * 2);

		// Phase 2: concurrent erases on disjoint keys from 2 threads
		{
			std::jthread t1([&map]() {
				for (u64 i = 0; i < ELEMENTS; i++) map.erase(i * 2);
			});
			std::jthread t2([&map]() {
				for (u64 i = 0; i < ELEMENTS; i++) map.erase(i * 2 + 1);
			});
		}
		ASSERT_EQUAL(map.size(), 0ULL);
	}
};

TESTER_COMMON_MAIN("/src/common/concurrent/tests/");
