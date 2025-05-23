#include <tester/tester.hpp>
#include <vm_tester_utils.hpp>

#include <base/variant.hpp>

#include <vm/api/data/status.hpp>
#include <vm/api/vm.hpp>
#include <vm/bytecode/validator/errors.hpp>

class VmVariantTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmVariantTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(verySimpleVariant);
		TESTER_ADD_TEST(simpleVariant0);
		TESTER_ADD_TEST(simpleVariant1);
		TESTER_ADD_TEST(simpleVariant2);
		TESTER_ADD_TEST(blocksDontDisappearTest);
		TESTER_ADD_TEST(nestedVariantTest);
		TESTER_ADD_TEST(variantInsideStruct);
		TESTER_ADD_TEST(emptyVariant);
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

	void verySimpleVariant() { runTestOnVm("very_simple_variant.dbc", "42", "42"); }

	void simpleVariant0() { runTestOnVm("simple_variant.dbc", "0", "13"); }

	void simpleVariant1() {
		assertExecutionPanickedWith(
			runTestOnVmGetResult("simple_variant.dbc", "1", "13"), "Copying to/from null pointer"
		);
	}

	void simpleVariant2() {
		assertExecutionPanickedWith(
			runTestOnVmGetResult("simple_variant.dbc", "2", "13"), "Data was freed"
		);
	}

	void blocksDontDisappearTest() { runTestOnVm("variant_blocks_dont_disappear.dbc", "5", "5"); }

	void nestedVariantTest() {
		runTestOnVm("nested.dbc", "15", "15");
		assertExecutionPanickedWith(
			runTestOnVmGetResult("nested_failing.dbc", "15", "15"), "Data was freed"
		);
	}

	void variantInsideStruct() { runTestOnVm("inside_struct.dbc"); }

	void emptyVariant() {
		loadInvalidDbc("empty_variant.dbc", { vm::code::EmptyVariantError::ERR_MSG });
	}
};

TESTER_COMMON_MAIN("/vm/tests/variant/");
