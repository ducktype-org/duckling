#include <tester/tester.hpp>

#include <vm/debugger/debugger.hpp>

#include <condition_variable>
#include <mutex>

#define altIndex(t) base::internal::alternativeIndex<vm::api::ProcStatus, t>()

class VmDebuggerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebuggerTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(runAndGetStatus);
		TESTER_ADD_TEST(getStatusWait);
		TESTER_ADD_TEST(getStatusBreakpoint);
		TESTER_ADD_TEST(rerunTest);
		TESTER_ADD_TEST(errorTest);
	}

private:
	void testTemplate(
		std::string_view          path_name,
		const size_t              ret_val_limit,
		const std::vector<usize>& expected_statuses
	) {
		size_t                  status_counter  = 0;
		size_t                  ret_val_counter = 0;
		std::mutex              m;
		std::condition_variable cv;

		events::Listener<vm::api::ProcStatus> status_listener([&](const vm::api::ProcStatus& status
		                                                      ) {
			ASSERT_TRUE(status_counter < expected_statuses.size());
			ASSERT_EQUAL_PRINT(expected_statuses[status_counter], status.index());
			status_counter++;
			if (status_counter == expected_statuses.size()) cv.notify_one();
		});

		events::Listener<std::string> execution_completed_listener([&](const std::string& str) {
			ASSERT_TRUE(ret_val_counter < ret_val_limit);
			ASSERT_EQUAL_PRINT("0", str);
			ret_val_counter++;
		});

		events::Listener<std::string> error_listener([&](const std::string& err) { fail(err); });

		vm::debugger::Debugger debugger{ fs::File(path(std::string(path_name))) };
		debugger.attachOnVMChangesStatusListener(status_listener);
		debugger.attachOnVMCompletesExecutionListener(execution_completed_listener);
		debugger.attachOnErrorListener(error_listener);
		debugger.runMain();
		std::unique_lock lk(m);
		// timeout for the test
		cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
			return status_counter == expected_statuses.size();
		});
		ASSERT_EQUAL_PRINT(expected_statuses.size(), status_counter);
		ASSERT_EQUAL_PRINT(ret_val_limit, ret_val_counter);
	}

	void runAndGetStatus() {
		{
			testTemplate(
				"debugger_test.dbc",
				1,
				{
					altIndex(vm::api::Running),
					altIndex(vm::api::ExecutionCompleted),
				}
			);
		}
	}

	void getStatusWait() {
		testTemplate(
			"vm_api_tests.dbc",
			0,
			{
				altIndex(vm::api::Running),
				altIndex(vm::api::Sleeping),
			}
		);
	}

	void getStatusBreakpoint() {
		testTemplate(
			"breakpoint.dbc",
			0,
			{
				altIndex(vm::api::Running),
				altIndex(vm::api::Paused),
			}
		);
	}

	void rerunTest() {
		size_t                  ret_val_limit   = 3;
		size_t                  status_counter  = 0;
		size_t                  ret_val_counter = 0;
		std::mutex              m;
		std::condition_variable cv;

		const std::vector<usize> expected_statuses = {
			altIndex(vm::api::Running),
			altIndex(vm::api::ExecutionCompleted),
		};

		events::Listener<vm::api::ProcStatus> status_listener([&](const vm::api::ProcStatus& status
		                                                      ) {
			ASSERT_TRUE(status_counter < expected_statuses.size());
			ASSERT_EQUAL_PRINT(expected_statuses[status_counter], status.index());
			status_counter++;
			if (status_counter == expected_statuses.size()) cv.notify_one();
		});

		events::Listener<std::string> execution_completed_listener([&](const std::string& str) {
			ASSERT_TRUE(ret_val_counter < ret_val_limit);
			ASSERT_EQUAL_PRINT("0", str);
			ret_val_counter++;
		});

		events::Listener<std::string> error_listener([&](const std::string& err) { fail(err); });

		vm::debugger::Debugger debugger{ fs::File(path("debugger_test.dbc")) };

		debugger.attachOnVMChangesStatusListener(Ref(&status_listener));
		debugger.attachOnVMCompletesExecutionListener(Ref(&execution_completed_listener));
		debugger.attachOnErrorListener(Ref(&error_listener));

		int loop = 3;

		while (loop-- > 0) {
			status_counter = 0;
			debugger.runMain();
			std::unique_lock lk(m);
			// Test timeout
			cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
				return status_counter == expected_statuses.size();
			});
			ASSERT_EQUAL_PRINT(expected_statuses.size(), status_counter);
		}
		ASSERT_TRUE(std::holds_alternative<vm::api::ExecutionCompleted>(debugger.getStatus()));
		ASSERT_EQUAL_PRINT(ret_val_limit, ret_val_counter);
	}

	void errorTest() {
		size_t                  error_counter   = 0;
		size_t                  expected_errors = 1;
		std::mutex              m;
		std::condition_variable cv;
		vm::debugger::Debugger  debugger{ fs::File(path("while_true_no_breakpoint.dbc")) };

		events::Listener<std::string> error_listener([&](const std::string&) {
			error_counter++;
			cv.notify_one();
		});

		debugger.attachOnErrorListener(error_listener);

		debugger.runMain();
		debugger.runMain();

		std::unique_lock lk(m);
		cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
			return error_counter == expected_errors;
		});
		ASSERT_TRUE(std::holds_alternative<vm::api::Running>(debugger.getStatus()));
		ASSERT_EQUAL_PRINT(expected_errors, error_counter);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
