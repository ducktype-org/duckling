#include <api/data/status.hpp>
#include <api/vm.hpp>
#include <api/api.hpp>
#include <cstdint>
#include <tester/tester.hpp>
#include <variant>
#include <thread>
#include <chrono>

class SimpleVmTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleVmTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(parsesTheFile);
		TESTER_ADD_TEST(stopTest);
		TESTER_ADD_TEST(killTest);
	}


private:
	vm::PID loadProgram(std::string_view path_name) {
		auto process_pid_response = vm::api::spawn();
		assertTrue(process_pid_response.has_value(), "Spawn failed (loadProgram)");
		auto pid = process_pid_response.value().pid;

		fs::FilePath file(path(std::string(path_name)));
		auto         loaded_file_response = vm::api::loadFile(pid, file);
		assertTrue(loaded_file_response.has_value(), "Load failed (loadProgram)");
		return pid;
	}

	// Checks if the program status is correct after loading a file.
	void parsesTheFile() {
		auto process_pid_response = vm::api::spawn();
		assertTrue(process_pid_response.has_value(), "Spawn failed (1)");
		auto pid = process_pid_response.expect("Spawn failed (2)").pid;

		auto status = vm::api::getExecutionStatus(pid);
		assertTrue(status.has_value(), "Status failed (1)");
		assertTrue(std::holds_alternative<vm::api::Executing>(status.value()), "Wrong status (1)");
		auto exec_status = std::get<vm::api::Executing>(status.value()).exec_status;
		assertTrue(
			std::holds_alternative<vm::api::NotStarted>(exec_status), "Wrong exec status (1)"
		);

		fs::FilePath file(path("vm_api_tests.dbc"));
		auto         loaded_file_response = vm::api::loadFile(pid, file);
		assertTrue(loaded_file_response.has_value(), "Load failed (1)");

		status = vm::api::getExecutionStatus(pid);
		assertTrue(status.has_value(), "Status failed (2)");
		assertTrue(std::holds_alternative<vm::api::Executing>(status.value()), "Wrong status (2)");
		exec_status = std::get<vm::api::Executing>(status.value()).exec_status;
		assertTrue(
			std::holds_alternative<vm::api::NotStarted>(exec_status), "Wrong exec status (2)"
		);
	}

	void stopTest() {
		auto pid = loadProgram("vm_api_tests.dbc");

		auto run_response = vm::api::run(pid);
		assertTrue(run_response.has_value(), "Run failed (1)");

		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		auto stop_response = vm::api::stop(pid);
		assertTrue(stop_response.has_value(), "Stop failed (1)");
	}

	void killTest() {
		auto pid = loadProgram("vm_api_tests.dbc");

		auto run_response = vm::api::run(pid);
		assertTrue(run_response.has_value(), "Run failed (1)");

		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		auto kill_response = vm::api::kill(pid);
		assertTrue(kill_response.has_value(), "Kill failed (1)");
	}
};

TESTER_COMMON_MAIN("/VM/tests/basic/");
