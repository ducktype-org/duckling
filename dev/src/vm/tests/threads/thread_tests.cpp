
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
        runTestOnVmGetResult("demo2.dbc", "", "20000", {});
	}

    void mutexTest(){
        runTestOnVmGetResult("demo4.dbc", "", "20000", {});
    }

};

TESTER_COMMON_MAIN("/src/vm/tests/threads/");
