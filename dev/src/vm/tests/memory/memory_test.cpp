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
		TESTER_ADD_TEST(deinitAfterPanicIsRefused);
		TESTER_ADD_TEST(deinitOrKillTearsDownEitherWay);
		TESTER_ADD_TEST(deinitOrKillFallsBackToKillWhenDeinitFails);
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

	void variantDestructor() { runTestOnVm("variant_destructor.dbc", "", ""); }

	// Note: Those will be moved to `api_test` in the next PR. I just needed a place to put them in.

	/// refused deinit should leave the process alive, so its we can still kill it.
	void deinitAfterPanicIsRefused() {
		const auto result = runTestOnVmGetResult("local_leak.dbc", "", "");
		ASSERT_NO_VALUE(result.run_result);

		const auto deinit = vm::api::deinitAndValidate(result.pid);
		ASSERT_NO_VALUE(deinit);
		ASSERT_TRUE(v_matches(deinit.error(), vm::api::StateError));

		const auto status = vm::api::getExecutionStatus(result.pid);
		ASSERT_HAS_VALUE(status);
		ASSERT_TRUE(v_matches(status.value(), vm::api::ExecutionPanicked));

		ASSERT_HAS_VALUE(vm::api::kill(result.pid));
		ASSERT_NO_VALUE(vm::api::getExecutionStatus(result.pid));
	}

	/// `deinitOrKill` deinitializes a completed process and kills a panicked one.
	void deinitOrKillTearsDownEitherWay() {
		const auto completed = runTestOnVmGetResult("no_double_destructor.dbc", "", "");
		ASSERT_HAS_VALUE(completed.run_result);
		const auto clean_teardown = vm::api::deinitOrKill(completed.pid);
		ASSERT_HAS_VALUE(clean_teardown);
		ASSERT_TRUE(clean_teardown.value().has_value());
		ASSERT_TRUE(clean_teardown.value().value());

		const auto panicked = runTestOnVmGetResult("local_leak.dbc", "", "");
		ASSERT_NO_VALUE(panicked.run_result);
		const auto forced_teardown = vm::api::deinitOrKill(panicked.pid);
		ASSERT_HAS_VALUE(forced_teardown);
		ASSERT_TRUE(!forced_teardown.value().has_value());

		ASSERT_NO_VALUE(vm::api::getExecutionStatus(completed.pid));
		ASSERT_NO_VALUE(vm::api::getExecutionStatus(panicked.pid));
	}

	/// Run completes and the deinit is legal, but we panicking in a global destructor
	// `deinitOrKill` should fall back to the kill.
	void deinitOrKillFallsBackToKillWhenDeinitFails() {
		const auto result = runTestOnVmGetResult("panicking_global_destructor.dbc", "", "");
		ASSERT_HAS_VALUE(result.run_result);

		const auto status = vm::api::getExecutionStatus(result.pid);
		ASSERT_HAS_VALUE(status);
		ASSERT_TRUE(vm::api::canDeinit(status.value()));

		const auto deinit = vm::api::deinitAndValidate(result.pid);
		ASSERT_NO_VALUE(deinit);
		ASSERT_TRUE(v_matches(deinit.error(), vm::api::OtherError));
		ASSERT_HAS_VALUE(vm::api::getExecutionStatus(result.pid));

		const auto teardown = vm::api::deinitOrKill(result.pid);
		ASSERT_HAS_VALUE(teardown);
		ASSERT_TRUE(!teardown.value().has_value());
		ASSERT_NO_VALUE(vm::api::getExecutionStatus(result.pid));
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/memory/");
