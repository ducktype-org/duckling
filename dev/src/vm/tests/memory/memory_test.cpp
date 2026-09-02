#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/core/safe/exceptions.hpp>

#include <sstream>

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
		TESTER_ADD_TEST(failedDeinitDoesNotRerunFinishedGlobalDestructors);
		TESTER_ADD_TEST(deinitOnNotStartedProcessRunsNoGlobalDestructors);
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

	// Note: Those will be moved to `api_test` in the next PR. I just needed a place to put them in.

	/// A refused deinit should leave the process alive, so we can still kill it.
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
	/// `deinitOrKill` should fall back to the kill.
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
		ASSERT_HAS_VALUE(vm::api::kill(result.pid));

		const auto killed = runTestOnVmGetResult("panicking_global_destructor.dbc", "", "");
		ASSERT_HAS_VALUE(killed.run_result);
		const auto teardown = vm::api::deinitOrKill(killed.pid);
		ASSERT_HAS_VALUE(teardown);
		ASSERT_TRUE(!teardown.value().has_value());
		ASSERT_NO_VALUE(vm::api::getExecutionStatus(killed.pid));
	}

	/**
	 * @brief A failed deinit keeps the process alive, so it can be deinitialized again. The second
	 * attempt must not run the destructors the first one already finished.
	 */
	void failedDeinitDoesNotRerunFinishedGlobalDestructors() {
		const auto result = runTestOnVmGetResult("two_globals_second_destructor_aborts.dbc");
		ASSERT_HAS_VALUE(result.run_result);

		std::ostringstream captured;
		ASSERT_HAS_VALUE(vm::api::attach(result.pid, std::cin, captured));

		const auto first = vm::api::deinitAndValidate(result.pid);
		ASSERT_NO_VALUE(first);
		ASSERT_EQUAL_PRINT(std::string("2"), captured.str());

		// The second attempt skips `b_dtor` and `a_dtor`, so it should append.
		const auto second = vm::api::deinitAndValidate(result.pid);
		ASSERT_HAS_VALUE(second);
		ASSERT_EQUAL_PRINT(std::string("2"), captured.str());
		ASSERT_NO_VALUE(vm::api::getExecutionStatus(result.pid));
	}

	/**
	 * @brief No constructor ran on a process which was only loaded, so the deinit must not run any
	 * destructor either.
	 */
	void deinitOnNotStartedProcessRunsNoGlobalDestructors() {
		const vm::PID pid = initProcess();
		ASSERT_HAS_VALUE(
			vm::api::loadFiles(pid, { fs::File(path("two_globals_second_destructor_aborts.dbc")) })
		);

		std::ostringstream captured;
		ASSERT_HAS_VALUE(vm::api::attach(pid, std::cin, captured));

		const auto deinit = vm::api::deinitAndValidate(pid);
		ASSERT_HAS_VALUE(deinit);
		ASSERT_TRUE(deinit.value());
		ASSERT_EQUAL_PRINT(std::string(""), captured.str());
		ASSERT_NO_VALUE(vm::api::getExecutionStatus(pid));
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/memory/");
