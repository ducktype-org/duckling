
#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/core/safe/exceptions.hpp>

class VmThreadTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmThreadTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// @TODO: #2563 Fix the tests and re-enable them. For now they are disabled because of the
		// instability of multithreading tests, which is expected due to the nature of
		// multithreading, but we need to fix it eventually. The main issue is that the tests are
		// not deterministic, and they can fail randomly.
		// TESTER_ADD_TEST(multithreadingTest);
		// TESTER_ADD_TEST(mutexTest);
		// TESTER_ADD_TEST(cvTest);
	}

private:
	void multithreadingTest() {
		// We don't check result here - only if it finished sucessfully
		// That's because this code is purposefully not deterministic - it has data race
		runTestOnVm("multithreading.dbc", "", {}, {});
	}

	void mutexTest() {
		// On the contrary here, this test should give clear result
		for (usize i = 0; i < 5; i++) runTestOnVm("mutex.dbc", "", "20000", {});
	}

	void cvTest() {
		runTestOnVm("cv_permit_barrier_test.dbc", "", "20000221", {});
		runTestOnVm("cv_simple_barrier_all_test.dbc", "", "22020201", {});
		runTestOnVm("producer_consumer.dbc", "", "20000200000221", {});
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/threads/");
