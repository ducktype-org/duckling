#include <base/misc/int_conv.hpp>

#include <tester/tester.hpp>

#include <vm/api/vm.hpp>

#include <chrono>
#include <thread>

class VmDebugTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebugTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(stopTest);
		TESTER_ADD_TEST(killTest);
		TESTER_ADD_TEST(pausesOnBreakpointAndResumes);
		TESTER_ADD_TEST(executesStepByStep);
		TESTER_ADD_TEST(vmApiMemory);
	}


private:
	vm::PID loadProgram(std::string_view path_name) {
		auto process_pid_response = vm::api::spawn();
		assertTrue(process_pid_response.has_value(), "Spawn failed (loadProgram)");
		auto pid = process_pid_response.value().pid;

		fs::File file(path(std::string(path_name)));
		auto     loaded_file_response = vm::api::loadFiles(pid, { file });
		assertTrue(loaded_file_response.has_value(), "Load failed (loadProgram)");
		return pid;
	}

	/**
	 * @brief Checks if the program can be stopped while waiting for input.
	 */
	void stopTest() {
		auto pid = loadProgram("vm_api_tests.dbc");

		auto run_response = vm::api::run(pid);
		assertTrue(run_response.has_value(), "Run failed (1)");

		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		auto stop_response = vm::api::stop(pid);
		assertTrue(stop_response.has_value(), "Stop failed (1)");
	}

	/**
	 * @brief Checks if the program can be killed while waiting for input.
	 */
	void killTest() {
		auto pid = loadProgram("vm_api_tests.dbc");

		auto run_response = vm::api::run(pid);
		assertTrue(run_response.has_value(), "Run failed (1)");

		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		auto kill_response = vm::api::kill(pid);
		assertTrue(kill_response.has_value(), "Kill failed (1)");
	}

	/**
	 * @brief Checks if the program will pause on breakpoint.
	 * Checks if `api::waitForPause` and `api::resume` functions work correctly.
	 */
	void pausesOnBreakpointAndResumes() {
		auto pid = loadProgram("breakpoint.dbc");
		vm::api::run(pid).value();  // "Run failed (1)"

		auto execution_position
			= vm::api::waitForBreakpoint(pid).value();  // "Wait for breakpoint failed (1)"
		assertEqual(6, execution_position.instr_number, "Line number is not correct");

		vm::api::resume(pid).value();  // "Resume failed (1)"

		execution_position
			= vm::api::waitForBreakpoint(pid).value();  // "Wait for breakpoint failed (1)"
		assertEqual(10, execution_position.instr_number, "Line number is not correct");

		vm::api::resume(pid).value();  // "Resume failed (2)"

		vm::api::stop(pid).value();    // "Stop failed (1)"
	}

	/**
	 * @brief Checks if the program will execute step by step.
	 */
	void executesStepByStep() {
		auto pid = loadProgram("breakpoint.dbc");

		vm::api::run(pid).value();  // "Run failed (1)"

		auto execution_position
			= vm::api::waitForBreakpoint(pid).value();  // "Wait for breakpoint failed (1)"
		assertEqual(6, execution_position.instr_number, "Line number is not correct");

		u64 line = stepAndGetLine(pid);
		assertEqual(7, line, "Line number is not correct (2)");

		line = stepAndGetLine(pid);
		assertEqual(8, line, "Line number is not correct (3)");

		vm::api::resume(pid).value();  // "Resume failed (1)"

		execution_position
			= vm::api::waitForBreakpoint(pid).value();  // "Wait for breakpoint failed (2)"
		assertEqual(10, execution_position.instr_number, "Line number is not correct (4)");

		vm::api::resume(pid).value();  // "Resume failed (2)"

		vm::api::stop(pid).value();    // "Stop failed (1)"
	}

	u64 stepAndGetLine(u64 pid) {
		vm::api::step(base::safeIntConv<vm::PID>(pid)).value();  // "Step failed"
		auto execution_position = vm::api::getCurrentPosition(base::safeIntConv<vm::PID>(pid))
		                              .value();                  // "Get current position failed"
		return execution_position.instr_number;
	}

	/**
	 * @brief Checks if the vm api functions related to memory and stack frames work correctly.
	 * Checks the number of stack frames, then resumes the program and checks if it finishes
	 * correctly.
	 */
	void vmApiMemory() {
		auto pid = loadProgram("breakpoint.dbc");

		{
			auto error_response
				= vm::api::debuggerGetTypeInfo(pid, base::StrID("non_existent_type"));
			assertFalse(error_response.has_value(), "Getting type info should have failed");
		}

		vm::api::run(pid).value();  // "Run failed (1)"

		auto execution_position
			= vm::api::waitForBreakpoint(pid).value();  // "Wait for breakpoint failed (1)"
		assertEqual(6, execution_position.instr_number, "Line number is not correct");

		auto num_frames_response = vm::api::debuggerGetNumberOfStackFrames(pid).value(
		);  // "Get number of stack frames failed"
		assertEqual(
			2, num_frames_response.number_of_stack_frames, "Number of stack frames is not correct"
		);

		auto error_response = vm::api::debuggerGetStackFrameData(pid, 2);
		assertFalse(error_response.has_value(), "Getting stack frame data should have failed");

		vm::api::resume(pid).value();  // "Resume failed (1)"

		execution_position
			= vm::api::waitForBreakpoint(pid).value();  // "Wait for breakpoint failed (2)"
		assertEqual(10, execution_position.instr_number, "Line number is not correct (2)");

		num_frames_response = vm::api::debuggerGetNumberOfStackFrames(pid).value(
		);  // "Get number of stack frames failed (2)"
		assertEqual(
			2,
			num_frames_response.number_of_stack_frames,
			"Number of stack frames is not correct (2)"
		);

		auto stack_frame_data
			= vm::api::debuggerGetStackFrameData(pid, 1).value();  // "Get stack frame data failed"
		assertEqual("main", stack_frame_data.function_name, "Function name is not correct");

		for (const auto& var: stack_frame_data.frame_vars) {
			auto var_type_info_reponse = vm::api::debuggerGetTypeInfo(pid, var.type);
			assertTrue(var_type_info_reponse.has_value(), "Get type info failed");
			auto var_type_info = var_type_info_reponse.value().type;

			if (var.offset == 0) {
				assertEqual(
					var_type_info->getName(), base::StrID("i64"), "Variable type is not correct"
				);

				auto pointer_data_response
					= vm::api::debuggerGetPointerData(
						  pid,
						  var.pointer,
						  var_type_info->getSize().assumePointerSize(vm::Type::POINTER_SIZE).asInt()
					)
				          .value();  // "Get pointer data failed"
				auto value = vm::safeReadPointerBytes<i64>(pointer_data_response.data.getBegin());
				assertEqual(0, value, "Variable value is not correct");
			}
			if (var_type_info->getName().strView().starts_with("ptr")) {
				auto response = vm::api::debuggerDereferencePointer(pid, var.pointer)
				                    .value();  // "Dereference pointer failed"
				assertTrue(response.pointer.isNull(), "Pointer should be null");

				auto dereference_response
					= vm::api::debuggerDereferencePointer(pid, response.pointer);
				assertFalse(
					dereference_response.has_value(), "Getting null pointer data should have failed"
				);
			}
		}

		vm::api::resume(pid).value();                                  // "Resume failed (2)"

		vm::api::join(pid).value();                                    // "Join failed (1)"

		auto exit_code_response = vm::api::getExitValue(pid).value();  // "Get exit value failed"
		assertEqual(0, exit_code_response->readBytes<i64>(), "Exit value is not correct");
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
