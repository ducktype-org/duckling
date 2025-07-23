#include <vm_tester_utils.hpp>

class FixedSizeTableVmTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS FixedSizeTableVmTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(oneValue);
		TESTER_ADD_TEST(arrSum);
		TESTER_ADD_TEST(initWithZero);
		TESTER_ADD_TEST(lea);
	}

private:
	void oneValue() { runTestOnVm("one_value.dbc", "", "0", {}, 3); }

	void arrSum() { runTestOnVm("arr_sum.dbc", "", "903", {}); }

	void initWithZero() { runTestOnVm("init_with_zero.dbc", "", "0", {}); }

	void lea() { runTestOnVm("lea.dbc", "", "42", {}); }
};

TESTER_COMMON_MAIN("/src/vm/tests/fixed_size_table/");
