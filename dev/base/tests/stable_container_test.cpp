#include <tester/tester.hpp>
#include "base/stable_container.hpp"
#include "base/stable_hashmap.hpp"
#include <base/strongly_typed_int.hpp>

STRONG_TYPEDEF_INT_DIMENSIONAL(SomeID, usize);

class StableListTestSimple: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StableListTestSimple

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(customKeyTest);
		TESTER_ADD_TEST(stableHashMapTest);
		TESTER_ADD_TEST(stableHashMapTestStability);
	}

private:
	void simpleTest() {
		message("Parts of this state only make sense under valgrind");

		base::StableVector<i32> vector;

		assertTrue(vector.empty(), "bad list empty");
		assertTrue(!vector.notEmpty(), "bad list not empty");
		assertTrue(vector.size() == 0, "bad list size");

		vector.pushBack(2);
		vector.pushBack(3);
		vector.pushBack(4);

		assertTrue(vector[0] == 2, "Bad stable list pushBack (0)");
		assertTrue(vector[1] == 3, "Bad stable list pushBack (1)");
		assertTrue(vector[2] == 4, "Bad stable list pushBack (2)");

		auto maybe_ref = vector.getRef(1);
		assertTrue(maybe_ref.has_value(), "No value");
		auto ref = maybe_ref.value();

		assertTrue(*ref == 3, "Bad stable list ref");
		*ref = 4;
		assertTrue(*ref == 4, "Bad stable list ref");

		auto maybe_c_ref = vector.getCRef(0);
		assertTrue(maybe_c_ref.has_value(), "No value");
		auto c_ref = maybe_c_ref.value();
		assertTrue(*c_ref == 2, "Bad stable list c_ref");

		for (int i = 0; i < 100; i++) vector.pushBack(i);

		assertTrue(vector.getRef(102).has_value(), "No value where there should be");
		assertTrue(vector.getRef(103).empty(), "Value where there should be none");
		assertTrue(vector.getRef(104).empty(), "Value where there should be none");

		assertTrue(vector.size() == 103, "bad list size");
		assertTrue(!vector.empty(), "bad list empty");
		assertTrue(vector.notEmpty(), "bad list not empty");

		assertTrue(*ref == 4, "Stable list ref not stable");
		assertTrue(*c_ref == 2, "Bad stable list c_ref");
	}

	void customKeyTest() {
		base::StableVector<int, SomeID> list;

		assertTrue(list.empty(), "bad list empty");

		auto key1 = list.pushBack(1);
		auto key2 = list.emplaceBack(2);

		assertTrue(key1 != key2, "some keys");
		assertTrue(key1 + SomeID(1) == key2, "Strange key chosen");

		assertTrue(list[key1] == 1, "bad value in list");

		auto ref = list.getRef(key1).value();
		assertTrue(*ref == 1, "bad reference");
		*ref = 100;
		assertTrue(*ref == 100, "bad reference");

		for (usize i = 0; i < 100; i++) list.pushBack(int(i));

		assertTrue(*ref == 100, "bad reference");
		*ref = 1'000;
		assertTrue(*ref == 1'000, "bad reference");
	}

	void stableHashMapTest() {
		base::StableHashMap<std::string, std::string> map;
		map["lol"] = "test";

		ASSERT_EQUAL("test", map["lol"]);
		ASSERT_EQUAL(1, map.size());

		ASSERT_EQUAL("test", map["lol"]);
		ASSERT_EQUAL("test", map.at("lol"));
		ASSERT_EQUAL(true, map.atMaybe("lol2").empty());
		ASSERT_EQUAL("test", map.atMaybe("lol").value());

		map.clear();
		ASSERT_EQUAL(0, map.size());

		map.put("lol", "test 1");
		map.put("a", "test 2");
		map.put("b", "test 3");
		auto put_res = map.put("lol", "test");

		assertTrue(put_res.second == false, "Value was wrongly inserted");

		ASSERT_EQUAL(3, map.size());
		ASSERT_EQUAL(map["a"], "test 2");
		ASSERT_EQUAL(map["b"], "test 3");
		ASSERT_EQUAL(map["lol"], "test 1");
	}

	void stableHashMapTestStability() {
		base::StableHashMap<usize, i64> map;
		const i64*                      ptr = nullptr;
		for (usize i = 0; i < 10'000; i++) {
			map[i] = static_cast<i64>(i);
			if (i == 0) ptr = &map[0];
			assertEqual(ptr, &map[0], base::strConcat("A StableHashMap is not stable :O"));
		}
	}
};

TESTER_COMMON_MAIN("/base/tests/");
