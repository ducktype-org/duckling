#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/bytecode/validator/errors.hpp>

class VmMemoryTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmMemoryTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(localLeakTest);
		TESTER_ADD_TEST(globalLeakTest);
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
		ASSERT_TRUE(result.run_result.has_value());
		const auto validation_result = vm::api::deinitAndValidate(result.pid);
		ASSERT_TRUE(validation_result.has_value());
		ASSERT_TRUE(validation_result.value() == false);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/memory/");
