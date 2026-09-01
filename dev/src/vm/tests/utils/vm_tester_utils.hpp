#pragma once

#include <base/collections/optional.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <tester/tester.hpp>

#include <vm/api/api.hpp>
#include <vm/api/data/api_error.hpp>
#include <vm/api/data/status.hpp>
#include <vm/api/vm.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <expected>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
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

	// ------------------------------------------------------------------
	// Driving a process through the API
	//
	// The API is the only supported way of driving the VM, so tests built on these helpers never
	// reach past `vm::api::*`. That makes a few things awkward on purpose - there is no per-thread
	// status endpoint, for example, so "thread B is paused" has to be asserted through the requests
	// that are only legal on a paused thread (`resume`, `step`) rather than by reading a state.
	// ------------------------------------------------------------------

	/// Budget for a single scenario. Generous, since it is a "something hung" detector and not a
	/// performance assertion.
	static constexpr auto SCENARIO_TIME_BUDGET = std::chrono::seconds(60);

	/// Budget for a single `waitUntilStatus` poll loop.
	static constexpr auto STATUS_WAIT_BUDGET = std::chrono::seconds(30);

	/// Highest instruction index `releaseUntilTerminal` clears breakpoints up to.
	static constexpr u64 MAX_BREAKPOINT_INDEX = 12;

	/**
	 * @brief Aborts the whole suite (printing the case name) if a scenario does not finish within
	 * its time budget.
	 *
	 * A hung API call cannot be recovered from inside the process, so failing hard with the name of
	 * the case is the only useful outcome.
	 */
	class Watchdog {
	public:
		Watchdog(std::string what, std::chrono::seconds budget = SCENARIO_TIME_BUDGET):
			  what(std::move(what)) {
			thread = std::thread([this, budget] {
				std::unique_lock lock(mutex);
				if (!cv.wait_for(lock, budget, [this] { return done; })) {
					std::cerr << "DVM API WATCHDOG: '" << this->what
							  << "' exceeded its time budget\n";
					std::abort();
				}
			});
		}

		~Watchdog() {
			{
				std::lock_guard lock(mutex);
				done = true;
			}
			cv.notify_all();
			thread.join();
		}

		Watchdog(const Watchdog&)            = delete;
		Watchdog& operator=(const Watchdog&) = delete;
		Watchdog(Watchdog&&)                 = delete;
		Watchdog& operator=(Watchdog&&)      = delete;

	private:
		std::string             what;
		std::thread             thread;
		std::mutex              mutex;
		std::condition_variable cv;
		bool                    done = false;
	};

	/**
	 * @brief Spawns a process with the default configuration.
	 */
	vm::PID spawnProcess();

	/**
	 * @brief Spawns a process and loads one bytecode file from the suite's test-file directory.
	 */
	vm::PID spawnAndLoad(const std::string& dbc_filename);

	static bool isRunning(const vm::api::ProcStatus& s) { return v_matches(s, vm::api::Running); }

	static bool isPaused(const vm::api::ProcStatus& s) { return v_matches(s, vm::api::Paused); }

	static bool isSleeping(const vm::api::ProcStatus& s) { return v_matches(s, vm::api::Sleeping); }

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
	 * @brief Runs the program the rest of the way. Disables every breakpoint the test may have set
	 * in @p function_name and resumes whenever the program parks, until it reaches a terminal
	 * status.
	 */
	void releaseUntilTerminal(vm::PID pid, const std::string& function_name = "main");

	// ------------------------------------------------------------------
	// Status transition validation
	// ------------------------------------------------------------------

	/**
	 * @brief Records every status the process emits, so the emitted sequence can be checked
	 * against the process-state model.
	 */
	struct TransitionLog {
		static constexpr usize STATUS_COUNT = std::variant_size_v<vm::api::ProcStatus>;

		void record(const vm::api::ProcStatus& status) {
			std::lock_guard lock(mutex);
			statuses.push_back(status);
		}

		/**
		 * @brief Checks the recorded sequence against `legalStatusEdge`.
		 * @return A description of the first illegal edge, or an empty optional if the whole
		 * recorded sequence is legal.
		 */
		[[nodiscard]] base::Optional<std::string> findIllegalEdge() const {
			std::lock_guard lock(mutex);
			for (usize i = 1; i < statuses.size(); i++) {
				const usize from = statuses[i - 1].index();
				const usize to   = statuses[i].index();
				// A status is only recorded on change, but be defensive.
				if (from == to) continue;
				if (!legalStatusEdge(from, to))
					return std::string("Illegal status transition emitted by the process: ")
					     + std::string(statusName(from)) + " -> " + std::string(statusName(to));
			}
			return std::nullopt;
		}

		/**
		 * @brief True if the given `vm::api::ProcStatus` alternative was ever emitted.
		 */
		[[nodiscard]] bool wasEmitted(usize status_index) const {
			std::lock_guard lock(mutex);
			return std::ranges::any_of(statuses, [status_index](const vm::api::ProcStatus& s) {
				return s.index() == status_index;
			});
		}

	private:
		static std::string_view statusName(usize index);

		/**
		 * @brief Legal directed edges of the emitted process-status sequence.
		 */
		static bool legalStatusEdge(usize from, usize to);

		mutable std::mutex               mutex;
		std::vector<vm::api::ProcStatus> statuses;
	};

	/**
	 * @brief Attaches a status-recording listener for the lifetime of a scenario.
	 */
	struct ScopedStatusLog {
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

	// ------------------------------------------------------------------
	// API result assertions
	// ------------------------------------------------------------------

	/**
	 * @brief Renders whatever error an `std::expected` holds, for assertion messages.
	 */
	template<typename T, typename E>
	static std::string errorOf(const std::expected<T, E>& result) {
		if (result.has_value()) return "<no error>";
		if constexpr (std::same_as<E, vm::api::ApiError>)
			return vm::api::errorToString(result.error());
		else
			return "<error>";
	}

	/**
	 * @brief Asserts the call was refused, that the error is of the given `ApiError` alternative
	 * and that the reason mentions @p expected_reason.
	 *
	 * @param expected_reason Substring the error message must contain. Empty means "do not look at
	 * the message", which is the only option for the alternatives carrying no message.
	 */
	template<typename ErrorAlternative, typename T>
	void assertRefusedWith(
		const std::expected<T, vm::api::ApiError>& result,
		std::string_view                           what,
		std::string_view                           expected_reason = ""
	) {
		if (result.has_value()) {
			fail(base::strConcat(what, " was expected to be refused, but it succeeded"));
			return;
		}
		const vm::api::ApiError& error = result.error();
		if (!std::holds_alternative<ErrorAlternative>(error)) {
			fail(base::strConcat(
				what, " was refused with an unexpected error kind: ", vm::api::errorToString(error)
			));
			return;
		}
		if (expected_reason.empty()) return;

		const std::string message = reasonOf(std::get<ErrorAlternative>(error));
		assertTrue(
			message.find(expected_reason) != std::string::npos,
			base::strConcat(
				what,
				" was refused, but the reason does not mention '",
				expected_reason,
				"'. The reason was: ",
				message
			)
		);
	}

	/**
	 * @brief Asserts the call succeeded, printing the error otherwise.
	 */
	template<typename T>
	void assertSucceeded(const std::expected<T, vm::api::ApiError>& result, std::string_view what) {
		assertTrue(
			result.has_value(),
			base::strConcat(what, " was expected to succeed, but failed with: ", errorOf(result))
		);
	}

	/// The `ApiError` alternatives spell their message field differently, so it is unwrapped here.
	static std::string reasonOf(const vm::api::ResumeError& e) { return e.why; }

	static std::string reasonOf(const vm::api::PauseError& e) { return e.why; }

	static std::string reasonOf(const vm::api::StateError& e) { return e.why; }

	static std::string reasonOf(const vm::api::RunError& e) { return e.error; }

	static std::string reasonOf(const vm::api::OtherError& e) { return e.error; }

	static std::string reasonOf(const vm::api::IOError& e) { return e.error; }

	static std::string reasonOf(const vm::api::LoadProgramError& e) { return e.why; }

	static std::string reasonOf(const vm::api::UnsupportedOperation& e) { return e.why; }

	static std::string reasonOf(const vm::api::NotImplementedError& e) { return e.why; }

	static std::string reasonOf(const vm::api::JoinError&) { return ""; }

	static std::string reasonOf(const vm::api::AttachDetachError&) { return ""; }

	static std::string reasonOf(const vm::api::ProcessNotFound&) { return ""; }

	static std::string reasonOf(const vm::api::WrongResponse&) { return ""; }

	vm::PID initProcess(
		const vm::api::ProcessConfig& config = {}, vm::api::ExecutionConfig execution_config = {}
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
};
