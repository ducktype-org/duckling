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
		// No instructions
		TESTER_ADD_TEST(testEmpty);

		// Only the simplest operations
		TESTER_ADD_TEST(testArithmetic);

		// Executing code within a compiled function
		TESTER_ADD_TEST(testUnconditionalJumps);
		TESTER_ADD_TEST(testConditionalJumps);
		TESTER_ADD_TEST(testJumps);

		// Executing code outside of a compiled function
		TESTER_ADD_TEST(testCalls);
		TESTER_ADD_TEST(testVirtualCalls);
		TESTER_ADD_TEST(testRecursiveCalls);

		// Skipping exts
		TESTER_ADD_TEST(testFixedArray);
		TESTER_ADD_TEST(testDynamicArray);
		TESTER_ADD_TEST(testVariant);

		// Many things combined
		TESTER_ADD_TEST(testAll);
	}

private:
	void testEmpty() {
		runTestOnVm("empty_test.dbc", "10", "");
	}

	void testArithmetic() {
		runTestOnVm("arithmetic_test.dbc", "10", "");
	}

	void testJumps() {
		runTestOnVm("jmp_test.dbc", "10", "3333666121212");
	}

	void testUnconditionalJumps() {
		runTestOnVm("jmp_cond_test.dbc", "10", "");
	}

	void testConditionalJumps() {
		runTestOnVm("jmp_cond_test.dbc", "10", "");
	}

	void testCalls() {
		runTestOnVm("call_test.dbc", "10", "1110110911081107110611051104110311021101");
	}

	void testVirtualCalls() {
		runTestOnVm("call_virtual_test.dbc", "10", "124124124124124124124124124124");
	}
	
	void testRecursiveCalls() {
		runTestOnVm("call_recursive_test.dbc", "10", "55342113853211");
	}

	void testFixedArray() {
		runTestOnVm("fixed_array_test.dbc", "10", "");
	}

	void testDynamicArray() {
		runTestOnVm("dynamic_array_test.dbc", "10", "");
	}

	void testVariant() {
		runTestOnVm("variant_test.dbc", "10", "");
	}

	void testAll() {
		runTestOnVm("mega_test.dbc", "10", "3333333333333333666666666666121212121212121212121212");
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/jit/");
