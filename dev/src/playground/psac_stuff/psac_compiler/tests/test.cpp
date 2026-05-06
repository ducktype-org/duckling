#include <tester/tester.hpp>

class PsacklingerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS PsacklingerTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(test1); }

	void test1() {
	}
};

TESTER_COMMON_MAIN("/src/playground/psac_stuff/psac_compiler/tests");
