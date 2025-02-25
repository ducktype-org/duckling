
#include <base/string_id.hpp>
#include <tester/tester.hpp>
#include <system_command/system_command.hpp>

class SystemCommandTests final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SystemCommandTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(exitCode); }

private:
	void exitCode() {
		// https://en.wikipedia.org/wiki/True_and_false_(commands)
		auto true_ec = system_command::SystemCommand(base::StrID("true")).execute();
		assertTrue(true_ec == 0, "true command should return 0");
		auto false_ec = system_command::SystemCommand(base::StrID("false")).execute(true, false);
		assertTrue(false_ec == 1, "false command should return 1");

		assertThrows<base::Panic>(
			[&]() { system_command::SystemCommand(base::StrID("false")).execute(); },
			"command should panic on non-zero exit code"
		);
	}
};


TESTER_COMMON_MAIN("/compiler/driver/tests/")
