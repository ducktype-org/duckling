#include "vm_tester_utils.hpp"

#include "tester/tester.hpp"

#include <nlohmann/json_fwd.hpp>

#include <vm/api/data/status.hpp>
#include <vm/api/vm.hpp>

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

void VmTestSuite::loadInvalidDbc(
	const std::string& dbc_filename, const std::vector<std::string_view>& error_keywords
) {
	fs::FilePath file(path(dbc_filename));
	auto         loaded_file_response = vm::api::loadFiles(initProcess(), { file });
	ASSERT_TRUE(!loaded_file_response.has_value());
	auto err = loaded_file_response.error();
	ASSERT_TRUE(std::holds_alternative<vm::api::CoreOperationError>(err));
	auto core_op = std::get<vm::api::CoreOperationError>(err);
	ASSERT_TRUE(std::holds_alternative<vm::api::LoadProgramError>(core_op));
	auto err_str = std::get<vm::api::LoadProgramError>(core_op).why;
	// std::cerr << err_str << '\n';
	for (auto err_key: error_keywords) {
		assertTrue(
			err_str.find(err_key) != std::string::npos, base::strConcat("Not found: ", err_key)
		);
	}
}

void VmTestSuite::loadValidDbc(const std::string& dbc_filename) {
	ASSERT_TRUE(vm::api::loadFiles(initProcess(), { fs::FilePath(path(dbc_filename)) }).has_value());
}

#define EXPECT_VOID(result) \
	if (!result.has_value()) return { .pid = pid, .run_result = std::unexpected(result.error()) };

auto VmTestSuite::runTestImpl(
	vm::PID                            pid,
	const base::Optional<std::string>& optional_input,
	const base::Optional<std::string>& optional_output,
	const std::vector<std::string>&    args
) -> TestResult {
	EXPECT_VOID(vm::api::run(pid, args));

	if_opt_some(optional_input, input) { EXPECT_VOID(vm::api::input(pid, input)); }

	EXPECT_VOID(vm::api::join(pid));

	if_opt_some(optional_output, output) {
		auto output_response = vm::api::output(pid);
		EXPECT_VOID(output_response);
		ASSERT_EQUAL_PRINT(output, output_response->output);
	}

	return { .pid = pid, .run_result = vm::api::getExitCode(pid) };
}

auto VmTestSuite::runTestOnVmGetResult(
	const std::string&                 dbc_filename,
	const base::Optional<std::string>& optional_input,
	const base::Optional<std::string>& optional_output,
	const std::vector<std::string>&    args
) -> TestResult {
	auto pid  = initProcess();
	auto file = fs::FilePath(path(dbc_filename));
	EXPECT_VOID(vm::api::loadFiles(pid, { file }));
	return runTestImpl(pid, optional_input, optional_output, args);
}

auto VmTestSuite::runTestOnVmGetResult(
	const vm::code::CodeCollection&    code,
	const base::Optional<std::string>& optional_input,
	const base::Optional<std::string>& optional_output,
	const std::vector<std::string>&    args
) -> TestResult {
	auto pid = initProcess();

	EXPECT_VOID(vm::api::loadCode(pid, { code }));
	return runTestImpl(pid, optional_input, optional_output, args);
}

#undef EXPECT_VOID

void VmTestSuite::handleTestResult(const TestResult& test_result, i64 exit_code) {
	if (!test_result.run_result.has_value()) {
		auto err_str = to_string(nlohmann::json(test_result.run_result.error()));
		fail(err_str);
	}
	ASSERT_EQUAL_PRINT(test_result.run_result.value(), exit_code);
}
