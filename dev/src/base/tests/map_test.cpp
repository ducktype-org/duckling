#include <base/collections/stable_hashmap.hpp>

#include <tester/tester.hpp>

class MapTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS MapTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
        TESTER_ADD_TEST(basicTest<0>);
        TESTER_ADD_TEST(basicTest<2>);
        TESTER_ADD_TEST(basicTest<10>);
        TESTER_ADD_TEST(basicTest<100>);
        TESTER_ADD_TEST(basicTest<10000>);
        TESTER_ADD_TEST(basicTest<1000000>);
    }

    template<u64 count>
	void basicTest() {
        base::StableHashMap20<int, int> map;

        for (int i = 0; i < count; i++) {
            map.put(i, i * 10);
        }

        u64 loop_count = 0;
        for (auto& [key, value]: map) {
            loop_count++;
            ASSERT_EQUAL(value, key * 10);
        }
        ASSERT_EQUAL(loop_count, count);
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
