// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/collections/stable_hashmap.hpp>

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

class MapTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS MapTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(basicTest<0>);
		TESTER_ADD_TEST(basicTest<2>);
		TESTER_ADD_TEST(basicTest<10>);
		TESTER_ADD_TEST(basicTest<100>);
		TESTER_ADD_TEST(basicTest<10'000>);
		TESTER_ADD_TEST(basicTest<100'000>);
		TESTER_ADD_TEST(containsTest);
		TESTER_ADD_TEST(clearTest);
		TESTER_ADD_TEST(moveTest);
	}

	template<u64 count>
	void basicTest() {
		u64 base_loop_count = 0;
		u64 base_result     = 0;
		{
			std::minstd_rand rng(42);

			base::StableHashMap<BigObject<13>, BigObject<16>> map;

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


			for (auto& [key, value]: map) {
				base_loop_count++;
				ASSERT_EQUAL(value.data[0], key.data[0] * 10);
			}

			const auto& const_map = map;
			for (auto& [key, value]: const_map) ASSERT_EQUAL(value.data[0], key.data[0] * 10);
		}

		u64 std_loop_count = 0;
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


			for (auto& [key, value]: map) {
				std_loop_count++;
				ASSERT_EQUAL(value.data[0], key.data[0] * 10);
			}
		}

		ASSERT_EQUAL(base_loop_count, std_loop_count);
		ASSERT_EQUAL(base_result, std_result);
	}

	void containsTest() {
		base::StableHashMap<u64, u64> map;
		map.put(1ull, 10ull);

		ASSERT_TRUE(map.contains(1ull));
		ASSERT_TRUE(not map.contains(2ull));
		ASSERT_TRUE(not map.contains(3ull));


		map.put(2ull, 20ull);
		map.put(3ull, 30ull);

		ASSERT_TRUE(map.contains(1ull));
		ASSERT_TRUE(map.contains(2ull));
		ASSERT_TRUE(map.contains(3ull));
		ASSERT_TRUE(not map.contains(4ull));
	}

	void clearTest() {
		base::StableHashMap<u64, u64> map;
		map.put(1ull, 10ull);
		map.put(2ull, 20ull);
		map.put(3ull, 30ull);

		ASSERT_EQUAL(map.size(), 3);

		map.clear();

		ASSERT_EQUAL(map.size(), 0);
		ASSERT_TRUE(not map.contains(1));
		ASSERT_TRUE(not map.contains(2));
		ASSERT_TRUE(not map.contains(3));
	}

	void moveTest() {
		base::StableHashMap<u64, u64> map;
		map.put(1ull, 10ull);
		map.put(2ull, 20ull);
		map.put(3ull, 30ull);

		base::StableHashMap<u64, u64> moved_map = std::move(map);

		// we don't lint here, so we can use map after move:
		// NOLINTBEGIN
		ASSERT_EQUAL(map.size(), 0);

		ASSERT_TRUE(not map.contains(1));
		ASSERT_TRUE(not map.contains(2));
		ASSERT_TRUE(not map.contains(3));
		// NOLINTEND

		ASSERT_EQUAL(moved_map.size(), 3);
		ASSERT_TRUE(moved_map.contains(1));
		ASSERT_TRUE(moved_map.contains(2));
		ASSERT_TRUE(moved_map.contains(3));

		ASSERT_EQUAL(moved_map[1], 10ull);
		ASSERT_EQUAL(moved_map[2], 20ull);
		ASSERT_EQUAL(moved_map[3], 30ull);
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
