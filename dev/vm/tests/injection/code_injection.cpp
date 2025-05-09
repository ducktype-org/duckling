#include <vm_tester_utils.hpp>

#include <base/optional.hpp>

#include <vm/api/api.hpp>
#include <vm/api/data/core_operation_error.hpp>

#include <string>
#include <vector>

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
		TESTER_ADD_TEST(replWithGlobals);
		TESTER_ADD_TEST(cyclicRepl);
		TESTER_ADD_TEST(injectExistingFunction);
	}

private:
	void runAndCheckExitCode(
		vm::PID                                                         pid,
		const base::Optional<std::string>&                              func_name          = {},
		const std::variant<std::vector<i64>, std::vector<std::string>>& args               = {},
		const base::Optional<std::string>&                              optional_input     = {},
		const base::Optional<std::string>&                              optional_output    = {},
		i64                                                             expected_exit_code = 0
	) {
		match_optional(func_name) {
			opt_some(func_name) {
				ASSERT_TRUE(std::holds_alternative<std::vector<i64>>(args));
				auto function_args = std::get<std::vector<i64>>(args);
				ASSERT_TRUE(vm::api::runFunction(pid, func_name, function_args).has_value());
			}
			opt_none {
				ASSERT_TRUE(std::holds_alternative<std::vector<std::string>>(args));
				auto program_args = std::get<std::vector<std::string>>(args);
				ASSERT_TRUE(vm::api::run(pid, program_args).has_value());
			}
		}

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

		runAndCheckExitCode(pid, {}, std::vector<std::string>{}, "123", "123", 0);
	}

	void injectCode() {
		vm::PID      pid = initProcess();
		fs::FilePath file1(path("inject_code_1.dbc"));
		fs::FilePath file2(path("inject_code_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1 }).has_value());
		ASSERT_TRUE(vm::api::loadFiles(pid, { file2 }).has_value());

		runAndCheckExitCode(pid, {}, std::vector<std::string>{}, "123", "123", 0);
	}

	void runNoArgFunction() {
		vm::PID      pid = initProcess();
		fs::FilePath file(path("call_no_arg_function.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		runAndCheckExitCode(pid, "summer", {}, {}, "735", 0);
	}

	void runVoidFunction() {
		vm::PID      pid = initProcess();
		fs::FilePath file(path("call_void_function.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		runAndCheckExitCode(pid, "summer", std::vector<i64>{ 1, 2 }, {}, "3", 0);
	}

	void runNonVoidFunction() {
		vm::PID      pid = initProcess();
		fs::FilePath file(path("call_non_void_function.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		runAndCheckExitCode(pid, "summer", std::vector<i64>{ 695, 40 }, {}, {}, 735);
	}

	void doubleRunFunction() {
		vm::PID      pid = initProcess();
		fs::FilePath file1(path("repl_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1 }).has_value());

		runAndCheckExitCode(pid, "spring", std::vector<i64>{ 4, 8 }, {}, {}, 32);
		runAndCheckExitCode(pid, "spring", std::vector<i64>{ 4, 6 }, {}, {}, 24);
	}

	void manyRunFunctions() {
		vm::PID      pid = initProcess();
		fs::FilePath file(path("repl_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		for (i32 i = 0; i < 100; i++)
			runAndCheckExitCode(pid, "spring", std::vector<i64>{ i, i }, {}, {}, i * i);
	}

	void repl() {
		vm::PID pid = initProcess();

		fs::FilePath file1(path("repl_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1 }).has_value());
		runAndCheckExitCode(pid, "spring", std::vector<i64>{ 4, 8 }, {}, {}, 32);

		fs::FilePath file2(path("repl_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file2 }).has_value());
		runAndCheckExitCode(pid, "summer", std::vector<i64>{ 1, 2 }, {}, {}, 3);
	}

	void replWithGlobals() {
		vm::PID      pid = initProcess();
		fs::FilePath file1(path("repl_with_globals_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1 }).has_value());
		runAndCheckExitCode(pid, "globaler_setter", std::vector<i64>{}, "1 2", {}, 0);

		fs::FilePath file2(path("repl_with_globals_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file2 }).has_value());
		runAndCheckExitCode(pid, "globaler_reader", std::vector<i64>{}, {}, "12", 0);
	}

	void cyclicRepl() {
		vm::PID pid = initProcess();

		fs::FilePath file1(path("cyclic_repl_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1 }).has_value());
		runAndCheckExitCode(pid, "summer", std::vector<i64>{ 4, 8 }, {}, {}, 12);

		fs::FilePath file2(path("cyclic_repl_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file2 }).has_value());
		runAndCheckExitCode(pid, "spring", std::vector<i64>{ 2, 3 }, {}, {}, 10);
	}

	void injectExistingFunction() {
		vm::PID      pid = initProcess();
		fs::FilePath file(path("inject_code_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());
		ASSERT_TRUE(!vm::api::loadFiles(pid, { file }).has_value());
	}
};

TESTER_COMMON_MAIN("/vm/tests/injection/");
