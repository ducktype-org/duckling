#include "api/data/status.hpp"
#include <api/api.hpp>
#include <tester/tester.hpp>
#include <chrono>
#include <thread>

class VmDebugTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebugTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(pausesOnBreakpoint); }


private:
	void pausesOnBreakpoint() {
		auto process_pid_response = vm::api::spawn();
		assertTrue(process_pid_response.has_value(), "Spawn failed (1)");
		auto pid = process_pid_response.expect("Spawn failed (2)").pid;

		fs::FilePath file(path("breakpoint.dbc"));
		auto         loaded_file_response = vm::api::loadFile(pid, file);
		assertTrue(loaded_file_response.has_value(), "Load failed (1)");

		auto run_response = vm::api::run(pid);
		assertTrue(run_response.has_value(), "Run failed (1)");


        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        assertPaused(vm::api::getExecutionStatus(pid));

		auto execution_position = vm::api::getCurrentPosition(pid).expect("Get current position failed (2)");
		std::cout << "Function ID: " << execution_position.function_id << ", Instruction number: " << execution_position.instr_number << std::endl;

		vm::api::resume(pid).expect("Resume failed (1)");

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        assertPaused(vm::api::getExecutionStatus(pid));

		execution_position = vm::api::getCurrentPosition(pid).expect("Get current position failed (2)");
		std::cout << "Function ID: " << execution_position.function_id << ", Instruction number: " << execution_position.instr_number << std::endl;

		auto stop_response = vm::api::stop(pid);
		assertTrue(stop_response.has_value(), "Stop failed (1)");
	}


	void assertPaused(const cpp::result<vm::api::ProcStatus, vm::api::ApiError>& status_response) {
        auto status = status_response.expect("Status failed (2)");
		auto exec_status = std::get<vm::api::Executing>(status).exec_status;
		assertTrue(std::holds_alternative<vm::api::Paused>(exec_status), "Status failed (3)");
	}
};

TESTER_COMMON_MAIN("/VM/tests/debugger/");
