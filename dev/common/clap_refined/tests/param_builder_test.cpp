#include <tester/tester.hpp>
#include <clap/clap.hpp>
#include "clap/param_builder.hpp"

using namespace clap;

class ClapParamBuilderTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ClapParamBuilderTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Clap Param Builder Test") { TESTER_ADD_TEST(simpleTest); }

private:
	void simpleTest() { auto param = clap::ParamBuilder::ofFlag().required().build(); }
};

TESTER_COMMON_MAIN("/common/clap/tests/");
