#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/core/safe/exceptions.hpp>

class VmMemoryTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmMemoryTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(localLeakTest);
		TESTER_ADD_TEST(globalLeakTest);
		TESTER_ADD_TEST(noDoubleDestructorCalls);
		TESTER_ADD_TEST(nestedLeaks);
		TESTER_ADD_TEST(nestedPtrInTableLeak);
	}

private:
	void localLeakTest() {
		assertExecutionPanickedWith(
			runTestOnVmGetResult("local_leak.dbc", "", ""),
			vm::exceptions::VMFoundMemoryLeakException::ERR_MSG
		);
	}

	void globalLeakTest() {
		const auto result = runTestOnVmGetResult("global_leak.dbc", "", "");
		ASSERT_HAS_VALUE(result.run_result);
		const auto validation_result = vm::api::deinitAndValidate(result.pid);
		ASSERT_HAS_VALUE(validation_result);
		ASSERT_TRUE(validation_result.value() == false);
	}

	void noDoubleDestructorCalls() { runTestOnVm("no_double_destructor.dbc", "", ""); }

	void nestedLeaks() {
		assertExecutionPanickedWith(
			runTestOnVmGetResult("nested_leaks.dbc", "", ""),
			vm::exceptions::VMFoundMemoryLeakException::ERR_MSG
		);
	}

	// #3225: a pointer nested inside a struct inside a table is not visited by
	// the destructor walk (iterateOverDataAndExecute is one level deep).
	// After the fix the nested pointer is found and properly released.
	void nestedPtrInTableLeak() {
		const auto result = runTestOnVmGetResult("nested_ptr_in_table.dbc", "", "");
		ASSERT_HAS_VALUE(result.run_result);
		const auto validation_result = vm::api::deinitAndValidate(result.pid);
		ASSERT_HAS_VALUE(validation_result);
		ASSERT_TRUE(validation_result.value() == true);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/memory/");
