#include <vm_tester_utils.hpp>

class StaticTableVmTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StaticTableVmTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(oneValue);
		TESTER_ADD_TEST(arrSum);
		TESTER_ADD_TEST(initWithZero);
	}

private:
	void oneValue() { runTestOnVm("one_value.dbc", "", "0"); }

	void arrSum() { runTestOnVm("arr_sum.dbc", "", "903"); }

	void initWithZero() { runTestOnVm("init_with_zero.dbc", "", "0"); }
};

TESTER_COMMON_MAIN("/vm/tests/static_table/");
