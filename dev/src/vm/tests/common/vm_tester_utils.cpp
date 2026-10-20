// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "vm_tester_utils.hpp"

#include <tester/tester.hpp>

#include <vm/api/data/status.hpp>
#include <vm/api/vm.hpp>

#include <nlohmann/json_fwd.hpp>

#include <chrono>
#include <ranges>
#include <sstream>
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

vm::PID VmTestSuiteCore::spawnAndLoad(const std::string& dbc_filename) {
	return spawnAndLoad(fs::File(path(dbc_filename)));
}

vm::PID VmTestSuiteCore::spawnAndLoad(const fs::File& dbc_file) {
	const vm::PID pid = initProcess();
	assertSucceeded(
		vm::api::loadFiles(pid, { dbc_file }), base::strConcat("Load of '", dbc_file.name(), "'")
	);
	return pid;
}

fs::File VmTestSuiteCore::inlineDbc(std::string_view name, std::string_view bytecode) {
	return fs::FileManager::createRandomVirtualFile(bytecode, base::strConcat("_", name, ".dbc"));
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

vm::PID VmTestSuiteCore::initProcess(
	const vm::api::ProcessConfig& config, vm::api::ExecutionConfig execution_config
) {
	auto process_pid_response = vm::api::spawn(config);
	ASSERT_HAS_VALUE(process_pid_response);
	auto set_config_response
		= vm::api::setExecutionConfig(process_pid_response->pid, execution_config);
	ASSERT_HAS_VALUE(set_config_response);
	return process_pid_response->pid;
}

void VmTestSuiteCore::runTestOnVm(
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

void VmTestSuiteCore::runTestOnVm(
	const vm::code::CodeCollection&    code,
	const base::Optional<std::string>& optional_input,
	const base::Optional<std::string>& optional_output,
	const std::vector<std::string>&    args,
	i64                                exit_code
) {
	handleTestResult(runTestOnVmGetResult(code, optional_input, optional_output, args), exit_code);
}

void VmTestSuiteCore::runTestOnVm(
	vm::PID                            pid,
	const base::Optional<std::string>& optional_input,
	const base::Optional<std::string>& optional_output,
	const std::vector<std::string>&    args,
	i64                                exit_code
) {
	handleTestResult(runTestOnVmGetResult(pid, optional_input, optional_output, args), exit_code);
}

void VmTestSuiteCore::assertExecutionPanickedWithAndKill(
	const TestResult& test_result, std::string_view err_piece
) {
	std::string local_error        = "";
	auto        exec_status_result = vm::api::getExecutionStatus(test_result.pid);
	ASSERT_HAS_VALUE(exec_status_result);

	// Killed before the asserts, so we remove the process even if this test fails.
	assertSucceeded(vm::api::kill(test_result.pid), "kill");

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

void VmTestSuiteCore::loadInvalidDbc(
	const std::string&                   dbc_filename,
	const std::vector<std::string_view>& error_keywords,
	const vm::api::ExecutionConfig       config
) {
	loadInvalidDbc(fs::File(path(dbc_filename)), error_keywords, config);
}

void VmTestSuiteCore::loadInvalidDbc(
	const fs::File&                      dbc_file,
	const std::vector<std::string_view>& error_keywords,
	const vm::api::ExecutionConfig       config
) {
	auto loaded_file_response
		= vm::api::loadFiles(initProcess(process_config, config), { dbc_file });
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

void VmTestSuiteCore::loadThenLoadInvalidDbc(
	const std::string&                   first_dbc,
	const std::string&                   second_dbc,
	const std::vector<std::string_view>& error_keywords,
	const vm::api::ExecutionConfig       config
) {
	// Load the first batch under an unrestricted config so its functions become already
	// validated "old" functions, then tighten the config before loading the second batch.
	auto pid = spawnAndLoad(first_dbc);

	assertSucceeded(vm::api::setExecutionConfig(pid, config), "setExecutionConfig");

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

void VmTestSuiteCore::loadValidDbc(
	const std::string& dbc_filename, const vm::api::ExecutionConfig config
) {
	loadValidDbc(fs::File(path(dbc_filename)), config);
}

void VmTestSuiteCore::loadValidDbc(const fs::File& dbc_file, const vm::api::ExecutionConfig config) {
	auto res = vm::api::loadFiles(initProcess(process_config, config), { dbc_file });
	if (!res.has_value()) std::cerr << nlohmann::json(res.error()) << '\n';
	ASSERT_HAS_VALUE(res);
}

#define EXPECT_VOID(action)                          \
	if (auto&& result = action; !result.has_value()) \
		return { .pid = pid, .run_result = std::unexpected(result.error()) };

auto VmTestSuiteCore::runTestOnVmGetResult(
	vm::PID                            pid,
	const base::Optional<std::string>& optional_input,
	const base::Optional<std::string>& optional_output,
	const std::vector<std::string>&    args
) -> TestResult {
	EXPECT_VOID(vm::api::run(pid, args));

	if_opt_some(optional_input, input) { EXPECT_VOID(vm::api::input(pid, input)); }

	auto join_result = vm::api::join(pid);
	EXPECT_VOID(join_result);

	if_opt_some(optional_output, wanted_output) {
		auto program_output = vm::api::output(pid);
		EXPECT_VOID(program_output);
		ASSERT_EQUAL_PRINT(wanted_output, program_output->output);
	}
	const auto exit_value = join_result.transform([&](vm::api::ExitValue values) {
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

auto VmTestSuiteCore::runTestOnVmGetResult(
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

auto VmTestSuiteCore::runTestOnVmGetResult(
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

void VmTestSuiteCore::handleTestResult(const TestResult& test_result, i64 exit_code) {
	if (!test_result.run_result.has_value()) {
		auto err_str = to_string(nlohmann::json(test_result.run_result.error()));
		fail(err_str);
	}
	ASSERT_EQUAL_PRINT(test_result.run_result.value(), exit_code);
	const auto validation_result = vm::api::deinitAndValidate(test_result.pid);
	ASSERT_HAS_VALUE(validation_result);
	ASSERT_TRUE(validation_result.value().valid());
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

void VmTestSuite::runAndCheckReturnValues(
	vm::PID                                 pid,
	const base::Optional<std::string>&      func_name,
	const vm::RunArguments&                 args,
	const base::Optional<std::string>&      optional_input,
	const base::Optional<std::string>&      optional_output,
	const base::Optional<std::vector<i64>>& expected_return_values
) {
	match_optional(func_name) {
		opt_some(func_name) {
			ASSERT_MATCHES(args, vm::FunctionRunArguments);
			const auto& function_args = std::get<vm::FunctionRunArguments>(args);
			assertSucceeded(vm::api::runFunction(pid, func_name, function_args), "runFunction");
		}
		opt_none {
			ASSERT_MATCHES(args, std::vector<std::string>);
			auto program_args = std::get<std::vector<std::string>>(args);
			assertSucceeded(vm::api::run(pid, program_args), "run");
		}
	}

	if_opt_some(optional_input, input) { ASSERT_HAS_VALUE(vm::api::input(pid, input)); }

	auto exit_code_response = vm::api::join(pid);
	ASSERT_HAS_VALUE(exit_code_response);

	if_opt_some(optional_output, output) {
		auto output_response = vm::api::output(pid);
		ASSERT_HAS_VALUE(output_response);
		ASSERT_EQUAL(output, output_response->output);
	}

	const auto& exit_value = exit_code_response.value();
	ASSERT_MATCHES(exit_value, std::vector<Ref<vm::IVMValue>>);
	auto& exit_value_vec = std::get<std::vector<Ref<vm::IVMValue>>>(exit_value);

	match_optional(expected_return_values) {
		opt_some(exp) {
			ASSERT_EQUAL_PRINT(exp.size(), exit_value_vec.size());
			for (usize i{ 0 }; i < exp.size(); i++)
				ASSERT_EQUAL_PRINT(exp.at(i), exit_value_vec.at(i)->readBytes<i64>());
		}
		opt_none {
			// @note: If expected_exit_code is an empty optional, it's expected that a called
			// function is void.
			ASSERT_TRUE(exit_value_vec.size() == 0);
		}
	}
}

void VmTestSuite::runAndCheckReturnValue(
	vm::PID                            pid,
	const base::Optional<std::string>& func_name,
	const vm::RunArguments&            args,
	const base::Optional<std::string>& optional_input,
	const base::Optional<std::string>& optional_output,
	const base::Optional<i64>&         expected_return_value
) {
	runAndCheckReturnValues(
		pid,
		func_name,
		args,
		optional_input,
		optional_output,
		expected_return_value.has_value()
			? base::Optional<std::vector<i64>>{ { expected_return_value.value() } }
			: std::nullopt
	);
}

Box<vm::IVMValue> VmTestSuite::getIntVMValue(vm::PID pid, i64 value) {
	auto response = vm::api::getVMValue(pid, "i64");
	ASSERT_HAS_VALUE(response);
	auto vm_value = std::move(response->vm_value);
	vm_value->writeBytes<i64>(value);
	return vm_value;
}

auto VmTestSuite::getOwnedArgumentList(vm::PID pid, std::vector<i64> values) -> OwnedArgumentList {
	return values
	     | std::views::transform([this, pid](i64 value) { return getIntVMValue(pid, value); })
	     | std::ranges::to<OwnedArgumentList>();
}

vm::FunctionRunArguments VmTestSuite::createArgumentList(OwnedArgumentList& arguments) {
	return arguments | std::views::transform([](auto& value) { return value.refMut(); })
	     | std::ranges::to<vm::FunctionRunArguments>();
}

void VmTestSuite::freeArguments(OwnedArgumentList& arguments) {
	for (auto& arg: arguments) arg->freeData();
}

auto VmParserTestSuite::parseValidDbc(const std::string& dbc_filename)
	-> std::vector<vm::loader::parser::ParsedFile> {
	auto parsing_result = vm::loader::parser::parse({ fs::File(path(dbc_filename)) });
	ASSERT_HAS_VALUE(parsing_result, "Expected successful parse.");
	return std::move(*parsing_result);
}

void VmParserTestSuite::parseInvalidDbc(
	const std::string& dbc_filename, const std::vector<std::string_view>& error_keywords
) {
	auto parsing_result = vm::loader::parser::parse({ fs::File(path(dbc_filename)) });
	match_optional(parsing_result) {
		opt_err(logger) {
			std::stringstream ss;
			logger.dumpLog(true, ss);
			for (auto err_key: error_keywords) {
				assertTrue(
					ss.str().find(err_key) != std::string::npos,
					base::strConcat("Not found: ", err_key)
				);
			}
		}
		opt_some(_) { CORE_PANIC("Expected error, but correctly parsed."); }
	}
}
