#include "safe_vmprocess.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
#include <vm/bytecode/validator/valid_function.hpp>
#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/safe/safe_vmthread.hpp>
#include <vm/core/vmvalue/vmvalue.hpp>
#include <vm/loader/loader.hpp>
#include <vm/loader/logger.hpp>

#include <expected>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <thread>
#include <variant>

namespace vm {
	Memory& SafeVMProcess::getMemory() { return memory; }

	std::expected<api::Response, api::LoadProgramError> SafeVMProcess::loadProgram(
		const std::variant<std::vector<fs::File>, code::CodeCollection>& source
	) {
		std::unique_lock                          lock(rw_global);
		std::expected<void, loader::LoaderLogger> code_result = [&] {
			variant_match(source) {
				variant_case(std::vector<fs::File>, files) { return loader.loadAndValidate(files); }
				variant_case(code::CodeCollection, code) { return loader.loadAndValidate(code); }
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

		getMainVMThread().runNoSpawn(func_name, run_arguments);
		variant_match(getStatus()) {
			variant_case(api::ExecutionCompleted, completed) { return completed.exit_value; }
			variant_default return std::unexpected(api::StateError(
				hasExecutionStarted(getStatus()) ? "Execution did not complete"
												 : "Execution did not start"
			));
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::join(api::ThreadID thread_id) {
		auto opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread)
			return std::unexpected(api::ApiError{ api::OtherError{ "Thread not found" } });
		return opt_thread.value()->join();
	}

	void SafeVMProcess::notifyPausedVMThread(api::ThreadID thread_id) {
		auto opt_thread = getVMThreadByID(thread_id);
		if (opt_thread) opt_thread.value()->notifyPaused();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::stop() {
		for (auto& thread: vm_threads) {
			auto response = thread.stop();

			if (!thread.joinExecutionThread())
				return std::unexpected(api::ApiError{ api::JoinError{} });

			// @TODO: #1222 make two different "stop" functions, one that throws error if
			// program was not stopped successfully and another that does nothing
			if (!response) return std::unexpected(api::OtherError{ "unexpected status response" });
		}

		return api::response::Empty{};
	}

	void SafeVMProcess::onTerminalStatus(const api::ProcStatus& status) noexcept {
		if (!api::isStatusTerminal(status)) return;

		const auto current_thread_id = std::this_thread::get_id();

		// Find which thread is currently executing (calling this hook)
		api::ThreadID executing_thread_id = api::ThreadID{ ~0ULL };
		for (auto& thread: vm_threads) {
			if (thread.exec_thread && thread.exec_thread->get_id() == current_thread_id) {
				executing_thread_id = thread.getThreadID();
				break;
			}
		}

		// Only auto-stop child threads if the main thread (id=0) triggered the terminal status.
		// If a child thread panicked, the main thread may be blocked in join() waiting for that
		// child, so trying to stop the main thread would cause deadlock. In that case, let the
		// panic propagate and rely on explicit cleanup.
		if (executing_thread_id.asInt() != 0) return;

		// Main thread became terminal: stop all active child threads
		for (auto& thread: vm_threads) {
			if (!thread.hasActiveThread()) continue;
			if (thread.getThreadID().asInt() == 0) continue;  // Skip main thread

			const bool stop_requested = thread.stop();
			if (!stop_requested) continue;

			thread.joinExecutionThread();
		}
	}

	base::Optional<api::ApiError> SafeVMProcess::assertProcessCanRespond() {
		api::ProcStatus status = getStatus();

		if (!api::canRespond(status))
			return api::ApiError{ api::OtherError{
				"Cannot do memory request while program is running" } };

		return {};
	}

	Ref<VmValue> SafeVMProcess::createVmValue(TypeCRef type) {
		auto value = Box<VmValue>::fromPointer(new VmValue(*this, type));
		owned_vm_values.push_back(std::move(value));
		return owned_vm_values.back().refMut();
	}

	Ref<VmValue> SafeVMProcess::createVmValue(TypeCRef type, Pointer src) {
		auto value = Box<VmValue>::fromPointer(new VmValue(*this, type, src));
		owned_vm_values.push_back(std::move(value));
		return owned_vm_values.back().refMut();
	}

	Box<VmValue> SafeVMProcess::createOwnedVmValue(TypeCRef type) {
		return Box<VmValue>::fromPointer(new VmValue(*this, type));
	}

	Box<VmValue> SafeVMProcess::createOwnedVmValue(TypeCRef type, Pointer src) {
		return Box<VmValue>::fromPointer(new VmValue(*this, type, src));
	}

	SafeVMProcess::SafeVMProcess(const PID my_pid, bool enable_deadlock_detection):
		  IVMProcess(my_pid),
		  loaded_program(&loaded_program_copy),
		  loaded_program_copy(compiler.getLowProgram()) {
		if (enable_deadlock_detection) deadlock_detector.emplace();
		vm_threads.add(*this);
	}

	SafeVMThread& SafeVMProcess::getMainVMThread() { return *vm_threads.get(api::ThreadID{ 0 }); }

	base::Optional<Ref<SafeVMThread>> SafeVMProcess::getVMThreadByID(api::ThreadID thread_id) {
		if_opt_some(vm_threads.maybeGet(thread_id), thread) return thread;
		return std::nullopt;
	}

	SafeVMThread& SafeVMProcess::getEmptyThread() {
		for (auto& thread: vm_threads) {
			// Thread must not be executing AND must not have an active exec_thread handle
			if (!api::isExecuting(thread.getStatus()) && !thread.hasActiveThread()) return thread;
		}
		return *vm_threads.get(vm_threads.add(*this));
	}

	std::expected<api::Response, api::StateError> SafeVMProcess::getExitCode() {
		std::unique_lock lock(rw_global);
		variant_match(getStatus()) {
			variant_case(api::ExecutionCompleted, completed) { return completed.exit_value; }
			variant_default return std::unexpected(api::StateError(
				hasExecutionStarted(getStatus()) ? "Execution did not complete"
												 : "Execution did not start"
			));
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::deinitAndValidate() {
		std::unique_lock lock(rw_global);
		for (auto& t: vm_threads) {
			if (api::isExecuting(t.getStatus()))
				if (auto res = stop(); !res.has_value()) return res;
		}
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
		auto opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread) return api::ApiError{ api::OtherError{ "Thread not found" } };
		auto response = opt_thread.value()->pause();
		if (!response) return api::ApiError{ api::PauseError{} };
		return {};
	}

	base::Optional<api::ApiError> SafeVMProcess::resumeVMThread(api::ThreadID thread_id) {
		auto opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread) return api::ApiError{ api::OtherError{ "Thread not found" } };
		auto response = opt_thread.value()->resume();
		if (!response) return api::ApiError{ api::ResumeError{} };
		return {};
	}

	base::Optional<api::ApiError> SafeVMProcess::stepVMThread(api::ThreadID thread_id) {
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
		api::ThreadID thread_id
	) {
		// Try to obtain thread
		auto opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread) return std::unexpected(api::OtherError{ "Thread not found" });
		auto thread = opt_thread.value();

		// Try to obtain low position
		auto maybe_lp = thread->getCurrentPosition();
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
		std::shared_lock lock(rw_status);
		status_cv.wait(lock, [&] {
			return std::holds_alternative<api::Paused>(status) || api::isStatusTerminal(status);
		});
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
				u64 frames = opt_thread.value()->getNumberOfCurrentStackFrames();
				if (frame_index >= frames)
					return std::unexpected(api::ApiError{
						api::OtherError{ "Frame index out of bounds" } });
				const Frame& frame = opt_thread.value()->getStackFrame(frame_index);

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
						.value  = VMValueRef(*this, memory.getBlockType(block), Pointer(block, 0)),
					});
				}

				auto opt_low_pos = opt_thread.value()->getCurrentPosition(frame_index);
				if_opt_none(opt_low_pos) api::Response(api::response::StackFrameData{
					.function_name = frame.current_function->name, .frame_vars = frame_vars });

				if_opt_some(
					compiler.mapLowVMProgramPositionToCodeCollectionPosition(*opt_low_pos), high_pos
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
		std::shared_lock lock(rw_global);
		match_optional(assertProcessCanRespond()) {
			opt_some(error) { return std::unexpected(error); }
			opt_none {
				auto maybe_type
					= loaded_program->getTypes().atMaybe(base::StrID(type_name.c_str()));
				match_optional(maybe_type) {
					opt_some(type) {
						auto vm_value = createOwnedVmValue(type);
						return api::response::VmValue{ std::move(vm_value) };
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
		std::shared_lock           lock(rw_global);
		std::vector<api::ThreadID> thread_ids;
		for (const auto& thread: vm_threads)
			if (api::isExecuting(thread.getStatus())) thread_ids.push_back(thread.getThreadID());
		return thread_ids;
	}

	api::ThreadID SafeVMProcess::getMainThreadID() {
		std::shared_lock lock(rw_global);
		return getMainVMThread().getThreadID();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::setBreakpoint(
		base::StrID function_name, usize instruction_index, bool enable
	) {
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

	std::expected<api::Response, api::ApiError> SafeVMProcess::evalRuntimeExpr(
		api::ThreadID thread_id, const std::variant<fs::File, code::Function>& source
	) {
		auto opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread)
			return std::unexpected(api::ApiError{ api::OtherError{ "Thread not found" } });

		auto thread_ref = *opt_thread;

		std::unique_lock                                                         lock(rw_global);
		std::expected<code::valid_function::ValidFunction, loader::LoaderLogger> valid_expr = [&] {
			variant_match(source) {
				variant_case(fs::File, files) { return loader.validateExpr(thread_ref, files); }
				variant_case(code::Function, func) { return loader.validateExpr(thread_ref, func); }
			}
			CORE_UNREACHABLE();
		}();

		if (valid_expr.has_value()) {
			auto success = thread_ref->loadAndExecRuntimeExpr(std::move(valid_expr).value());
			if (!success)
				return std::unexpected(api::ApiError{
					api::OtherError{ "Couldn't execute the expression" } });
			return api::Response(api::response::Empty());
		} else {
			std::stringstream ss;
			valid_expr.error().dump(ss);
			return std::unexpected(api::LoadProgramError{ ss.str() });
		}

		return api::Response(api::response::Empty());
	}

	low::LowFuncData SafeVMProcess::compileToLow(
		CRef<SafeVMThread> thread, const code::valid_function::ValidFunction& expr
	) const {
		return compiler.lowerExpr(expr, thread);
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

		for (auto& thread: vm_threads)
			thread.updateGlobalDataBufferPointers(new_global_buffer_pointers);
	}

	GIL& SafeVMProcess::getGIL() { return gil; }

	SynchronizationPrimitives& SafeVMProcess::getSynchronizationPrimitives() {
		return synchronization_primitives;
	}
}
