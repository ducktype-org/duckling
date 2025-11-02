#include <vm_tester_utils.hpp>

#include <string>

class VmPointersTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmPointersTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(linkedListTest);
		TESTER_ADD_TEST(linkedListNoStructLoadStoreTest);
	}

private:
	void linkedListTest() { runTestOnVm("linked_list.dbc", "3 11 22 33", "332211"); }

	void linkedListNoStructLoadStoreTest() {
		runTestOnVm("linked_list_no_struct_load_store.dbc", "3 11 22 33", "332211");
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/pointers/");
