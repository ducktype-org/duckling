#include "safe_vmthread.hpp"

#include "opcode_functions/opcodes_functions.hpp"
#include "opcode_functions/opcodes_functions_utils.hpp"

#include <events/emitter.hpp>

#include <base/collections/optional.hpp>
#include <base/config/target_info.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/defer.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/int_conv.hpp>
#include <base/types/ints.hpp>

#include <logger/logger.hpp>
#include <string_id/string_id.hpp>

#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
#include <vm/bytecode/builders/instruction_builder.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/core/safe/concurrency/gil.hpp>
#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/low_program/cfg/cf_graph.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/safe/low_program/opcodes.hpp>
#include <vm/core/safe/memory/local_slot_block.hpp>
#include <vm/core/safe/memory/pointer.hpp>
#include <vm/core/safe/opcode_functions/opcodes_functions.hpp>
#include <vm/core/safe/safe_vmprocess.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/safe/type_metadata/type.hpp>
#include <vm/core/safe/vmvalue/safe_vmvalue.hpp>
#include <vm/core/thread/thread_state.hpp>
#include <vm/module_flags/module_flags.hpp>
#include <vm/utils/interpret.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <expected>
#include <mutex>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

// helpers for building fat-bytecode
namespace {
	using namespace vm::opargs;
	using namespace vm::code::builders;
	using base::StrID;

	auto any(auto&& arg) { return PlaceAny{ StrID(arg) }; }

	auto getBuilder(vm::code::Function& start_function) {
		return [&start_function](OpKind kind, auto&&... op_args) {
			auto instr = InstructionBuilder{ kind, std::forward<decltype(op_args)>(op_args)... };
			start_function.body.push_back(instr.build());
		};
	}

	auto toAny(const auto& place) { return PlaceAny(place.var_name); }

	auto imm(auto&& arg) { return Immediate{ static_cast<u64>(std::forward<decltype(arg)>(arg)) }; }
};

namespace vm {
	namespace ts = thread_state;
	namespace te = thread_event;

#define MAKE_BYTECODE_INSTRUCTION(OPCODE_NAME, ARG_0, ARG_1) \
	makeLowInstruction(low::MicroOpcode::OPCODE_NAME, ARG_0, ARG_1)

	SafeVMThread::SafeVMThread(api::ThreadID thread_id, SafeVMProcess& process):
		  IVMThread(thread_id, process),
		  runtime_data(
			  process.getMemory().initializeFrameStack(), process.getMemory().getGlobalDataMemory()
		  ),
		  safe_process(process),
		  process_memory(process.getMemory()),
		  process_program(process.getLoadedProgram()) {}

	/**
	 * @brief Tail call written function that handles the execution pause request.
	 * @details Assumes that the instruction in the frame is to be executed before AND after running
	 * this function.
	 */
	RETURN_TYPE OpFuns::handle_execution_break(OPFUN_ARGS) {
		{
			save_execution_state(instr, local_stack, frame, thread);

			thread.breakActiveExecution();

			// Restore current registers and flow, because
			// they could be changed when doing "step by step" execution.
			frame       = thread.runtime_data.frame_stack_current;
			instr       = frame->instr;
			local_stack = frame->local_stack;
		}
		OPFUN_CONT(0);
	}

	/**
	 * @brief Saves all current execution state in thread memory.
	 *
	 * The opcodes functions in tail call mode passes some state values in the registers
	 * (in the function arguments). This functions saves them from the registers to the
	 * current frame.
	 */
	RETURN_TYPE OpFuns::save_execution_state(OPFUN_ARGS) {
		{
			// Save current registers and flow.
			frame->instr                            = instr;
			frame->local_stack                      = local_stack;
			thread.runtime_data.frame_stack_current = frame;
		}
	}

	low::MicroOpcode SafeVMThread::getCurrentOpcode() const {
		const Frame*           frame  = runtime_data.frame_stack_current;
		const low::MicroOpcode opcode = getInstructionOpcode(*frame->instr);
		if (opcode != low::MicroOpcode::breakpoint) return opcode;

		auto&      micro_func     = *frame->current_function;
		const auto low_instr_idx  = static_cast<usize>(frame->instr - micro_func.getBc().data());
		const auto original_instr = micro_func.getOrigBc()[low_instr_idx];

		return getInstructionOpcode(original_instr);
	}

	bool SafeVMThread::isAtExecutionEnd() const {
		return getCurrentOpcode() == low::MicroOpcode::exit;
	}

	/**
	 * @brief Main debug function that executes one step of the program.
	 */
	void SafeVMThread::executeOneStep() {
		Frame* frame       = runtime_data.frame_stack_current;
		auto*  instr       = frame->instr;
		byte*  local_stack = frame->local_stack;

		const low::MicroOpcode opcode = getCurrentOpcode();

		// Execute the instruction by calling the debug opcode function.
		OpFuns::DEBUG_OPFUNS.at(std::to_underlying(opcode))(instr, local_stack, frame, *this);

		runtime_data.frame_stack_current = frame;
		frame->local_stack               = local_stack;
		frame->instr                     = instr;
	}

	/**
	 * @brief Creates a dynamically generated (meaning it's generated at the moment of a program
	 * invocation) start function for a specified function. For now, mainly used by the REPL mode.
	 *
	 * @note For more detailed explanation go to `createProgramStartFunction`.
	 */
	code::Function SafeVMThread::createStartFunctionFor(
		const low::LowFuncData& func, const FunctionRunArguments& func_args
	) const {
		using namespace opargs;
		using namespace std::views;
		using enum code::builders::OpKind;
		using base::StrID, base::strConcat;

		auto high_func            = func.getHighFunc();
		auto called_function_name = FunctionName{ func.getName() };

		code::Function start_function;

		start_function.name      = code::Identifier{ StrID("vm_start_function") };
		start_function.signature = code::FuncSignature{ .result_types = {}, .parameters = {} };

		auto add_instr = getBuilder(start_function);

		for (auto [idx, result_type]: enumerate(high_func->signature.result_types)) {
			auto slot_type = opargs::Type{ StrID(result_type.str) };
			auto slot_name = any(strConcat("ret", idx).c_str());
			add_instr(init, slot_name, slot_type);
		}

		for (auto [idx, arg_value]: enumerate(func_args)) {
			VMValueIdentifier val_id;
			val_id.id     = arg_value.get();
			auto arg_name = any(strConcat("arg", idx).c_str());
			add_instr(init, arg_name, val_id);
		}

		add_instr(call, called_function_name);
		add_instr(exit);

		return start_function;
	}

	void SafeVMThread::compileAndLoadStartFunction(const code::Function& start_function) {
		start_function_high.emplace(safe_process.validateStartFunction(this, start_function));
		start_function_low.emplace(safe_process.compileToLow(this, *start_function_high));
	}

	/**
	 * @brief Creates a dynamically generated (meaning it's generated at the moment of a program
	 * invocation) start function for a program.
	 *
	 * Just like in libc, the start function pushes the program arguments on to the stack and
	 * performs the call to main. After the main returns, it deinitializes the argv memory and
	 * exits, leaving one block on the block stack, which contains the return value of the
	 * program.
	 *
	 * @note This is done in VMThread, since it depends on the arguments passed during the call
	 * which may vary from call to call and creating a generic start function using builders in
	 * the loading phase is not possible. This also results in the need to create the function
	 * in the micro-bytecode right away.
	 */
	code::Function SafeVMThread::createProgramStartFunction(
		const low::LowFuncData& func, const ProgramRunArguments& args
	) const {
		using namespace code::builders;
		using namespace opargs;
		using enum code::builders::OpKind;
		using base::StrID;

		code::Function start_function;
		start_function.name      = code::Identifier{ StrID("vm_start_function") };
		start_function.signature = code::FuncSignature{ .result_types = {}, .parameters = {} };

		auto add_instr = getBuilder(start_function);

		const auto main = FunctionName{ func.getName() };

		const auto ret_value      = Place64{ StrID("ret_value") };
		const auto argv_internal  = PlacePtr{ StrID("argv_internal") };
		const auto argc_internal  = Place64{ StrID("argc_internal") };
		const auto ix             = Place64{ StrID("ix") };
		const auto ptr_tmp_store  = PlacePtr{ StrID("ptr_tmp_store") };
		const auto char_tmp_store = Place8{ StrID("char_tmp_store") };
		const auto main_ret_val   = Place64{ StrID("main_ret_val") };

		const auto type_i64        = opargs::Type{ StrID("i64") };
		const auto type_ptr_argv   = opargs::Type{ StrID("ptr_argv") };
		const auto type_argv       = opargs::Type{ StrID("argv") };
		const auto type_ptr_string = opargs::Type{ StrID("ptr_string") };
		const auto type_byte       = opargs::Type{ StrID("byte") };
		const auto type_string     = opargs::Type{ StrID("string") };

		const bool main_has_args = !func.getParameters().empty();

		// Initialize the argc/argv bookkeeping. It is always materialized, so that the offsets
		// do not depend on whether `main` takes arguments.
		add_instr(init, toAny(ret_value), type_i64);
		add_instr(init, toAny(argv_internal), type_ptr_argv);
		add_instr(init, toAny(argc_internal), type_i64);
		add_instr(init, toAny(ix), type_i64);
		add_instr(mov, argc_internal, imm(args.size()));
		add_instr(dynTableReAlloc, argv_internal, type_argv, argc_internal);

		// Now fill in the argv table.
		if (main_has_args) {
			using namespace std::views;
			for (const auto& [argv_index, arg]: zip(iota(0u), args)) {
				add_instr(init, toAny(ptr_tmp_store), type_ptr_string);
				add_instr(init, toAny(char_tmp_store), type_byte);

				// `arg.size() + 1` accounts for the terminating `\0`.
				add_instr(mov, argc_internal, imm(arg.size() + 1));
				add_instr(dynTableReAlloc, ptr_tmp_store, type_string, argc_internal);
				add_instr(mov, ix, imm(0));

				for (auto c: arg) {
					add_instr(mov, char_tmp_store, imm(c));
					add_instr(dynTableStore, ptr_tmp_store, toAny(char_tmp_store), ix);
					add_instr(add, ix, imm(1));
				}

				// At this point `ix == arg.size()` - store the terminating `\0`.
				add_instr(mov, char_tmp_store, imm(0));
				add_instr(dynTableStore, ptr_tmp_store, toAny(char_tmp_store), ix);
				add_instr(mov, ix, imm(argv_index));
				add_instr(dynTableStore, argv_internal, toAny(ptr_tmp_store), ix);
				add_instr(deinit);
				add_instr(deinit);
			}
		}

		// Now actually prepare to call 'main'. Its return value is the first shared slot.
		add_instr(init, toAny(main_ret_val), type_i64);
		if (main_has_args) {
			const auto argc = Place64{ StrID("argc") };
			const auto argv = PlacePtr{ StrID("argv") };
			add_instr(init, toAny(argc), type_i64);
			add_instr(init, toAny(argv), type_ptr_argv);
			add_instr(mov, argc, imm(args.size()));
			add_instr(mov, argv, argv_internal);
		}

		add_instr(call, main);
		add_instr(mov, ret_value, main_ret_val);
		add_instr(mov, ix, imm(0));
		add_instr(init, toAny(ptr_tmp_store), type_ptr_string);

		// After 'main' returned, free all the allocated strings in the argv table.
		for ([[maybe_unused]] const auto& arg: args) {
			add_instr(dynTableLoad, toAny(ptr_tmp_store), argv_internal, ix);
			add_instr(free, ptr_tmp_store);
			add_instr(add, ix, imm(1));
		}

		// Lastly, free all the data allocated by the start function and pop the remaining
		// locals, leaving only the program's return value on the stack.
		add_instr(free, argv_internal);
		for (usize i = 0; i < 5; i++) add_instr(deinit);
		add_instr(exit);

		return start_function;
	}

	SafeVMThread::ScopedGilGuard::ScopedGilGuard(SafeVMThread& t): thread(t) {
		thread.acquireGil();
	}

	SafeVMThread::ScopedGilGuard::~ScopedGilGuard() {
		if (thread.has_gil) thread.releaseGil();
	}

	SafeVMThread::ScopedBlockingWait::ScopedBlockingWait(SafeVMThread& t): thread(t) {
		thread.reportAsSleeping();
		thread.releaseGilIfHeld();
	}

	SafeVMThread::ScopedBlockingWait::~ScopedBlockingWait() {
		thread.acquireGilIfNotHeld();
		thread.reportAsRunning();
	}

#if BASE_TARGET_COMPILER_CLANG
// @TODO: #2582 suppress code deduplication in Clang
#elif BASE_TARGET_COMPILER_GCC
	#pragma GCC push_options
	#pragma GCC optimize("-fno-crossjumping")
#endif
	// NOLINTBEGIN(cppcoreguidelines-avoid-goto)
	// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
	void runInterpreter(
		const MicroInstruction* instr, byte*& local_stack, Frame*& frame, SafeVMThread& thread
	) {
#ifdef USE_TAIL_CALLS
		return instr->tc_opfun(instr, local_stack, frame, thread);

#elifdef USE_SWITCH_CASE
		while (true) {
			switch (static_cast<low::MicroOpcode>(instr->nontc_opcode)) {
	#define HANDLE_MICRO_INSTR(opcode_name)                                                         \
	case low::MicroOpcode::opcode_name: {                                                           \
		vm::OpFuns::op_##opcode_name(instr, local_stack, frame, thread);                            \
		if constexpr (::vm::ENABLE_VM_DETAIL_LOGGING)                                               \
			CORE_DEV_LOG(                                                                           \
				DVMDetails, "opcode, ", #opcode_name, ", ", thread.getThreadID().asInt(), ";\n"     \
			);                                                                                      \
		if constexpr (constexpr std::string_view opcode_str = #opcode_name; opcode_str == "exit") { \
			goto End;                                                                               \
		} else {                                                                                    \
			break;                                                                                  \
		}                                                                                           \
	}
	#include <vm/core/safe/low_program/micro_instruction_definitions.def.hpp>
	#undef HANDLE_MICRO_INSTR

			default: {
				CORE_PANIC("Unknown operator: ", u64(instr->nontc_opcode));
			}
			}
		}
	End:
#endif
	}

	// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
	// NOLINTEND(cppcoreguidelines-avoid-goto)

#if BASE_TARGET_COMPILER_CLANG
// @TODO: #2582 suppress code deduplication in Clang
#elif BASE_TARGET_COMPILER_GCC
	#pragma GCC pop_options
#endif

	std::vector<Ref<SafeVMValue>> SafeVMThread::executeLoadedFunction(const low::LowFuncData& func) {
		ScopedGilGuard gil_guard(*this);

		CORE_ASSERT(start_function_low.has_value(), "No start function is loaded");
		CORE_ASSERT(start_function_high.has_value(), "No start function is loaded");
		CORE_ASSERT(
			start_function_low->getHighFunc().get() == &*start_function_high,
			"High function is not a source for low function"
		);

		// Frame of the called function.
		Frame* frame          = runtime_data.frame_stack_current;
		Frame* orig_frame_ptr = frame;
		Frame  orig_frame_cpy = *runtime_data.frame_stack_current;
		byte*  local_stack    = frame->local_stack;
		if (local_stack == nullptr) local_stack = runtime_data.local_stack_base;
		auto orig_slot_stack_size
			= usize(frame->local_slot_stack_end - frame->local_slot_stack_base);

		frame->current_function      = &*start_function_low;
		frame->local_slot_stack_base = runtime_data.slot_stack_base;
		frame->local_slot_stack_end  = runtime_data.slot_stack_base;

		const auto* instr = start_function_low->getBc().data();

		// Make sure the frame will be moved back after the interpreter runs. Even if it throws
		// a `KillProcessException` so the state stays valid.
		defer({
			*orig_frame_ptr                  = orig_frame_cpy;
			runtime_data.frame_stack_current = orig_frame_ptr;
		});

		runInterpreter(instr, local_stack, frame, *this);

		CORE_ASSERT(
			frame == orig_frame_ptr,
			"After executing function we have to return to original place in call stack"
		);
		// @note: The return value is the only slot left on the slot stack.
		CORE_ASSERT(
			frame->local_slot_stack_end - frame->local_slot_stack_base
				>= orig_slot_stack_size + func.getResultTypes().size(),
			"After function execution, there should be enough slots on the stack to retrieve "
			"result."
		);

		// Copy the exit values
		exit_value_storage = { std::vector<Ref<SafeVMValue>>{} };
		for (u64 idx = 0; idx < func.getResultTypes().size(); idx++) {
			const usize slot_index = orig_slot_stack_size + idx;
			// A result the callee never took a block for still has to be handed out as a
			// pointer, so it gets one here. This is the common case, not a rare one -
			// `vm_sc_unit_test` panics if the block is assumed to exist.
			Block* block = frame->local_slot_stack_base[slot_index].block;
			if (block == nullptr)
				block = createLocalSlotBlock(*frame, process_memory, slot_index).get();

			exit_value_storage.value().emplace_back(
				safe_process.createVMValue(func.getResultTypes()[idx], Pointer(block, 0))
			);
		}

		// Free the remaining data on the stack. The start functions emit their own
		// instructions, so they can leave locals live above the return values; asserting that
		// nothing is left deadlocks `vm_sc_threads_test`.
		for (usize idx = orig_slot_stack_size;
		     idx < usize(frame->local_slot_stack_end - frame->local_slot_stack_base);
		     idx++) {
			if (Block* block = frame->local_slot_stack_base[idx].block) {
				process_memory.freeBlockData(block);
				process_memory.decreaseBlockRefcount(block);
				continue;
			}

			// A variable that never needed a block still has to release whatever it points at,
			// which is what `freeBlockData` would have done for it.
			const LocalSlot& slot = frame->local_slot_stack_base[idx];
			process_memory.runDataDestructors(
				{ slot.data, slot.type->getSize().asInt() }, slot.type
			);
		}

		return exit_value_storage.value();
	}

	// executeLoadedFunction end

	/**
	 * @brief Starts the execution of a function with a given name and arguments.
	 */
	void SafeVMThread::run(const std::string& func_name, const RunArguments& run_arguments) {
		for (const auto& [global, id, name]: process_program->getGlobals().allData()) {
			auto block_ref
				= Ref(runtime_data.global_block_ref_buffer_base[global->global_block_idx]);
			if (not process_memory.isGlobalInitialized(block_ref)) {
				variant_match(global->init) {
					variant_case(low::GlobalCtorDtor, ctor_dtor) {
						if (ctor_dtor.ctor_name.has_value()) {
							const auto& func = *process_program->getFunctions()
							                        .atMaybe(ctor_dtor.ctor_name.value())
							                        .value();
							compileAndLoadStartFunction(createStartFunctionFor(func, {}));
							executeLoadedFunction(func);
						}
					}
					variant_case(low::GlobalInitialValue, value_init) {
						// Copy the constant initial value bytes directly to the global's memory.
						process_memory.initializeBlockFromConstValue(block_ref, value_init.value);
					}
				}
				process_memory.setGlobalInitialized(block_ref);
			}
		}

		const auto& maybe_func
			= process_program->getFunctions().atMaybe(base::StrID(func_name.data()));
		if (!maybe_func.has_value()) {
			throw exceptions::VMRuntimeException(
				base::strConcat("Called function '", func_name, "' does not exist.")
			);
		}
		const auto& func = *maybe_func.value();

		variant_match(run_arguments) {
			variant_case(ProgramRunArguments, program_run_arguments) {
				compileAndLoadStartFunction(createProgramStartFunction(func, program_run_arguments));
			}
			variant_case(FunctionRunArguments, function_run_data) {
				compileAndLoadStartFunction(createStartFunctionFor(func, function_run_data));
			}
		}

		const auto exit_value = executeLoadedFunction(func);
		applyEvent(te::Finish{ std::vector<Ref<IVMValue>>(exit_value.begin(), exit_value.end()) });
	}

	void SafeVMThread::execGlobalDestructors() {
		const auto& executing_program = process_program;
		auto        globals           = executing_program->getGlobals().allData();
		// Destructors should run in reverse order of construction so that any object depending
		// on earlier-created resources is destroyed first, preventing use-after-destruction and
		// keeping teardown safe and logically consistent.
		for (const auto& [global, id, name]: std::ranges::reverse_view(globals)) {
			auto* ctor_dtor = std::get_if<low::GlobalCtorDtor>(&global->init);
			if (!ctor_dtor || !ctor_dtor->dtor_name.has_value()) continue;

			// A global which was never constructed has nothing to destroy.
			auto block_ref
				= Ref(runtime_data.global_block_ref_buffer_base[global->global_block_idx]);
			if (not process_memory.isGlobalInitialized(block_ref)) continue;

			const auto& func
				= *executing_program->getFunctions()
			           .atMaybe(ctor_dtor->dtor_name.value())
			           .expect(
						   "Called function does not exist: " + ctor_dtor->dtor_name.value().str()
					   );
			compileAndLoadStartFunction(createStartFunctionFor(func, {}));
			executeLoadedFunction(func);
		}
	}

	std::expected<low::LowCodePosition, api::ApiError> SafeVMThread::getCurrentPosition(
		base::Optional<usize> frame_idx
	) const {
		variant_match(getThreadState()) {
			variant_case_novalue(ts::Paused) {
				const Frame* frame = runtime_data.frame_stack_current;
				// In caller frames, the instruction pointer rests on the return address (the
				// instruction after the call). We must adjust it backward by 1 to point to the
				// actual call site. One word is enough even for a call lowered to several micro
				// instructions - the adjustment only has to land inside the call's own range in
				// `instruction_mapping`.
				bool call_adjustment = false;
				if_opt_some(frame_idx, frame_index) {
					u64 frames = getNumberOfCurrentStackFrames();
					if (frame_index >= frames)
						return std::unexpected(api::ApiError{
							api::OtherError{ "Frame index out of bounds" } });

					frame = &getStackFrame(frame_index);
					if (frame_index + 1 != frames) call_adjustment = true;
				}

				auto& func = *frame->current_function;

				return low::LowCodePosition{ .function          = &func,
					                         .instruction_index = static_cast<u64>(
												 frame->instr - func.getBc().data()
												 - (call_adjustment ? 1 : 0)
											 ) };
			}
			variant_default {
				return std::unexpected(api::ApiError{
					api::OtherError{ "wrong execution status while reading current position" } });
			}
		}
		CORE_UNREACHABLE();
	}

	void SafeVMThread::stepGil() {
		if (has_gil) {
			// Check if you can hold it longer - releasing policy
			// If you can't hold it longer then
			// 1. say
			if (!safe_process.getGIL().shouldRelease()) return;
			// 2. release gil
			releaseGil();
			// 3. yield - to not reacquire instantly
			std::this_thread::yield();
		}
		// Try to acquire GIL
		acquireGil();
	}

	void SafeVMThread::releaseGilIfHeld() {
		if (has_gil) releaseGil();
	}

	void SafeVMThread::acquireGilIfNotHeld() {
		if (!has_gil) acquireGil();
	}

	void SafeVMThread::releaseGil() {
		CORE_ASSERT(has_gil, "Cannot release GIL without acquiring it first");
		has_gil = false;
		safe_process.getGIL().release();
	}

	void SafeVMThread::acquireGil() {
		CORE_ASSERT(!has_gil, "Cannot acquire GIL twice");
		safe_process.getGIL().acquire();
		has_gil = true;
	}

	void SafeVMThread::setThreadCtx(std::string str) { thread_ctx = std::move(str); }

	bool SafeVMThread::isCallableFunctionID(usize id) {
		return id != low::LowFuncData::NO_FUNCTION_ID;
	}

	u64 SafeVMThread::getNumberOfCurrentStackFrames() const {
		// +1 because frame_stack_current points to the current frame, not the next free slot.
		return u64(runtime_data.frame_stack_current - runtime_data.frame_stack_base) + 1;
	}

	Bytes SafeVMThread::getCurrentStackBytesSize() const {
		Frame* frame = runtime_data.frame_stack_current;
		byte*  top   = frame->local_stack;
		if (frame->local_slot_stack_end != frame->local_slot_stack_base) {
			const LocalSlot& slot = frame->local_slot_stack_end[-1];
			top                   = slot.data + slot.type->getSize().asInt();
		}
		return Bytes{ u64(top - runtime_data.local_stack_base) };
	}

	u64 SafeVMThread::getCurrentStackBlockSize() const {
		Frame* frame = runtime_data.frame_stack_current;
		return u64(frame->local_slot_stack_end - runtime_data.slot_stack_base);
	}

	CRef<IVMValue> SafeVMThread::getVMValue(CRef<opargs::VMValueIdentifier> vm_val) const {
		v_if_matches(vm_val->id, u64, id) return safe_process.accessVMValue(*id);
		return std::get<const IVMValue*>(vm_val->id);
	}

	bool SafeVMThread::isValidVMValueID(CRef<opargs::VMValueIdentifier> vm_val) const {
		v_if_matches(vm_val->id, u64, id) return *id < safe_process.numberOfOwnedVMValues();
		return true;  // we need to trust that the pointer is valid
	}

	const Frame& SafeVMThread::getStackFrame(u64 frame_index) const {
		return runtime_data.frame_stack_base[frame_index];
	}

	void SafeVMThread::updateGlobalDataBufferPointers(GlobalBufferPointersByte global_buffer_pointers
	) {
		runtime_data.global_data_buffer_base      = global_buffer_pointers.data_buffer_base;
		runtime_data.global_block_ref_buffer_base = global_buffer_pointers.blocks_buffer_base;
	}

	std::expected<
		std::vector<Ref<SafeVMValue>>,
		std::pair<SharedBox<events::Emitter<std::vector<Ref<SafeVMValue>>>>, std::string>>
		SafeVMThread::loadAndExecRuntimeExpr(code::valid_function::ValidFunction&& high_expr) {
		CORE_ASSERT(
			v_matches(getThreadState(), thread_state::Paused),
			"To load and evaluate expr we need the thread to be paused"
		);
		runtime_expr_high.emplace_back(std::move(high_expr));
		runtime_expr_low.emplace_back(safe_process.compileToLow(this, runtime_expr_high.back()));
		runtime_expr_res_handler.emplace_back(
			makeSharedBox<events::Emitter<std::vector<Ref<SafeVMValue>>>>()
		);

		auto  frame       = runtime_data.frame_stack_current;
		auto  prev_frame  = frame;
		auto  instr       = frame->instr;
		auto  local_stack = frame->local_stack;
		auto& called_expr = runtime_expr_low.back();

		auto maybe_position = getUpcomingHighPosition(getNumberOfCurrentStackFrames() - 1);
		CORE_ASSERT(
			maybe_position.has_value(), "A validated expression must have a mapped position"
		);
		auto maybe_stack_size = *maybe_position->byteSize();
		u64  callee_stack_distance
			= maybe_stack_size.assumePointerSize(vm::Type::POINTER_SIZE).asInt();

		u64 prev_summed = 0;

		for (auto type: called_expr.getResultTypes()) {
			auto size = type->getSize().asInt();
			OpFuns::pushLocalSlot(
				frame, type, local_stack + callee_stack_distance + prev_summed, size, nullptr
			);
			prev_summed += size;
		}
		OpFuns::performFunctionCall(
			instr, local_stack, frame, *this, called_expr, callee_stack_distance, 0
		);
		OpFuns::save_execution_state(instr, local_stack, frame, *this);

		// we modify the frame so that the base is the global base of the stacks
		frame->local_stack           = runtime_data.local_stack_base;
		frame->local_slot_stack_base = runtime_data.slot_stack_base;

		// updating the previous frame, because result variables
		// will not be returned to the caller
		prev_frame->local_slot_stack_end -= called_expr.getResultTypes().size();

		std::condition_variable                       cv;
		std::mutex                                    result_mutex;
		std::atomic<bool>                             result_ready = false;
		base::Optional<std::vector<Ref<SafeVMValue>>> ret_val      = std::nullopt;
		base::Optional<ThreadState>                   thread_state = std::nullopt;

		auto res_handler = runtime_expr_res_handler.back();

		events::Listener<std::vector<Ref<SafeVMValue>>> receiver([&](auto&& res) {
			// Payload and flag must be published under the same mutex the waiter checks with,
			// otherwise it can observe the flag before the value is visible.
			{
				std::lock_guard lock(result_mutex);
				if (result_ready.load()) return;
				ret_val = std::move(res);
				result_ready.store(true);
			}
			cv.notify_all();
		});

		events::Listener<ThreadState> interrupter([&](auto&& new_state) {
			if (!v_matches(new_state, thread_state::Paused) && !thread_state::isTerminal(new_state))
				return;
			{
				std::lock_guard lock(result_mutex);
				if (result_ready.load()) return;
				thread_state = new_state;
				result_ready.store(true);
			}
			cv.notify_all();
		});

		res_handler->attachListener(receiver);
		getProcessStateManager().attachThreadStatusListener(getThreadID(), interrupter);

		auto resumed = resume();
		if (!resumed.has_value())
			return std::unexpected{
				std::make_pair(res_handler, "resume failure: " + resumed.error())
			};

		{
			std::unique_lock lock(result_mutex);
			cv.wait_for(lock, std::chrono::milliseconds(EXPR_EXECUTION_TIMEOUT_MS), [&] {
				return result_ready.load();
			});
			result_ready.store(true);
		}

		if (ret_val.has_value()) return *ret_val;

		std::string err_msg = "";
		if (thread_state.has_value()) {
			err_msg += "status failure: ";
			err_msg += thread_state::threadStateName(*thread_state);
			err_msg += " during evaluation";
		} else {
			err_msg = "timeout: evaluation of expr took more than "
			        + std::to_string(EXPR_EXECUTION_TIMEOUT_MS) + " ms";
		}

		return std::unexpected{ std::make_pair(res_handler, err_msg) };
	}

	std::expected<void, std::string> SafeVMThread::setBreakpointAtFrame(
		usize frame_idx, usize idx, bool enable
	) {
		if (!v_matches(getThreadState(), vm::thread_state::Paused))
			return std::unexpected{ "error: breakpoint can be set only when thread is paused" };
		if (frame_idx >= getNumberOfCurrentStackFrames())
			return std::unexpected{ "error: frame index is out of bounds" };

		auto func_ref = runtime_data.frame_stack_base[frame_idx].current_function;
		CORE_ASSERT(
			func_ref, "When we access the stack frame it has to yield non-null function ref"
		);

		auto& func = const_cast<low::LowFuncData&>(*func_ref);  // NOLINT
		return func.setBreakpoint(idx, enable);
	}

	base::Optional<vm::loader::ValidFuncPosition> SafeVMThread::getUpcomingHighPosition(
		u64 frame_index
	) const {
		if (frame_index >= getNumberOfCurrentStackFrames()) return std::nullopt;
		const Frame& frame = getStackFrame(frame_index);

		// we don't use getCurrentPosition on purpose
		// we aren't interested from where the function was called
		// we want to know where thread will be once it continue
		auto& func = *frame.current_function;
		auto  low_pos
			= low::LowCodePosition{ .function = &func,
			                        .instruction_index
			                        = static_cast<u64>(frame.instr - func.getBc().data()) };

		auto fat_pos
			= safe_process.getCompiler()->mapLowVMProgramPositionToCodeCollectionPosition(low_pos);
		if_opt_none(fat_pos) return std::nullopt;

		return loader::ValidFuncPosition(
			fat_pos->instruction_index, *getFatBytecodeFunction(frame_index)
		);
	}

	base::Optional<CRef<code::valid_function::ValidFunction>> SafeVMThread::getFatBytecodeFunction(
		u64 frame_idx
	) const {
		if (frame_idx >= getNumberOfCurrentStackFrames()) return std::nullopt;

		auto& frame = getStackFrame(frame_idx);
		return frame.current_function->getHighFunc();
	}
}
