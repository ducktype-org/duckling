#include <vm_tester_utils.hpp>

class VmFunctionsTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmFunctionsTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(test_ret_l64_opcode);
		TESTER_ADD_TEST(test_ret_imm_opcode);
		TESTER_ADD_TEST(test_setFstArg_l64_opcode);
		TESTER_ADD_TEST(test_recurence);
		TESTER_ADD_TEST(test_many_functions);
		TESTER_ADD_TEST(test_preserved_flag);
	}

private:
	void test_ret_l64_opcode() {
		runTestOnVm("ret_l64.dbc", "17", "17");
		runTestOnVm("ret_l64.dbc", "2100", "2100");
	}

	void test_ret_imm_opcode() { runTestOnVm("ret_imm.dbc", "", "17"); }

	void test_setFstArg_l64_opcode() {
		runTestOnVm("setFstArg_l64.dbc", "18", "18");
		runTestOnVm("setFstArg_l64.dbc", "4200", "4200");
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
