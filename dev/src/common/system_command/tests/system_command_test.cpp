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
		// Do NOT use `true` nor `false`: they are shell built-ins, and not executables.
		auto true_ec = system_command::SystemCommand("/bin/ls").execute();
		assertTrue(true_ec == 0, "true command should return 0");
		auto false_ec = system_command::SystemCommand("/i-really-hope-this-is-not-executable")
		                    .execute(system_command::SystemCommand::ExitCodeHandling::Ignore);
		assertTrue(false_ec == 1, "false command should return 1");

		assertThrows<base::Panic>(
			[&]() {
				[[maybe_unused]] auto rc
					= system_command::SystemCommand("/i-really-hope-this-is-not-executable")
			              .execute();
			},
			"command should panic on non-zero exit code"
		);
	}
};


TESTER_COMMON_MAIN("/src/compiler/driver/tests/")
