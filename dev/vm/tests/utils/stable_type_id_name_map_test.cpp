#include <tester/tester.hpp>

#include <base/int_conv.hpp>
#include <base/str_utils.hpp>

#include <vm/preprocessor/stable_type_id_name_map.hpp>

#include <array>

// @TODO: Move this to preprocessor/loader directory after #648
class StableTypeIdNameMapTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StableTypeIdNameMapTester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(testSimple); }

	void testSimple() {
		std::array strs = { base::StrID("zero"), base::StrID("one"), base::StrID("two") };
		vm::StableTypeIdNameMap<int> map;
		auto                         id0 = map.insert(0, strs[0]);
		ASSERT_EQUAL(0, id0);
		auto id1 = map.insert(1, strs[1]);
		ASSERT_EQUAL(1, id1);
		auto id2 = map.insert(2, strs[2]);
		ASSERT_EQUAL(2, id2);
		ASSERT_EQUAL(0, *map.at(id0));
		ASSERT_EQUAL(0, *map.at(strs[0]));
		ASSERT_EQUAL(1, *map.at(id1));
		ASSERT_EQUAL(1, *map.at(strs[1]));
		ASSERT_EQUAL(2, *map.at(id2));
		ASSERT_EQUAL(2, *map.at(strs[2]));
		int v = 0;
		for (auto& i: map) ASSERT_EQUAL(i, v++);
		for (int i = 0; i < map.size(); i++) ASSERT_EQUAL(i, *map.at(base::safeIntConv<usize>(i)));
		ASSERT_TRUE(map.contains(0));
		ASSERT_TRUE(map.contains(strs[0]));
		ASSERT_TRUE(!map.contains(3));
		ASSERT_TRUE(!map.contains(base::StrID("three")));
	}
};

TESTER_COMMON_MAIN("/vm/tests/utils/");
