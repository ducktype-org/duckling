#include <tester/tester.hpp>
#include <vm_tester_utils.hpp>

#include "base/variant.hpp"

#include "vm/api/data/status.hpp"
#include "vm/api/vm.hpp"

class VmVariantTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmVariantTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simpleVariant);
		TESTER_ADD_TEST(blocksDontDisappearTest);
	}

private:
	void assertExecutionPanickedWith(const TestResult& test_result, std::string_view err_piece) {
		std::string local_error        = "";
		auto        exec_status_result = vm::api::getExecutionStatus(test_result.pid);
		ASSERT_TRUE(exec_status_result.has_value());

		variant_match(exec_status_result.value()) {
			variant_case(vm::api::Executing, executing) {
				variant_match(executing.exec_status) {
					variant_case(vm::api::ExecutionPanicked, panicked) {
						ASSERT_TRUE(panicked.error_message.contains(err_piece));
					}
					variant_default {
						fail(base::strConcat(
							"Expected ",
							TypeParseTraits<vm::api::ExecutionPanicked>::name.data(),
							", but found: " + to_string(nlohmann::json(executing))
						));
					}
				}
			}
			variant_default {
				fail(base::strConcat(
					"Expected ",
					TypeParseTraits<vm::api::Executing>::name.data(),
					", but found: " + to_string(nlohmann::json(test_result.run_result.error()))
				));
			}
		}
	}

	void simpleVariant() {
		runTestOnVm("simple_variant.dbc", "0", "13");
		assertExecutionPanickedWith(
			runTestOnVmGetResult("simple_variant.dbc", "1", "13"), "Accessing null pointer"
		);
		assertExecutionPanickedWith(
			runTestOnVmGetResult("simple_variant.dbc", "2", "13"), "Data was freed"
		);
	}

	void blocksDontDisappearTest() { runTestOnVm("variant_blocks_dont_disappear.dbc", "5", "5"); }
};

TESTER_COMMON_MAIN("/vm/tests/variant/");
