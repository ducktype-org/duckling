// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <system_command/system_command.hpp>
#include <tester/tester.hpp>

class SystemCommandTests final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SystemCommandTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(exitCode); }

private:
	void exitCode() {
		// https://en.wikipedia.org/wiki/True_and_false_(commands)
		auto true_ec = system_command::SystemCommand("true").execute();
		assertTrue(true_ec == 0, "true command should return 0");
		auto false_ec = system_command::SystemCommand("false").execute(
			system_command::SystemCommand::ExitCodeHandling::Ignore
		);
		assertTrue(false_ec == 1, "false command should return 1");

		assertThrows<base::Panic>(
			[&]() { system_command::SystemCommand("false").execute(); },
			"command should panic on non-zero exit code"
		);
	}
};


TESTER_COMMON_MAIN("/src/compiler/driver/tests/")
