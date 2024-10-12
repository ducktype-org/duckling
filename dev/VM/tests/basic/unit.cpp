#include <vm_tester_utils.hpp>

class VmUnitTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmUnitTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(init_primitives_with_zero); }

private:
	void init_primitives_with_zero() { runTestOnVm("init_primitives_with_zero.dbc", "", "0"); }
};

TESTER_COMMON_MAIN("/VM/tests/basic/");
