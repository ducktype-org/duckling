// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <concurrent/base/collections/hash_map.hpp>
#include <concurrent/race_tester/race_tester.hpp>

#include <tester/tester.hpp>

#include <algorithm>
#include <random>
#include <set>
#include <unordered_map>

// strConcat specializations for result types used in race testing
// These must be declared before including race_tester.hpp
namespace base {
	// Specialization for Optional<u64> — specific to hashMapRaceTest
	static void strConcat(std::string& out, const Optional<u64>& opt) {
		if (opt.empty()) {
			out.append("Optional(empty)");
		} else {
			out.append("Optional(");
			strConcat(out, opt.value());
			out.append(")");
		}
	}
}

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

		TESTER_ADD_TEST(multiThreadedMoveConstructorTest<1>);
		TESTER_ADD_TEST(multiThreadedMoveConstructorTest<2>);
		TESTER_ADD_TEST(multiThreadedMoveConstructorTest<4>);

		TESTER_ADD_TEST(multiThreadedEraseTest<1>);
		TESTER_ADD_TEST(multiThreadedEraseTest<2>);
		TESTER_ADD_TEST(multiThreadedEraseTest<4>);

		TESTER_ADD_TEST(sizeTrackingTest);
		TESTER_ADD_TEST(simpleIteratorTest);
		TESTER_ADD_TEST(iteratorTest);
		TESTER_ADD_TEST(constIteratorTest);
		TESTER_ADD_TEST(multiThreadedSizeTest);

		TESTER_ADD_TEST(multiThreadedExtractTest<1>);
		TESTER_ADD_TEST(multiThreadedExtractTest<2>);
		TESTER_ADD_TEST(multiThreadedExtractTest<4>);

		TESTER_ADD_TEST(multiThreadedMaybeCallOnTest<1>);
		TESTER_ADD_TEST(multiThreadedMaybeCallOnTest<2>);
		TESTER_ADD_TEST(multiThreadedMaybeCallOnTest<4>);

		TESTER_ADD_TEST(multiThreadedCallOnTest<1>);
		TESTER_ADD_TEST(multiThreadedCallOnTest<2>);
		TESTER_ADD_TEST(multiThreadedCallOnTest<4>);

		TESTER_ADD_TEST(multiThreadedPutOrAssignTest<1>);
		TESTER_ADD_TEST(multiThreadedPutOrAssignTest<2>);
		TESTER_ADD_TEST(multiThreadedPutOrAssignTest<4>);

		TESTER_ADD_TEST(testGetAllKeyValuePairs);

		TESTER_ADD_TEST(hashMapRaceTest);
	}

protected:
	void beforeAll() override { concurrent::worker::setWorkerCount(4); }

private:
	void testGetAllKeyValuePairs() {
		concurrent::ConHashMap<int, int> map;
		for (int i = 0; i < 100; ++i) map.put(i, i);
		auto pairs = map.getAllKeyValuePairs();
		ASSERT_EQUAL(pairs.size(), 100ul);
		for (auto p: pairs) ASSERT_EQUAL(p->key, p->value);
	}

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

		ASSERT_HAS_VALUE(map.atMaybe(2));
		ASSERT_TRUE(map.atMaybe(4).empty());
		ASSERT_TRUE(map.atMaybe(5).empty());

		ASSERT_HAS_VALUE(map.atMaybeCopy(2));
		ASSERT_TRUE(map.atMaybeCopy(4).empty());
		ASSERT_TRUE(map.atMaybeCopy(5).empty());

		map.update(2, 25);
		ASSERT_TRUE(map.getCopy(2) == 25);

		auto val_ref = map.atMaybe(3).value();
		*val_ref     = 35;
		ASSERT_TRUE(map.getCopy(3) == 35);
		ASSERT_TRUE(*map.atMaybe(3).value() == 35);


		map.maybePutAndUpdate(4, 40, [](Ref<int> v) { *v += 5; });
		ASSERT_TRUE(map.getCopy(4) == 45);
		map.maybePutAndUpdate(1, 100, [](Ref<int> v) { *v += 5; });
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
				auto put_res = map.maybePut(BigObject<13>(v), BigObject<16>(v * 10));

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
					map.maybePutAndUpdate(u64(1), u64(0), [](Ref<u64> v) { *v += 10; });
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
				for (u64 j = 0; j < OPS_PER_THREAD; j++) map.update(u64(1), j * thread_count + i);
			});
		}

		for (u64 i = 0; i < thread_count; i++) threads.at(i).join();

		ASSERT_TRUE(map.contains(1));

		// we should have the last update from one of the threads:
		ASSERT_TRUE(map.getCopy(1) >= (OPS_PER_THREAD - 1) * thread_count);
	}

	template<u64 thread_count>
	void multiThreadedMoveConstructorTest() {
		constexpr u64 OPS_PER_THREAD = 100'000;

		concurrent::ConHashMap<u64, u64>                 map;
		base::Optional<concurrent::ConHashMap<u64, u64>> moved_map_opt;

		// All the threads will be writing to the map while the first thread will move-construct
		// moved_map_opt from the map in the middle of the computation.

		std::vector<std::jthread> threads;
		threads.reserve(thread_count);
		for (u64 thread_id = 0; thread_id < thread_count; thread_id++) {
			threads.emplace_back([&map, &moved_map_opt, thread_id]() {
				for (u64 j = 0; j < OPS_PER_THREAD; j++) {
					if (thread_id == 0 && j == OPS_PER_THREAD / 2) {
						// move-construct moved_map_opt from map in the middle of the computation
						moved_map_opt.emplace(std::move(map));
					}

					u64 key = j * thread_count + thread_id;
					map.put(key, key * 10);
				}
			});
		}

		for (auto& t: threads) t.join();

		// Validate state:
		ASSERT_HAS_VALUE(moved_map_opt);
		ASSERT_EQUAL(moved_map_opt->size() + map.size(), thread_count * OPS_PER_THREAD);

		for (u64 thread_id = 0; thread_id < thread_count; thread_id++) {
			for (u64 j = 0; j < OPS_PER_THREAD; j++) {
				u64 key = j * thread_count + thread_id;
				if (moved_map_opt->contains(key)) {
					ASSERT_EQUAL(moved_map_opt->getCopy(key), key * 10);
					ASSERT_TRUE(!map.contains(key));
				} else {
					ASSERT_TRUE(map.contains(key));
					ASSERT_EQUAL(map.getCopy(key), key * 10);
					ASSERT_TRUE(!moved_map_opt->contains(key));
				}
			}
		}

		// Test iteration:
		std::set<std::pair<u64, u64>> all_pairs;

		for (auto [key, value]: *moved_map_opt) {
			ASSERT_TRUE(!all_pairs.contains(std::make_pair(key, value)));
			all_pairs.emplace(key, value);
		}

		ASSERT_EQUAL(all_pairs.size(), moved_map_opt->size());

		for (auto [key, value]: map) {
			ASSERT_TRUE(!all_pairs.contains(std::make_pair(key, value)));
			all_pairs.emplace(key, value);
		}

		ASSERT_EQUAL(all_pairs.size(), thread_count * OPS_PER_THREAD);
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
			std::minstd_rand rng(static_cast<std::minstd_rand::result_type>(42 * i));

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
		map.maybePutAndUpdate(1, 0, [](Ref<int> v) { *v += 1; });
		ASSERT_EQUAL(map.size(), 4ULL);

		// maybePutAndUpdate on new key should increment size
		map.maybePutAndUpdate(5, 50, [](Ref<int> v) { *v += 1; });
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
	 * Simple single-threaded test: insert keys, then iterate and verify
	 * that iteration yields exactly the same key-value pairs.
	 */
	void simpleIteratorTest() {
		concurrent::ConHashMap<int, int> map;

		constexpr int N = 100;
		for (int i = 0; i < N; i++) map.put(i, i * 10);

		std::vector<std::pair<int, int>> collected;
		for (auto it = map.begin(); it != map.end(); ++it)
			collected.emplace_back(it->key, it->value);

		ASSERT_EQUAL(collected.size(), static_cast<usize>(N));

		std::ranges::sort(collected, [](const auto& a, const auto& b) { return a.first < b.first; });

		for (int i = 0; i < N; i++) {
			ASSERT_EQUAL(collected[static_cast<usize>(i)].first, i);
			ASSERT_EQUAL(collected[static_cast<usize>(i)].second, i * 10);
		}
	}

	/**
	 * Tests that 3 threads can concurrently iterate using mutable iterators
	 * while a 4th thread attempts to put() into the map.
	 * The put() calls will block on the spinlock until the iterators release all shards.
	 */
	void iteratorTest() {
		concurrent::ConHashMap<int, int> map;

		constexpr int N         = 200;
		constexpr int EXTRA_KEY = N + 1'000;  // keys that won't collide with existing ones

		for (int i = 0; i < N; i++) map.put(i, i * 10);

		std::array<std::vector<int>, 3> per_thread_keys;
		std::atomic<bool>               writer_done = false;

		{
			std::jthread t0([&map, &per_thread_keys]() {
				for (auto el: map) per_thread_keys[0].push_back(el.key);
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
			for (int i = 0; i < N; i++) ASSERT_EQUAL(original.at(static_cast<usize>(i)), i);
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

	template<u64 thread_count>
	void multiThreadedExtractTest() {
		constexpr u64 OPS_PER_THREAD = 10'000;
		constexpr u64 KEY_RANGE      = 5'000;

		concurrent::ConHashMap<u64, u64> map;

		// fill the map
		for (u64 i = 0; i < KEY_RANGE; i++) map.put(i, i * 10);

		// Each thread tries to extract keys [0, KEY_RANGE).
		std::vector<std::jthread> threads;
		threads.reserve(thread_count);
		std::vector<std::vector<std::pair<u64, u64>>> extracted_values(thread_count);

		for (u64 thread_id = 0; thread_id < thread_count; thread_id++) {
			threads.emplace_back([&map, &extracted_values, thread_id]() {
				for (u64 j = 0; j < OPS_PER_THREAD; j++) {
					u64  key       = (j * thread_id * 1'000'000'007) % KEY_RANGE;
					auto extracted = map.extract(key);
					if (extracted) extracted_values[thread_id].emplace_back(key, extracted.value());
				}
			});
		}
		for (auto& t: threads) t.join();

		// Verify that each key was extracted at most once and that the extracted value is correct.
		std::vector<bool> extracted_keys(KEY_RANGE, false);
		u64               total_extracted = 0;

		for (const auto& thread_values: extracted_values) {
			for (const auto& [key, value]: thread_values) {
				ASSERT_TRUE(key < KEY_RANGE);
				ASSERT_TRUE(value == key * 10);
				ASSERT_TRUE(!extracted_keys[key]);

				extracted_keys[key] = true;
				total_extracted++;
			}
		}

		// Verify that remaining keys in the map are correct and were not extracted.
		ASSERT_TRUE(total_extracted + map.size() == KEY_RANGE);
		u64 remaining_in_map = 0;
		u64 expected_size    = map.size();
		for (auto [key, value]: map) {
			ASSERT_TRUE(key < KEY_RANGE);
			ASSERT_TRUE(value == key * 10);
			ASSERT_TRUE(!extracted_keys[key]);
			remaining_in_map++;
		}
		ASSERT_EQUAL(remaining_in_map, expected_size);

		// Double check it with other map operations to ensure no extracted keys are still accessible.
		for (u64 key = 0; key < KEY_RANGE; key++) {
			if (extracted_keys[key]) {
				ASSERT_TRUE(!map.contains(key));
				ASSERT_TRUE(map.atMaybe(key).empty());
			} else {
				ASSERT_TRUE(map.contains(key));
				ASSERT_HAS_VALUE(map.atMaybe(key));
				ASSERT_TRUE(map.getCopy(key) == key * 10);
			}
		}
	}

	template<u64 thread_count>
	void multiThreadedMaybeCallOnTest() {
		constexpr u64 OPS_PER_THREAD = 10'000;
		constexpr u64 KEY_RANGE      = 5'000;

		concurrent::ConHashMap<u64, u64> map;

		// fill the map
		for (u64 i = 0; i < KEY_RANGE; i++) map.put(i, i * 10);

		// Each thread tries to maybeCallOn keys [0, KEY_RANGE * 3).
		std::vector<std::jthread> threads;
		threads.reserve(thread_count);
		std::vector<std::vector<std::pair<u64, u64>>> accessed_values(thread_count);
		std::vector<std::vector<std::pair<u64, u64>>> const_accessed_values(thread_count);

		for (u64 thread_id = 0; thread_id < thread_count; thread_id++) {
			threads.emplace_back([&map, &accessed_values, &const_accessed_values, thread_id]() {
				for (u64 j = 0; j < OPS_PER_THREAD; j++) {
					u64 key = (j * thread_id * 1'000'000'007) % (KEY_RANGE * 3);

					// we also add to map here in range [KEY_RANGE, KEY_RANGE*2) to test that
					// maybeCallOn can see new keys added by other threads:
					auto add_key = (key % KEY_RANGE) + KEY_RANGE;
					map.maybePut(add_key, add_key * 100);

					map.maybeCallOn(key, [&accessed_values, thread_id, key](Ref<u64> value_ref) {
						accessed_values[thread_id].emplace_back(key, *value_ref);
						if (*value_ref == key * 10)
							*value_ref += 1;  // if it's an original value, update it
					});

					// also test const variant of maybeCallOn:
					const auto& cmap  = map;
					auto        c_key = (key * 1'000'000'009) % (KEY_RANGE * 3);
					cmap.maybeCallOn(
						c_key,
						[&const_accessed_values, thread_id, c_key](CRef<u64> value_ref) {
							const_accessed_values[thread_id].emplace_back(c_key, *value_ref);
						}
					);
				}
			});
		}
		for (auto& t: threads) t.join();

		// Verify that accessed keys are correct and that original values were updated.
		for (const auto& thread_values: accessed_values) {
			for (const auto& [key, value]: thread_values) {
				if (key < KEY_RANGE) {
					// original keys should have been updated to key*10 + 1
					ASSERT_TRUE(value == key * 10 || value == key * 10 + 1);
					ASSERT_TRUE(map.contains(key));
					ASSERT_EQUAL(map.getCopy(key), key * 10 + 1);

				} else if (key < KEY_RANGE * 2) {
					// maybePut should have added keys in [KEY_RANGE, KEY_RANGE*2) with value = key*100
					ASSERT_TRUE(value == key * 100);
					ASSERT_TRUE(map.contains(key));
					ASSERT_EQUAL(map.getCopy(key), key * 100);
				} else {
					// keys >= KEY_RANGE*2 should not be present
					fail("Accessed key that should not be present: " + std::to_string(key));
				}
			}
		}

		// Verify that const maybeCallOn accessed the keys with correct values.
		for (const auto& thread_values: const_accessed_values) {
			for (const auto& [key, value]: thread_values) {
				if (key < KEY_RANGE) {
					// original keys should have been accessed with value = key * 10 or key * 10 + 1
					ASSERT_TRUE(value == key * 10 or value == key * 10 + 1);
				} else if (key < KEY_RANGE * 2) {
					// maybePut should have added keys in [KEY_RANGE, KEY_RANGE*2) with value = key*100
					ASSERT_TRUE(value == key * 100);
				} else {
					// keys >= KEY_RANGE*2 should not be present
					fail("Accessed const key that should not be present: " + std::to_string(key));
				}
			}
		}
	}

	template<u64 thread_count>
	void multiThreadedCallOnTest() {
		constexpr u64 OPS_PER_THREAD = 10'000;
		constexpr u64 KEY_RANGE      = 5'000;

		concurrent::ConHashMap<u64, u64> map;

		// fill the map
		for (u64 i = 0; i < KEY_RANGE; i++) map.put(i, i * 10);

		// Each thread tries to callOn keys [0, KEY_RANGE).
		std::vector<std::jthread> threads;
		threads.reserve(thread_count);
		std::vector<std::vector<std::pair<u64, u64>>> accessed_values(thread_count);

		for (u64 thread_id = 0; thread_id < thread_count; thread_id++) {
			threads.emplace_back([&map, &accessed_values, thread_id]() {
				for (u64 j = 0; j < OPS_PER_THREAD; j++) {
					u64 key = (j * thread_id * 1'000'000'007) % KEY_RANGE;

					map.callOn(key, [&accessed_values, thread_id, key](Ref<u64> value_ref) {
						accessed_values[thread_id].emplace_back(key, *value_ref);
						if (*value_ref == key * 10)
							*value_ref += 1;  // if it's an original value, update it
					});
				}
			});
		}
		for (auto& t: threads) t.join();

		// Verify that accessed keys are correct and that original values were updated.
		for (const auto& thread_values: accessed_values) {
			for (const auto& [key, value]: thread_values) {
				ASSERT_TRUE(key < KEY_RANGE);
				ASSERT_TRUE(value == key * 10 || value == key * 10 + 1);
				ASSERT_TRUE(map.contains(key));
				ASSERT_EQUAL(map.getCopy(key), key * 10 + 1);
			}
		}
	}

	/**
	 * Tests that putOrAssign works correctly under concurrent access.
	 * Multiple threads call putOrAssign on overlapping keys;
	 * each final value must equal the last write for that key.
	 */
	template<u64 thread_count>
	void multiThreadedPutOrAssignTest() {
		constexpr u64 OPS_PER_THREAD = 10'000;
		constexpr u64 KEY_RANGE      = 500;

		concurrent::ConHashMap<u64, u64> map;

		// Each thread writes keys [0, KEY_RANGE) with value = thread_id * OPS_PER_THREAD + j.
		// putOrAssign must insert or overwrite atomically.
		std::vector<std::jthread> threads;
		threads.reserve(thread_count);

		for (u64 i = 0; i < thread_count; i++) {
			threads.emplace_back([&map, i]() {
				for (u64 j = 0; j < OPS_PER_THREAD; j++) {
					u64 key   = j % KEY_RANGE;
					u64 value = i * OPS_PER_THREAD + j;
					map.putOrAssign(key, value);
				}
			});
		}

		for (auto& t: threads) t.join();

		// Every key in [0, KEY_RANGE) must be present.
		ASSERT_EQUAL(map.size(), KEY_RANGE);

		for (u64 k = 0; k < KEY_RANGE; k++) {
			ASSERT_TRUE(map.contains(k));
			// Value was set by some thread; just verify it is within the valid range.
			u64 v = map.getCopy(k);
			ASSERT_TRUE(v < thread_count * OPS_PER_THREAD);
		}
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

	/**
	 * Race test for concurrent::ConHashMap using the RaceTester framework.
	 * Tests maybePut, atMaybeCopy, and erase operations for linearizability.
	 */
	void hashMapRaceTest() {
		using Key   = u64;
		using Value = u64;

		// Common interface for both implementations
		class HashMapInterface {
		public:
			[[nodiscard]]
			virtual bool maybePut(const Key& key, const Value& value)
				= 0;
			[[nodiscard]]
			virtual base::Optional<Value> atMaybeCopy(const Key& key) const
				= 0;
			[[nodiscard]]
			virtual bool erase(const Key& key)
				= 0;

			virtual ~HashMapInterface() = default;
		};

		// Wrapper for ConHashMap
		class ConHashMapWrapper: public HashMapInterface {
			concurrent::ConHashMap<Key, Value> map;

		public:
			bool maybePut(const Key& key, const Value& value) override {
				auto result = map.maybePut(key, value);
				return result != nullptr;
			}

			base::Optional<Value> atMaybeCopy(const Key& key) const override {
				return map.atMaybeCopy(key);
			}

			bool erase(const Key& key) override { return map.erase(key); }
		};

		// Wrapper for std::unordered_map
		class SequentialHashMapWrapper: public HashMapInterface {
			std::unordered_map<Key, Value> map;

		public:
			bool maybePut(const Key& key, const Value& value) override {
				auto [_, inserted] = map.try_emplace(key, value);
				return inserted;
			}

			base::Optional<Value> atMaybeCopy(const Key& key) const override {
				auto it = map.find(key);
				if (it == map.end()) return base::Optional<Value>{};
				return base::Optional{ it->second };
			}

			bool erase(const Key& key) override { return map.erase(key) > 0; }
		};

		// Run the race test multiple times for confidence
		const usize reps           = 100;
		const usize worker_count   = 2;
		const usize ops_per_thread = 12;

		for (usize rep = 0; rep < reps; ++rep) {
			std::cerr << "\rRep: " << rep + 1 << " / " << reps;
			auto tested     = makeBox<ConHashMapWrapper>();
			auto sequential = makeBox<SequentialHashMapWrapper>();

			using RaceTester = concurrent::tester::RaceTester<
				HashMapInterface,
				ConHashMapWrapper,
				SequentialHashMapWrapper,
				bool,
				base::Optional<Value>>;

			RaceTester race_tester{ tested.refMut(), sequential.ref() };

			// Worker function that performs random operations
			const std::function<void(u32, RaceTester::Executor_)> worker
				= [](u32, RaceTester::Executor_ executor) {
					  std::minstd_rand rng(std::random_device{}());

					  for (usize i = 0; i < ops_per_thread; ++i) {
						  // Use only one key because we want to test races on a single shard.
					      // Running the test on multiple independent shards grows the search space.
						  Key   key   = 1;
						  Value value = rng() % 4;

						  double op = static_cast<double>(rng() % 100) / 100.0;

						  if (op < 0.30) {
							  // maybePut operation (30%)
							  executor.execute(
								  base::strConcat("maybePut(", key, ", ", value, ")"),
								  [key, value](Ref<HashMapInterface> map) {
									  return map->maybePut(key, value);
								  }
							  );
						  } else if (op < 0.65) {
							  // atMaybeCopy operation (35%)
							  executor.execute(
								  base::strConcat("atMaybeCopy(", key, ")"),
								  [key](Ref<HashMapInterface> map) { return map->atMaybeCopy(key); }
							  );
						  } else {
							  // erase operation (35%)
							  executor.execute(
								  base::strConcat("erase(", key, ")"),
								  [key](Ref<HashMapInterface> map) { return map->erase(key); }
							  );
						  }
					  }
				  };

			// Run the test and check linearizability
			assertTrue(
				race_tester.runAndCheck(worker_count, worker), "ConHashMap should be linearizable."
			);
		}
		std::cerr << "\n";
	}
};

TESTER_COMMON_MAIN("/src/common/concurrent/tests/");
