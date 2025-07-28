#include <vm_tester_utils.hpp>

class DynamicTableVmTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DynamicTableVmTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(dynArrSum);
		TESTER_ADD_TEST(lea);
	}

private:
	void dynArrSum() { runTestOnVm("dyn_arr_sum.dbc", "", "55", {}); }

	void lea() { runTestOnVm("lea.dbc", "", "4", {}); }
};

TESTER_COMMON_MAIN("/src/vm/tests/dynamic_table/");
