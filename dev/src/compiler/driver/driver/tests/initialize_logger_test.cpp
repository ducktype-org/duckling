#include <driver/initialize.hpp>
#include <global_state/global_logger.hpp>

#include <tester/tester.hpp>

class InitializeLoggerTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS InitializeLoggerTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(initializeGlobalLoggerSetsLogger); }

private:
	void initializeGlobalLoggerSetsLogger() {
		compiler::driver::initializeGlobalLogger();
		ASSERT_TRUE(global_state::hasGlobalLogger());
	}

public:
	~InitializeLoggerTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/");
