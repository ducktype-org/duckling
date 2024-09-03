#pragma once

#include <api/api.hpp>
#include <tester/tester.hpp>


#define VM_TESTER_TEST_SIMPLE_CONSTRUCTOR(name, ...)                      \
	TESTER_CLASS(tester::TestConfig&& config __VA_OPT__(, ) __VA_ARGS__): \
		  VmTestSuite(std::move(config), name)

class VmTestSuite: public tester::TestSuite {
public:
	VmTestSuite(tester ::TestConfig&& config, std::string_view name):
		  tester ::TestSuite(std ::move(config), name){};

protected:
	void runTestOnVm(
		const std::string& dbc_filename, const std::string& input, const std::string& output
	);
};
