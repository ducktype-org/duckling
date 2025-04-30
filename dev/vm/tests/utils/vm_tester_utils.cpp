#include "vm_tester_utils.hpp"

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
	auto pid  = initProcess();
	auto file = fs::FilePath(path(dbc_filename));
	ASSERT_TRUE(vm::api::loadFile(pid, file).has_value());
	runTestImpl(pid, optional_input, optional_output, args, exit_code);
}

void VmTestSuite::runTestOnVm(
	const vm::code::CodeCollection&    code,
	const base::Optional<std::string>& optional_input,
	const base::Optional<std::string>& optional_output,
	const std::vector<std::string>&    args,
	i64                                exit_code
) {
	auto pid = initProcess();

	ASSERT_TRUE(vm::api::loadCode(pid, code).has_value());
	runTestImpl(pid, optional_input, optional_output, args, exit_code);
}

void VmTestSuite::runTestImpl(
	vm::PID                            pid,
	const base::Optional<std::string>& optional_input,
	const base::Optional<std::string>& optional_output,
	const std::vector<std::string>&    args,
	i64                                exit_code
) {
	ASSERT_TRUE(vm::api::run(pid, args).has_value());

	if_opt_some(optional_input, input) { ASSERT_TRUE(vm::api::input(pid, input).has_value()); }

	ASSERT_TRUE(vm::api::join(pid).has_value());

	if_opt_some(optional_output, output) {
		auto output_response = vm::api::output(pid);
		ASSERT_TRUE(output_response.has_value());
		ASSERT_EQUAL(output, output_response->output);
	}

	auto exit_code_response = vm::api::getExitCode(pid);
	ASSERT_TRUE(exit_code_response.has_value());
	ASSERT_EQUAL_PRINT(exit_code, *exit_code_response);
}

void VmTestSuite::loadInvalidDbc(
	const std::string& dbc_filename, const std::vector<std::string_view>& error_keywords
) {
	fs::FilePath file(path(dbc_filename));
	auto         loaded_file_response = vm::api::loadFile(initProcess(), file);
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
	ASSERT_TRUE(
		vm::api::loadFile(vm::api::spawn()->pid, fs::FilePath(path(dbc_filename))).has_value()
	);
}
