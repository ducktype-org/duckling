#include <tester/tester.hpp>

class QueryTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS QueryTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Query Test") {

	}

private:
	
};

TESTER_COMMON_MAIN("/common/query_framework/tests/");

