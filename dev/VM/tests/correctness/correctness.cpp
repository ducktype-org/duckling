#include <vm_tester_utils.hpp>

class VmCorrectnessTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmCorrectnessTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(test_tailcall);
		TESTER_ADD_TEST(test_ackermann_old);
		TESTER_ADD_TEST(test_ackermann_new);
		TESTER_ADD_TEST(test_collatz);
		TESTER_ADD_TEST(test_fib_iter);
		TESTER_ADD_TEST(test_fib_rec);
	}

private:
	// Note: these tests treat 16f17da CG as a reference
	// TODO: add more inputs and some corner cases
	void test_ackermann_old() { runTestOnVm("ackermann_old.dbc", "3 3", "61"); }

	void test_ackermann_new() { runTestOnVm("ackermann_new.dbc", "3 3", "61"); }

	void test_collatz() { runTestOnVm("collatz.dbc", "424242", "24648077896"); }

	void test_fib_iter() { runTestOnVm("fib_iter.dbc", "1000000 10000", "6875"); }

	void test_fib_rec() { runTestOnVm("fib_rec.dbc", "32", "2178309"); }

	void test_tailcall() { runTestOnVm("tailcall.dbc", "1000000", "0"); }
};

TESTER_COMMON_MAIN("/VM/tests/correctness/");
