#include <base/misc/lazy_implies.hpp>

#include <tester/tester.hpp>

class BaseTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BaseTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(lazyImpliesTest); }

	void lazyImpliesTest() {
		ASSERT_TRUE(LAZY_IMPLIES(true, true));
		ASSERT_TRUE(LAZY_IMPLIES(false, true));
		ASSERT_TRUE(LAZY_IMPLIES(false, false));
		ASSERT_TRUE(not LAZY_IMPLIES(true, false));

		u64  counter = 0;
		auto bool_id = [&](bool b) {
			counter++;
			return b;
		};

		counter = 0;
		ASSERT_TRUE(LAZY_IMPLIES(false, bool_id(true)));
		ASSERT_EQUAL(0, counter);

		counter = 0;
		ASSERT_TRUE(not LAZY_IMPLIES(true, bool_id(false)));
		ASSERT_EQUAL(1, counter);

		counter = 0;
		ASSERT_TRUE(LAZY_IMPLIES(true, bool_id(true)));
		ASSERT_EQUAL(1, counter);
	}
};

TESTER_COMMON_MAIN("/src/base/tests/");
