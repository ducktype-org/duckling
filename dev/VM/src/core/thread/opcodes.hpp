#include "op_case.hpp"
#include "vmthread.hpp"
#include <core/process/vmprocess.hpp>

namespace vm {

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
			auto type     = thread.process_types.getType(vm::TypeID(instr->arg0));
			auto data_ptr = local_stack + frame->local_stack_head;
			auto block    = thread.process_memory.allocateStack(type, data_ptr);
			frame->block_stack.push_back(block);
			frame->local_stack_head += type->getSize();
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_deinit(OPFUN_ARGS) {
		{
			auto block = frame->block_stack.back();
			auto type  = thread.process_memory.getBlockType(block);
			frame->block_stack.pop_back();
			thread.process_memory.freeBlock(block);
			frame->local_stack_head -= type->getSize();
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_input_l64(OPFUN_ARGS) {
		{
			thread.notifyProcess(api::WaitingForInput{});
			derefStack<i64>(local_stack, instr->arg0)
				= thread.process.getIO().getInput<i64>(thread);
			thread.notifyProcess(api::Running{});
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
			auto type  = thread.process_types.getType(vm::TypeID(instr->arg1));
			auto block = thread.process_memory.allocateHeap(type);
			derefStack<Pointer>(local_stack, instr->arg0) = Memory::getPointer(block);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_free_lptr(OPFUN_ARGS) {
		{
			thread.process_memory.freeBlock(derefStack<Pointer>(local_stack, instr->arg0).getBlock()
			);
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::op_load_l64_lptr_ofs(OPFUN_ARGS) {
		constexpr const uint8_t view_size = 8;
		int                     next      = 1;
		{
			auto pointer = derefStack<Pointer>(local_stack, instr->arg1);
			auto view    = Memory::getPointerData(pointer, view_size);
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
			auto view    = Memory::getPointerData(pointer, view_size);
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

	RETURN_TYPE OpFuns::op_breakpoint(OPFUN_ARGS) {
		{	
			save_execution_state(instr, local_stack, frame, thread);

			thread.handleBreakpoint();

			// Restore current registers and flow.
			// They can be changed when doing "step by step" execution.
			frame       = thread.runtime_data.frame_stack_current;
			instr       = frame->instr;
			local_stack = frame->local_stack;
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::handle_execution_break(OPFUN_ARGS) {
		{
			save_execution_state(instr, local_stack, frame, thread);

			thread.handleExecutionBreak();

			// Restore current registers and flow.
			// They can be changed when doing "step by step" execution.
			frame       = thread.runtime_data.frame_stack_current;
			instr       = frame->instr;
			local_stack = frame->local_stack;
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::save_execution_state(OPFUN_ARGS) {
		{
			// Save current registers and flow.
			frame->instr       = instr;
			frame->local_stack = local_stack;
			thread.runtime_data.frame_stack_current = frame;
		}
	}
}