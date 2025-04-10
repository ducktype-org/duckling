#pragma once

#include <tester/tester.hpp>

#include <vm/api/api.hpp>


#define VM_TESTER_TEST_SIMPLE_CONSTRUCTOR(...)                            \
	TESTER_CLASS(tester::TestConfig&& config __VA_OPT__(, ) __VA_ARGS__): \
		  VmTestSuite(std::move(config), TESTER_SUITE_NAME)

class VmTestSuite: public tester::TestSuite {
public:
	VmTestSuite(tester ::TestConfig&& config, std::string_view name):
		  tester::TestSuite(std::move(config), name) {}

protected:
	void runTestOnVm(
		const std::string& rbc_filename, const std::string& input, const std::string& output
	);

	/**
	 * @brief Parses a file containing a program which violates syntactic or static verification
	 * guidelines. the error message.
	 */
	void loadInvalidDbc(
		const std::string& dbc_filename, const std::vector<std::string_view>& error_keywords
	);

	/**
	 * @brief Parses a file containing a program which does not violate syntactic and static
	 * verification guidelines.
	 */
	void loadValidDbc(const std::string& dbc_filename);
};
