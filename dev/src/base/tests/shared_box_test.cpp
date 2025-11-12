#include <base/pointers/shared_box.hpp>
#include <tester/tester.hpp>

class SharedBoxTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BoxRefTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(xd);
	}

private:
    void xd() {

    }
};

TESTER_COMMON_MAIN("/src/base/tests/");