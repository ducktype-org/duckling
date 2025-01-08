#include <api/data/status.hpp>
#include <api/vm.hpp>
#include <api/api.hpp>
#include <cstdint>
#include <tester/tester.hpp>
#include <variant>
#include <thread>
#include <chrono>

class SimpleVmTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleVmTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(parsesTheFile);
		TESTER_ADD_TEST(stopTest);
		TESTER_ADD_TEST(killTest);
	}


private:
};

TESTER_COMMON_MAIN("/VM/tests/basic/");
