// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/collections/optional.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <tester/tester.hpp>

#include <vm/api/api.hpp>
#include <vm/api/data/api_error.hpp>
#include <vm/api/data/status.hpp>
#include <vm/api/vm.hpp>

#include <algorithm>
#include <chrono>
#include <expected>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>


#define VM_TESTER_TEST_SIMPLE_CONSTRUCTOR(...)                            \
	TESTER_CLASS(tester::TestConfig&& config __VA_OPT__(, ) __VA_ARGS__): \
		  VmTestSuite(std::move(config), TESTER_SUITE_NAME)

class VmTestSuite: public tester::TestSuite {
public:
	VmTestSuite(tester::TestConfig&& config, std::string_view name):
		  tester::TestSuite(std::move(config), name) {}

protected:
	struct TestResult {
		vm::PID                               pid;
		std::expected<i64, vm::api::ApiError> run_result;  // exit code or error
	};

	/// Budget for a single poll loop waiting for a process to reach a state.
	static constexpr auto STATUS_WAIT_BUDGET = std::chrono::seconds(30);

	/// Highest instruction index `releaseUntilTerminal` clears breakpoints up to.
	static constexpr u64 MAX_BREAKPOINT_INDEX = 12;

	/// How long a call that is expected to block is given to prove it is still blocked.
	static constexpr auto BLOCKED_CALL_PROBE = std::chrono::milliseconds(300);

	/// How long a call that is expected to return is given to do so.
	static constexpr auto UNBLOCKED_CALL_BUDGET = std::chrono::seconds(15);

	/// Thread count of `spin_threads.dbc`: `main` plus three workers, all in an endless loop.
	static constexpr usize SPIN_THREAD_COUNT = 4;

	/**
	 * @brief Spawns a process and loads one bytecode file from the suite's test-file directory.
	 */
	vm::PID spawnAndLoad(const std::string& dbc_filename);

	static bool isRunning(const vm::api::ProcStatus& s) { return v_matches(s, vm::api::Running); }

	static bool isPaused(const vm::api::ProcStatus& s) { return v_matches(s, vm::api::Paused); }

	static bool isSleeping(const vm::api::ProcStatus& s) { return v_matches(s, vm::api::Sleeping); }

	static bool isPanicked(const vm::api::ProcStatus& s) {
		return v_matches(s, vm::api::ExecutionPanicked);
	}

	static bool isCompleted(const vm::api::ProcStatus& s) {
		return v_matches(s, vm::api::ExecutionCompleted);
	}

	static bool isStopped(const vm::api::ProcStatus& s) {
		return v_matches(s, vm::api::ExecutionStopped);
	}

	static bool isTerminal(const vm::api::ProcStatus& s) { return vm::api::isStatusTerminal(s); }

	/**
	 * @brief Polls the status until `pred` holds or the budget elapses.
	 * @return The status that satisfied `pred`. Fails the test on a timeout.
	 */
	template<typename Pred>
	vm::api::ProcStatus waitUntilStatus(vm::PID pid, Pred pred, std::string_view what) {
		const auto deadline = std::chrono::steady_clock::now() + STATUS_WAIT_BUDGET;
		while (std::chrono::steady_clock::now() < deadline) {
			auto status = vm::api::getExecutionStatus(pid);
			if (status.has_value() && pred(status.value())) return status.value();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		fail(base::strConcat("Process never reached the expected status: ", what));
		return vm::api::NotStarted{};
	}

	/**
	 * @brief Runs the program the rest of the way: clears every breakpoint the test may have set in
	 * @p function_name and resumes whenever the program parks, until it reaches a terminal status.
	 *
	 * @note Only breakpoints up to `MAX_BREAKPOINT_INDEX` are cleared. A breakpoint outside that
	 * set keeps parking the program, which fails the test once `STATUS_WAIT_BUDGET` runs out.
	 */
	void releaseUntilTerminal(vm::PID pid, const std::string& function_name = "main");

	/**
	 * @brief Waits until @p thread_count threads of the process are up, and leaves them all
	 * running.
	 *
	 * `pauseAll` is the only way to count the live threads through the API, so it doubles as the
	 * readiness check. It is retried because a program starts its workers one by one, so a request
	 * can arrive before they all exist. Fails the test if they never all show up.
	 */
	void waitUntilEveryThreadRuns(vm::PID pid, usize thread_count);

	/**
	 * @brief Records every status the process emits, so the emitted sequence can be checked against
	 * the process-state model.
	 */
	struct TransitionLog final {
		void record(const vm::api::ProcStatus& status) {
			std::lock_guard lock(mutex);
			statuses.push_back(status);
		}

		/**
		 * @return A description of the first illegal transition, or an empty optional if the whole
		 * recorded sequence is legal.
		 */
		[[nodiscard]] base::Optional<std::string> findIllegalEdge() const;

		/**
		 * @brief True if the given `vm::api::ProcStatus` alternative was ever emitted.
		 */
		template<typename Alternative>
		[[nodiscard]] bool wasEmitted() const {
			std::lock_guard lock(mutex);
			return std::ranges::any_of(statuses, [](const vm::api::ProcStatus& s) {
				return v_matches(s, Alternative);
			});
		}

	private:
		mutable std::mutex               mutex;
		std::vector<vm::api::ProcStatus> statuses;
	};

	/**
	 * @brief Attaches a status-recording listener for the lifetime of a scenario.
	 */
	struct ScopedStatusLog final {
		TransitionLog                         log;
		events::Listener<vm::api::ProcStatus> listener;

		ScopedStatusLog(VmTestSuite& test, vm::PID pid);

		~ScopedStatusLog() { listener.detach(); }

		ScopedStatusLog(const ScopedStatusLog&)            = delete;
		ScopedStatusLog& operator=(const ScopedStatusLog&) = delete;
	};

	/**
	 * @brief Fails the test if the recorded status sequence contains an illegal transition.
	 */
	void validateTransitions(const TransitionLog& log, std::string_view what);

	/**
	 * @brief Asserts the call was refused with the given `ApiError` alternative, and that the
	 * rendered error mentions @p expected_reason. An empty @p expected_reason skips the message
	 * check, which is the only option for the alternatives carrying no message.
	 */
	template<typename ErrorAlternative, typename T>
	void assertRefusedWith(
		const std::expected<T, vm::api::ApiError>& result,
		std::string_view                           what,
		std::string_view                           expected_reason = ""
	) {
		ASSERT_NO_VALUE(result, what, " was expected to be refused, but it succeeded");

		const std::string message = vm::api::errorToString(result.error());
		ASSERT_MATCHES_MSG(
			result.error(),
			base::strConcat(what, " was refused with an unexpected error kind: ", message),
			ErrorAlternative
		);
		assertTrue(
			expected_reason.empty() || message.find(expected_reason) != std::string::npos,
			base::strConcat(
				what,
				" was refused, but the reason does not mention '",
				expected_reason,
				"'. The error was: ",
				message
			)
		);
	}

	/**
	 * @brief Asserts the call succeeded, printing the error otherwise.
	 */
	template<typename T>
	void assertSucceeded(const std::expected<T, vm::api::ApiError>& result, std::string_view what) {
		ASSERT_HAS_VALUE(
			result, what, " was expected to succeed, but failed with: ", errorOf(result)
		);
	}

	/**
	 * @brief Renders whatever error an `std::expected` holds, for assertion messages.
	 */
	template<typename T>
	static std::string errorOf(const std::expected<T, vm::api::ApiError>& result) {
		return result.has_value() ? "<no error>" : vm::api::errorToString(result.error());
	}

	vm::PID initProcess() { return initProcess(process_config, {}); }

	vm::PID initProcess(
		const vm::api::ProcessConfig& config, vm::api::ExecutionConfig execution_config = {}
	);
	void handleTestResult(const TestResult& test_result, i64 exit_code);

	/**
	 * @brief Runs a program from a given filepath with the specified input and command-line
	 * arguments. Asserts that the actual output matches the expected one.
	 */
	void runTestOnVm(
		const std::string&                 dbc_filename,
		const base::Optional<std::string>& optional_input  = {},
		const base::Optional<std::string>& optional_output = {},
		const std::vector<std::string>&    args            = {},
		i64                                exit_code       = 0
	);
	/**
	 * @brief Same as above, but the program is given as an argument
	 */
	void runTestOnVm(
		const vm::code::CodeCollection&    code,
		const base::Optional<std::string>& optional_input  = {},
		const base::Optional<std::string>& optional_output = {},
		const std::vector<std::string>&    args            = {},
		i64                                exit_code       = 0
	);

	void runTestOnVm(
		vm::PID                            pid,
		const base::Optional<std::string>& optional_input  = {},
		const base::Optional<std::string>& optional_output = {},
		const std::vector<std::string>&    args            = {},
		i64                                exit_code       = 0
	);

	TestResult runTestOnVmGetResult(
		const std::string&                 dbc_filename,
		const base::Optional<std::string>& optional_input  = {},
		const base::Optional<std::string>& optional_output = {},
		const std::vector<std::string>&    args            = {}
	);

	TestResult runTestOnVmGetResult(
		const vm::code::CodeCollection&    code,
		const base::Optional<std::string>& optional_input  = {},
		const base::Optional<std::string>& optional_output = {},
		const std::vector<std::string>&    args            = {}
	);

	TestResult runTestOnVmGetResult(
		vm::PID                            pid,
		const base::Optional<std::string>& optional_input  = {},
		const base::Optional<std::string>& optional_output = {},
		const std::vector<std::string>&    args            = {}
	);

	/**
	 * @brief Runs a function synchronously and checks its exit code, output and input.
	 * This is an alternative to `runTestOnVm` which allows to run a specific function instead of
	 * the main function and pass arguments to it.
	 * @param pid Process ID of the process to run the function on.
	 * @param func_name Name of the function to run
	 * @param args Arguments to pass to the function
	 * @param optional_input If provided, the function will send this string as input to the process
	 * @param optional_output If provided, the function will check if the process output is equal to
	 * this string
	 * @param expected_exit_code If provided, the function will check if the process exit code is
	 * equal to this value. If not provided, it will check if the exit code is of type void.
	 */
	void runFunctionSynchronouslyAsTest(
		vm::PID                            pid,
		const std::string&                 func_name          = {},
		const vm::FunctionRunArguments&    args               = {},
		const base::Optional<std::string>& optional_input     = {},
		const base::Optional<std::string>& optional_output    = {},
		const base::Optional<i64>          expected_exit_code = {}
	);

	TestResult runFunctionExpectPanic(
		vm::PID pid, const std::string& func_name, const vm::FunctionRunArguments& args
	);

	/**
	 * @brief Asserts the process panicked with @p err_piece in its message and kills it.
	 */
	void assertExecutionPanickedWithAndKill(
		const TestResult& test_result, std::string_view err_piece
	);

	/**
	 * @brief Asserts that `runFunction` refused the call and that the process is still runnable
	 * after the rejected validation.
	 */
	void assertRunFunctionRefusedWith(
		vm::PID                         pid,
		const std::string&              func_name,
		const vm::FunctionRunArguments& args,
		std::string_view                expected_reason
	);

	/**
	 * @brief Loads a file containing a program which violates syntactic or static verification
	 * guidelines. Asserts what error keywords are present in the error message.
	 */
	void loadInvalidDbc(
		const std::string&                   dbc_filename,
		const std::vector<std::string_view>& error_keywords,
		vm::api::ExecutionConfig             config = {}
	);

	/**
	 * @brief Loads @p first_dbc under an unrestricted config, then loads @p second_dbc under
	 * @p config on the same process, asserting the second load fails verification with the
	 * given error keywords. Exercises flag propagation from already-loaded ("old") functions
	 * to newly loaded ones.
	 */
	void loadThenLoadInvalidDbc(
		const std::string&                   first_dbc,
		const std::string&                   second_dbc,
		const std::vector<std::string_view>& error_keywords,
		vm::api::ExecutionConfig             config = {}
	);

	/**
	 * @brief Loads a file containing a valid bytecode program and asserts it was loaded correctly.
	 */
	void loadValidDbc(const std::string& dbc_filename, vm::api::ExecutionConfig config = {});

	/**
	 * @brief ProcessConfig used by the no-arg `initProcess()` (and therefore by `spawnAndLoad`).
	 * Suites may override it in their constructor; `VmApiTest` disables the JIT this way
	 * (see #3612).
	 */
	vm::api::ProcessConfig process_config{};
};
