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
		TESTER_ADD_TEST(singleThreadedRandomTest<0>);
		TESTER_ADD_TEST(singleThreadedRandomTest<2>);
		TESTER_ADD_TEST(singleThreadedRandomTest<10>);
		TESTER_ADD_TEST(singleThreadedRandomTest<100>);
		TESTER_ADD_TEST(singleThreadedRandomTest<10'000>);
		TESTER_ADD_TEST(singleThreadedRandomTest<100'000>);

		TESTER_ADD_TEST(hashMapSingleThreadTest1);
	}

private:
	/**
	 * Simple single-threaded test of concurrent::HashMap.
	 */
	void hashMapSingleThreadTest1() {
		concurrent::HashMap<int, int> map;

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
	 * Single-threaded random test of concurrent::HashMap adapted from tests of maps from base.
	 */
	template<u64 count>
	void singleThreadedRandomTest() {
		u64 base_result     = 0;
		{
			std::minstd_rand rng(42);

			concurrent::HashMap<BigObject<13>, BigObject<16>> map;

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

		u64 std_result     = 0;
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
};

TESTER_COMMON_MAIN("/src/common/concurrent/tests/");
