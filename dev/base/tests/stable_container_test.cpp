#include <tester/tester.hpp>
#include <base/stable_container.hpp>
#include <base/strongly_typed_int.hpp>
#include <filesystem/file.hpp>

STRONG_TYPEDEF_INT_DIMENSIONAL(SomeId, usize);

class StableListTestSimple: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StableListTestSimple

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Stable list test") {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(customKeyTest);
		TESTER_ADD_TEST(stableHashMapTest);
		TESTER_ADD_TEST(stableHashMapTestStability);
	}

private:
	void simpleTest() {
		message("Parts of this state only make sense under valgrind");

		base::StableIntVector<i32> vector;

		assert(vector.empty(), "bad list empty");
		assert(!vector.notEmpty(), "bad list not empty");
		assert(vector.size() == 0, "bad list size");

		vector.pushBack(2);
		vector.pushBack(3);
		vector.pushBack(4);

		assert(vector[0] == 2, "Bad stable list pushBack (0)");
		assert(vector[1] == 3, "Bad stable list pushBack (1)");
		assert(vector[2] == 4, "Bad stable list pushBack (2)");

		auto maybe_ref = vector.getRef(1);
		assert(maybe_ref.has_value(), "No value");
		auto ref = maybe_ref.value();

		assert(*ref == 3, "Bad stable list ref");
		*ref = 4;
		assert(*ref == 4, "Bad stable list ref");

		auto maybe_c_ref = vector.getCRef(0);
		assert(maybe_c_ref.has_value(), "No value");
		auto c_ref = maybe_c_ref.value();
		assert(*c_ref == 2, "Bad stable list c_ref");

		for (int i = 0; i < 100; i++) vector.pushBack(i);

		assert(vector.getRef(102).has_value(), "No value where there should be");
		assert(vector.getRef(103).empty(), "Value where there should be none");
		assert(vector.getRef(104).empty(), "Value where there should be none");

		assert(vector.size() == 103, "bad list size");
		assert(!vector.empty(), "bad list empty");
		assert(vector.notEmpty(), "bad list not empty");

		assert(*ref == 4, "Stable list ref not stable");
		assert(*c_ref == 2, "Bad stable list c_ref");
	}

	void customKeyTest() {
		base::StableVector<SomeId, int> list;

		assert(list.empty(), "bad list empty");

		auto key1 = list.pushBack(1);
		auto key2 = list.emplaceBack(2);

		assert(key1 != key2, "some keys");
		assert(key1 + SomeId(1) == key2, "Strange key chosen");

		assert(list[key1] == 1, "bad value in list");

		auto ref = list.getRef(key1).value();
		assert(*ref == 1, "bad reference");
		*ref = 100;
		assert(*ref == 100, "bad reference");

		for (usize i = 0; i < 100; i++) list.pushBack(int(i));

		assert(*ref == 100, "bad reference");
		*ref = 1'000;
		assert(*ref == 1'000, "bad reference");
	}

	void stableHashMapTest() {
		base::StableHashMap<std::string, std::string> map;
		map["lol"] = "test";

		ASSERT_EQUAL("test", map["lol"]);
		ASSERT_EQUAL(1, map.size());

		map.clear();
		ASSERT_EQUAL(0, map.size());
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

TESTER_COMMON_MAIN("/common/stable_list/tests/");
