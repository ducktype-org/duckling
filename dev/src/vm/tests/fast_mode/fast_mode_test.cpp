// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/process_info.hpp>
#include <vm/api/vm.hpp>
#include <vm/bytecode/validator/errors.hpp>

class VmFunctionsTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmFunctionsTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testSimple);
		TESTER_ADD_TEST(testControlFlow);
		TESTER_ADD_TEST(testFunctionCallWithArgs);
		TESTER_ADD_TEST(testFibIter);
		TESTER_ADD_TEST(testFibRec);
	}

private:
	vm::PID spawnFastAndLoad(const std::string& dbc_file_path) {
		auto pid         = initProcess({
					.mode = vm::api::ProcessMode::Fast,
        });
		auto file        = fs::File(path(dbc_file_path));
		auto load_result = vm::api::loadFiles(pid, { file });
		if (!load_result.has_value()) {
			std::cerr << std::format("Error loading file: {}\n", errorToString(load_result.error()));
		}
		ASSERT_HAS_VALUE(load_result);
		return pid;
	}

	template<class... Args>
	void runTestOnFast(
		const std::string&                 dbc_file_path,
		const base::Optional<std::string>& optional_input  = {},
		const base::Optional<std::string>& optional_output = {},
		i64                                exit_code       = 0
	) {
		runTestOnVm(spawnFastAndLoad(dbc_file_path), optional_input, optional_output, {}, exit_code);
	}

	void testSimple() { runTestOnFast("simple.dbc", "", "", 42); }

	void testControlFlow() { runTestOnFast("control_flow.dbc", "", "50"); }

	void testFunctionCallWithArgs() { runTestOnFast("function_with_args.dbc", "5 7", "12"); }

	void testFibIter() { runTestOnFast("../correctness/fib_iter.dbc", "1000000 10000", "6875"); }

	void testFibRec() { runTestOnFast("../correctness/fib_rec.dbc", "28", "317811"); }
};

TESTER_COMMON_MAIN("/src/vm/tests/fast_mode/");
