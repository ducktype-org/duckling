#include <tester/tester.hpp>

#include <vm/debugger/debugger.hpp>

#include <condition_variable>
#include <mutex>

class VmDebuggerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebuggerTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(runAndGetStatus);
		TESTER_ADD_TEST(getStatusWait);
		TESTER_ADD_TEST(getStatusBreakpoint);
		TESTER_ADD_TEST(rerunTest);
	}

private:
	void testTemplate(
		std::string_view                path_name,
		const size_t                    ret_val_limit,
		const std::vector<std::string>& expected_statuses
	) {
		size_t                  status_counter  = 0;
		size_t                  ret_val_counter = 0;
		std::mutex              m;
		std::condition_variable cv;

		events::Listener<std::string> status_listener([&](const std::string& status) {
			ASSERT_TRUE(status_counter < expected_statuses.size());
			ASSERT_EQUAL_PRINT(expected_statuses[status_counter], status);
			status_counter++;
			if (status_counter == expected_statuses.size()) cv.notify_one();
		});

		events::Listener<std::string> execution_completed_listener([&](const std::string& str) {
			ASSERT_TRUE(ret_val_counter < ret_val_limit);
			ASSERT_EQUAL_PRINT("0", str);
			ret_val_counter++;
		});

		events::Listener<std::string> error_listener([&](const std::string err) { fail(err); });

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
		{ testTemplate("debugger_test.dbc", 1, { "Running", "ExecutionCompleted" }); }
	}

	void getStatusWait() { testTemplate("vm_api_tests.dbc", 0, { "Running", "Sleeping" }); }

	void getStatusBreakpoint() { testTemplate("breakpoint.dbc", 0, { "Running", "Paused" }); }

	void rerunTest() {
		size_t                  ret_val_limit   = 3;
		size_t                  status_counter  = 0;
		size_t                  ret_val_counter = 0;
		std::mutex              m;
		std::condition_variable cv;

		const std::vector<std::string> expected_statuses = { "Running", "ExecutionCompleted" };

		events::Listener<std::string> status_listener([&](const std::string& status) {
			ASSERT_TRUE(status_counter < expected_statuses.size());
			ASSERT_EQUAL_PRINT(expected_statuses[status_counter], status);
			status_counter++;
			if (status_counter == expected_statuses.size()) cv.notify_one();
		});

		events::Listener<std::string> execution_completed_listener([&](const std::string& str) {
			ASSERT_TRUE(ret_val_counter < ret_val_limit);
			ASSERT_EQUAL_PRINT("0", str);
			ret_val_counter++;
		});

		events::Listener<std::string> error_listener([&](const std::string err) { fail(err); });

		vm::debugger::Debugger debugger{ fs::File(path("debugger_test.dbc")) };

		debugger.attachOnVMChangesStatusListener(status_listener);
		debugger.attachOnVMCompletesExecutionListener(execution_completed_listener);
		debugger.attachOnErrorListener(error_listener);

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
		ASSERT_EQUAL_PRINT(ret_val_limit, ret_val_counter);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
