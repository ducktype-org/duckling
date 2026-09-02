#include <base/misc/int_conv.hpp>

#include <tester/tester.hpp>

#include <vm/api/vm.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

class VmDebugTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebugTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(stopTest);
		TESTER_ADD_TEST(killTest);
		TESTER_ADD_TEST(pausesOnBreakpointAndResumes);
		TESTER_ADD_TEST(notPausesOnRemovedBreakpoint);
		TESTER_ADD_TEST(executesStepByStep);
		TESTER_ADD_TEST(stepsUntilProgramTerminates);
		TESTER_ADD_TEST(backMapTest);
		TESTER_ADD_TEST(vmApiMemoryAllTypes);
		TESTER_ADD_TEST(outputTest);
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
		for (auto breakpoint: { 5ULL, 8ULL })
			ASSERT_TRUE(
				vm::api::setBreakpoint(pid, base::StrID("main"), breakpoint, true).has_value()
			);

		vm::api::run(pid).value();  // "Run failed (1)"

		auto execution_position
			= vm::api::waitForBreakpoint(pid).value();  // "Wait for breakpoint failed (1)"
		ASSERT_EQUAL_PRINT(5, execution_position.instr_number);

		vm::api::resume(pid).value();  // "Resume failed (1)"

		execution_position
			= vm::api::waitForBreakpoint(pid).value();  // "Wait for breakpoint failed (1)"
		ASSERT_EQUAL_PRINT(8, execution_position.instr_number);

		vm::api::resume(pid).value();  // "Resume failed (2)"

		vm::api::stop(pid).value();    // "Stop failed (1)"
	}

	/**
	 * @brief Checks if the program will not pause on removed breakpoint.
	 * Checks if `api::waitForPause` and `api::resume` functions work correctly.
	 */
	void notPausesOnRemovedBreakpoint() {
		auto pid = loadProgram("breakpoint.dbc");
		for (auto breakpoint: { 5ULL, 8ULL })
			ASSERT_TRUE(
				vm::api::setBreakpoint(pid, base::StrID("main"), breakpoint, true).has_value()
			);

		ASSERT_HAS_VALUE(vm::api::setBreakpoint(pid, base::StrID("main"), 5, false));

		ASSERT_HAS_VALUE(vm::api::run(pid));

		auto execution_position = vm::api::waitForBreakpoint(pid);
		ASSERT_HAS_VALUE(execution_position);

		ASSERT_EQUAL_PRINT(8, execution_position->instr_number);

		ASSERT_HAS_VALUE(vm::api::resume(pid));
		ASSERT_HAS_VALUE(vm::api::stop(pid));
	}

	/**
	 * @brief Checks if the program will execute step by step.
	 */
	void executesStepByStep() {
		auto pid = loadProgram("breakpoint.dbc");
		for (auto breakpoint: { 5ULL, 8ULL })
			ASSERT_TRUE(
				vm::api::setBreakpoint(pid, base::StrID("main"), breakpoint, true).has_value()
			);

		vm::api::run(pid).value();  // "Run failed (1)"

		auto execution_position
			= vm::api::waitForBreakpoint(pid).value();  // "Wait for breakpoint failed (1)"
		ASSERT_EQUAL_PRINT(5, execution_position.instr_number);

		u64 line = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(6, line);

		line = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(7, line);

		vm::api::resume(pid).value();  // "Resume failed (1)"

		execution_position
			= vm::api::waitForBreakpoint(pid).value();  // "Wait for breakpoint failed (2)"
		ASSERT_EQUAL_PRINT(8, execution_position.instr_number);

		vm::api::resume(pid).value();  // "Resume failed (2)"

		vm::api::stop(pid).value();    // "Stop failed (1)"
	}

	void stepsUntilProgramTerminates() {
		auto pid = loadProgram("breakpoint.dbc");
		ASSERT_HAS_VALUE(vm::api::setBreakpoint(pid, base::StrID("main"), 2, true));
		ASSERT_HAS_VALUE(vm::api::run(pid));
		ASSERT_HAS_VALUE(vm::api::waitForBreakpoint(pid));
		ASSERT_HAS_VALUE(vm::api::setBreakpoint(pid, base::StrID("main"), 2, false));

		constexpr u64 STEP_LIMIT = 100;

		bool terminated = false;
		for (u64 i = 0; i < STEP_LIMIT && !terminated; i++) {
			ASSERT_HAS_VALUE(vm::api::step(pid));

			auto status = vm::api::getExecutionStatus(pid);
			ASSERT_HAS_VALUE(status);
			terminated = vm::api::isStatusTerminal(status.value());
		}
		ASSERT_TRUE(terminated);

		auto status = vm::api::getExecutionStatus(pid);
		ASSERT_HAS_VALUE(status);
		ASSERT_TRUE(v_matches(status.value(), vm::api::ExecutionCompleted));

		auto exit_value = vm::api::join(pid);
		ASSERT_HAS_VALUE(exit_value);
		ASSERT_EQUAL_PRINT(
			v_get(exit_value.value(), std::vector<Ref<vm::IVMValue>>).at(0)->readBytes<i64>(), 1
		);

		// Memory state should be intact.
		auto validation_result = vm::api::deinitAndValidate(pid);
		ASSERT_HAS_VALUE(validation_result);
		ASSERT_TRUE(validation_result.value());
	}

	u64 stepAndGetLine(vm::PID pid) {
		vm::api::step(pid).value();                      // "Step failed"
		auto execution_position
			= vm::api::getCurrentPosition(pid).value();  // "Get current position failed"

		return execution_position.instr_number;
	}

	template<typename FieldDataType>
	FieldDataType getVMValueRefData(const SharedBox<vm::IVMValueRef>& vmvalue_ref) {
		auto data_opt = vmvalue_ref->readData();
		assertTrue(data_opt.has_value(), "VMValueRef: Referenced memory is dead");
		return std::get<FieldDataType>(data_opt.value());
	}

	template<typename FieldDataType>
	FieldDataType getStructField(
		vm::interpreted_data_variant::Data data_data, base::StrID type_id, base::StrID field_name
	) {
		auto field_index = data_data.field_name_map[field_name];
		auto field       = data_data.fields[field_index];
		assertEqual(
			field.value->getType()->getName(),
			type_id,
			"Variable type is not correct for field " + field_name.str()
		);
		return getVMValueRefData<FieldDataType>(field.value);
	}

	/**
	 * @brief Checks dbc to cc mapping.
	 */
	void backMapTest() {
		auto pid = loadProgram("vm_api_tests.dbc");

		auto assert_mapping = [&](usize line, usize index) {
			fs::File file(path("vm_api_tests.dbc"));
			auto     map_response = vm::api::mapFileLineToCodeCollectionPosition(pid, file, line);

			ASSERT_HAS_VALUE(map_response);
			auto code_position = map_response.value();

			ASSERT_EQUAL_PRINT(code_position.function_name, "main");
			ASSERT_EQUAL_PRINT(code_position.instr_number, index);
		};

		auto assert_no_maping = [&](usize line) {
			fs::File file(path("vm_api_tests.dbc"));
			auto     map_response = vm::api::mapFileLineToCodeCollectionPosition(pid, file, line);
			ASSERT_TRUE(!map_response);
		};

		assert_mapping(11, 2);
		assert_mapping(20, 9);
		assert_mapping(3, 0);
		assert_mapping(8, 0);
		assert_no_maping(2);
		assert_no_maping(4);
		assert_no_maping(16);
		assert_no_maping(21);
		assert_no_maping(22);
	}

	/**
	 * @brief Checks if the vm api functions related to memory and stack frames work correctly.
	 * Checks the number of stack frames, then resumes the program and checks if it finishes
	 * correctly.
	 */
	void vmApiMemoryAllTypes() {
		auto pid = loadProgram("breakpoint_all_types.dbc");
		ASSERT_HAS_VALUE(vm::api::setBreakpoint(pid, base::StrID("main"), 20, true));
		auto tid = vm::api::ThreadID(0);

		ASSERT_HAS_VALUE(vm::api::run(pid));
		ASSERT_HAS_VALUE(vm::api::waitForBreakpoint(pid));

		{
			auto response = vm::api::debuggerGetNumberOfStackFrames(pid, tid);
			ASSERT_HAS_VALUE(response);
			auto num_frames_response = response.value();
			ASSERT_EQUAL_PRINT(num_frames_response.number_of_stack_frames, 2);
		}

		{
			auto response = vm::api::debuggerGetStackFrameData(pid, tid, 2);
			ASSERT_NO_VALUE(response);
		}

		{
			namespace idv = vm::interpreted_data_variant;

			auto response = vm::api::debuggerGetStackFrameData(pid, tid, 1);
			ASSERT_HAS_VALUE(response);

			auto stack_frame_data = response.value();
			ASSERT_EQUAL_PRINT(stack_frame_data.function_name, "main");

			for (const auto& var: stack_frame_data.frame_vars) {
				if (var.value->getType()->getName() == base::StrID("ptr_struct")) {
					auto struct_pointer_data_opt = var.value->readData();
					ASSERT_HAS_VALUE(struct_pointer_data_opt);

					auto struct_pointer_data
						= std::get<idv::Pointer>(struct_pointer_data_opt.value());
					ASSERT_HAS_VALUE(struct_pointer_data.referenced);

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
						ASSERT_HAS_VALUE(table_pointer.referenced);
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
						ASSERT_HAS_VALUE(table_pointer.referenced);
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

		ASSERT_HAS_VALUE(vm::api::resume(pid));

		{
			auto exit_code_response = vm::api::join(pid);
			ASSERT_HAS_VALUE(exit_code_response);
			ASSERT_TRUE(
				std::holds_alternative<std::vector<Ref<vm::IVMValue>>>(exit_code_response.value())
			);
			auto& exit_value_vec
				= std::get<std::vector<Ref<vm::IVMValue>>>(exit_code_response.value());
			ASSERT_EQUAL(exit_value_vec.size(), 1);
			ASSERT_EQUAL_PRINT(exit_value_vec.at(0)->readBytes<i64>(), 0);
		}
	}

	/**
	 * @brief test if the output emitter works
	 */
	void outputTest() {
		std::atomic_bool        output = false;
		std::condition_variable cv;
		std::mutex              m;

		auto                          pid = loadProgram("vm_api_tests.dbc");
		events::Listener<std::string> output_listener([&](const std::string& str) {
			ASSERT_EQUAL_PRINT("7", str);
			output.store(true);
			cv.notify_all();
		});

		ASSERT_HAS_VALUE(vm::api::attachOutputListener(pid, &output_listener));

		ASSERT_HAS_VALUE(vm::api::run(pid));

		std::unique_lock lk(m);
		// Test timeout
		ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(200), [&] { return output.load(); }));

		ASSERT_HAS_VALUE(vm::api::stop(pid));
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
