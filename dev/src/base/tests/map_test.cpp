#include <base/collections/stable_hashmap.hpp>

#include <tester/tester.hpp>

class MapTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS MapTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(basicTest); }

	void basicTest() {
        base::StableHashMap20<int, int> map;

        map.put(1, 10);
        map.put(2, 20);

        u64 loop_count = 0;
        for (auto& [key, value]: map) {
            loop_count++;
            if (key == 1) {
                ASSERT_EQUAL(value, 10);
            } else if (key == 2) {
                ASSERT_EQUAL(value, 20);
            } else {
                fail("Unexpected key in map iteration");
            }
        }
        ASSERT_EQUAL(loop_count, 2);
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
