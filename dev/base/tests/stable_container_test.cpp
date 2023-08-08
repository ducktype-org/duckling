#include <tester/tester.hpp>
#include <base/stable_container.hpp>
#include <base/strongly_typed_int.hpp>

STRONG_TYPEDEF_INT(SomeId, usize);

class StableListTestSimple: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StableListTestSimple

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Stable list test") {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(customKeyTest);
	}

private:
	void simpleTest() {
		message("Parts of this state only make sense under valgrind");

		base::StableIntList<int> list;

		assert(list.empty(), "bad list empty");
		assert(!list.notEmpty(), "bad list not empty");
		assert(list.size() == 0, "bad list size");

		list.pushBack(2);
		list.pushBack(3);
		list.pushBack(4);

		assert(list[0] == 2, "Bad stable list pushBack (0)");
		assert(list[1] == 3, "Bad stable list pushBack (1)");
		assert(list[2] == 4, "Bad stable list pushBack (2)");

		auto maybe_ref = list.getRef(1);
		assert(maybe_ref.has_value(), "No value");
		auto ref = maybe_ref.expect("No value");

		assert(*ref == 3, "Bad stable list ref");
		*ref = 4;
		assert(*ref == 4, "Bad stable list ref");

		auto maybe_c_ref = list.getCRef(0);
		assert(maybe_c_ref.has_value(), "No value");
		auto c_ref = maybe_c_ref.expect("No value");
		assert(*c_ref == 2, "Bad stable list c_ref");

		for (usize i = 0; i < 100; i++) {
			list.pushBack(i);
		}

		assert(list.getRef(102).has_value(), "No value where there should be");
		assert(list.getRef(103).has_error(), "No error where there should be");
		assert(list.getRef(104).has_error(), "No error where there should be");

		assert(list.size() == 103, "bad list size");
		assert(!list.empty(), "bad list empty");
		assert(list.notEmpty(), "bad list not empty");
		
		assert(*ref == 4, "Stable list ref not stable");
		assert(*c_ref == 2, "Bad stable list c_ref");

	}

	void customKeyTest() {
		base::StableList<SomeId, int> list;
		
		assert(list.empty(), "bad list empty");
		
		auto key1 = list.pushBack(1);
		auto key2 = list.emplaceBack(2);

		assert(key1 != key2, "some keys");
		assert(key1 + SomeId(1) == key2, "Strange key chosen");

		assert(list[key1] == 1, "bad value in list");

		auto ref = list.getRef(key1).expect("No value in list");
		assert(*ref == 1, "bad reference");
		*ref = 100;
		assert(*ref == 100, "bad reference");

		for (usize i = 0; i < 100; i++) {
			list.pushBack(i);
		}

		assert(*ref == 100, "bad reference");
		*ref = 1000;
		assert(*ref == 1000, "bad reference");
	}
};

TESTER_COMMON_MAIN("/common/stable_list/tests/");
