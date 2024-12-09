#include <base/ints.hpp>
#include <code_data/instruction.hpp>
#include <code_data/opcodes.hpp>
#include <code_data/code.hpp>
#include <cstring>
#include <core/process/type_metadata/type.hpp>
#include <core/supervisor/supervisor.hpp>
#include "core/kill_process_exception.hpp"
#include "op_case.hpp"
#include "vmthread.hpp"

#include <iostream>

#include <base/exceptions.hpp>
#include <base/optional.hpp>
#include <utility>

namespace vm {
	VMThread::VMThread(VMProcess& process):
		  frame_stack(FRAME_COUNT, internalInitFrame()),
		  local_stack_reserved(STACK_LENGTH),
		  runtime_data(frame_stack, local_stack_reserved),
		  process(process),
		  process_memory(process.getMemory()),
		  process_types(process.getTypeMetadata()),
		  process_stack_allocator(process_memory.getStackAllocator()),
		  process_dynamic_allocator(process_memory.getDynamicAllocator()) {
		// @TODO: not loaded status
		setStatus(api::NotStarted{});
	}

	void VMThread::handleExecutionStrategy() {
		std::unique_lock lock(external_api_mutex);
		if (execution_strategy == ExecutionStrategy::Paused) setStatus(api::Paused{});
		pause_cv.wait(lock, [this] { return execution_strategy != ExecutionStrategy::Paused; });
		setStatus(api::Running{});
		if (execution_strategy == ExecutionStrategy::Stoped) throw KillProcessException{};
		if (execution_strategy == ExecutionStrategy::StepByStep)
			execution_strategy = ExecutionStrategy::Paused;
	}

	[[gnu::always_inline]]
	inline void VMThread::handleExecutionStrategyIfNeeded() {
		if (is_running) return;
		handleExecutionStrategy();
	}

	Frame VMThread::internalInitFrame() {
		return Frame{
			.instr       = nullptr,
			.local_stack = nullptr,
			.regs        = Registers{ .p64_reg_0 = 0, .pointer_reg_0 = process_memory.nullPtr() },
			.flags       = FlagData{ .flag = false },
			.ret_val     = 0,
			.next_args   = nullptr,
			.args        = nullptr,

			.block_id_stack   = std::vector<BlockID>(),
			.local_stack_head = 0,
		};
	}

	[[gnu::always_inline]]
	inline base::ModRawView VMThread::internalDerefPointer(Pointer pointer) {
		auto block_id = pointer.getBlock();
		auto offset   = pointer.getOffset();

		auto block = process_memory.getBlock(block_id);
		auto type  = block->innerType();
		auto view  = block->deref(type, offset);

		CORE_ASSERT(view.size() == type->getSize(), "Bad deref size");
		return view;
	}

	template<typename T>
	[[gnu::always_inline]]
	inline static T& derefStack(std::byte* stack, i64 position) {
		return *(reinterpret_cast<T*>(&stack[position]));
	}

	// NOTE: functions that implement opcodes (opfunctions) must be done this way:
	//
	// RETURN_TYPE OpFuns::op_<opcode_name>(OPFUN_ARGS) {
	//  {
	//    <function_body>
	//  }
	//  OPFUN_CONT(<step>);
	// }
	//
	// Function body must be seperated from the scope of OPFUN_CONT to make sure
	// that all its destructors have been called before invoking next tail call.
	// Otherwise, the compiler may get confused and may schedule destructors from
	// the body after the next tail call, which then becomes a regular function
	// call and may cause the stack to explode.

	// `op_exit` is the only opcode without the `OPFUN_CONT` or `OPFUN_CONT_CHECK_STRATEGY` macro.
	// This means, every other will jump to the next instruction at the end of it with
	// `OPFUN_CONT`/`OPFUN_CONT_CHECK_STRATEGY`, so the the only way to end execution is to use this
	// opcode. It also requires different macro surrounding the function call in the computed goto's
	// and switch case, because in those approaches we can't end execution from within the function,
	// but we have to add some instructions on the outside of it. Hence we use the `OP_CASE_END`
	// macro that adds `goto End` instruction, residing after opcode function, inside interpeter
	// loop.
	RETURN_TYPE OpFuns::op_exit(OPFUN_ARGS) { IF_TC(return;) }

	RETURN_TYPE OpFuns::op_handle_strategy(OPFUN_ARGS) {
		{ thread.handleExecutionStrategy(); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mov_l64_imm(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) = instr->arg1; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mov_l64_l64(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) = derefStack<u64>(local_stack, instr->arg1); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_cmov_l64_l64(OPFUN_ARGS) {
		{
			if (frame->flags.flag)
				derefStack<u64>(local_stack, instr->arg0)
					= derefStack<u64>(local_stack, instr->arg1);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mov_l64_r0(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) = frame->regs.p64_reg_0; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mov_r0_l64(OPFUN_ARGS) {
		{ frame->regs.p64_reg_0 = derefStack<u64>(local_stack, instr->arg0); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_add_l64_l64(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) += derefStack<u64>(local_stack, instr->arg1); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_add_l64_imm(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) += instr->arg1; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_sub_l64_l64(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) -= derefStack<u64>(local_stack, instr->arg1); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_sub_l64_imm(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) -= instr->arg1; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mul_l64_imm(OPFUN_ARGS) {
		{
			// @TODO: check types
			derefStack<u64>(local_stack, instr->arg0) *= instr->arg1;
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mod_l64_imm(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) %= instr->arg1; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mod_l64_l64(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) %= derefStack<u64>(local_stack, instr->arg1); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_div_l64_imm(OPFUN_ARGS) {
		{ derefStack<u64>(local_stack, instr->arg0) /= instr->arg1; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_cmpEq_l64_l64(OPFUN_ARGS) {
		{
			frame->flags.flag = derefStack<u64>(local_stack, instr->arg0)
			                 == derefStack<u64>(local_stack, instr->arg1);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_cmpEq_l64_imm(OPFUN_ARGS) {
		{ frame->flags.flag = derefStack<u64>(local_stack, instr->arg0) == instr->arg1; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_cmpG_l64_l64(OPFUN_ARGS) {
		{
			frame->flags.flag = derefStack<u64>(local_stack, instr->arg0)
			                  > derefStack<u64>(local_stack, instr->arg1);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_cmpG_l64_imm(OPFUN_ARGS) {
		{ frame->flags.flag = derefStack<u64>(local_stack, instr->arg0) > instr->arg1; }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_jmpRel_label(OPFUN_ARGS) {
		{ instr += instr->arg0; }
		OPFUN_CONT_CHECK_STRATEGY(1);
	}

	RETURN_TYPE OpFuns::op_jmpRelIf_label(OPFUN_ARGS) {
		{
			if (frame->flags.flag) instr += instr->arg0;
		}
		OPFUN_CONT_CHECK_STRATEGY(1);
	}

	RETURN_TYPE OpFuns::op_jmpRelNotIf_label(OPFUN_ARGS) {
		{
			if (!frame->flags.flag) instr += instr->arg0;
		}
		OPFUN_CONT_CHECK_STRATEGY(1);
	}

	RETURN_TYPE OpFuns::op_mov_l64_arg64(OPFUN_ARGS) {
		{ derefStack<i64>(local_stack, instr->arg0) = derefStack<i64>(frame->args, instr->arg1); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mov_lptr_argptr(OPFUN_ARGS) {
		{
			derefStack<Pointer>(local_stack, instr->arg0)
				= derefStack<Pointer>(frame->args, instr->arg1);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_getFstArg_l64(OPFUN_ARGS) {
		{ derefStack<i64>(local_stack, instr->arg0) = derefStack<i64>(frame->args, 0); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_getFstArg_lptr(OPFUN_ARGS) {
		{ derefStack<Pointer>(local_stack, instr->arg0) = derefStack<Pointer>(frame->args, 0); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_setFstArg_l64(OPFUN_ARGS) {
		{ derefStack<i64>(frame->next_args, 0) = derefStack<i64>(local_stack, instr->arg0); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_setFstArg_lptr(OPFUN_ARGS) {
		{
			derefStack<Pointer>(frame->next_args, 0)
				= derefStack<Pointer>(local_stack, instr->arg0);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mov_arg64_l64(OPFUN_ARGS) {
		{
			derefStack<i64>(frame->next_args, instr->arg0)
				= derefStack<i64>(local_stack, instr->arg1);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_mov_argptr_lptr(OPFUN_ARGS) {
		{
			derefStack<Pointer>(frame->next_args, instr->arg0)
				= derefStack<Pointer>(local_stack, instr->arg1);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_call_func(OPFUN_ARGS) {
		{
			// We have to change variables passed in the arguments (OPFUN_ARGS). After this
			// function: `instr` should be pointer to the instruction in the new function, `frame`
			// should be pointer to the next frame, `local_stack` should be pointer to the local
			// stack of the new function. Old values of `instr` nad `local_stack` should be saved on
			// the frame of the caller.
			auto& runtime_data = thread.runtime_data;

			// Save current registers and flow.
			frame->instr       = instr + 1;
			frame->local_stack = local_stack;

			// Prepare new frame.
			auto* prev_frame = frame;
			frame++;
			i32 function_id = instr->arg0;
			if (frame + 1 >= runtime_data.frame_stack_end) CORE_PANIC("VM stack overflow.");
			frame->args = prev_frame->next_args;

			// Update values passed as arguments.
			instr = thread.executing_code->functions[function_id].bc.data();

			u64 local_stack_size = thread.executing_code->functions[function_id].stack_size;
			local_stack          = runtime_data.local_stack_top;
			runtime_data.local_stack_top += local_stack_size;

			frame->next_args = runtime_data.local_stack_top;
			runtime_data.local_stack_top
				+= thread.executing_code->functions[function_id].next_arg_size;

			if (runtime_data.local_stack_top > runtime_data.local_stack_end)
				CORE_PANIC("VM stack overflow.");
		}
		// After acquiring the `executing_code` of the new function we have instruction pointer
		// (`instr`) pointing at the first instruction of the new function, so moving forward by one
		// would mean that we skipped the first instruction. That's why we move forward zero
		// instructions. For future returns, the first instruction that should be executed after
		// call is saved on frame so that op_ret's have to move forward zero instructions after
		// restoring `instr` from frame.
		OPFUN_CONT_CHECK_STRATEGY(0);
	}

	RETURN_TYPE OpFuns::op_ret_tailcall(OPFUN_ARGS) {
		{
			auto function_id = instr->arg0;

			instr = thread.executing_code->functions[function_id].bc.data();

			swap(frame->args, frame->next_args);
		}
		OPFUN_CONT_CHECK_STRATEGY(0);
	}

	RETURN_TYPE OpFuns::op_ret_l64(OPFUN_ARGS) {
		{
			// @TODO: refactor op_rets to reduce code duplication

			// We have to update values passed in arguments.
			// Old `instr` and `local_stack` are stored on the previous frame.
			// Previous frame is just before current frame in the array, so that
			// substracting one from the pointer will give us the previous frame.
			// The `instr`, `local_stack` and `frame` values should be restored from the previous
			// call stack frame.
			u64 ret_val = derefStack<u64>(local_stack, instr->arg0);

			frame--;

			frame->regs.p64_reg_0               = ret_val;
			thread.runtime_data.local_stack_top = local_stack;

			// Load previous frame
			instr       = frame->instr;  // This is already a pointer to next instr
			local_stack = frame->local_stack;
		}
		// Here the argument is `0` becasue of the convention defined in the op_call_func.
		OPFUN_CONT_CHECK_STRATEGY(0);
	}

	RETURN_TYPE OpFuns::op_ret_imm(OPFUN_ARGS) {
		{
			// For explanation go to op_ret_l64.
			frame--;

			frame->regs.p64_reg_0               = instr->arg0;
			thread.runtime_data.local_stack_top = local_stack;

			// Load previous frame
			instr       = frame->instr;  // This is already a pointer to next instr
			local_stack = frame->local_stack;
		}
		// Here the argument is `0` becasue of the convention defined in the op_call_func.
		OPFUN_CONT_CHECK_STRATEGY(0);
	}

	RETURN_TYPE OpFuns::op_init_type(OPFUN_ARGS) {
		{
			auto             type      = thread.process_types.getType(vm::TypeID(instr->arg0));
			auto             type_size = type->getSize();
			base::ModRawView data(&local_stack[frame->local_stack_head], type_size);
			frame->local_stack_head += type_size;
			auto block = thread.process_stack_allocator.makeTypeBlock(type, data);
			frame->block_id_stack.push_back(block);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_deinit(OPFUN_ARGS) {
		{
			auto block_id  = frame->block_id_stack.back();
			auto type_size = thread.process_memory.getBlock(block_id)->rawPointer().size();
			// if (type_size < frame->local_stack_head) CORE_PANIC("init/deinits not paired");
			frame->local_stack_head -= type_size;
			thread.process_stack_allocator.deleteBlock(block_id);
			frame->block_id_stack.pop_back();
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_input_l64(OPFUN_ARGS) {
		{
			thread.setStatus(api::WaitingForInput{});
			derefStack<i64>(local_stack, instr->arg0)
				= thread.process.getIO().getInput<i64>(thread);
			thread.setStatus(api::Running{});
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_output_l64(OPFUN_ARGS) {
		{ thread.process.getIO().writeOutput(derefStack<u64>(local_stack, instr->arg0)); }
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_nop(OPFUN_ARGS) { OPFUN_CONT(1); }

	RETURN_TYPE OpFuns::op_ext_l64(OPFUN_ARGS) {
		CORE_PANIC("ext_l64 not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::op_alloc_lptr_type(OPFUN_ARGS) {
		{
			auto type = thread.process_types.getType(vm::TypeID(instr->arg1));
			// This is disasbled, because we don't want to pay performance for initializing it
			// NOLINTBEGIN(cppcoreguidelines-pro-type-member-init)
			BlockID block;
			// NOLINTEND(cppcoreguidelines-pro-type-member-init)
			if (type->getKind() == vm::Type::Kind::StaticTable) {
				// @TODO: As noted in type.hpp, interface used below may change
				auto inner_type = type->getInnerType().value();
				auto table_size = type->getStaticTableSize().value();

				// @TODO: It is possible to access process_memory.of the array via
				// mov_l64_imm, which should be at least detected, if not illegal
				block = thread.process_dynamic_allocator.makeArrayBlock(inner_type, table_size);
			} else {
				block = thread.process_dynamic_allocator.makeTypeBlock(type);
			}
			derefStack<Pointer>(local_stack, instr->arg0) = Pointer(block, 0);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_free_lptr(OPFUN_ARGS) {
		{
			thread.process_dynamic_allocator.deleteBlock(
				derefStack<Pointer>(local_stack, instr->arg0).getBlock()
			);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_load_l64_lptr_ofs(OPFUN_ARGS) {
		constexpr const uint8_t view_size = 8;
		int                     next      = 1;
		{
			auto pointer = derefStack<Pointer>(local_stack, instr->arg1);
			auto view    = thread.internalDerefPointer(pointer);
			u64  idx     = 0;
#ifdef USE_TAIL_CALLS
			if (instr[1].opfun == OpFuns::op_ext_l64) [[likely]] {
				idx = derefStack<u64>(local_stack, instr[1].arg0);
				next++;
			}
#else
			if (static_cast<OpcodeFix8>(instr[1].opcode) == OpcodeFix8::ext_l64) [[likely]] {
				idx = derefStack<u64>(local_stack, instr[1].arg0);
				next++;
			}
#endif

			// @TODO: this assert degrades performance by 5-10%
			// CORE_ASSERT(view.size() == view_size, "bad type");

			std::memcpy(&local_stack[instr->arg0], view.getBegin() + idx * view_size, view_size);
		}
		OPFUN_CONT(next);
	}

	RETURN_TYPE OpFuns::op_store_lptr_l64_ofs(OPFUN_ARGS) {
		constexpr const uint8_t view_size = 8;
		int                     next      = 1;
		{
			auto pointer = derefStack<Pointer>(local_stack, instr->arg0);
			auto view    = thread.internalDerefPointer(pointer);
			u64  idx     = 0;
#ifdef USE_TAIL_CALLS
			if (instr[1].opfun == OpFuns::op_ext_l64) [[likely]] {
				idx = derefStack<u64>(local_stack, instr[1].arg0);
				next++;
			}
#else
			if (static_cast<OpcodeFix8>(instr[1].opcode) == OpcodeFix8::ext_l64) [[likely]] {
				idx = derefStack<u64>(local_stack, instr[1].arg0);
				next++;
			}
#endif

			// @TODO: this assert degrades performance by 5-10%
			// CORE_ASSERT(view.size() == view_size, "bad type");

			std::memcpy(view.getBegin() + idx * view_size, &local_stack[instr->arg1], view_size);
		}
		OPFUN_CONT(next);
	}

#if defined(__clang__)
// @TODO: suppress code deduplication in Clang
#elif defined(__GNUG__)
	#pragma GCC push_options
	#pragma GCC optimize("-fno-crossjumping")
#endif

	u64 VMThread::internalCallMain(const FuncData& main_func) {
		// We create one artificial "pre" frame, that when main function returns
		// it will go to it and end execution.
		Frame* pre_frame = runtime_data.frame_stack_base;

#ifdef USE_TAIL_CALLS
		Fix8Instruction exit_instr{ .opfun = OpFuns::op_exit, .arg0 = 0, .arg1 = 0 };
#else
		Fix8Instruction exit_instr{ .opcode = static_cast<u16>(OpcodeFix8::exit),
			                        .arg0   = 0,
			                        .arg1   = 0 };
#endif

		pre_frame->instr       = &exit_instr;
		pre_frame->local_stack = runtime_data.local_stack_top;

		// Frame of the main function.
		Frame*     frame       = runtime_data.frame_stack_base + 1;
		std::byte* local_stack = runtime_data.local_stack_top;
		runtime_data.local_stack_top += main_func.stack_size;

		frame->next_args = runtime_data.local_stack_top;
		runtime_data.local_stack_top += main_func.next_arg_size;

		auto* instr = main_func.bc.data();

#ifdef USE_TAIL_CALLS
		instr->opfun(instr, local_stack, frame, *this);
		return runtime_data.frame_stack_base->regs.p64_reg_0;
#else
	// Computed gotos labels:
	#ifdef USE_COMPUTED_GOTO
		constexpr static std::array<void*, OP_CASES_COUNT> opcode_label = {
		#define DEF_OPCODE(opcode) LABEL_PTR(opcode),
		#include <code_data/opcodes_list.hpp>
		#undef DEF_OPCODE
		};
	#endif

		IF_NOT_CG(while (true)) {
			IF_CG(DISPATCH_OPCODE());

			if constexpr (!IGNORE_EXECUTION_STRATEGY) handleExecutionStrategyIfNeeded();

			IF_NOT_CG(switch (static_cast<OpcodeFix8>(instr->opcode))) {
	#define DEF_OPCODE(opcode)     OP_CASE(opcode)
	#define DEF_OPCODE_END(opcode) OP_CASE_END(opcode)
	#include <code_data/opcodes_list.hpp>
	#undef DEF_OPCODE
	#undef DEF_OPCODE_END
				IF_NOT_CG(default : { CORE_PANIC("Unknown operator:", u64(instr->opcode)); })
			}
		}
	End:
		return runtime_data.frame_stack_base->regs.p64_reg_0;
#endif
	}

#if defined(__clang__)
// @TODO: suppress code deduplication in Clang
#elif defined(__GNUG__)
	#pragma GCC pop_options
#endif

	void VMThread::run(const Code& code) {
		// @TODO: ensure correct status

		setStatus(api::Running{});

		executing_code = &code;
		try {
			internalCallMain(executing_code->functions[code.main_id]);
			setStatus(api::NotStarted{});
		} catch (KillProcessException) { setStatus(api::NotStarted{}); }
	}

	void VMThread::prestart() {
		std::unique_lock lock(external_api_mutex);
		if (execution_strategy == ExecutionStrategy::Stoped) {
			is_running         = true;
			execution_strategy = ExecutionStrategy::Normal;
		}
	}

	void VMThread::stop() {
		std::unique_lock lock(external_api_mutex);
		is_running         = false;
		execution_strategy = ExecutionStrategy::Stoped;
		pause_cv.notify_all();
	}

	bool VMThread::resume() {
		std::unique_lock lock(external_api_mutex);
		if (execution_strategy == ExecutionStrategy::Stoped
		    || execution_strategy == ExecutionStrategy::Normal) {
			return false;
		}
		is_running         = true;
		execution_strategy = ExecutionStrategy::Normal;
		pause_cv.notify_all();
		return true;
	}

	bool VMThread::pause() {
		std::unique_lock lock(external_api_mutex);
		if (execution_strategy == ExecutionStrategy::Stoped
		    || execution_strategy == ExecutionStrategy::Paused) {
			return false;
		} else if (execution_strategy == ExecutionStrategy::StepByStep) {
			return true;
		}
		is_running         = false;
		execution_strategy = ExecutionStrategy::Paused;
		std::cout << "Set strategy to paused\n";
		return true;
	}

	bool VMThread::step() {
		std::unique_lock lock(external_api_mutex);
		if (execution_strategy == ExecutionStrategy::Stoped)
			return false;
		else if (execution_strategy != ExecutionStrategy::Paused)
			return true;
		is_running         = false;
		execution_strategy = ExecutionStrategy::StepByStep;
		pause_cv.notify_all();
		return true;
	}

	void VMThread::setStatus(vm::api::ExecStatus new_status) {
		// @TODO: check if change is legal
		this->status = std::move(new_status);
		process.onEvent(api::Executing{ status });
	}

	bool VMThread::isPaused() {
		std::unique_lock lock(external_api_mutex);
		return execution_strategy == ExecutionStrategy::Paused;
	}

	bool VMThread::isAlive() {
		std::unique_lock lock(external_api_mutex);
		return execution_strategy != ExecutionStrategy::Stoped;
	}

	void VMThread::notifyPaused() { pause_cv.notify_all(); }
}  // namespace vm
