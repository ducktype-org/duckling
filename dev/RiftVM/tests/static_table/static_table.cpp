#include <vm_tester_utils.hpp>

class StaticTableVmTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StaticTableVmTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR("Static Table VM Test") {
		TESTER_ADD_TEST(oneValue);
		TESTER_ADD_TEST(arrSum);
		TESTER_ADD_TEST(initWithZero);
	}

private:
	void oneValue() { runTestOnVm("one_value.rbc", "", "0"); }

	void arrSum() { runTestOnVm("arr_sum.rbc", "", "903"); }

	void initWithZero() { runTestOnVm("init_with_zero.rbc", "", "0"); }
};

TESTER_COMMON_MAIN("/RiftVM/tests/static_table/");
