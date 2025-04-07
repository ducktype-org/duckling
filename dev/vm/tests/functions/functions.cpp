#include <vm_tester_utils.hpp>

class VmFunctionsTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmFunctionsTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testSimpleFunctionCall);
		TESTER_ADD_TEST(testSimpleReturnValue);
		TESTER_ADD_TEST(testReturnL32);
		TESTER_ADD_TEST(testDifferentSizedParams);
		TESTER_ADD_TEST(testDoubleCall);
        // @TODO should this be allowed or not?
		// TESTER_ADD_TEST(testDeinitializeReturnValue);
		TESTER_ADD_TEST(testRecurence);
		TESTER_ADD_TEST(testManyFunctions);
		TESTER_ADD_TEST(testPreservedFlag);
	}

private:
	void testSimpleFunctionCall() {
		runTestOnVm("simple_function_call.dbc", "18", "18");
		runTestOnVm("simple_function_call.dbc", "1234", "1234");
	}

	void testSimpleReturnValue() {
		runTestOnVm("simple_return_value.dbc", "18", "18");
		runTestOnVm("simple_return_value.dbc", "1234", "1234");
	}

	void testReturnL32() {
		runTestOnVm("return_l32.dbc", "18", "18");
		runTestOnVm("return_l32.dbc", "1234", "1234");
	}

	void testDifferentSizedParams() {
		runTestOnVm("different_sized_params.dbc", "5 1 5", "25");
		runTestOnVm("different_sized_params.dbc", "42 1 31", "1302");
	}

	void testDoubleCall() {
		runTestOnVm("double_call.dbc", "1 2 3 4", "10");
		runTestOnVm("double_call.dbc", "123 456 789 100", "1468");
	}

	void testDeinitializeReturnValue() {
		runTestOnVm("deinit_ret_val.dbc", "", "42");
		runTestOnVm("deinit_main_ret_val.dbc", "", "42");
	}

	void testRecurence() {
		runTestOnVm("rec_func_sum.dbc", "20", "210");
		runTestOnVm("rec_func_sum.dbc", "100", "5050");
	}

	void testManyFunctions() {
		runTestOnVm("many_functions.dbc", "5", "1115");
		runTestOnVm("many_functions.dbc", "21", "1131");
	}

	void testPreservedFlag() { runTestOnVm("preserved_flag.dbc", "", "1"); }
};

TESTER_COMMON_MAIN("/vm/tests/functions/");
