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
		TESTER_ADD_TEST(vmApiMemoryAllTypes);
	}


private:
	vm::PID loadProgram(std::string_view path_name) {
		auto process_pid_response = vm::api::spawn({ .enable_mapping = true });
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
		ASSERT_EQUAL_PRINT(6, execution_position.instr_number.value());

		vm::api::resume(pid).value();  // "Resume failed (1)"

		execution_position
			= vm::api::waitForBreakpoint(pid).value();  // "Wait for breakpoint failed (1)"
		ASSERT_EQUAL_PRINT(10, execution_position.instr_number.value());

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
		ASSERT_EQUAL_PRINT(7, line);

		line = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(8, line);

		vm::api::resume(pid).value();  // "Resume failed (1)"

		execution_position
			= vm::api::waitForBreakpoint(pid).value();  // "Wait for breakpoint failed (2)"
		ASSERT_EQUAL_PRINT(10, execution_position.instr_number.value());

		vm::api::resume(pid).value();  // "Resume failed (2)"

		vm::api::stop(pid).value();    // "Stop failed (1)"
	}

	u64 stepAndGetLine(u64 pid) {
		vm::api::step(base::safeIntConv<vm::PID>(pid)).value();  // "Step failed"
		auto execution_position = vm::api::getCurrentPosition(base::safeIntConv<vm::PID>(pid))
		                              .value();                  // "Get current position failed"

		ASSERT_TRUE(execution_position.instr_number.has_value());
		return execution_position.instr_number.value();
	}

	template<typename FiedDataType>
	FiedDataType getVMValueRefData(vm::VMValueRef vmvalue_ref) {
		auto data_opt = vmvalue_ref.readData();
		assertTrue(data_opt.has_value(), "VMValueRef: Referenced memory is dead");
		return std::get<FiedDataType>(data_opt.value());
	}

	template<typename FiedDataType>
	FiedDataType getStructField(
		vm::interpreted_data_variant::Data data_data, base::StrID type_id, base::StrID field_name
	) {
		auto field_index = data_data.field_name_map[field_name];
		auto field       = data_data.fields[field_index];
		assertEqual(
			field.value.getType()->getName(),
			type_id,
			"Variable type is not correct for field " + field_name.str()
		);
		return getVMValueRefData<FiedDataType>(field.value);
	}

	/**
	 * @brief Checks if the vm api functions related to memory and stack frames work correctly.
	 * Checks the number of stack frames, then resumes the program and checks if it finishes
	 * correctly.
	 */
	void vmApiMemoryAllTypes() {
		auto pid = loadProgram("breakpoint_all_types.dbc");
		auto tid = vm::api::ThreadID(0);

		ASSERT_TRUE(vm::api::run(pid).has_value());
		ASSERT_TRUE(vm::api::waitForBreakpoint(pid).has_value());

		{
			auto response = vm::api::debuggerGetNumberOfStackFrames(pid, tid);
			ASSERT_TRUE(response.has_value());
			auto num_frames_response = response.value();
			ASSERT_EQUAL_PRINT(num_frames_response.number_of_stack_frames, 2);
		}

		{
			auto response = vm::api::debuggerGetStackFrameData(pid, tid, 2);
			ASSERT_TRUE(!response.has_value());
		}

		{
			namespace idv = vm::interpreted_data_variant;

			auto response = vm::api::debuggerGetStackFrameData(pid, tid, 1);
			ASSERT_TRUE(response.has_value());

			auto stack_frame_data = response.value();
			ASSERT_EQUAL_PRINT(stack_frame_data.function_name, "main");

			for (const auto& var: stack_frame_data.frame_vars) {
				if (var.value.getType()->getName() == base::StrID("ptr_struct")) {
					auto struct_pointer_data_opt = var.value.readData();
					ASSERT_TRUE(struct_pointer_data_opt.has_value());

					auto struct_pointer_data
						= std::get<idv::Pointer>(struct_pointer_data_opt.value());
					ASSERT_TRUE(struct_pointer_data.referenced.has_value());

					auto struct_data
						= getVMValueRefData<idv::Data>(struct_pointer_data.referenced.value());

#define CHECK_PRIMITIVE_FIELD(field_name, expected_type, expected_value)     \
	{                                                                        \
		auto data = getStructField<idv::Primitive>(                          \
			struct_data, base::StrID(expected_type), base::StrID(field_name) \
		);                                                                   \
		auto value = data.value;                                             \
		ASSERT_EQUAL_PRINT(expected_value, value);                           \
	}

					CHECK_PRIMITIVE_FIELD("var_byte", "byte", 0);
					CHECK_PRIMITIVE_FIELD("var_i16", "i16", 0);
					CHECK_PRIMITIVE_FIELD("var_i32", "i32", 0);
					CHECK_PRIMITIVE_FIELD("var_i64", "i64", 0);

#undef CHECK_PRIMITIVE_FIELD

					{  // Check pointer field
						auto data = getStructField<idv::Pointer>(
							struct_data, base::StrID("ptr_struct"), base::StrID("var_pointer")
						);
						auto value = data.referenced;
						getVMValueRefData<idv::Data>(value.value());
					}

					{  // Check dynamic table field
						auto table_pointer = getStructField<idv::Pointer>(
							struct_data,
							base::StrID("ptr_dyntable_i64"),
							base::StrID("var_dyntable_pointer")
						);
						ASSERT_TRUE(table_pointer.referenced.has_value());
						auto table
							= getVMValueRefData<idv::Table>(table_pointer.referenced.value());
						auto first_elem      = table.get(0);
						auto primitive_value = getVMValueRefData<idv::Primitive>(first_elem);
						ASSERT_EQUAL_PRINT(primitive_value.value, 0);
					}

					{  // Check fixed size table field
						auto table_pointer = getStructField<idv::Pointer>(
							struct_data,
							base::StrID("ptr_fixtable_i64"),
							base::StrID("var_fixtable_pointer")
						);
						ASSERT_TRUE(table_pointer.referenced.has_value());
						auto table
							= getVMValueRefData<idv::Table>(table_pointer.referenced.value());
						auto first_elem      = table.get(0);
						auto primitive_value = getVMValueRefData<idv::Primitive>(first_elem);
						ASSERT_EQUAL_PRINT(primitive_value.value, 0);
					}

					{  // Check variant field
						auto variant = getStructField<idv::Variant>(
							struct_data, base::StrID("simple_variant"), base::StrID("var_variant")
						);
						ASSERT_EQUAL_PRINT(variant.type_tag, 0);
						auto primitive_value
							= getVMValueRefData<idv::Primitive>(variant.referenced);
						ASSERT_EQUAL_PRINT(primitive_value.value, 42);
					}
				}
			}
		}

		ASSERT_TRUE(vm::api::resume(pid).has_value());
		ASSERT_TRUE(vm::api::join(pid).has_value());

		{
			auto exit_code_response = vm::api::getExitValue(pid);
			ASSERT_TRUE(exit_code_response.has_value());
			ASSERT_EQUAL(exit_code_response.value().size(), 1);
			ASSERT_EQUAL_PRINT(exit_code_response.value().at(0)->readBytes<i64>(), 0);
		}
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
