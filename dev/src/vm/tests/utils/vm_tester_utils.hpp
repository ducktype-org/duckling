#pragma once

#include <tester/tester.hpp>

#include <vm/api/api.hpp>
#include <vm/api/data/api_error.hpp>

#include <expected>


#define VM_TESTER_TEST_SIMPLE_CONSTRUCTOR(...)                            \
	TESTER_CLASS(tester::TestConfig&& config __VA_OPT__(, ) __VA_ARGS__): \
		  VmTestSuite(std::move(config), TESTER_SUITE_NAME)

class VmTestSuite: public tester::TestSuite {
public:
	VmTestSuite(tester::TestConfig&& config, std::string_view name):
		  tester::TestSuite(std::move(config), name) {}

protected:
	struct TestResult {
		vm::PID                               pid;
		std::expected<i64, vm::api::ApiError> run_result;  // exit code or error
	};

protected:
	vm::PID initProcess(vm::api::ExecutionConfig config = {});

	void handleTestResult(const TestResult& test_result, i64 exit_code);

	/**
	 * @brief Runs a program from a given filepath with the specified input and command-line
	 * arguments. Asserts that the actual output matches the expected one.
	 */
	void runTestOnVm(
		const std::string&                 dbc_filename,
		const base::Optional<std::string>& optional_input  = {},
		const base::Optional<std::string>& optional_output = {},
		const std::vector<std::string>&    args            = {},
		i64                                exit_code       = 0
	);
	/**
	 * @brief Same as above, but the program is given as an argument
	 */
	void runTestOnVm(
		const vm::code::CodeCollection&    code,
		const base::Optional<std::string>& optional_input  = {},
		const base::Optional<std::string>& optional_output = {},
		const std::vector<std::string>&    args            = {},
		i64                                exit_code       = 0
	);

	void runTestOnVm(
		vm::PID                            pid,
		const base::Optional<std::string>& optional_input  = {},
		const base::Optional<std::string>& optional_output = {},
		const std::vector<std::string>&    args            = {},
		i64                                exit_code       = 0
	);

	TestResult runTestOnVmGetResult(
		const std::string&                 dbc_filename,
		const base::Optional<std::string>& optional_input  = {},
		const base::Optional<std::string>& optional_output = {},
		const std::vector<std::string>&    args            = {}
	);

	TestResult runTestOnVmGetResult(
		const vm::code::CodeCollection&    code,
		const base::Optional<std::string>& optional_input  = {},
		const base::Optional<std::string>& optional_output = {},
		const std::vector<std::string>&    args            = {}
	);

	TestResult runTestOnVmGetResult(
		vm::PID                            pid,
		const base::Optional<std::string>& optional_input  = {},
		const base::Optional<std::string>& optional_output = {},
		const std::vector<std::string>&    args            = {}
	);

	/**
	 * @brief Runs a function synchronously and checks its exit code, output and input.
	 * This is an alternative to `runTestOnVm` which allows to run a specific function instead of
	 * the main function and pass arguments to it.
	 * @param pid Process ID of the process to run the function on.
	 * @param func_name Name of the function to run
	 * @param args Arguments to pass to the function
	 * @param optional_input If provided, the function will send this string as input to the process
	 * @param optional_output If provided, the function will check if the process output is equal to
	 * this string
	 * @param expected_exit_code If provided, the function will check if the process exit code is
	 * equal to this value. If not provided, it will check if the exit code is of type void.
	 */
	void runFunctionSynchronouslyAsTest(
		vm::PID                            pid,
		const std::string&                 func_name          = {},
		const vm::FunctionRunArguments&    args               = {},
		const base::Optional<std::string>& optional_input     = {},
		const base::Optional<std::string>& optional_output    = {},
		const base::Optional<i64>          expected_exit_code = {}
	);

	TestResult runFunctionExpectPanic(
		vm::PID pid, const std::string& func_name, const vm::FunctionRunArguments& args
	);

	void assertExecutionPanickedWith(const TestResult& test_result, std::string_view err_piece);

	/**
	 * @brief Loads a file containing a program which violates syntactic or static verification
	 * guidelines. Asserts what error keywords are present in the error message.
	 */
	void loadInvalidDbc(
		const std::string& dbc_filename, const std::vector<std::string_view>& error_keywords, vm::api::ExecutionConfig config = {}
	);

	/**
	 * @brief Loads a file containing a valid bytecode program and asserts it was loaded correctly.
	 */
	void loadValidDbc(const std::string& dbc_filename, vm::api::ExecutionConfig config = {});
};
