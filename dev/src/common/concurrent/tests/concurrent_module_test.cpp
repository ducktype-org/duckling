#include <concurrent/collections/hash_map.hpp>

#include <tester/tester.hpp>

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
		concurrent::setWorkerCount(4);

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

		ASSERT_TRUE(map.contains(1));
		ASSERT_TRUE(map.contains(2));
		ASSERT_TRUE(map.contains(3));
		ASSERT_TRUE(!map.contains(4));

		ASSERT_TRUE(map.atMaybe(2).has_value());
		ASSERT_TRUE(map.atMaybe(4).empty());
		ASSERT_TRUE(map.atMaybe(5).empty());


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
	}

	/**
	 * Single-threaded random test of concurrent::ConHashMap adapted from tests of maps from base.
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
		constexpr u64 LOOK_OPS_PER_THREAD = 10'000;

		concurrent::ConHashMap<u64, u64> map;

		std::vector<std::jthread> threads;
		threads.reserve(thread_count);

		for (u64 i = 0; i < thread_count; i++) {
			threads.emplace_back([&map, i]() {
				for (u64 j = 0; j < LOOK_OPS_PER_THREAD; j++) {
					u64 key = j * thread_count + i;
					map.put(key, key * 10);
				}
			});
		}

		for (u64 i = 0; i < thread_count; i++) threads.at(i).join();

		for (u64 i = 0; i < thread_count; i++) {
			for (u64 j = 0; j < LOOK_OPS_PER_THREAD; j++) {
				u64 key = j * thread_count + i;
				ASSERT_TRUE(map.contains(key));
				ASSERT_EQUAL(map.getCopy(key), key * 10);
			}
		}
	}

	/**
	 * Tests multi-threaded writes to the concurrent::ConHashMap on the same key using maybePutAndUpdate.
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
					map.maybePutAndUpdate(1, 0, [](u64& v) { v += 10; });
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
};

TESTER_COMMON_MAIN("/src/common/concurrent/tests/");
