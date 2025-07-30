#include <vm_tester_utils.hpp>

#include <base/optional.hpp>

#include <vm/api/api.hpp>
#include <vm/api/data/api_error.hpp>

#include <memory>
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
		TESTER_ADD_TEST(separateGlobals);
		TESTER_ADD_TEST(cyclicRepl);
		TESTER_ADD_TEST(injectExistingFunction);
	}

private:
	/**
	 * @brief Executes a function or a program within a VM process and verifies the results.
	 *
	 * @param pid The Process ID of the target VM process.
	 * @param func_name An optional name of the function to execute. If empty, the function assumes
	 * it's running a whole program (the `main` function).
	 * @param args A variant holding either `vm::FunctionRunArguments` (for `runFunction`) or
	 * `vm::ProgramRunArguments` (for `run`). The correct type must be provided based on whether
	 * `func_name` is set.
	 * @param optional_input An optional string to be passed as standard input to the process.
	 * @param optional_output An optional string to which the process's standard output will be
	 * compared.
	 * @param expected_exit_code An optional expected exit code (`i64`). If provided, the function's
	 * return value is asserted to be equal to it. If not provided, the function asserts that the
	 * return type was `void`.
	 */
	void runAndCheckExitCode(
		vm::PID                            pid,
		const base::Optional<std::string>& func_name          = {},
		const vm::RunArguments&            args               = {},
		const base::Optional<std::string>& optional_input     = {},
		const base::Optional<std::string>& optional_output    = {},
		const base::Optional<i64>          expected_exit_code = {}
	) {
		match_optional(func_name) {
			opt_some(func_name) {
				ASSERT_TRUE(std::holds_alternative<vm::FunctionRunArguments>(args));
				const auto& function_args = std::get<vm::FunctionRunArguments>(args);
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

		auto exit_code_response = vm::api::getExitValue(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		const auto& exit_value = exit_code_response.value();
		if (expected_exit_code.has_value())
			ASSERT_EQUAL_PRINT(expected_exit_code.value(), exit_value->interpret<i64>());
		else
			// @note: If expected_exit_code is an empty optional, it's expected that a called
			// function is void.
			ASSERT_TRUE(exit_value->type->getName() == base::StrID("void"));
	}

	Ref<vm::VmValue> getIntVmValue(vm::PID pid, i64 value) {
		auto response = vm::api::getVmValue(pid, "i64");
		ASSERT_TRUE(response.has_value());
		auto vm_value              = response->vm_value;
		vm_value->interpret<i64>() = value;
		return vm_value;
	}

	void multipleFiles() {
		vm::PID  pid = initProcess();
		fs::File file1(path("multiple_files_1.dbc"));
		fs::File file2(path("multiple_files_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1, file2 }).has_value());

		runAndCheckExitCode(pid, {}, vm::ProgramRunArguments{}, "123", "123", 0);
	}

	void injectCode() {
		vm::PID  pid = initProcess();
		fs::File file1(path("inject_code_1.dbc"));
		fs::File file2(path("inject_code_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1 }).has_value());
		ASSERT_TRUE(vm::api::loadFiles(pid, { file2 }).has_value());

		runAndCheckExitCode(pid, {}, vm::ProgramRunArguments{}, "123", "123", 0);
	}

	void runNoArgFunction() {
		vm::PID  pid = initProcess();
		fs::File file(path("call_no_arg_function.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		runAndCheckExitCode(pid, "summer", vm::FunctionRunArguments{}, {}, "735", {});
	}

	void runVoidFunction() {
		vm::PID  pid = initProcess();
		fs::File file(path("call_void_function.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		vm::FunctionRunArguments args = { getIntVmValue(pid, 1), getIntVmValue(pid, 2) };
		runAndCheckExitCode(pid, "summer", args, {}, "3", {});
	}

	void runNonVoidFunction() {
		vm::PID  pid = initProcess();
		fs::File file(path("call_non_void_function.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		vm::FunctionRunArguments args = { getIntVmValue(pid, 695), getIntVmValue(pid, 40) };
		runAndCheckExitCode(pid, "summer", args, {}, {}, 735);
	}

	void doubleRunFunction() {
		vm::PID  pid = initProcess();
		fs::File file1(path("repl_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1 }).has_value());

		vm::FunctionRunArguments args = { getIntVmValue(pid, 4), getIntVmValue(pid, 8) };
		runAndCheckExitCode(pid, "spring", args, {}, {}, 32);
		vm::FunctionRunArguments args2 = { getIntVmValue(pid, 4), getIntVmValue(pid, 6) };
		runAndCheckExitCode(pid, "spring", args2, {}, {}, 24);
	}

	void manyRunFunctions() {
		vm::PID  pid = initProcess();
		fs::File file(path("repl_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		for (i32 i = 0; i < 100; i++) {
			vm::FunctionRunArguments args = { getIntVmValue(pid, i), getIntVmValue(pid, i) };
			runAndCheckExitCode(pid, "spring", args, {}, {}, i * i);
		}
	}

	void repl() {
		vm::PID pid = initProcess();

		fs::File file1(path("repl_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1 }).has_value());
		vm::FunctionRunArguments args = { getIntVmValue(pid, 4), getIntVmValue(pid, 8) };
		runAndCheckExitCode(pid, "spring", args, {}, {}, 32);

		fs::File file2(path("repl_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file2 }).has_value());
		vm::FunctionRunArguments args2 = { getIntVmValue(pid, 1), getIntVmValue(pid, 2) };
		runAndCheckExitCode(pid, "summer", args2, {}, {}, 3);
	}

	void replWithGlobals() {
		vm::PID  pid = initProcess();
		fs::File file1(path("repl_with_globals_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1 }).has_value());
		runAndCheckExitCode(pid, "globaler_setter", vm::FunctionRunArguments{}, "1 2", {}, {});

		fs::File file2(path("repl_with_globals_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file2 }).has_value());
		runAndCheckExitCode(pid, "globaler_reader", vm::FunctionRunArguments{}, {}, "12", {});
	}

	void cyclicRepl() {
		vm::PID pid = initProcess();

		fs::File file1(path("loaded_func_call_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1 }).has_value());
		vm::FunctionRunArguments args = { getIntVmValue(pid, 4), getIntVmValue(pid, 8) };
		runAndCheckExitCode(pid, "summer", args, {}, {}, 12);

		fs::File file2(path("loaded_func_call_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file2 }).has_value());
		vm::FunctionRunArguments args2 = { getIntVmValue(pid, 2), getIntVmValue(pid, 3) };
		runAndCheckExitCode(pid, "spring", args2, {}, {}, 10);
	}

	void separateGlobals() {
		vm::PID  pid = initProcess();
		fs::File file1(path("separate_globals_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1 }).has_value());

		fs::File file2(path("separate_globals_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file2 }).has_value());
		runAndCheckExitCode(pid, "globaler_setter", vm::FunctionRunArguments{}, "12", "12", {});
	}

	void injectExistingFunction() {
		vm::PID  pid = initProcess();
		fs::File file(path("inject_code_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());
		ASSERT_TRUE(!vm::api::loadFiles(pid, { file }).has_value());
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/injection/");
