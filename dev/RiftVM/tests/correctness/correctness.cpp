#include <vm_tester_utils.hpp>

class VmCorrectnessTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmCorrectnessTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR("VM Correctness Tests") {
		TESTER_ADD_TEST(test_ackermann_old);
		TESTER_ADD_TEST(test_ackermann_new);
		TESTER_ADD_TEST(test_collatz);
		TESTER_ADD_TEST(test_fib_iter);
		TESTER_ADD_TEST(test_fib_rec);
		TESTER_ADD_TEST(test_tailcall);
	}

private:
	// Note: these tests treat 16f17da CG as a reference
	// TODO: add more inputs and some corner cases
	void test_ackermann_old() { runTestOnVm("ackermann_old.rbc", "3 3", "61"); }

	void test_ackermann_new() { runTestOnVm("ackermann_new.rbc", "3 3", "61"); }

	void test_collatz() { runTestOnVm("collatz.rbc", "424242", "24648077896"); }

	void test_fib_iter() { runTestOnVm("fib_iter.rbc", "1000000 10000", "6875"); }

	void test_fib_rec() { runTestOnVm("fib_rec.rbc", "32", "2178309"); }

	void test_tailcall() { runTestOnVm("tailcall.rbc", "1000000", "0"); }
};

TESTER_COMMON_MAIN("/RiftVM/tests/correctness/");
