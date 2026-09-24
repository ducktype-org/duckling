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
		TESTER_ADD_TEST(variantDestructor);
		TESTER_ADD_TEST(doubleFreeIsRefused);
		TESTER_ADD_TEST(dynTableReAllocAfterFreeIsRefused);
		TESTER_ADD_TEST(dynTableFreeAfterFreeIsRefused);
	}

private:
	void localLeakTest() {
		assertExecutionPanickedWithAndKill(
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
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("nested_leaks.dbc", "", ""),
			vm::exceptions::VMFoundMemoryLeakException::ERR_MSG
		);
	}

	void variantDestructor() { runTestOnVm("variant_destructor.dbc", "", ""); }

	void doubleFreeIsRefused() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("double_free.dbc", "", ""),
			vm::exceptions::VMDoubleFreeException::ERR_MSG
		);
	}

	void dynTableReAllocAfterFreeIsRefused() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("dyn_table_realloc_after_free.dbc", "", ""),
			vm::exceptions::VMUseAfterFreeException::ERR_MSG
		);
	}

	void dynTableFreeAfterFreeIsRefused() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("dyn_table_free_after_free.dbc", "", ""),
			vm::exceptions::VMUseAfterFreeException::ERR_MSG
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/memory/");
