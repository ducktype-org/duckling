// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <tester/tester.hpp>

#include <vm/api/api.hpp>

class SimpleVmTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleVmTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(simpleRun); }


private:
	void simpleRun() {
		auto process_pid_response = vm::api::spawn();
		ASSERT_HAS_VALUE(process_pid_response, "Spawn failed (1)");
		auto pid = process_pid_response.value().pid;  // "Spawn failed (2)"

		fs::File file(path("working_dbc.dbc"));
		auto     loaded_file_response = vm::api::loadFiles(pid, { file });
		ASSERT_HAS_VALUE(loaded_file_response, "Load failed (1)");

		auto run_response = vm::api::run(pid);
		ASSERT_HAS_VALUE(run_response, "Run failed (1)");

		auto join_response = vm::api::join(pid);
		ASSERT_HAS_VALUE(join_response, "Join failed (1)");
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/basic/");
