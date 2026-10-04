#include <vm_tester_utils.hpp>

#include <base/misc/int_conv.hpp>

#include <tester/tester.hpp>

#include <vm/api/vm.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>

namespace api = vm::api;

/**
 * @brief Tests of the DVM debugger endpoints on a single-threaded process.
 *
 * The multi-threaded behaviour of the same endpoints is covered by
 * `multithreaded_debugger_tests.cpp`.
 */
class VmDebugTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebugTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(pausesOnBreakpointAndResumes);
		TESTER_ADD_TEST(notPausesOnRemovedBreakpoint);
		TESTER_ADD_TEST(executesStepByStep);
		TESTER_ADD_TEST(stepsUntilProgramTerminates);
		TESTER_ADD_TEST(pauseRefusals);
		TESTER_ADD_TEST(pausesAThreadSleepingOnIo);
		TESTER_ADD_TEST(resumeRefusals);
		TESTER_ADD_TEST(stepRefusals);
		TESTER_ADD_TEST(pauseAllEndpoint);
		TESTER_ADD_TEST(waitForBreakpointRefusals);
		TESTER_ADD_TEST(currentPositionEndpoint);
		TESTER_ADD_TEST(stackFrameRefusals);
		TESTER_ADD_TEST(setBreakpointRefusals);
		TESTER_ADD_TEST(backMapTest);
		TESTER_ADD_TEST(vmApiMemoryAllTypes);
		TESTER_ADD_TEST(outputTest);
	}

private:
	/**
	 * @brief Checks if the program will pause on breakpoint.
	 * Checks if `api::waitForPause` and `api::resume` functions work correctly.
	 */
	void pausesOnBreakpointAndResumes() {
		auto pid = spawnAndLoad("breakpoint.dbc");
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
		auto pid = spawnAndLoad("breakpoint.dbc");
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
		auto pid = spawnAndLoad("breakpoint.dbc");
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
		auto pid = spawnAndLoad("breakpoint.dbc");
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

		ASSERT_HAS_VALUE(vm::api::join(pid));

		auto exit_value = vm::api::getExitValue(pid);
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

	void pauseRefusals() {
		// `pause` refuses a thread that never started, one that already terminated and one it does
		// not know. Pausing a thread that is already paused is a no-op, not an error.

		const vm::PID pid = spawnAndLoad("while_true.dbc");

		assertRefusedWith<api::OtherError>(
			api::pause(pid, api::ThreadID{ 77 }), "pause of an unknown thread", "Thread not found"
		);
		assertRefusedWith<api::PauseError>(
			api::pause(pid), "pause before a run", "the thread has not started"
		);

		assertSucceeded(api::run(pid), "run");
		waitUntilStatus(pid, isRunning, "Running");

		assertSucceeded(api::pause(pid), "pause of a running thread");
		waitUntilStatus(pid, isPaused, "Paused");
		assertSucceeded(api::pause(pid), "pause of an already paused thread");
		waitUntilStatus(pid, isPaused, "still Paused");

		assertSucceeded(api::stop(pid), "stop");
		waitUntilStatus(pid, isTerminal, "Stopped");
		assertRefusedWith<api::PauseError>(
			api::pause(pid), "pause of a terminated thread", "the thread already terminated"
		);
		(void) api::kill(pid);
	}

	void pausesAThreadSleepingOnIo() {
		// A thread sleeping on IO cannot park while it sleeps - `waitInterruptible` only looks at
		// the stop flag - so `pause` keeps waiting and takes effect once the IO completes. The
		// waiting `pause` must not lock the API out while it does: the `input` that ends the wait
		// comes from another client thread.

		const vm::PID pid = spawnAndLoad("io_then_loop.dbc");
		assertSucceeded(api::run(pid), "run of io_then_loop.dbc");
		waitUntilStatus(pid, isSleeping, "Sleeping on IO");

		auto pending = std::async(std::launch::async, [pid] { return api::pause(pid); });
		assertTrue(
			pending.wait_for(BLOCKED_CALL_PROBE) != std::future_status::ready,
			"pause returned while the thread was still sleeping on IO, instead of waiting for it"
		);

		assertSucceeded(api::input(pid, "5 "), "input that wakes the sleeping thread");

		assertTrue(
			pending.wait_for(UNBLOCKED_CALL_BUDGET) == std::future_status::ready,
			"pause never returned after the IO it was waiting for completed"
		);
		auto position = pending.get();
		assertSucceeded(position, "pause of a thread that woke up from IO");
		ASSERT_EQUAL_PRINT(std::string("main"), position->function_name.str());
		waitUntilStatus(pid, isPaused, "Paused after the woken thread parked");

		assertSucceeded(api::stop(pid), "stop");
		(void) api::kill(pid);
	}

	void resumeRefusals() {
		// `resume` only means something to a paused thread.

		const vm::PID pid = spawnAndLoad("while_true.dbc");

		assertRefusedWith<api::OtherError>(
			api::resume(pid, api::ThreadID{ 77 }), "resume of an unknown thread", "Thread not found"
		);
		assertRefusedWith<api::ResumeError>(
			api::resume(pid), "resume before a run", "the thread is not paused"
		);

		assertSucceeded(api::run(pid), "run");
		waitUntilStatus(pid, isRunning, "Running");
		assertRefusedWith<api::ResumeError>(
			api::resume(pid), "resume of a running thread", "the thread is not paused"
		);

		assertSucceeded(api::stop(pid), "stop");
		(void) api::kill(pid);
	}

	void stepRefusals() {
		// A thread that is not paused has no position to step from.

		const vm::PID pid = spawnAndLoad("while_true.dbc");

		assertRefusedWith<api::OtherError>(
			api::step(pid, api::ThreadID{ 77 }), "step of an unknown thread", "Thread not found"
		);
		assertRefusedWith<api::OtherError>(
			api::step(pid), "step before a run", "wrong execution status"
		);

		assertSucceeded(api::run(pid), "run");
		waitUntilStatus(pid, isRunning, "Running");
		assertRefusedWith<api::OtherError>(
			api::step(pid), "step of a running thread", "wrong execution status"
		);

		assertSucceeded(api::stop(pid), "stop");
		(void) api::kill(pid);
	}

	void pauseAllEndpoint() {
		// On a single-threaded process `pauseAll` is `pause` of the main thread, and it reports an
		// empty list when there is nothing to pause.

		const vm::PID pid = spawnAndLoad("while_true.dbc");

		auto nothing_running = api::pauseAll(pid);
		assertSucceeded(nothing_running, "pauseAll on a NotStarted process");
		ASSERT_EQUAL_PRINT(usize{ 0 }, nothing_running->thread_ids.size());

		assertSucceeded(api::run(pid), "run");
		waitUntilStatus(pid, isRunning, "Running");

		auto paused = api::pauseAll(pid);
		assertSucceeded(paused, "pauseAll of a running process");
		ASSERT_EQUAL_PRINT(usize{ 1 }, paused->thread_ids.size());
		ASSERT_TRUE(paused->thread_ids.at(0) == api::MAIN_THREAD_ID);
		// Every thread parked, so the whole process reports Paused.
		waitUntilStatus(pid, isPaused, "Paused after pauseAll");

		// Idempotent: the already paused thread is reported again.
		auto again = api::pauseAll(pid);
		assertSucceeded(again, "a second pauseAll");
		ASSERT_EQUAL_PRINT(usize{ 1 }, again->thread_ids.size());

		assertSucceeded(api::resume(pid), "resume");
		assertSucceeded(api::stop(pid), "stop");
		(void) api::kill(pid);
	}

	void waitForBreakpointRefusals() {
		// `waitForBreakpoint` says the thread terminated instead of hanging forever when no
		// breakpoint is ever hit.

		const vm::PID pid = spawnAndLoad("breakpoint.dbc");
		assertRefusedWith<api::OtherError>(
			api::waitForBreakpoint(pid, api::ThreadID{ 77 }),
			"waitForBreakpoint on an unknown thread",
			"Thread not found"
		);

		assertSucceeded(api::run(pid), "run without any breakpoint");
		assertRefusedWith<api::StateError>(
			api::waitForBreakpoint(pid),
			"waitForBreakpoint of a thread that completes",
			"reached a terminal state instead of a breakpoint"
		);
		(void) api::kill(pid);
	}

	void currentPositionEndpoint() {
		// `getCurrentPosition` answers for a paused thread only, and bounds-checks the frame index.

		const vm::PID pid = spawnAndLoad("while_true.dbc");
		assertRefusedWith<api::OtherError>(
			api::getCurrentPosition(pid), "getCurrentPosition before a run", "wrong execution status"
		);

		assertSucceeded(api::run(pid), "run");
		waitUntilStatus(pid, isRunning, "Running");
		assertRefusedWith<api::OtherError>(
			api::getCurrentPosition(pid),
			"getCurrentPosition of a running thread",
			"wrong execution status"
		);

		assertSucceeded(api::pause(pid), "pause");
		auto top_frame = api::getCurrentPosition(pid);
		assertSucceeded(top_frame, "getCurrentPosition of a paused thread");
		ASSERT_EQUAL_PRINT(std::string("main"), top_frame->function_name.str());

		assertSucceeded(api::getCurrentPosition(pid, 0), "getCurrentPosition of frame 0");
		assertRefusedWith<api::OtherError>(
			api::getCurrentPosition(pid, 99),
			"getCurrentPosition of a frame that does not exist",
			"Frame index out of bounds"
		);

		assertSucceeded(api::stop(pid), "stop");
		(void) api::kill(pid);
	}

	void stackFrameRefusals() {
		// The two stack-frame endpoints read guest memory, so they refuse a process that is still
		// executing - before they even look at the thread id. What they return for a parked thread
		// is covered by `vmApiMemoryAllTypes`.

		const vm::PID pid = spawnAndLoad("while_true.dbc");
		assertSucceeded(api::run(pid), "run");
		waitUntilStatus(pid, isRunning, "Running");

		assertRefusedWith<api::OtherError>(
			api::debuggerGetNumberOfStackFrames(pid, api::MAIN_THREAD_ID),
			"debuggerGetNumberOfStackFrames of a running process",
			"while program is running"
		);
		assertRefusedWith<api::OtherError>(
			api::debuggerGetStackFrameData(pid, api::MAIN_THREAD_ID, 0),
			"debuggerGetStackFrameData of a running process",
			"while program is running"
		);

		assertSucceeded(api::pause(pid), "pause");
		waitUntilStatus(pid, isPaused, "Paused");

		assertRefusedWith<api::OtherError>(
			api::debuggerGetNumberOfStackFrames(pid, api::ThreadID{ 77 }),
			"debuggerGetNumberOfStackFrames of an unknown thread",
			"Thread not found"
		);
		assertRefusedWith<api::OtherError>(
			api::debuggerGetStackFrameData(pid, api::ThreadID{ 77 }, 0),
			"debuggerGetStackFrameData of an unknown thread",
			"Thread not found"
		);

		assertSucceeded(api::stop(pid), "stop");
		(void) api::kill(pid);
	}

	void setBreakpointRefusals() {
		// `setBreakpoint` names why it refuses, and is idempotent in both directions.

		const vm::PID pid = spawnAndLoad("breakpoint.dbc");

		assertRefusedWith<api::OtherError>(
			api::setBreakpoint(pid, base::StrID("no_such_function"), 0, true),
			"setBreakpoint in a function that does not exist",
			"Function does not exist"
		);
		assertRefusedWith<api::OtherError>(
			api::setBreakpoint(pid, base::StrID("main"), 9'999, true),
			"setBreakpoint past the end of a function",
			"Function too short"
		);

		assertSucceeded(api::setBreakpoint(pid, base::StrID("main"), 5, true), "setBreakpoint");
		assertSucceeded(
			api::setBreakpoint(pid, base::StrID("main"), 5, true), "the same setBreakpoint again"
		);
		assertSucceeded(
			api::setBreakpoint(pid, base::StrID("main"), 5, false), "clearing the breakpoint"
		);
		assertSucceeded(
			api::setBreakpoint(pid, base::StrID("main"), 5, false),
			"clearing a breakpoint that is not set"
		);
		(void) api::kill(pid);
	}

	/**
	 * @brief Checks dbc to cc mapping.
	 */
	void backMapTest() {
		auto pid = spawnAndLoad("vm_api_tests.dbc");

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
		auto pid = spawnAndLoad("breakpoint_all_types.dbc");
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
		ASSERT_HAS_VALUE(vm::api::join(pid));

		{
			auto exit_code_response = vm::api::getExitValue(pid);
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

		auto                          pid = spawnAndLoad("vm_api_tests.dbc");
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
