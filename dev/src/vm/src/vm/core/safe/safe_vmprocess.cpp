#include "safe_vmprocess.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
#include <vm/core/process/process_state.hpp>
#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/safe/safe_vmthread.hpp>
#include <vm/core/safe/vmvalue/safe_vmvalue.hpp>
#include <vm/core/safe/vmvalue/safe_vmvalueref.hpp>
#include <vm/core/thread/thread_state.hpp>
#include <vm/loader/loader.hpp>
#include <vm/loader/logger.hpp>

#include <expected>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <variant>

namespace vm {
	namespace ps = process_sm::process_state;
	namespace ts = thread_sm::thread_state;

	Memory& SafeVMProcess::getMemory() { return memory; }

	std::expected<api::Response, api::LoadProgramError> SafeVMProcess::loadProgram(
		const std::variant<std::vector<fs::File>, code::CodeCollection>& source
	) {
		std::unique_lock                          lock(rw_global);
		std::expected<void, loader::LoaderLogger> code_result = [&] {
			variant_match(source) {
				variant_case(std::vector<fs::File>, files) {
					return loader.loadAndValidate(files, execution_config);
				}
				variant_case(code::CodeCollection, code) {
					return loader.loadAndValidate(code, execution_config);
				}
			}
			CORE_UNREACHABLE();
		}();

		if (code_result.has_value()) {
			compiler.recompile();
			loaded_program_copy.selfUpdate();
			updateGlobalDataMemory(&loaded_program_copy);
			return api::Response(api::response::Empty());
		} else {
			std::stringstream ss;
			code_result.error().dump(ss);
			return std::unexpected(api::LoadProgramError{ ss.str() });
		}
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::runFunction(
		const std::string& func_name, const RunArguments& run_arguments
	) {
		std::unique_lock lock(rw_global);
		SafeVMThread&    thread = getEmptyThread();
		thread.setThreadCtx(func_name);

		bool response = thread.spawnThreadAndRun(func_name, run_arguments);

		if (!response) {
			thread.setThreadCtx("");
			return std::unexpected(api::ApiError{
				api::RunError{ "Failed to spawn thread for function: " + func_name } });
		}
		return api::Response(thread.getThreadID());
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::runFunctionAwait(
		const std::string& func_name, const RunArguments& run_arguments
	) {
		std::unique_lock lock(rw_global);

		SafeVMThread& main = getMainVMThread();
		main.setThreadCtx(func_name);
		if (!main.runNoSpawn(func_name, run_arguments)) {
			main.setThreadCtx("");
			return std::unexpected(api::ApiError{
				api::RunError{ "Main thread is already executing: " + func_name } });
		}

		variant_match(getProcessState()) {
			variant_case(ps::Completed, completed) { return api::Response{ completed.exit_value }; }
			variant_case(ps::Panicked, panicked) {
				return std::unexpected(api::StateError{ panicked.err });
			}
			variant_default {
				return std::unexpected(api::StateError("Execution did not complete"));
			}
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::join(api::ThreadID thread_id) {
		auto opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread)
			return std::unexpected(api::ApiError{ api::OtherError{ "Thread not found" } });
		return opt_thread.value()->join();
	}

	void SafeVMProcess::notifyVMThreadWaiters(api::ThreadID thread_id) {
		auto opt_thread = getVMThreadByID(thread_id);
		if (opt_thread) opt_thread.value()->notifyWaiters();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::stop() {
		// Destroying threads down races the memory-touching endpoints, thus the lock.
		std::unique_lock global_lock(rw_global);
		// First raise the process stop flag.
		(void) state_manager.requestStop();
		// Send a Stop request to all threads.
		requestStopAllThreads();
		// Wait for all threads to stop.
		(void) waitForProcessState([](const ProcessState& s) { return ps::isTerminal(s); });

		// `joinExecutionThread` blocks until the exec thread exits, and an exec thread
		// that has not exited yet can still take the `threads_pool_mutex` during panic etc. causing
		// a deadlock. Thus we iterate over a copy to not hold `threads_pool_mutex`.
		std::vector<Ref<SafeVMThread>> threads;
		{
			std::lock_guard lock(threads_pool_mutex);
			for (auto& thread: vm_threads) threads.emplace_back(&thread);
		}

		for (auto thread: threads) thread->joinExecutionThread();

		return api::Response(api::response::Empty());
	}

	void SafeVMProcess::requestStopAllThreads() noexcept {
		std::lock_guard lock(threads_pool_mutex);
		for (auto& thread: vm_threads) thread.requestStop();
	}

	base::Optional<api::ApiError> SafeVMProcess::assertProcessCanRespond() {
		if (!ps::canRespond(getProcessState()))
			return api::ApiError{ api::OtherError{
				"Cannot do memory request while program is running" } };

		return {};
	}

	Ref<IVMValue> SafeVMProcess::createVMValue(code::valid_type::ValidTypeID type_id) {
		// Safe TypeIDs are asserted (in the type builder) to be numerically equal to ValidTypeIDs.
		return createVMValue(loaded_program->getTypes().at(TypeID(type_id.asInt())));
	}

	Box<IVMValue> SafeVMProcess::createOwnedVMValue(code::valid_type::ValidTypeID type_id) {
		return createOwnedVMValue(loaded_program->getTypes().at(TypeID(type_id.asInt())));
	}

	Ref<SafeVMValue> SafeVMProcess::createVMValue(TypeCRef type) {
		auto value = Box<SafeVMValue>::fromPointer(new SafeVMValue(*this, type));
		owned_vm_values.emplace_back(std::move(value));
		return owned_vm_values.back().refMut();
	}

	Ref<SafeVMValue> SafeVMProcess::createVMValue(TypeCRef type, Pointer src) {
		auto value = Box<SafeVMValue>::fromPointer(new SafeVMValue(*this, type, src));
		owned_vm_values.emplace_back(std::move(value));
		return owned_vm_values.back().refMut();
	}

	Box<SafeVMValue> SafeVMProcess::createOwnedVMValue(TypeCRef type) {
		return Box<SafeVMValue>::fromPointer(new SafeVMValue(*this, type));
	}

	Box<SafeVMValue> SafeVMProcess::createOwnedVMValue(TypeCRef type, Pointer src) {
		return Box<SafeVMValue>::fromPointer(new SafeVMValue(*this, type, src));
	}

	SafeVMProcess::SafeVMProcess(const PID my_pid, bool enable_deadlock_detection):
		  IVMProcess(my_pid),
		  loaded_program(&loaded_program_copy),
		  loaded_program_copy(compiler.getLowProgram()) {
		if (enable_deadlock_detection) deadlock_detector.emplace();
		vm_threads.add(*this);
	}

	SafeVMThread& SafeVMProcess::getMainVMThread() { return *vm_threads.get(api::MAIN_THREAD_ID); }

	base::Optional<Ref<SafeVMThread>> SafeVMProcess::getVMThreadByID(api::ThreadID thread_id) {
		if_opt_some(vm_threads.maybeGet(thread_id), thread) return thread;
		return std::nullopt;
	}

	SafeVMThread& SafeVMProcess::getEmptyThread() {
		std::lock_guard lock(threads_pool_mutex);
		for (auto& thread: vm_threads) {
			// Thread must not be executing AND must not have an active exec_thread handle
			if (!ts::isActive(thread.getThreadState()) && !thread.hasActiveThread()) return thread;
		}
		return *vm_threads.get(vm_threads.add(*this));
	}

	std::expected<api::Response, api::StateError> SafeVMProcess::getExitCode() {
		const ProcessState state = getProcessState();
		variant_match(state) {
			variant_case(ps::Completed, completed) { return completed.exit_value; }
			variant_default return std::unexpected(api::StateError(
				v_matches(state, ps::NotStarted) ? "Execution did not start"
												 : "Execution did not complete"
			));
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::deinitAndValidate() {
		if (auto stop_result = stop(); !stop_result.has_value()) return stop_result;

		std::unique_lock lock(rw_global);
		try {
			// There might be numerous runtime exceptions during the deinitialization,
			// any of those means there was an issue during the validation.
			getMainVMThread().execGlobalDestructors();

			for (const auto& vm_value: owned_vm_values) vm_value->freeData();

			memory.deinitGlobals();
		} catch (exceptions::VMRuntimeException& e) {
			std::cerr << " - VM has detected issues during program\'s deinitialization: "
					  << e.what() << '\n';
			return false;
		}
		return memory.validateMemoryState();
	}

	base::Optional<api::ApiError> SafeVMProcess::pauseVMThread(api::ThreadID thread_id) {
		// API reads cannot happen while the thread is changing state.
		std::unique_lock lock(rw_global);
		auto             opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread) return api::ApiError{ api::OtherError{ "Thread not found" } };
		auto response = opt_thread.value()->pause();
		if (!response) return api::ApiError{ api::PauseError{} };
		return {};
	}

	base::Optional<api::ApiError> SafeVMProcess::resumeVMThread(api::ThreadID thread_id) {
		// API reads cannot happen while the thread is changing run state.
		std::unique_lock lock(rw_global);
		auto             opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread) return api::ApiError{ api::OtherError{ "Thread not found" } };
		auto response = opt_thread.value()->resume();
		if (!response) return api::ApiError{ api::ResumeError{} };
		return {};
	}

	base::Optional<api::ApiError> SafeVMProcess::stepVMThread(api::ThreadID thread_id) {
		// Stepping actually executes bytecode, which mutates process memory. It must not
		// run concurrently with other memory-touching endpoints.
		std::unique_lock lock(rw_global);

		// Try to obtain thread
		auto opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread) return api::ApiError{ api::OtherError{ "Thread not found" } };
		auto thread = opt_thread.value();

		// Try to obtain low position
		auto maybe_lp = thread->getCurrentPosition();
		if (!maybe_lp) return maybe_lp.error();
		auto low_position = maybe_lp.value();

		auto function = low_position.function;
		auto mapping  = function->instruction_mapping;

		// Default instruction range to step over is the whole function, in case we fail to obtain
		// high position
		low::LowFuncData::InstructionRange instr_range = {
			.begin = 0,
			.end   = std::numeric_limits<usize>::max(),
		};

		// Try to obtain high position and optimize instruction range to step over
		auto maybe_hp = compiler.mapLowVMProgramPositionToCodeCollectionPosition(low_position);
		if (maybe_hp) instr_range = mapping[maybe_hp->instruction_index];

		// We do one step, then we go until we're outside the exclusive range (begin, end).
		// Naive approach "while (in range [begin, end)) { microstep(); }" would fail on instruction
		// jumping to itself.

		auto in_exclusive_range = [=](const low::LowCodePosition& pos) {
			return pos.function == function && instr_range.begin < pos.instruction_index
			    && pos.instruction_index < instr_range.end;
		};

		do {
			auto response = thread->step();
			if (!response) return api::ApiError{ api::OtherError{ "step error" } };

			auto maybe_new_lp = thread->getCurrentPosition();
			if (!maybe_new_lp) return maybe_new_lp.error();
			low_position = maybe_new_lp.value();
		} while (in_exclusive_range(low_position));

		return std::nullopt;
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::getVMThreadCurrentPosition(
		api::ThreadID thread_id, base::Optional<usize> frame_idx
	) {
		std::shared_lock lock(rw_global);

		// Try to obtain thread
		auto opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread) return std::unexpected(api::OtherError{ "Thread not found" });
		auto thread = opt_thread.value();

		// Try to obtain low position
		auto maybe_lp = thread->getCurrentPosition(frame_idx);
		if (!maybe_lp) return std::unexpected(maybe_lp.error());
		auto low_position = maybe_lp.value();

		api::response::CodePosition code_position = {
			.function_name   = low_position.function->name,
			.instr_number    = 0,
			.source_position = std::nullopt,
		};

		// Try to obtain high position
		auto maybe_hp = compiler.mapLowVMProgramPositionToCodeCollectionPosition(low_position);
		if (!maybe_hp) return code_position;
		auto high_position = maybe_hp.value();

		code_position.instr_number = high_position.instruction_index;

		// Try to obtain source position
		auto maybe_sp = loader.mapCodeCollectionPositionToFilePosition(high_position);
		if (!maybe_sp) return code_position;
		auto source_position = maybe_sp.value();

		code_position.source_position = source_position;

		return code_position;
	}

	void SafeVMProcess::waitForBreakpoint() {
		(void) waitForProcessState([](const ProcessState& s) {
			return v_matches(s, ps::Paused) || ps::isTerminal(s);
		});
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::setExecutionConfig(
		const api::ExecutionConfig& config
	) {
		std::unique_lock lock(rw_global);
		this->execution_config = config;
		return api::Response(api::response::Empty());
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::getNumberOfCurrentStackFrames(
		api::ThreadID thread_id
	) {
		std::shared_lock lock(rw_global);
		match_optional(assertProcessCanRespond()) {
			opt_some(error) { return std::unexpected(error); }
			opt_none {
				auto opt_thread = getVMThreadByID(thread_id);
				if (!opt_thread)
					return std::unexpected(api::ApiError{ api::OtherError{ "Thread not found" } });
				u64 frames = opt_thread.value()->getNumberOfCurrentStackFrames();
				return api::Response(api::response::NumberOfCurrentStackFrames{
					.number_of_stack_frames = frames });
			}
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::getStackFrameData(
		api::ThreadID thread_id, u64 frame_index
	) {
		std::shared_lock lock(rw_global);
		match_optional(assertProcessCanRespond()) {
			opt_some(error) { return std::unexpected(error); }
			opt_none {
				auto opt_thread = getVMThreadByID(thread_id);
				if (!opt_thread)
					return std::unexpected(api::ApiError{ api::OtherError{ "Thread not found" } });
				auto thread        = opt_thread.value();
				auto maybe_low_pos = thread->getCurrentPosition(frame_index);
				if (!maybe_low_pos) return std::unexpected(maybe_low_pos.error());
				const low::LowCodePosition low_pos = maybe_low_pos.value();

				Frame& frame = thread->getStackFrame(frame_index);

				auto block_span
					= std::span(frame.local_block_ref_stack_base, frame.local_block_ref_stack_end);

				std::vector<api::response::StackFrameData::FrameVar> frame_vars;
				for (Block* const& block_ptr: block_span) {
					Ref<Block> block  = Ref(block_ptr);
					u64        offset = base::safeIntConv<u64>(
                        memory.getBlockViewUnsafe(block).getBegin() - frame.local_stack
                    );
					frame_vars.push_back(api::response::StackFrameData::FrameVar{
						.offset = offset,
						.name   = std::nullopt,
						.type   = std::nullopt,
						.value  = SafeVMValueRef::makeShared(
                            *this, memory.getBlockType(block), Pointer(block, 0)
                        ),
					});
				}

				if_opt_some(
					compiler.mapLowVMProgramPositionToCodeCollectionPosition(low_pos), high_pos
				) {
					auto func_opt
						= loader.getHighProgram()->functions().atMaybe(high_pos.function_name);
					CORE_ASSERT(func_opt, "We mapped low position to high, high-func should exist");
					auto  func_ref    = *func_opt;
					auto  stack_state = func_ref->stack_states.at(high_pos.instruction_index);
					auto& ls_db       = func_ref->local_stack;

					using namespace std::views;
					for (auto&& [block_idx, frame_var]: zip(iota(0u), frame_vars)) {
						frame_var.name = ls_db.getName(stack_state, block_idx);
						frame_var.type = ls_db.getTypeName(stack_state, block_idx);
						CORE_ASSERT(frame_var.type, "we should have a type of a variable on stack");
						CORE_ASSERT(frame_var.name, "we should have a name of a variable on stack");
					}
				}

				return api::Response(api::response::StackFrameData{
					.function_name = frame.current_function->name, .frame_vars = frame_vars });
			}
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::getTypeMetadata(
		const std::string& type_name
	) {
		std::shared_lock lock(rw_global);
		match_optional(assertProcessCanRespond()) {
			opt_some(error) { return std::unexpected(error); }
			opt_none {
				auto res = loaded_program->getTypes().atMaybe(base::StrID(type_name.c_str()));
				match_optional(res) {
					opt_some(value) { return api::response::Type{ value }; }
					opt_none {
						return std::unexpected(api::ApiError{ api::OtherError{ "Type not found" } });
					}
				}
			}
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::getVMValueForType(
		const std::string& type_name
	) {
		// Constructing VMValue allocates a block in memory, so it must be guarded against every
		// other memory-touching endpoint.
		std::unique_lock lock(rw_global);
		match_optional(assertProcessCanRespond()) {
			opt_some(error) { return std::unexpected(error); }
			opt_none {
				auto maybe_type
					= loaded_program->getTypes().atMaybe(base::StrID(type_name.c_str()));
				match_optional(maybe_type) {
					opt_some(type) {
						auto vm_value = createOwnedVMValue(type);
						return api::response::VMValue{ std::move(vm_value) };
					}
					opt_none {
						return std::unexpected(api::ApiError{ api::OtherError{ "Type not found" } });
					}
				}
			}
		}
		CORE_UNREACHABLE();
	}

	std::vector<api::ThreadID> SafeVMProcess::getAllThreadIDs() {
		std::lock_guard            lock(threads_pool_mutex);
		std::vector<api::ThreadID> thread_ids;
		for (const auto& thread: vm_threads)
			if (ts::isActive(thread.getThreadState())) thread_ids.push_back(thread.getThreadID());
		return thread_ids;
	}

	api::ThreadID SafeVMProcess::getMainThreadID() { return api::MAIN_THREAD_ID; }

	std::expected<api::Response, api::ApiError> SafeVMProcess::setBreakpoint(
		base::StrID function_name, usize instruction_index, bool enable
	) {
		std::unique_lock lock(rw_global);

		// Try to obtain original function
		auto maybe_original_function
			= loaded_program_copy.getOriginalProgram()->getFunctions().atMaybe(function_name);
		if (!maybe_original_function)
			return std::unexpected(api::OtherError{ "setBreakpoint: Function does not exist" });
		auto original_function = *maybe_original_function;

		// Obtain function copy (should never fail)
		auto function_copy = loaded_program_copy.getFunctions().at(function_name);

		// Try to obtain micro index
		if (original_function->instruction_mapping.size() <= instruction_index)
			return std::unexpected(api::OtherError{ "setBreakpoint: Function too short" });
		usize micro_instruction_index
			= original_function->instruction_mapping[instruction_index].begin;

		// Ensure micro index is in range (can happen when last FatBC instruction compiles to nothing)
		if (original_function->bc.size() <= micro_instruction_index
		    || function_copy->bc.size() <= micro_instruction_index)
			return std::unexpected(api::OtherError{ "setBreakpoint: No code after breakpoint" });

		auto new_opcode = enable
		                    ? vm::low::MicroOpcode::breakpoint
		                    : getInstructionOpcode(original_function->bc[micro_instruction_index]);

		// Try to replace the opcode
		auto maybe_old_opcode
			= loaded_program_copy.replaceOpcode(function_name, micro_instruction_index, new_opcode);
		if (!maybe_old_opcode) [[unlikely]]
			return std::unexpected(api::OtherError{ "setBreakpoint: Failed to set breakpoint" });

		return api::response::Empty{};
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::mapFileLineToCodeCollectionPosition(
		const fs::File& file, usize line_number
	) {
		std::shared_lock lock(rw_global);

		auto maybe_position = loader.mapFileLineToCodeCollectionPosition(file, line_number);
		if (!maybe_position)
			return std::unexpected(api::ApiError{
				api::OtherError{ "No instruction at given position" } });
		auto position = maybe_position.value();

		auto source_position = loader.mapCodeCollectionPositionToFilePosition(position).value();

		return api::response::CodePosition{
			.function_name   = position.function_name,
			.instr_number    = position.instruction_index,
			.source_position = source_position,
		};
	}

	void SafeVMProcess::updateGlobalDataMemory(CRef<low::ILowVMProgram> program) {
		using namespace std::ranges;
		auto global_buffer_config = program->getGlobalBufferConfig();
		auto global_indices       = program->getGlobals()
		                    | views::transform(&low::LowGlobalData::global_block_idx)
		                    | to<std::vector>();
		auto global_offsets = program->getGlobals()
		                    | views::transform(&low::LowGlobalData::global_buffer_offset)
		                    | to<std::vector>();
		auto global_types = program->getGlobals() | views::transform(&low::LowGlobalData::type)
		                  | to<std::vector>();

		auto new_global_buffer_pointers
			= memory.initializeNewGlobalBlocks(Memory::GlobalBlocksConfig{
				.global_data_offsets    = std::move(global_offsets),
				.global_blocks_idxs     = std::move(global_indices),
				.global_types           = std::move(global_types),
				.total_global_data_size = global_buffer_config.buffer_size,
				.global_count           = global_buffer_config.global_count,
			});

		std::lock_guard lock(threads_pool_mutex);
		for (auto& thread: vm_threads)
			thread.updateGlobalDataBufferPointers(new_global_buffer_pointers);
	}

	GIL& SafeVMProcess::getGIL() { return gil; }

	SynchronizationPrimitives& SafeVMProcess::getSynchronizationPrimitives() {
		return synchronization_primitives;
	}
}
