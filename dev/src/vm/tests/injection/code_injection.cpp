#include <vm_tester_utils.hpp>

#include <base/collections/optional.hpp>

#include <ranges>
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
		TESTER_ADD_TEST(runMultipleReturnValuesFunction);
		TESTER_ADD_TEST(doubleRunFunction);
		TESTER_ADD_TEST(manyRunFunctions);
		TESTER_ADD_TEST(repl);
		TESTER_ADD_TEST(replWithGlobals);
		TESTER_ADD_TEST(incrementalGlobalVariantPersistsValue);
		TESTER_ADD_TEST(separateGlobals);
		TESTER_ADD_TEST(cyclicRepl);
		TESTER_ADD_TEST(injectExistingFunction);
		TESTER_ADD_TEST(runFunctionArgumentValidation);
	}

private:
	using OwnedArgumentList = std::vector<Box<vm::VmValue>>;

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
	 * @param expected_exit_code An optional vector of expected exit codes (`i64`). If provided, the
	 * function's return value is asserted to be equal to it. If not provided, the function asserts
	 * that the return type was `void`.
	 */
	void runAndCheckReturnValues(
		vm::PID                                 pid,
		const base::Optional<std::string>&      func_name              = {},
		const vm::RunArguments&                 args                   = {},
		const base::Optional<std::string>&      optional_input         = {},
		const base::Optional<std::string>&      optional_output        = {},
		const base::Optional<std::vector<i64>>& expected_return_values = {}
	) {
		match_optional(func_name) {
			opt_some(func_name) {
				ASSERT_TRUE(std::holds_alternative<vm::FunctionRunArguments>(args));
				const auto& function_args = std::get<vm::FunctionRunArguments>(args);
				ASSERT_HAS_VALUE(vm::api::runFunction(pid, func_name, function_args));
			}
			opt_none {
				ASSERT_TRUE(std::holds_alternative<std::vector<std::string>>(args));
				auto program_args = std::get<std::vector<std::string>>(args);
				ASSERT_HAS_VALUE(vm::api::run(pid, program_args));
			}
		}

		if_opt_some(optional_input, input) { ASSERT_HAS_VALUE(vm::api::input(pid, input)); }

		ASSERT_HAS_VALUE(vm::api::join(pid));

		if_opt_some(optional_output, output) {
			auto output_response = vm::api::output(pid);
			ASSERT_HAS_VALUE(output_response);
			ASSERT_EQUAL(output, output_response->output);
		}

		auto exit_code_response = vm::api::getExitValue(pid);
		ASSERT_HAS_VALUE(exit_code_response);
		const auto& exit_value = exit_code_response.value();
		ASSERT_TRUE(std::holds_alternative<std::vector<Ref<vm::VmValue>>>(exit_value));
		auto& exit_value_vec = std::get<std::vector<Ref<vm::VmValue>>>(exit_value);

		match_optional(expected_return_values) {
			opt_some(exp) {
				ASSERT_EQUAL_PRINT(exp.size(), exit_value_vec.size());
				for (usize i{ 0 }; i < exp.size(); i++)
					ASSERT_EQUAL_PRINT(exp.at(i), exit_value_vec.at(i)->readBytes<i64>());
			}
			opt_none {
				// @note: If expected_exit_code is an empty optional, it's expected that a called
				// function is void.
				ASSERT_TRUE(exit_value_vec.size() == 0);
			}
		}
	}

	/**
	 * @brief Utility for a more generalised runAndCheckReturnValues.
	 */
	void runAndCheckReturnValue(
		vm::PID                            pid,
		const base::Optional<std::string>& func_name             = {},
		const vm::RunArguments&            args                  = {},
		const base::Optional<std::string>& optional_input        = {},
		const base::Optional<std::string>& optional_output       = {},
		const base::Optional<i64>&         expected_return_value = {}
	) {
		runAndCheckReturnValues(
			pid,
			func_name,
			args,
			optional_input,
			optional_output,
			expected_return_value.has_value()
				? base::Optional<std::vector<i64>>{ { expected_return_value.value() } }
				: std::nullopt
		);
	}

	/**
	 * @brief Create an owned VmValue containing a specified value.
	 */
	Box<vm::VmValue> getIntVmValue(vm::PID pid, i64 value) {
		auto response = vm::api::getVmValue(pid, "i64");
		ASSERT_HAS_VALUE(response);
		auto vm_value = std::move(response->vm_value);
		vm_value->writeBytes<i64>(value);
		return vm_value;
	}

	/**
	 * @brief Create a list of owned VmValues containing a specified values.
	 */
	OwnedArgumentList getOwnedArgumentList(vm::PID pid, std::vector<i64> values) {
		return values
		     | std::views::transform([this, pid](i64 value) { return getIntVmValue(pid, value); })
		     | std::ranges::to<OwnedArgumentList>();
	}

	/**
	 * @brief Create a list of references to owned VmValues which can be passed to the VM.
	 */
	vm::FunctionRunArguments createArgumentList(OwnedArgumentList& arguments) {
		return arguments | std::views::transform([](auto& value) { return value.refMut(); })
		     | std::ranges::to<vm::FunctionRunArguments>();
	}

	void freeArguments(OwnedArgumentList& arguments) {
		for (auto& arg: arguments) arg->freeData();
	}

	void multipleFiles() {
		vm::PID  pid = initProcess();
		fs::File file1(path("multiple_files_1.dbc"));
		fs::File file2(path("multiple_files_2.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file1, file2 }));

		runAndCheckReturnValue(pid, {}, vm::ProgramRunArguments{}, "123", "123", 0);
		vm::api::deinitAndValidate(pid);
	}

	void injectCode() {
		vm::PID  pid = initProcess();
		fs::File file1(path("inject_code_1.dbc"));
		fs::File file2(path("inject_code_2.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file1 }));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file2 }));

		runAndCheckReturnValue(pid, {}, vm::ProgramRunArguments{}, "123", "123", 0);
		vm::api::deinitAndValidate(pid);
	}

	void runNoArgFunction() {
		vm::PID  pid = initProcess();
		fs::File file(path("call_no_arg_function.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file }));

		runAndCheckReturnValue(pid, "summer", vm::FunctionRunArguments{}, {}, "735", {});
		vm::api::deinitAndValidate(pid);
	}

	void runVoidFunction() {
		vm::PID  pid = initProcess();
		fs::File file(path("call_void_function.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file }));

		OwnedArgumentList owned_arguments = getOwnedArgumentList(pid, { 1, 2 });
		runAndCheckReturnValue(pid, "summer", createArgumentList(owned_arguments), {}, "3", {});
		freeArguments(owned_arguments);
		vm::api::deinitAndValidate(pid);
	}

	void runNonVoidFunction() {
		vm::PID  pid = initProcess();
		fs::File file(path("call_non_void_function.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file }));

		OwnedArgumentList owned_arguments = getOwnedArgumentList(pid, { 695, 40 });
		runAndCheckReturnValue(pid, "summer", createArgumentList(owned_arguments), {}, {}, 735);
		freeArguments(owned_arguments);
		vm::api::deinitAndValidate(pid);
	}

	void runMultipleReturnValuesFunction() {
		vm::PID  pid = initProcess();
		fs::File file(path("multiple_retvals.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file }));

		OwnedArgumentList owned_arguments = getOwnedArgumentList(pid, { 40, 695 });
		runAndCheckReturnValues(
			pid,
			"unsafe_div",
			createArgumentList(owned_arguments),
			{},
			{},
			base::Optional<std::vector<i64>>{ { 17, 15 } }
		);

		// Double run to check if the frame is reset correctly.
		runAndCheckReturnValues(
			pid,
			"unsafe_div",
			createArgumentList(owned_arguments),
			{},
			{},
			base::Optional<std::vector<i64>>{ { 17, 15 } }
		);
		freeArguments(owned_arguments);
		vm::api::deinitAndValidate(pid);
	}

	void doubleRunFunction() {
		vm::PID  pid = initProcess();
		fs::File file1(path("repl_1.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file1 }));

		OwnedArgumentList owned_arguments = getOwnedArgumentList(pid, { 4, 8 });
		runAndCheckReturnValue(pid, "spring", createArgumentList(owned_arguments), {}, {}, 32);
		OwnedArgumentList owned_arguments2 = getOwnedArgumentList(pid, { 4, 6 });
		runAndCheckReturnValue(pid, "spring", createArgumentList(owned_arguments2), {}, {}, 24);
		freeArguments(owned_arguments);
		freeArguments(owned_arguments2);
		vm::api::deinitAndValidate(pid);
	}

	void manyRunFunctions() {
		vm::PID  pid = initProcess();
		fs::File file(path("repl_1.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file }));

		for (i32 i = 0; i < 100; i++) {
			OwnedArgumentList owned_arguments = getOwnedArgumentList(pid, { i, i });
			runAndCheckReturnValue(
				pid, "spring", createArgumentList(owned_arguments), {}, {}, i * i
			);
			freeArguments(owned_arguments);
		}
		vm::api::deinitAndValidate(pid);
	}

	void repl() {
		vm::PID pid = initProcess();

		fs::File file1(path("repl_1.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file1 }));
		OwnedArgumentList owned_arguments = getOwnedArgumentList(pid, { 4, 8 });
		runAndCheckReturnValue(pid, "spring", createArgumentList(owned_arguments), {}, {}, 32);
		freeArguments(owned_arguments);

		fs::File file2(path("repl_2.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file2 }));
		OwnedArgumentList owned_arguments2 = getOwnedArgumentList(pid, { 1, 2 });
		runAndCheckReturnValue(pid, "summer", createArgumentList(owned_arguments2), {}, {}, 3);
		freeArguments(owned_arguments2);
		vm::api::deinitAndValidate(pid);
	}

	void replWithGlobals() {
		vm::PID  pid = initProcess();
		fs::File file1(path("repl_with_globals_1.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file1 }));
		runAndCheckReturnValue(pid, "globaler_setter", vm::FunctionRunArguments{}, "1 2", {}, {});

		fs::File file2(path("repl_with_globals_2.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file2 }));
		runAndCheckReturnValue(pid, "globaler_reader", vm::FunctionRunArguments{}, {}, "12", {});
		vm::api::deinitAndValidate(pid);
	}

	void incrementalGlobalVariantPersistsValue() {
		vm::PID  pid = initProcess();
		fs::File file1(path("incremental_global_variant_1.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file1 }));

		runAndCheckReturnValue(
			pid, "set_global_variant_value", vm::FunctionRunArguments{}, {}, {}, {}
		);

		fs::File file2(path("incremental_global_variant_2.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file2 }));

		runAndCheckReturnValue(
			pid, "read_global_variant_value", vm::FunctionRunArguments{}, {}, "735", {}
		);
		vm::api::deinitAndValidate(pid);
	}

	void cyclicRepl() {
		vm::PID pid = initProcess();

		fs::File file1(path("loaded_func_call_1.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file1 }));
		OwnedArgumentList owned_arguments = getOwnedArgumentList(pid, { 4, 8 });
		runAndCheckReturnValue(pid, "summer", createArgumentList(owned_arguments), {}, {}, 12);
		freeArguments(owned_arguments);

		fs::File file2(path("loaded_func_call_2.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file2 }));
		OwnedArgumentList owned_arguments2 = getOwnedArgumentList(pid, { 2, 3 });
		runAndCheckReturnValue(pid, "spring", createArgumentList(owned_arguments2), {}, {}, 10);
		freeArguments(owned_arguments2);
		vm::api::deinitAndValidate(pid);
	}

	void separateGlobals() {
		vm::PID  pid = initProcess();
		fs::File file1(path("separate_globals_1.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file1 }));

		fs::File file2(path("separate_globals_2.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file2 }));
		runAndCheckReturnValue(pid, "globaler_setter", vm::FunctionRunArguments{}, "12", "12", {});
		vm::api::deinitAndValidate(pid);
	}

	void injectExistingFunction() {
		vm::PID  pid = initProcess();
		fs::File file(path("inject_code_1.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file }));
		ASSERT_NO_VALUE(vm::api::loadFiles(pid, { file }));
		vm::api::deinitAndValidate(pid);
	}

	void runFunctionArgumentValidation() {
		vm::PID  pid = initProcess();
		fs::File file(path("call_non_void_function.dbc"));
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file }));
		{
			// Not enough arguments.
			OwnedArgumentList arguments;
			arguments.push_back(getIntVmValue(pid, 10));
			auto func_args = createArgumentList(arguments);

			assertExecutionPanickedWith(
				runFunctionExpectPanic(pid, "summer", func_args),
				"expects 2 arguments, but 1 were provided"
			);

			freeArguments(arguments);
		}
		{
			// Too many arguments.
			OwnedArgumentList arguments;
			arguments.push_back(getIntVmValue(pid, 10));
			arguments.push_back(getIntVmValue(pid, 20));
			arguments.push_back(getIntVmValue(pid, 30));
			auto func_args = createArgumentList(arguments);

			assertExecutionPanickedWith(
				runFunctionExpectPanic(pid, "summer", func_args),
				"expects 2 arguments, but 3 were provided"
			);

			freeArguments(arguments);
		}

		{
			// Argument type mismatch.
			OwnedArgumentList arguments;
			arguments.push_back(getIntVmValue(pid, 10));

			auto i32_value = vm::api::getVmValue(pid, "i32");
			ASSERT_HAS_VALUE(i32_value);
			arguments.push_back(std::move(i32_value->vm_value));

			auto func_args = createArgumentList(arguments);

			assertExecutionPanickedWith(
				runFunctionExpectPanic(pid, "summer", func_args),
				"Type mismatch for argument 1 of function 'summer': expected i64, got i32"
			);

			freeArguments(arguments);
		}

		{
			// VMValue from different process.
			vm::PID other_pid = initProcess();

			OwnedArgumentList arguments;
			arguments.push_back(getIntVmValue(pid, 5));
			arguments.push_back(getIntVmValue(other_pid, 99));
			auto func_args = createArgumentList(arguments);

			assertExecutionPanickedWith(
				runFunctionExpectPanic(pid, "summer", func_args),
				"VMValue for argument 1 comes from a different process"
			);

			freeArguments(arguments);
			vm::api::deinitAndValidate(other_pid);
		}

		vm::api::deinitAndValidate(pid);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/injection/");
