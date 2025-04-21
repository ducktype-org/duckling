#pragma once

#include <tester/tester.hpp>

#include <vm/api/api.hpp>
#include <vm/bytecode/bytecode.hpp>


#define VM_TESTER_TEST_SIMPLE_CONSTRUCTOR(...)                            \
	TESTER_CLASS(tester::TestConfig&& config __VA_OPT__(, ) __VA_ARGS__): \
		  VmTestSuite(std::move(config), TESTER_SUITE_NAME)

class VmTestSuite: public tester::TestSuite {
public:
	VmTestSuite(tester ::TestConfig&& config, std::string_view name):
		  tester::TestSuite(std::move(config), name) {}

private:
	vm::PID initProcess();

	void runTestImpl(
		vm::PID                            pid,
		const base::Optional<std::string>& optional_input,
		const base::Optional<std::string>& optional_output,
		i64                                exit_code
	);

protected:
	void runTestOnVm(
		const std::string&                 rbc_filename,
		const base::Optional<std::string>& optional_input  = {},
		const base::Optional<std::string>& optional_output = {},
		i64                                exit_code       = 0
	);

	void runTestOnVm(
		const vm::code::CodeCollection&    code,
		const base::Optional<std::string>& optional_input  = {},
		const base::Optional<std::string>& optional_output = {},
		i64                                exit_code       = 0
	);

	/**
	 * @brief Loads a file containing a program which violates syntactic or static verification
	 * guidelines. Asserts what error keywords are present in the error message.
	 */
	void loadInvalidDbc(
		const std::string& dbc_filename, const std::vector<std::string_view>& error_keywords
	);

	/**
	 * @brief Loads a file containing a valid bytecode program and asserts it was loaded correctly.
	 */
	void loadValidDbc(const std::string& dbc_filename);
};
