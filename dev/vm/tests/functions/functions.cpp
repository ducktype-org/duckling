#include <vm_tester_utils.hpp>

class VmFunctionsTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmFunctionsTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(test_simple_function_call);
		TESTER_ADD_TEST(test_simple_return_value);
		TESTER_ADD_TEST(test_return_l32);
		TESTER_ADD_TEST(test_different_sized_params);
		TESTER_ADD_TEST(test_double_call);
		TESTER_ADD_TEST(test_recurence);
		TESTER_ADD_TEST(test_many_functions);
		TESTER_ADD_TEST(test_preserved_flag);
	}

private:
	void test_simple_function_call() {
		runTestOnVm("simple_function_call.dbc", "18", "18");
		runTestOnVm("simple_function_call.dbc", "1234", "1234");
	}

	void test_simple_return_value() {
		runTestOnVm("simple_return_value.dbc", "18", "18");
		runTestOnVm("simple_return_value.dbc", "1234", "1234");
	}

	void test_return_l32() {
		runTestOnVm("return_l32.dbc", "18", "18");
		runTestOnVm("return_l32.dbc", "1234", "1234");
	}

	void test_different_sized_params() {
		runTestOnVm("different_sized_params.dbc", "5 1 5", "25");
		runTestOnVm("different_sized_params.dbc", "42 1 31", "1302");
	}

	void test_double_call() {
		runTestOnVm("double_call.dbc", "1 2 3 4", "10");
		runTestOnVm("double_call.dbc", "123 456 789 100", "1468");
	}

	void test_recurence() {
		runTestOnVm("rec_func_sum.dbc", "20", "210");
		runTestOnVm("rec_func_sum.dbc", "100", "5050");
	}

	void test_many_functions() {
		runTestOnVm("many_functions.dbc", "5", "1115");
		runTestOnVm("many_functions.dbc", "21", "1131");
	}

	void test_preserved_flag() { runTestOnVm("preserved_flag.dbc", "", "1"); }
};

TESTER_COMMON_MAIN("/vm/tests/functions/");
