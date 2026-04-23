#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/bytecode/validator/errors.hpp>

/**
 * @brief Tests here invoke another function multiple times, inducing the 
 * necessary call-count, and allowing for it to be jit-compiled.
 */
class JitTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS JitTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testArithmetic);
		TESTER_ADD_TEST(testJumps);
	}

private:
	void testArithmetic() {
		runTestOnVm("arithmetic_ops.dbc", "100");
	}

	void testJumps() {
		runTestOnVm("jmp_test.dbc", "7", "33366612");
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/jit/");
