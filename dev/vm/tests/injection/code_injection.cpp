#include <vm_tester_utils.hpp>

#include <base/optional.hpp>

#include <vm/api/api.hpp>
#include <vm/api/data/core_operation_error.hpp>

class VmCodeInjectionTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmCodeInjectionTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(multipleFiles);
		TESTER_ADD_TEST(injectCode);
		TESTER_ADD_TEST(runNoArgFunction);
		TESTER_ADD_TEST(runVoidFunction);
		TESTER_ADD_TEST(runNonVoidFunction);
		TESTER_ADD_TEST(doubleRunFunction);
		TESTER_ADD_TEST(manyRunFunctions);
		TESTER_ADD_TEST(repl);
		TESTER_ADD_TEST(injectExistingFunction);
	}

private:
	void runFunctionAndCheckExitCode(
		vm::PID                            pid,
		const std::string&                 func_name,
		const std::vector<i64>&            args               = {},
		const base::Optional<std::string>& optional_input     = {},
		const base::Optional<std::string>& optional_output    = {},
		i64                                expected_exit_code = 0
	) {
		ASSERT_TRUE(vm::api::runFunction(pid, func_name, args).has_value());
		if_opt_some(optional_input, input) { ASSERT_TRUE(vm::api::input(pid, input).has_value()); }

		ASSERT_TRUE(vm::api::join(pid).has_value());

		if_opt_some(optional_output, output) {
			auto output_response = vm::api::output(pid);
			ASSERT_TRUE(output_response.has_value());
			ASSERT_EQUAL(output, output_response->output);
		}

		auto exit_code_response = vm::api::getExitCode(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		ASSERT_EQUAL_PRINT(expected_exit_code, *exit_code_response);
	}

	void multipleFiles() {
		vm::PID      pid = initProcess();
		fs::FilePath file1(path("multiple_files_1.dbc"));
		fs::FilePath file2(path("multiple_files_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1, file2 }).has_value());

		ASSERT_TRUE(vm::api::run(pid).has_value());
		ASSERT_TRUE(vm::api::input(pid, "123").has_value());
		ASSERT_TRUE(vm::api::join(pid).has_value());

		auto output_response = vm::api::output(pid);
		ASSERT_TRUE(output_response.has_value());
		ASSERT_EQUAL("123", output_response->output);

		auto exit_code_response = vm::api::getExitCode(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		ASSERT_EQUAL_PRINT(0, *exit_code_response);
	}

	void injectCode() {
		vm::PID pid = initProcess();

		fs::FilePath file1(path("inject_code_1.dbc"));
		fs::FilePath file2(path("inject_code_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1 }).has_value());
		ASSERT_TRUE(vm::api::loadFiles(pid, { file2 }).has_value());

		ASSERT_TRUE(vm::api::run(pid).has_value());
		ASSERT_TRUE(vm::api::input(pid, "123").has_value());
		ASSERT_TRUE(vm::api::join(pid).has_value());

		auto output_response = vm::api::output(pid);
		ASSERT_TRUE(output_response.has_value());
		ASSERT_EQUAL("123", output_response->output);

		auto exit_code_response = vm::api::getExitCode(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		ASSERT_EQUAL_PRINT(0, *exit_code_response);
	}

	void runNoArgFunction() {
		vm::PID      pid = initProcess();
		fs::FilePath file(path("call_no_arg_function.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		runFunctionAndCheckExitCode(pid, "summer", {}, {}, "735", 0);
	}

	void runVoidFunction() {
		vm::PID      pid = initProcess();
		fs::FilePath file(path("call_void_function.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		runFunctionAndCheckExitCode(pid, "summer", { 1, 2 }, {}, "3", 0);
	}

	void runNonVoidFunction() {
		vm::PID      pid = initProcess();
		fs::FilePath file(path("call_non_void_function.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		runFunctionAndCheckExitCode(pid, "summer", { 695, 40 }, {}, {}, 735);
	}

	void doubleRunFunction() {
		vm::PID      pid = initProcess();
		fs::FilePath file1(path("repl_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1 }).has_value());

		runFunctionAndCheckExitCode(pid, "spring", { 4, 8 }, {}, {}, 32);
		runFunctionAndCheckExitCode(pid, "spring", { 4, 6 }, {}, {}, 24);
	}

	void manyRunFunctions() {
		vm::PID      pid = initProcess();
		fs::FilePath file(path("repl_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		for (i32 i = 0; i < 100; i++)
			runFunctionAndCheckExitCode(pid, "spring", { i, i }, {}, {}, i * i);
	}

	void repl() {
		vm::PID pid = initProcess();

		fs::FilePath file1(path("repl_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1 }).has_value());
		runFunctionAndCheckExitCode(pid, "spring", { 4, 8 }, {}, {}, 32);

		fs::FilePath file2(path("repl_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file2 }).has_value());
		runFunctionAndCheckExitCode(pid, "summer", { 1, 2 }, {}, {}, 3);
	}

	void injectExistingFunction() {
		vm::PID      pid = initProcess();
		fs::FilePath file(path("inject_code_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());
		ASSERT_TRUE(!vm::api::loadFiles(pid, { file }).has_value());
	}
};

TESTER_COMMON_MAIN("/vm/tests/injection/");
