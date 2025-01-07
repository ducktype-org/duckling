#include "vm_tester_utils.hpp"

void VmTestSuite::runTestOnVm(
	const std::string& rbc_filename, const std::string& input, const std::string& output
) {
	auto process_pid_response = vm::api::spawn();
	assertTrue(process_pid_response.has_value(), "Spawn failed (1)");
	auto pid = process_pid_response.expect("Spawn failed (2)").pid;

	fs::FilePath file(path(rbc_filename));
	auto         loaded_file_response = vm::api::loadFile(pid, file);
	assertTrue(loaded_file_response.has_value(), "Load failed (1)");

	auto run_response = vm::api::run(pid);
	assertTrue(run_response.has_value(), "Run failed (1)");

	auto input_response = vm::api::input(pid, input);
	assertTrue(input_response.has_value(), "Input failed (1)");

	auto output_response = vm::api::output(pid);
	assertTrue(output_response.has_value(), "Output failed (1)");
	std::cerr << output_response.value().output << "\n";
	ASSERT_EQUAL( output, output_response.value().output);

	auto join_response = vm::api::join(pid);
	assertTrue(join_response.has_value(), "Join failed (1)");
}
