#include <tester/tester.hpp>

class ClapParamBuilderTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ClapParamBuilderTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Clap Param Builder Test") { TESTER_ADD_TEST(simpleTest); }

private:
	void simpleTest() {
		base::HashMap<base::RawView, usize> m;
		base::RawView                       r1 = "abc";
		base::RawView                       r2 = "abc";
	}
};

TESTER_COMMON_MAIN("/common/clap/tests/");
