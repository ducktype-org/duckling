#include "vm_tester_utils.hpp"

#include <tester/tester.hpp>

#include <vm/api/data/status.hpp>
#include <vm/api/vm.hpp>

#include <nlohmann/json_fwd.hpp>

#include <chrono>
#include <thread>
#include <variant>

namespace {
	using vm::api::ProcStatus;

	template<typename Alternative>
	constexpr usize statusIndex() {
		return base::variantTypeIndex<ProcStatus, Alternative>();
	}

	constexpr usize NOT_STARTED = statusIndex<vm::api::NotStarted>();
	constexpr usize RUNNING     = statusIndex<vm::api::Running>();
	constexpr usize PAUSED      = statusIndex<vm::api::Paused>();
	constexpr usize SLEEPING    = statusIndex<vm::api::Sleeping>();
	constexpr usize STOPPING    = statusIndex<vm::api::ExecutionStopping>();
	constexpr usize COMPLETED   = statusIndex<vm::api::ExecutionCompleted>();
	constexpr usize STOPPED     = statusIndex<vm::api::ExecutionStopped>();
	constexpr usize PANICKED    = statusIndex<vm::api::ExecutionPanicked>();

	static_assert(
		std::variant_size_v<ProcStatus> == 8,
		"a new ProcStatus alternative needs an entry in legalStatusEdge"
	);

	/**
	 * @brief Legal directed edges of the emitted process-status sequence.
	 */
	bool legalStatusEdge(usize from, usize to) {
		switch (from) {
		case NOT_STARTED:
			return to != NOT_STARTED;
		case RUNNING:
			return to != NOT_STARTED;
		// Paused and Sleeping reach each other directly in a multi-threaded process: the aggregate
		// ranks Sleeping above Paused, so a process with one sleeping and one paused thread reports
		// Sleeping, and reports Paused the moment the sleeping thread terminates.
		case PAUSED:
		case SLEEPING:
			return to != NOT_STARTED;
		case STOPPING:
			return to == COMPLETED || to == STOPPED || to == PANICKED;
		// A rerun is only legal after a completed run.
		case COMPLETED:
			return to == NOT_STARTED;
		default:
			return false;
		}
	}
}

base::Optional<std::string> VmTestSuite::TransitionLog::findIllegalEdge() const {
	std::lock_guard lock(mutex);
	for (usize i = 1; i < statuses.size(); i++) {
		const ProcStatus& from = statuses[i - 1];
		const ProcStatus& to   = statuses[i];
		if (from.index() == to.index()) continue;
		if (!legalStatusEdge(from.index(), to.index()))
			return base::strConcat(
				"Illegal status transition emitted by the process: ",
				vm::api::statusName(from),
				" -> ",
				vm::api::statusName(to)
			);
	}
	return std::nullopt;
}

VmTestSuite::ScopedStatusLog::ScopedStatusLog(VmTestSuite& test, vm::PID pid):
	  listener([this](const ProcStatus& status) { log.record(status); }) {
	test.assertTrue(
		vm::api::attachStatusListener(pid, &listener).has_value(), "Attach listener failed"
	);
}

void VmTestSuite::validateTransitions(const TransitionLog& log, std::string_view what) {
	const auto illegal_edge = log.findIllegalEdge();
	ASSERT_NO_VALUE(illegal_edge, illegal_edge.has_value() ? *illegal_edge : "", " (", what, ")");
}

vm::PID VmTestSuite::spawnAndLoad(const std::string& dbc_filename) {
	const vm::PID pid    = initProcess();
	auto          loaded = vm::api::loadFiles(pid, { fs::File(path(dbc_filename)) });
	ASSERT_HAS_VALUE(loaded, "Load of '", dbc_filename, "' failed: ", errorOf(loaded));
	return pid;
}

void VmTestSuite::releaseUntilTerminal(vm::PID pid, const std::string& function_name) {
	for (u64 instr = 0; instr <= MAX_BREAKPOINT_INDEX; instr++)
		(void) vm::api::setBreakpoint(pid, base::StrID(function_name), instr, false);

	const auto deadline = std::chrono::steady_clock::now() + STATUS_WAIT_BUDGET;
	while (std::chrono::steady_clock::now() < deadline) {
		auto status = vm::api::getExecutionStatus(pid);
		assertSucceeded(status, "getExecutionStatus while waiting for the program to finish");
		if (vm::api::isStatusTerminal(status.value())) return;
		(void) vm::api::resume(pid);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	fail(base::strConcat(
		"The program never reached a terminal status. Only breakpoints in '",
		function_name,
		"' up to instruction ",
		MAX_BREAKPOINT_INDEX,
		" are cleared here, so a breakpoint outside that set keeps parking it."
	));
}

void VmTestSuite::waitUntilEveryThreadRuns(vm::PID pid, usize thread_count) {
	namespace api = vm::api;

	waitUntilStatus(pid, isRunning, "Running");

	const auto deadline = std::chrono::steady_clock::now() + STATUS_WAIT_BUDGET;
	while (std::chrono::steady_clock::now() < deadline) {
		auto paused = api::pauseAll(pid);
		assertSucceeded(paused, "pauseAll while waiting for the workers to start");
		const bool all_up = paused->thread_ids.size() == thread_count;
		for (const api::ThreadID tid: paused->thread_ids)
			assertSucceeded(
				api::resume(pid, tid),
				base::strConcat("resume of thread ", tid.asInt(), " after the readiness check")
			);
		if (all_up) {
			waitUntilStatus(pid, isRunning, "Running again after the readiness check");
			return;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	fail(base::strConcat("The program never got all ", thread_count, " of its threads running"));
}

vm::PID VmTestSuite::initProcess(
	const vm::api::ProcessConfig& config, vm::api::ExecutionConfig execution_config
) {
	auto process_pid_response = vm::api::spawn(config);
	ASSERT_HAS_VALUE(process_pid_response);
	auto set_config_response
		= vm::api::setExecutionConfig(process_pid_response->pid, execution_config);
	ASSERT_HAS_VALUE(set_config_response);
	return process_pid_response->pid;
}

void VmTestSuite::runTestOnVm(
	const std::string&                 dbc_filename,
	const base::Optional<std::string>& optional_input,
	const base::Optional<std::string>& optional_output,
	const std::vector<std::string>&    args,
	i64                                exit_code
) {
	handleTestResult(
		runTestOnVmGetResult(dbc_filename, optional_input, optional_output, args), exit_code
	);
}

void VmTestSuite::runTestOnVm(
	const vm::code::CodeCollection&    code,
	const base::Optional<std::string>& optional_input,
	const base::Optional<std::string>& optional_output,
	const std::vector<std::string>&    args,
	i64                                exit_code
) {
	handleTestResult(runTestOnVmGetResult(code, optional_input, optional_output, args), exit_code);
}

void VmTestSuite::runTestOnVm(
	vm::PID                            pid,
	const base::Optional<std::string>& optional_input,
	const base::Optional<std::string>& optional_output,
	const std::vector<std::string>&    args,
	i64                                exit_code
) {
	handleTestResult(runTestOnVmGetResult(pid, optional_input, optional_output, args), exit_code);
}

void VmTestSuite::assertExecutionPanickedWithAndKill(
	const TestResult& test_result, std::string_view err_piece
) {
	std::string local_error        = "";
	auto        exec_status_result = vm::api::getExecutionStatus(test_result.pid);
	ASSERT_HAS_VALUE(exec_status_result);

	// Killed before the asserts, so we remove the process even if this test fails.
	ASSERT_HAS_VALUE(vm::api::kill(test_result.pid));

	variant_match(exec_status_result.value()) {
		variant_case(vm::api::ExecutionPanicked, panicked) {
			ASSERT_TRUE(panicked.error_message.contains(err_piece));
		}
		variant_default {
			ASSERT_NO_VALUE(test_result.run_result);
			fail(base::strConcat(
				"Expected ",
				TypeParseTraits<vm::api::ExecutionPanicked>::NAME.data(),
				", but found: " + to_string(nlohmann::json(test_result.run_result.error()))
			));
		}
	}
}

void VmTestSuite::loadInvalidDbc(
	const std::string&                   dbc_filename,
	const std::vector<std::string_view>& error_keywords,
	const vm::api::ExecutionConfig       config
) {
	fs::File file(path(dbc_filename));

	auto loaded_file_response = vm::api::loadFiles(initProcess(process_config, config), { file });
	ASSERT_NO_VALUE(loaded_file_response);

	auto err = loaded_file_response.error();
	ASSERT_MATCHES(err, vm::api::LoadProgramError);
	auto err_str = std::get<vm::api::LoadProgramError>(err).why;
	std::cerr << err_str << '\n';
	for (auto err_key: error_keywords) {
		assertTrue(
			err_str.find(err_key) != std::string::npos, base::strConcat("Not found: ", err_key)
		);
	}
}

void VmTestSuite::loadThenLoadInvalidDbc(
	const std::string&                   first_dbc,
	const std::string&                   second_dbc,
	const std::vector<std::string_view>& error_keywords,
	const vm::api::ExecutionConfig       config
) {
	// Load the first batch under an unrestricted config so its functions become already
	// validated "old" functions, then tighten the config before loading the second batch.
	auto pid = initProcess();
	ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { fs::File(path(first_dbc)) }));

	ASSERT_HAS_VALUE(vm::api::setExecutionConfig(pid, config));

	auto second_response = vm::api::loadFiles(pid, { fs::File(path(second_dbc)) });
	ASSERT_NO_VALUE(second_response);

	auto err = second_response.error();
	ASSERT_MATCHES(err, vm::api::LoadProgramError);
	auto err_str = std::get<vm::api::LoadProgramError>(err).why;
	std::cerr << err_str << '\n';
	for (auto err_key: error_keywords) {
		assertTrue(
			err_str.find(err_key) != std::string::npos, base::strConcat("Not found: ", err_key)
		);
	}
}

void VmTestSuite::loadValidDbc(
	const std::string& dbc_filename, const vm::api::ExecutionConfig config
) {
	auto res
		= vm::api::loadFiles(initProcess(process_config, config), { fs::File(path(dbc_filename)) });
	if (!res.has_value()) std::cerr << nlohmann::json(res.error()) << '\n';
	ASSERT_HAS_VALUE(res);
}

#define EXPECT_VOID(action)                          \
	if (auto&& result = action; !result.has_value()) \
		return { .pid = pid, .run_result = std::unexpected(result.error()) };

auto VmTestSuite::runTestOnVmGetResult(
	vm::PID                            pid,
	const base::Optional<std::string>& optional_input,
	const base::Optional<std::string>& optional_output,
	const std::vector<std::string>&    args
) -> TestResult {
	EXPECT_VOID(vm::api::run(pid, args));

	if_opt_some(optional_input, input) { EXPECT_VOID(vm::api::input(pid, input)); }

	EXPECT_VOID(vm::api::join(pid));

	if_opt_some(optional_output, wanted_output) {
		auto program_output = vm::api::output(pid);
		EXPECT_VOID(program_output);
		ASSERT_EQUAL_PRINT(wanted_output, program_output->output);
	}
	const auto exit_value = vm::api::getExitValue(pid).transform([&](vm::api::ExitValue values) {
		variant_match(values) {
			variant_case(i64, exit_code) return exit_code;
			variant_case(std::vector<Ref<vm::IVMValue>>, values) {
				ASSERT_TRUE(values.size() == 1);
				auto& value = values.at(0);
				ASSERT_TRUE(value->getType()->getName().str() == "i64");
				auto exit_code = value->readBytes<i64>();
				return exit_code;
			}
		}
		CORE_UNREACHABLE();
	});
	return { .pid = pid, .run_result = exit_value };
}

auto VmTestSuite::runTestOnVmGetResult(
	const std::string&                 dbc_filename,
	const base::Optional<std::string>& optional_input,
	const base::Optional<std::string>& optional_output,
	const std::vector<std::string>&    args
) -> TestResult {
	auto pid  = initProcess();
	auto file = fs::File(path(dbc_filename));
	EXPECT_VOID(vm::api::loadFiles(pid, { file }));
	return runTestOnVmGetResult(pid, optional_input, optional_output, args);
}

auto VmTestSuite::runTestOnVmGetResult(
	const vm::code::CodeCollection&    code,
	const base::Optional<std::string>& optional_input,
	const base::Optional<std::string>& optional_output,
	const std::vector<std::string>&    args
) -> TestResult {
	auto pid = initProcess();
	EXPECT_VOID(vm::api::loadCode(pid, { code }));
	return runTestOnVmGetResult(pid, optional_input, optional_output, args);
}

#undef EXPECT_VOID

void VmTestSuite::handleTestResult(const TestResult& test_result, i64 exit_code) {
	if (!test_result.run_result.has_value()) {
		auto err_str = to_string(nlohmann::json(test_result.run_result.error()));
		fail(err_str);
	}
	ASSERT_EQUAL_PRINT(test_result.run_result.value(), exit_code);
	const auto validation_result = vm::api::deinitAndValidate(test_result.pid);
	ASSERT_HAS_VALUE(validation_result);
	ASSERT_TRUE(validation_result.value());
}

void VmTestSuite::runFunctionSynchronouslyAsTest(
	vm::PID                            pid,
	const std::string&                 func_name,
	const vm::FunctionRunArguments&    args,
	const base::Optional<std::string>& optional_input,
	const base::Optional<std::string>& optional_output,
	const base::Optional<i64>          expected_exit_code
) {
	const auto run_result = vm::api::runFunctionAwait(pid, func_name, args);
	ASSERT_HAS_VALUE(run_result);
	ASSERT_NO_VALUE(vm::api::join(pid));

	if_opt_some(optional_input, input) { ASSERT_HAS_VALUE(vm::api::input(pid, input)); }

	if_opt_some(optional_output, output) {
		auto output_response = vm::api::output(pid);
		ASSERT_HAS_VALUE(output_response);
		ASSERT_EQUAL(output, output_response->output);
	}

	const auto& exit_value = run_result.value();
	variant_match(exit_value) {
		variant_case(i64, exit_code) {
			if (expected_exit_code.has_value())
				ASSERT_EQUAL_PRINT(expected_exit_code.value(), exit_code);
			else
				ASSERT_TRUE(exit_code == 0);
		}
		variant_case(std::vector<Ref<vm::IVMValue>>, values) {
			if (expected_exit_code.has_value()) {
				ASSERT_TRUE(values.size() == 1);
				ASSERT_EQUAL_PRINT(expected_exit_code.value(), values.at(0)->readBytes<i64>());
			} else {
				// @note: If expected_exit_code is an empty optional, it's expected that a called
				// function doesn't return any values
				ASSERT_TRUE(values.size() == 0);
			}
		}
	}
}

void VmTestSuite::assertRunFunctionRefusedWith(
	vm::PID                         pid,
	const std::string&              func_name,
	const vm::FunctionRunArguments& args,
	std::string_view                expected_reason
) {
	auto result = vm::api::runFunction(pid, func_name, args);
	ASSERT_NO_VALUE(result);
	ASSERT_MATCHES(result.error(), vm::api::RunError);
	const std::string why = v_get(result.error(), vm::api::RunError).error;
	assertTrue(
		why.find(expected_reason) != std::string::npos,
		"Expected to see'" + std::string(expected_reason)
			+ "' in the error message, but the message was: " + why
	);

	auto status = vm::api::getExecutionStatus(pid);
	ASSERT_HAS_VALUE(status);
	ASSERT_MATCHES(status.value(), vm::api::NotStarted);
}

auto VmTestSuite::runFunctionExpectPanic(
	vm::PID pid, const std::string& func_name, const vm::FunctionRunArguments& args
) -> TestResult {
	auto run_result = vm::api::runFunction(pid, func_name, args);
	if (!run_result.has_value())
		return { .pid = pid, .run_result = std::unexpected(run_result.error()) };
	auto join_result = vm::api::join(pid);
	return { .pid = pid, .run_result = std::unexpected(join_result.error()) };
}
