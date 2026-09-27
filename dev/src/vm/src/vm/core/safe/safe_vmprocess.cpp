#include "safe_vmprocess.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/core/process/process_state.hpp>
#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/safe/memory/local_slot_block.hpp>
#include <vm/core/safe/safe_vmthread.hpp>
#include <vm/core/safe/vmvalue/safe_vmvalue.hpp>
#include <vm/core/safe/vmvalue/safe_vmvalueref.hpp>
#include <vm/core/thread/kill_process_exception.hpp>
#include <vm/core/thread/thread_state.hpp>
#include <vm/loader/loader.hpp>
#include <vm/loader/logger.hpp>

#include <expected>
#include <iostream>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <sstream>
#include <variant>

namespace vm {
	namespace ps = process_state;
	namespace ts = thread_state;

	Memory& SafeVMProcess::getMemory() { return memory; }

	std::string argumentCountMismatchMessage(const low::LowFuncData& func, usize provided) {
		return base::strConcat(
			"Function '",
			func.getName().str(),
			"' expects ",
			func.getParameters().size(),
			" arguments, but ",
			provided,
			" were provided."
		);
	}

	std::expected<api::Response, api::LoadProgramError> SafeVMProcess::loadProgram(
		const std::variant<std::vector<fs::File>, code::CodeCollection>& source
	) {
		std::unique_lock                          lock(api_lock);
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
			// Exec threads never take api_lock; the GIL is what excludes them. recompile()
			// grows LowVMProgram::functions (a std::vector-backed map), which moves every
			// LowFuncData — and with it every CRef<LowFuncData> held by running frames
			// (frame->current_function) and every jit_data reference in the entrypoint
			// opfuns — so it must run under the GIL.
			GIL::ScopedLock gil_lock(gil);
			compiler.recompile();
			updateGlobalDataMemory(loaded_program);
			return api::Response(api::response::Empty());
		} else {
			std::stringstream ss;
			code_result.error().dump(ss);
			return std::unexpected(api::LoadProgramError{ ss.str() });
		}
	}

	std::expected<void, api::ApiError> SafeVMProcess::validateRunArguments(
		const std::string& func_name, const RunArguments& run_arguments
	) const {
		// Doesn't modify, just reads.
		std::shared_lock lock(api_lock);

		const auto refuse = [](std::string why) {
			return std::unexpected(api::ApiError{ api::RunError{ std::move(why) } });
		};

		const auto& maybe_func = loaded_program->getFunctions().atMaybe(base::StrID(func_name));
		if (!maybe_func.has_value())
			return refuse(base::strConcat("Called function '", func_name, "' does not exist."));

		// Only function arguments need validation.
		if (not v_matches(run_arguments, FunctionRunArguments)) return {};

		const auto& func      = *maybe_func.value();
		const auto& func_args = v_get(run_arguments, FunctionRunArguments);

		if (func_args.size() != func.getParameters().size())
			return refuse(argumentCountMismatchMessage(func, func_args.size()));

		for (const auto& [i, arg_value]: std::views::zip(std::views::iota(0u), func_args)) {
			if (arg_value->getPID() != getPID())
				return refuse(
					base::strConcat("VMValue for argument ", i, " comes from a different process")
				);

			if (dynamic_cast<const SafeVMValue*>(&*arg_value) == nullptr)
				return refuse(base::strConcat(
					"VMValue for argument ",
					i,
					" is invalid: it does not belong to the safe VM implementation"
				));

			// Safe TypeIDs are asserted (in the type builder) to be numerically equal to
			// ValidTypeIDs, so the interface-level type ID can be compared with the safe one.
			const auto& arg_type = func.getParameters()[i];
			if (arg_value->getTypeID() != code::valid_type::ValidTypeID(arg_type->getID().asInt()))
				return refuse(base::strConcat(
					"Type mismatch for argument ",
					i,
					" of function '",
					func.getName().str(),
					"': expected ",
					arg_type->getName().str(),
					", got ",
					arg_value->getType()->getName().str()
				));
		}
		return {};
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::runFunction(
		const std::string& func_name, const RunArguments& run_arguments
	) {
		std::unique_lock lock(api_lock);
		return spawnThread(func_name, run_arguments);
	}

	std::expected<api::ThreadID, api::ApiError> SafeVMProcess::startNewThreadFromExecutionThread(
		const std::string& func_name
	) {
		// On purpose without `api_lock`. This runs on the exec_thread as part of bytecode
		// execution, and `stepVMThread`/`stop`/`pauseVMThread` hold `api_lock`. Taking it here
		// again would deadlocks when stepping onto `call_builtinfunc builtin_start_thread`.
		if (v_matches(getProcessState(), ps::Stopping))
			return std::unexpected(api::ApiError{
				api::StateError{ "Cannot start a thread while the process is stopping" } });

		return spawnThread(func_name, {}).transform([](const api::Response& response) {
			return v_get(response, api::ThreadID);
		});
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::spawnThread(
		const std::string& func_name, const RunArguments& run_arguments
	) {
		std::lock_guard lock(threads_pool_mutex);

		SafeVMThread& thread = getEmptyThreadLocked();
		thread.setThreadCtx(func_name);

		if (!thread.spawnThreadAndRun(func_name, run_arguments)) {
			thread.setThreadCtx("");
			return std::unexpected(api::ApiError{
				api::RunError{ "Failed to spawn thread for function: " + func_name } });
		}
		return api::Response(thread.getThreadID());
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::runFunctionAwait(
		const std::string& func_name, const RunArguments& run_arguments
	) {
		std::unique_lock lock(api_lock);

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
				return std::unexpected(api::Panicked{ panicked.err });
			}
			variant_default {
				return std::unexpected(api::OtherError("Execution did not complete"));
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
		std::unique_lock global_lock(api_lock);
		// First raise the process stop flag.
		(void) state_manager.requestStop();
		// Send a Stop request to all threads.
		requestStopAllThreads();
		// Wait for all threads to stop (or `NotStarted`)
		(void) waitForProcessState([](const ProcessState& s) { return !ps::isExecuting(s); });

		joinAllExecutionThreads();
		return api::Response(api::response::Empty());
	}

	void SafeVMProcess::joinAllExecutionThreads() {
		// `joinExecutionThread` blocks until the exec thread exits, and an exec thread that has
		// not exited yet can still take the `threads_pool_mutex` during panic etc. causing a
		// deadlock. Thus we iterate over a copy to not hold `threads_pool_mutex`.
		std::vector<Ref<SafeVMThread>> threads;
		{
			std::lock_guard lock(threads_pool_mutex);
			for (auto& thread: vm_threads) threads.emplace_back(&thread);
		}

		for (auto thread: threads) thread->joinExecutionThread();
	}

	void SafeVMProcess::requestStopAllThreads() noexcept {
		std::lock_guard lock(threads_pool_mutex);
		for (auto& thread: vm_threads) thread.requestStop();
	}

	std::expected<void, api::ApiError> SafeVMProcess::assertProcessCanRespond() {
		if (!ps::canRespond(getProcessState()))
			return std::unexpected(api::ApiError{
				api::OtherError{ "Cannot do memory request while program is running" } });

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

	SafeVMProcess::SafeVMProcess(
		const PID my_pid, bool enable_deadlock_detection, [[maybe_unused]] bool enable_jit
	):
		  IVMProcess(my_pid),
		  compiler(*loader.getHighProgram(), enable_jit),
		  loaded_program(compiler.getLowProgram()) {
		if (enable_deadlock_detection) deadlock_detector.emplace();
		vm_threads.add(*this);
	}

	SafeVMProcess::~SafeVMProcess() {
		// A destructor is `noexcept`, and `freeAllocatedBlockData` runs a virtual `deallocate` per
		// block, so anything escaping it would terminate the process.
		try {
			memory.freeAllocatedBlockData();
		} catch (const std::exception& e) {
			std::cerr << "Failed to free the block data of process " << my_pid << ": " << e.what()
					  << "\n";
		} catch (...) {
			std::cerr << "Failed to free the block data of process " << my_pid
					  << ": unknown error\n";
		}
	}

	SafeVMThread& SafeVMProcess::getMainVMThread() {
		std::lock_guard lock(threads_pool_mutex);
		return *vm_threads.get(api::MAIN_THREAD_ID);
	}

	base::Optional<Ref<SafeVMThread>> SafeVMProcess::getVMThreadByID(api::ThreadID thread_id) {
		std::lock_guard lock(threads_pool_mutex);
		return vm_threads.maybeGet(thread_id);
	}

	SafeVMThread& SafeVMProcess::getEmptyThreadLocked() {
		for (auto& thread: vm_threads) {
			// Thread must not be executing AND must not have an active exec_thread handle
			if (!ts::isActive(thread.getThreadState()) && !thread.hasActiveThread()) return thread;
		}
		return *vm_threads.get(vm_threads.add(*this));
	}

	std::expected<api::Response, api::StateError> SafeVMProcess::getExitCode() {
		// `api_lock` is not needed here, since the only shared read here is the process state which
		// is being synchronized by `ProcessStateManager`.
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
		// We just `joinAllExecutionThreads` here as this endpoint assumes that the process is
		// stopped already.
		joinAllExecutionThreads();

		std::unique_lock lock(api_lock);
		try {
			getMainVMThread().execGlobalDestructors();

			for (const auto& vm_value: owned_vm_values) vm_value->freeData();

			memory.deinitGlobals();
		} catch (const exceptions::VMFoundMemoryLeakException&) {
			return false;
		} catch (const KillProcessException& e) {
			return std::unexpected(api::ApiError{ api::Panicked{
				base::strConcat("A global destructor was interrupted: ", e.what()) } });
		} catch (const exceptions::VMRuntimeException& e) {
			return std::unexpected(api::ApiError{ api::Panicked{
				base::strConcat("The process could not be deinitialized: ", e.what()) } });
		}
		return memory.validateMemoryState();
	}

	std::expected<void, api::ApiError> SafeVMProcess::requestPauseOfVMThread(api::ThreadID thread_id
	) {
		std::unique_lock lock(api_lock);
		auto             opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread)
			return std::unexpected(api::ApiError{ api::OtherError{ "Thread not found" } });

		if (auto requested = opt_thread.value()->requestPause(); !requested.has_value())
			return std::unexpected(api::ApiError{ api::PauseError{ requested.error() } });
		return {};
	}

	std::expected<void, api::ApiError> SafeVMProcess::pauseVMThread(api::ThreadID thread_id) {
		// API reads cannot happen while the thread is changing state.
		std::unique_lock lock(api_lock);
		auto             opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread)
			return std::unexpected(api::ApiError{ api::OtherError{ "Thread not found" } });
		auto thread = opt_thread.value();

		if (auto requested = thread->requestPause(); !requested.has_value())
			return std::unexpected(api::ApiError{ api::PauseError{ requested.error() } });
		// Unlock before we block on the pause, so an incoming `stop` request can be received.
		lock.unlock();

		if (auto paused = thread->awaitPause(); !paused.has_value())
			return std::unexpected(api::ApiError{ api::PauseError{ paused.error() } });
		return {};
	}

	std::expected<void, api::ApiError> SafeVMProcess::resumeVMThread(api::ThreadID thread_id) {
		// API reads cannot happen while the thread is changing run state.
		std::unique_lock lock(api_lock);
		auto             opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread)
			return std::unexpected(api::ApiError{ api::OtherError{ "Thread not found" } });
		auto response = opt_thread.value()->resume();
		if (!response)
			return std::unexpected(api::ApiError{ api::ResumeError{ response.error() } });
		return {};
	}

	std::expected<void, api::ApiError> SafeVMProcess::stepVMThread(api::ThreadID thread_id) {
		// Stepping actually executes bytecode, which mutates process memory. It must not
		// run concurrently with other memory-touching endpoints.
		std::unique_lock lock(api_lock);

		// Try to obtain thread
		auto opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread)
			return std::unexpected(api::ApiError{ api::OtherError{ "Thread not found" } });
		auto thread = opt_thread.value();

		// Try to obtain low position
		auto maybe_lp = thread->getCurrentPosition();
		if (!maybe_lp) return std::unexpected(maybe_lp.error());
		auto low_position = maybe_lp.value();

		auto [function, low_instr_index] = low_position;
		auto mapping                     = function->getInstructionMapping();

		// Default instruction range to step over is the whole function, in case we fail to obtain
		// high position
		low::LowFuncData::InstructionRange instr_range = {
			.begin = 0,
			.end   = std::numeric_limits<usize>::max(),
		};

		// Try to obtain high position and optimize instruction range to step over
		auto maybe_hp = function->mapLowVMProgramPositionToCodeCollectionPosition(low_instr_index);
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
			if (!response)
				return std::unexpected(api::ApiError{ api::OtherError{ response.error() } });

			// `getCurrentPosition` only works in `Paused` state, so if this step ended the
			// execution we return early.
			if (ts::isTerminal(thread->getThreadState())) return {};

			auto maybe_new_lp = thread->getCurrentPosition();
			if (!maybe_new_lp) return std::unexpected(maybe_new_lp.error());
			low_position = maybe_new_lp.value();
		} while (in_exclusive_range(low_position));

		return {};
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::getVMThreadCurrentPosition(
		api::ThreadID thread_id, base::Optional<usize> frame_idx
	) {
		std::shared_lock lock(api_lock);

		// Try to obtain thread
		auto opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread) return std::unexpected(api::OtherError{ "Thread not found" });
		auto thread = opt_thread.value();

		// Try to obtain low position
		auto maybe_lp = thread->getCurrentPosition(frame_idx);
		if (!maybe_lp) return std::unexpected(maybe_lp.error());
		auto [low_func, low_instr_idx] = maybe_lp.value();

		api::response::CodePosition code_position = {
			.function_name   = low_func->getName(),
			.instr_number    = 0,
			.source_position = std::nullopt,
		};

		// Try to obtain high position
		auto maybe_hp = low_func->mapLowVMProgramPositionToCodeCollectionPosition(low_instr_idx);
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

	std::expected<api::Response, api::ApiError> SafeVMProcess::waitForBreakpointAndReportPosition(
		api::ThreadID thread_id
	) {
		if (!getVMThreadByID(thread_id))
			return std::unexpected(api::ApiError{ api::OtherError{ "Thread not found" } });

		const ts::ThreadState state
			= state_manager.waitForThreadState(thread_id, [](const ts::ThreadState& s) {
				  return v_matches(s, ts::Paused) || ts::isTerminal(s);
			  });

		if (!v_matches(state, ts::Paused))
			return std::unexpected(api::ApiError{ api::StateError{ base::strConcat(
				"Thread ", thread_id.asInt(), " reached a terminal state instead of a breakpoint"
			) } });

		return getVMThreadCurrentPosition(thread_id);
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::setExecutionConfig(
		const api::ExecutionConfig& config
	) {
		std::unique_lock lock(api_lock);
		this->execution_config = config;
		return api::Response(api::response::Empty());
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::getNumberOfCurrentStackFrames(
		api::ThreadID thread_id
	) {
		std::shared_lock lock(api_lock);
		match_optional(assertProcessCanRespond()) {
			opt_err(error) return std::unexpected(error);
			opt_some() {
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
		// Inspecting a variable that was initialized without a block creates one, so this must be
		// guarded against every other memory-touching endpoint.
		std::unique_lock lock(api_lock);
		match_optional(assertProcessCanRespond()) {
			opt_err(error) return std::unexpected(error);
			opt_some() {
				auto opt_thread = getVMThreadByID(thread_id);
				if (!opt_thread)
					return std::unexpected(api::ApiError{ api::OtherError{ "Thread not found" } });
				auto thread = opt_thread.value();
				if (frame_index >= thread->getNumberOfCurrentStackFrames())
					return std::unexpected(api::OtherError{ "Frame not found" });

				const Frame& frame = thread->getStackFrame(frame_index);

				auto& func          = *frame.current_function;
				auto  low_instr_idx = static_cast<u64>(frame.instr - func.getBc().data());
				auto  fat_pos = func.mapLowVMProgramPositionToCodeCollectionPosition(low_instr_idx);
				if (!fat_pos) return std::unexpected(api::OtherError{ "Frame not found" });

				auto valid_pos
					= vm::loader::ValidFuncPosition(fat_pos->instruction_index, func.getHighFunc());

				const u64 slot_count = base::safeIntConv<u64>(
					frame.local_slot_stack_end - frame.local_slot_stack_base
				);
				auto expected = *valid_pos.absoluteSize();
				CORE_ASSERT(
					slot_count == expected,
					"Compile metadata must always be consistent with runtime: "
						+ std::to_string(slot_count) + " != " + std::to_string(expected)
				);

				auto relative_size = *valid_pos.size();
				u64  base_offset   = valid_pos.getBaseOffset().first;
				std::vector<api::response::StackFrameData::FrameVar> frame_vars;
				for (u64 slot_index = 0; slot_index < relative_size; slot_index++) {
					const LocalSlot& slot = frame.local_slot_stack_base[base_offset + slot_index];

					// Variables are initialized without a block, and a value can only be read
					// through one, so it is created here exactly as the executor does.
					Ref<Block> block
						= slot.block != nullptr
					        ? Ref(slot.block)
					        : createLocalSlotBlock(frame, memory, base_offset + slot_index);

					frame_vars.push_back(api::response::StackFrameData::FrameVar{
						.offset = base::safeIntConv<u64>(slot.data - frame.local_stack),
						.name   = valid_pos.getName(slot_index),
						.type   = valid_pos.getTypeName(slot_index),
						.value  = SafeVMValueRef::makeShared(
                            *this, memory.getBlockType(block), Pointer(block, 0)
                        ),
					});
				}

				return api::Response(api::response::StackFrameData{
					.function_name = frame.current_function->getName(), .frame_vars = frame_vars });
			}
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::getTypeMetadata(
		const std::string& type_name
	) {
		std::shared_lock lock(api_lock);
		match_optional(assertProcessCanRespond()) {
			opt_err(error) return std::unexpected(error);
			opt_some() {
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
		std::unique_lock lock(api_lock);
		match_optional(assertProcessCanRespond()) {
			opt_err(error) return std::unexpected(error);
			opt_some() {
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

	std::vector<api::ThreadID> SafeVMProcess::unjoinedThreadIds() const {
		std::lock_guard            lock(threads_pool_mutex);
		std::vector<api::ThreadID> ids;
		for (const auto& thread: vm_threads)
			if (thread.hasActiveThread()) ids.push_back(thread.getThreadID());
		return ids;
	}

	std::vector<api::ThreadID> SafeVMProcess::getAllActiveThreadIDs() {
		std::lock_guard            lock(threads_pool_mutex);
		std::vector<api::ThreadID> thread_ids;
		for (const auto& thread: vm_threads)
			if (ts::isActive(thread.getThreadState())) thread_ids.push_back(thread.getThreadID());
		return thread_ids;
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::setBreakpoint(
		base::StrID function_name, usize instruction_index, bool enable
	) {
		// @TODO: #3585 In JIT builds this can clobber or incorrectly restore the
		// jitFuncEntrypoint/jitLoopEntrypoint opcodes patched in by the compiler: disabling a
		// breakpoint restores the opcode from the original program, losing the JIT entrypoint.
		std::unique_lock lock(api_lock);
		auto response = compiler.setBreakpoint(function_name, instruction_index, enable);
		if (!response) return std::unexpected(api::OtherError{ response.error() });

		return api::response::Empty{};
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::mapFileLineToCodeCollectionPosition(
		const fs::File& file, usize line_number
	) {
		std::shared_lock lock(api_lock);

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
		if (!std::holds_alternative<thread_state::Paused>(thread_ref->getThreadState()))
			return std::unexpected(api::ApiError{ api::LoadProgramError{
				std::string(code::EvaluatingExprOnRunningThreadError::ERR_MSG) } });

		auto comp_details = code::Expression{
			.vm_values       = &owned_vm_values,
			.call_stack_base = thread_ref->getRuntimeData().frame_stack_base,
			.call_stack_size = thread_ref->getNumberOfCurrentStackFrames(),
		};

		std::unique_lock                                                         lock(api_lock);
		std::expected<code::valid_function::ValidFunction, loader::LoaderLogger> valid_expr = [&] {
			variant_match(source) {
				variant_case(fs::File, files) { return loader.validateExpr(comp_details, files); }
				variant_case(code::Function, func) { return validateFunction(func, comp_details); }
			}
			CORE_UNREACHABLE();
		}();

		if (!valid_expr.has_value()) {
			std::stringstream ss;
			valid_expr.error().dump(ss);
			return std::unexpected(api::LoadProgramError{ ss.str() });
		}

		auto started = thread_ref->loadAndExecRuntimeExpr(*std::move(valid_expr));
		if (!started.has_value())
			return std::unexpected(api::ApiError{ api::OtherError{ started.error() } });

		return api::Response(std::move(*started));
	}

	low::LowFuncData SafeVMProcess::compileToLow(const code::valid_function::ValidFunction& expr
	) const {
		return compiler.lowerExpr(expr);
	}

	std::expected<code::valid_function::ValidFunction, loader::LoaderLogger> SafeVMProcess::validateFunction(
		const code::Function& function, code::CompilationMode mode
	) const {
		return loader.validateFunction(function, mode);
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
