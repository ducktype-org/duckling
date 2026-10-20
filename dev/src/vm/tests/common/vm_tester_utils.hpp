// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/collections/optional.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <filesystem/file.hpp>
#include <tester/tester.hpp>

#include <vm/api/api.hpp>
#include <vm/api/data/api_error.hpp>
#include <vm/api/data/status.hpp>
#include <vm/api/vm.hpp>
#include <vm/loader/parser/parser.hpp>

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

#define VM_RUNTIME_TESTER_TEST_SIMPLE_CONSTRUCTOR(...)                    \
	TESTER_CLASS(tester::TestConfig&& config __VA_OPT__(, ) __VA_ARGS__): \
		  VmRuntimeTestSuite(std::move(config), TESTER_SUITE_NAME)

#define VM_LOADER_TESTER_TEST_SIMPLE_CONSTRUCTOR(...)                     \
	TESTER_CLASS(tester::TestConfig&& config __VA_OPT__(, ) __VA_ARGS__): \
		  VmLoaderTestSuite(std::move(config), TESTER_SUITE_NAME)

#define VM_PARSER_TESTER_TEST_SIMPLE_CONSTRUCTOR(...)                     \
	TESTER_CLASS(tester::TestConfig&& config __VA_OPT__(, ) __VA_ARGS__): \
		  VmParserTestSuite(std::move(config), TESTER_SUITE_NAME)

/**
 * @brief The helpers shared by the VM suite bases. A suite never derives from it directly (its
 * constructor is private). It derives from `VmTestSuite`, `VmRuntimeTestSuite` or
 * `VmLoaderTestSuite`, and the base decides which helpers the suite gets.
 *
 * The helpers every VM suite needs are protected. The load and run helpers are private, and a base
 * that may use a group lists its helpers with `using` declarations. So every base is an allow-list.
 * A helper added to a private group reaches no base until it is listed there, and a suite that
 * calls a helper its base does not list fails to compile, even with a qualified
 * `VmTestSuiteCore::` call.
 */
class VmTestSuiteCore: public tester::TestSuite {
	friend class VmTestSuite;
	friend class VmRuntimeTestSuite;
	friend class VmLoaderTestSuite;

	VmTestSuiteCore(tester::TestConfig&& config, std::string_view name):
		  tester::TestSuite(std::move(config), name) {}

protected:
	/**
	 * @brief Spawns a process and loads one bytecode file from the suite's test-file directory.
	 */
	vm::PID spawnAndLoad(const std::string& dbc_filename);

	/**
	 * @brief Same as above, but the program is given as a file (e.g. from `inlineDbc`).
	 */
	vm::PID spawnAndLoad(const fs::File& dbc_file);

	/**
	 * @brief Wraps bytecode written in the test itself into an in-memory `.dbc` file, for
	 * programs that depend on values only known at run time (e.g. the path of a library). The
	 * file never touches the disk, so suites running in parallel cannot clash. @p name is part of
	 * the file name, so it shows in the positions of the load errors.
	 */
	static fs::File inlineDbc(std::string_view name, std::string_view bytecode);

	vm::PID initProcess() { return initProcess(process_config, {}); }

	vm::PID initProcess(
		const vm::api::ProcessConfig& config, vm::api::ExecutionConfig execution_config = {}
	);

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

	/**
	 * @brief ProcessConfig used by the no-arg `initProcess()` (and therefore by `spawnAndLoad`).
	 * Suites may override it in their constructor. `VmApiTest` disables the JIT this way
	 * (see #3612).
	 */
	vm::api::ProcessConfig process_config{};

private:
	// Load helpers. Only `VmTestSuite` and `VmLoaderTestSuite` expose them. A runtime suite that
	// needs one is testing the loader, so the test belongs in a `loader/` suite.

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
	 * @brief Same as above, but the program is given as a file (e.g. from `inlineDbc`).
	 */
	void loadInvalidDbc(
		const fs::File&                      dbc_file,
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
	 * @brief Same as above, but the program is given as a file (e.g. from `inlineDbc`).
	 */
	void loadValidDbc(const fs::File& dbc_file, vm::api::ExecutionConfig config = {});

	// Run helpers. Only `VmTestSuite` and `VmRuntimeTestSuite` expose them. A loader suite that
	// needs one is testing what a program does, so the test belongs in a `runtime/` suite.

	struct TestResult {
		vm::PID                               pid;
		std::expected<i64, vm::api::ApiError> run_result;  // exit code or error
	};

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
	 * @brief Asserts the process panicked with @p err_piece in its message and kills it.
	 */
	void assertExecutionPanickedWithAndKill(
		const TestResult& test_result, std::string_view err_piece
	);
};

/**
 * @brief Base of the suites that drive a VM process through the API (`api/`, the DVM backend), and
 * of the old suites that are not split into `loader/` and `runtime/` yet. It exposes both the load
 * and the run helpers, and adds the process-control helpers that only the API suites need.
 */
class VmTestSuite: public VmTestSuiteCore {
public:
	VmTestSuite(tester::TestConfig&& config, std::string_view name):
		  VmTestSuiteCore(std::move(config), name) {}

protected:
	using VmTestSuiteCore::loadInvalidDbc;
	using VmTestSuiteCore::loadThenLoadInvalidDbc;
	using VmTestSuiteCore::loadValidDbc;

	using VmTestSuiteCore::assertExecutionPanickedWithAndKill;
	using VmTestSuiteCore::handleTestResult;
	using VmTestSuiteCore::runTestOnVm;
	using VmTestSuiteCore::runTestOnVmGetResult;
	using VmTestSuiteCore::TestResult;

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
	 * @brief Asserts that `runFunction` refused the call and that the process is still runnable
	 * after the rejected validation.
	 */
	void assertRunFunctionRefusedWith(
		vm::PID                         pid,
		const std::string&              func_name,
		const vm::FunctionRunArguments& args,
		std::string_view                expected_reason
	);

	using OwnedArgumentList = std::vector<Box<vm::IVMValue>>;

	/**
	 * @brief Executes a function or a program within a VM process and verifies the results.
	 *
	 * @param pid The Process ID of the target VM process.
	 * @param func_name An optional name of the function to execute. If empty, the function assumes
	 * it's running a whole program (the `main` function).
	 * @param args A variant holding either `vm::FunctionRunArguments` (for `runFunction`) or
	 * `vm::ProgramRunArguments` (for `run`). The correct type must be provided based on whether
	 * `func_name` is set.
	 * @param optional_input An optional string to be passed as standard input to the process.
	 * @param optional_output An optional string to which the process's standard output will be
	 * compared.
	 * @param expected_exit_code An optional vector of expected exit codes (`i64`). If provided, the
	 * function's return value is asserted to be equal to it. If not provided, the function asserts
	 * that the return type was `void`.
	 */
	void runAndCheckReturnValues(
		vm::PID                                 pid,
		const base::Optional<std::string>&      func_name              = {},
		const vm::RunArguments&                 args                   = {},
		const base::Optional<std::string>&      optional_input         = {},
		const base::Optional<std::string>&      optional_output        = {},
		const base::Optional<std::vector<i64>>& expected_return_values = {}
	);

	/**
	 * @brief Utility for a more generalised runAndCheckReturnValues.
	 */
	void runAndCheckReturnValue(
		vm::PID                            pid,
		const base::Optional<std::string>& func_name             = {},
		const vm::RunArguments&            args                  = {},
		const base::Optional<std::string>& optional_input        = {},
		const base::Optional<std::string>& optional_output       = {},
		const base::Optional<i64>&         expected_return_value = {}
	);

	/**
	 * @brief Create an owned VMValue containing a specified value.
	 */
	Box<vm::IVMValue> getIntVMValue(vm::PID pid, i64 value);

	/**
	 * @brief Create a list of owned VMValues containing a specified values.
	 */
	OwnedArgumentList getOwnedArgumentList(vm::PID pid, std::vector<i64> values);

	/**
	 * @brief Create a list of references to owned VMValues which can be passed to the VM.
	 */
	static vm::FunctionRunArguments createArgumentList(OwnedArgumentList& arguments);

	static void freeArguments(OwnedArgumentList& arguments);
};

/**
 * @brief Base of the suites under `runtime/`. They check what a program does when it runs, so they
 * get the run helpers but not the load helpers. A test that expects a load error belongs in a
 * `loader/` suite, which runs once instead of once per VM flavour.
 */
class VmRuntimeTestSuite: public VmTestSuiteCore {
public:
	VmRuntimeTestSuite(tester::TestConfig&& config, std::string_view name):
		  VmTestSuiteCore(std::move(config), name) {}

protected:
	using VmTestSuiteCore::assertExecutionPanickedWithAndKill;
	using VmTestSuiteCore::handleTestResult;
	using VmTestSuiteCore::runTestOnVm;
	using VmTestSuiteCore::runTestOnVmGetResult;
	using VmTestSuiteCore::TestResult;
};

/**
 * @brief Base of the suites under `loader/`. They check whether a program loads and what the loader
 * builds from it, so they get the load helpers but not the run helpers. A test that runs a program
 * belongs in a `runtime/` suite, which runs it once per VM flavour instead of only on SC.
 */
class VmLoaderTestSuite: public VmTestSuiteCore {
public:
	VmLoaderTestSuite(tester::TestConfig&& config, std::string_view name):
		  VmTestSuiteCore(std::move(config), name) {}

protected:
	using VmTestSuiteCore::loadInvalidDbc;
	using VmTestSuiteCore::loadThenLoadInvalidDbc;
	using VmTestSuiteCore::loadValidDbc;
};

/**
 * @brief Base of the suites that test the text assembly parser on its own. They never start a VM
 * process, so they derive from `tester::TestSuite` directly and only get the parse helpers.
 */
class VmParserTestSuite: public tester::TestSuite {
public:
	VmParserTestSuite(tester::TestConfig&& config, std::string_view name):
		  tester::TestSuite(std::move(config), name) {}

protected:
	/**
	 * @brief Parses @p dbc_filename, asserts it parses, and returns what the parser produced.
	 */
	std::vector<vm::loader::parser::ParsedFile> parseValidDbc(const std::string& dbc_filename);

	/**
	 * @brief Parses a syntactically incorrect @p dbc_filename and asserts that every one of
	 * @p error_keywords is in the parse error.
	 */
	void parseInvalidDbc(
		const std::string& dbc_filename, const std::vector<std::string_view>& error_keywords
	);
};
