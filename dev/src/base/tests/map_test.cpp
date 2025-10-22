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


        
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
