#include "vm_tester_utils.hpp"

#include <tester/tester.hpp>

#include <vm/api/data/status.hpp>
#include <vm/api/vm.hpp>

#include <nlohmann/json_fwd.hpp>

#include <variant>

vm::PID VmTestSuite::initProcess() {
	auto process_pid_response = vm::api::spawn();
	ASSERT_TRUE(process_pid_response.has_value());
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

void VmTestSuite::assertExecutionPanickedWith(
	const TestResult& test_result, std::string_view err_piece
) {
	std::string local_error        = "";
	auto        exec_status_result = vm::api::getExecutionStatus(test_result.pid);
	ASSERT_TRUE(exec_status_result.has_value());

	variant_match(exec_status_result.value()) {
		variant_case(vm::api::ExecutionPanicked, panicked) {
			ASSERT_TRUE(panicked.error_message.contains(err_piece));
		}
		variant_default {
			ASSERT_TRUE(!test_result.run_result.has_value());
			fail(base::strConcat(
				"Expected ",
				TypeParseTraits<vm::api::ExecutionPanicked>::NAME.data(),
				", but found: " + to_string(nlohmann::json(test_result.run_result.error()))
			));
		}
	}
}

void VmTestSuite::loadInvalidDbc(
	const std::string& dbc_filename, const std::vector<std::string_view>& error_keywords
) {
	fs::File file(path(dbc_filename));
	auto     loaded_file_response = vm::api::loadFiles(initProcess(), { file });
	ASSERT_TRUE(!loaded_file_response.has_value());
	auto err = loaded_file_response.error();
	ASSERT_TRUE(std::holds_alternative<vm::api::LoadProgramError>(err));
	auto err_str = std::get<vm::api::LoadProgramError>(err).why;
	std::cerr << err_str << '\n';
	for (auto err_key: error_keywords) {
		assertTrue(
			err_str.find(err_key) != std::string::npos, base::strConcat("Not found: ", err_key)
		);
	}
}

void VmTestSuite::loadValidDbc(const std::string& dbc_filename) {
	auto res = vm::api::loadFiles(initProcess(), { fs::File(path(dbc_filename)) });
	if (!res.has_value()) std::cerr << nlohmann::json(res.error()) << '\n';
	ASSERT_TRUE(res.has_value());
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
	const auto exit_value = vm::api::getExitValue(pid).transform([&](Ref<vm::VmValue> value) {
		ASSERT_TRUE(value->type->getName().str() == "i64");
		auto exit_code = value->readBytes<i64>();
		return exit_code;
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
	ASSERT_TRUE(validation_result.has_value());
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
	ASSERT_TRUE(run_result.has_value());
	ASSERT_TRUE(!vm::api::join(pid).has_value());

	if_opt_some(optional_input, input) { ASSERT_TRUE(vm::api::input(pid, input).has_value()); }

	if_opt_some(optional_output, output) {
		auto output_response = vm::api::output(pid);
		ASSERT_TRUE(output_response.has_value());
		ASSERT_EQUAL(output, output_response->output);
	}

	auto exit_value = run_result.value();
	if (expected_exit_code.has_value())
		ASSERT_EQUAL_PRINT(expected_exit_code.value(), exit_value->readBytes<i64>());
	else
		// @note: If expected_exit_code is an empty optional, it's expected that a called
		// function is void.
		ASSERT_TRUE(exit_value->type->getName() == base::StrID("void"));
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
