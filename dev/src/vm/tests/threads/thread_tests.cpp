
#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/core/process/exceptions.hpp>

class VmThreadTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmThreadTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(multithreadingTest);
		TESTER_ADD_TEST(mutexTest);
	}

private:
	void multithreadingTest() {
		// We don't check result here - only if it finished sucessfully
		// That's because this code is purposefully not deterministic - it has data race
		runTestOnVm("multithreading.dbc", "", {}, {});
	}

	void mutexTest() {
		// On the contrary here, this test should give clear result
		runTestOnVmGetResult("mutex.dbc", "", "20000", {});
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/threads/");
