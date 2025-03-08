/**
 * @file opcodes_functions_implementation.hpp
 * @brief The opcodes functions implementations.
 *
 * @warning Do not include this file directly. Include `opcodes_functions.hpp` or
 * `opcodes_functions_debug.hpp` instead.
 *
 * Motivation: each opcode that thread executes has its own function that is called to
 * perform the opcode operation. They are called "OpFuns". At the end of each
 * function, there is a call to `FUNCTION_CONT` macro that tail calls (in TC version)
 * to the next instruction. But in debug mode, if we want to step only one instruction,
 * we need OpFun that doesn't tail call to the next instruction, but pauses the execution instead.
 *
 * This file has **two versions**. One is for the OpFuns implementation and the other is
 * for the debug version of the OpFuns (DebugOpFun), which is a copy of the OpFuns but
 * with different `FUNCTION_CONT` and `ARGS`. This file **should not be included**.
 * If you want to inclue the OpFuns, include `opcodes_functions.hpp` or
 * `opcodes_functions_debug.hpp`.
 *
 * If `DEBUG_OPCODES` is defined, the debug version will be included, otherwise the regular
 * version will be included. This way we also have C++ language server support while writing
 * the code.
 *
 * @warning This file have to contain only the OpFuns. Any other functions will be declared
 * and defined twice leading to multiple definition error. Utilities functions are defined in
 * `opcodes_functions_utils.hpp`.
 */

#include <base/exceptions.hpp>
#include <code_data/instruction.hpp>
#include <core/process/vmprocess.hpp>
#include <base/ints.hpp>
#include "op_case.hpp"
#include "vmthread.hpp"
#include "opcodes_functions_utils.hpp"


#ifdef DEBUG_OPCODES
	#define OPCODE_NAME(name)                  op_debug_##name
	#define FUNCTION_ARGS                      OPFUN_REF_ARGS
	#define FUNCTION_CONT(step)                instr += step;
	#define FUNCTION_CONT_CHECK_STRATEGY(step) instr += step;
#else
	#define OPCODE_NAME(name)                  op_##name
	#define FUNCTION_ARGS                      OPFUN_ARGS
	#define FUNCTION_CONT(step)                OPFUN_CONT(step)
	#define FUNCTION_CONT_CHECK_STRATEGY(step) OPFUN_CONT_CHECK_STRATEGY(step)
#endif

namespace vm {

	// NOTE: functions that implement opcodes (opfunctions) must be done this way:
	//
	// RETURN_TYPE OpFuns::op_<opcode_name>(FUNCTION_ARGS) {
	//  {
	//    <function_body>
	//  }
	//  FUNCTION_CONT(<step>);
	// }
	//
	// Function body must be seperated from the scope of FUNCTION_CONT to make sure
	// that all its destructors have been called before invoking next tail call.
	// Otherwise, the compiler may get confused and may schedule destructors from
	// the body after the next tail call, which then becomes a regular function
	// call and may cause the stack to explode.

	// `op_exit` is the only opcode without the `FUNCTION_CONT` or `FUNCTION_CONT_CHECK_STRATEGY`
	// macro. This means, every other will jump to the next instruction at the end of it with
	// `FUNCTION_CONT`/`FUNCTION_CONT_CHECK_STRATEGY`, so the the only way to end execution is to
	// use this opcode. It also requires different macro surrounding the function call in the
	// computed goto's and switch case, because in those approaches we can't end execution from
	// within the function, but we have to add some instructions on the outside of it. Hence we use
	// the `OP_CASE_END` macro that adds `goto End` instruction, residing after opcode function,
	// inside interpeter loop.
	RETURN_TYPE OpFuns::OPCODE_NAME(exit)(FUNCTION_ARGS) { IF_TC(return;) }

#define DEFINE_MOVE_OPS(BITS_SIZE, TYPE)                                                 \
	RETURN_TYPE OpFuns::OPCODE_NAME(mov_l##BITS_SIZE##_imm)(FUNCTION_ARGS) {             \
		{ derefStack<TYPE>(local_stack, instr->arg0) = static_cast<TYPE>(instr->arg1); } \
		FUNCTION_CONT(1);                                                                \
	}                                                                                    \
	RETURN_TYPE OpFuns::OPCODE_NAME(mov_l##BITS_SIZE##_l##BITS_SIZE)(FUNCTION_ARGS) {    \
		{                                                                                \
			derefStack<TYPE>(local_stack, instr->arg0)                                   \
				= derefStack<TYPE>(local_stack, instr->arg1);                            \
		}                                                                                \
		FUNCTION_CONT(1);                                                                \
	}                                                                                    \
	RETURN_TYPE OpFuns::OPCODE_NAME(cmov_l##BITS_SIZE##_l##BITS_SIZE)(FUNCTION_ARGS) {   \
		{                                                                                \
			if (frame->flags.flag)                                                       \
				derefStack<TYPE>(local_stack, instr->arg0)                               \
					= derefStack<TYPE>(local_stack, instr->arg1);                        \
		}                                                                                \
		FUNCTION_CONT(1);                                                                \
	}

	DEFINE_MOVE_OPS(64, i64)
	DEFINE_MOVE_OPS(32, i32)
	DEFINE_MOVE_OPS(16, i16)
	DEFINE_MOVE_OPS(8, std::int8_t)

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_l64_r0)(FUNCTION_ARGS) {
		{ derefStack<i64>(local_stack, instr->arg0) = frame->regs.p64_reg_0; }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_r0_l64)(FUNCTION_ARGS) {
		{ frame->regs.p64_reg_0 = derefStack<i64>(local_stack, instr->arg0); }
		FUNCTION_CONT(1);
	}

#define DEFINE_ARITHMETIC_OP(NAME, BITS_SIZE, TYPE, OP)                                  \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##BITS_SIZE##_l##BITS_SIZE)(FUNCTION_ARGS) { \
		{                                                                                \
			derefStack<TYPE>(local_stack, instr->arg0)                                   \
				OP derefStack<TYPE>(local_stack, instr->arg1);                           \
		}                                                                                \
		FUNCTION_CONT(1);                                                                \
	}                                                                                    \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##BITS_SIZE##_imm)(FUNCTION_ARGS) {          \
		{ derefStack<TYPE>(local_stack, instr->arg0) OP instr->arg1; }                   \
		FUNCTION_CONT(1);                                                                \
	}

	DEFINE_ARITHMETIC_OP(add, 64, i64, +=)
	DEFINE_ARITHMETIC_OP(add, 32, i32, +=)
	DEFINE_ARITHMETIC_OP(sub, 64, i64, -=)
	DEFINE_ARITHMETIC_OP(sub, 32, i32, -=)
	DEFINE_ARITHMETIC_OP(mul, 64, i64, *=)
	DEFINE_ARITHMETIC_OP(mul, 32, i32, *=)
	DEFINE_ARITHMETIC_OP(mod, 64, i64, %=)
	DEFINE_ARITHMETIC_OP(mod, 32, i32, %=)
	DEFINE_ARITHMETIC_OP(div, 64, i64, /=)
	DEFINE_ARITHMETIC_OP(div, 32, i32, /=)

#define DEFINE_COMPARISON_OP(NAME, BITS_SIZE, TYPE, OP)                                         \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##BITS_SIZE##_l##BITS_SIZE)(FUNCTION_ARGS) {        \
		{                                                                                       \
			frame->flags.flag = derefStack<TYPE>(local_stack, instr->arg0)                      \
				OP derefStack<TYPE>(local_stack, instr->arg1);                                  \
		}                                                                                       \
		FUNCTION_CONT(1);                                                                       \
	}                                                                                           \
	RETURN_TYPE OpFuns::OPCODE_NAME(NAME##_l##BITS_SIZE##_imm)(FUNCTION_ARGS) {                 \
		{                                                                                       \
			frame->flags.flag                                                                   \
				= derefStack<TYPE>(local_stack, instr->arg0) OP static_cast<TYPE>(instr->arg1); \
		}                                                                                       \
		FUNCTION_CONT(1);                                                                       \
	}

	DEFINE_COMPARISON_OP(cmpEq, 64, i64, ==)
	DEFINE_COMPARISON_OP(cmpG, 64, i64, >)
	DEFINE_COMPARISON_OP(cmpEq, 32, i32, ==)
	DEFINE_COMPARISON_OP(cmpG, 32, i32, >)
	DEFINE_COMPARISON_OP(cmpEq, 8, std::int8_t, ==)
	DEFINE_COMPARISON_OP(cmpG, 8, std::int8_t, >)

	RETURN_TYPE OpFuns::OPCODE_NAME(jmpRel_label)(FUNCTION_ARGS) {
		{ instr += instr->arg0; }
		FUNCTION_CONT_CHECK_STRATEGY(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(jmpRelIf_label)(FUNCTION_ARGS) {
		{
			if (frame->flags.flag) instr += instr->arg0;
		}
		FUNCTION_CONT_CHECK_STRATEGY(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(jmpRelNotIf_label)(FUNCTION_ARGS) {
		{
			if (!frame->flags.flag) instr += instr->arg0;
		}
		FUNCTION_CONT_CHECK_STRATEGY(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_l64_arg64)(FUNCTION_ARGS) {
		{ derefStack<i64>(local_stack, instr->arg0) = derefStack<i64>(frame->args, instr->arg1); }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_lptr_argptr)(FUNCTION_ARGS) {
		{
			derefStack<Pointer>(local_stack, instr->arg0)
				= derefStack<Pointer>(frame->args, instr->arg1);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(getFstArg_l64)(FUNCTION_ARGS) {
		{ derefStack<i64>(local_stack, instr->arg0) = derefStack<i64>(frame->args, 0); }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(getFstArg_lptr)(FUNCTION_ARGS) {
		{ derefStack<Pointer>(local_stack, instr->arg0) = derefStack<Pointer>(frame->args, 0); }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(setFstArg_l64)(FUNCTION_ARGS) {
		{ derefStack<i64>(frame->next_args, 0) = derefStack<i64>(local_stack, instr->arg0); }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(setFstArg_lptr)(FUNCTION_ARGS) {
		{
			derefStack<Pointer>(frame->next_args, 0)
				= derefStack<Pointer>(local_stack, instr->arg0);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_arg64_l64)(FUNCTION_ARGS) {
		{
			derefStack<i64>(frame->next_args, instr->arg0)
				= derefStack<i64>(local_stack, instr->arg1);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(mov_argptr_lptr)(FUNCTION_ARGS) {
		{
			derefStack<Pointer>(frame->next_args, instr->arg0)
				= derefStack<Pointer>(local_stack, instr->arg1);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(neg_l64)(FUNCTION_ARGS) {
		{ derefStack<i64>(frame->next_args, instr->arg0) *= -1; }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(neg_l32)(FUNCTION_ARGS) {
		{ derefStack<i32>(frame->next_args, instr->arg0) *= -1; }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(call_func)(FUNCTION_ARGS) {
		{
			// We have to change variables passed in the arguments (FUNCTION_ARGS). After this
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
			auto function_id = static_cast<u32>(instr->arg0);
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
		FUNCTION_CONT_CHECK_STRATEGY(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ret_tailcall)(FUNCTION_ARGS) {
		{
			auto function_id = static_cast<u32>(instr->arg0);

			instr = thread.executing_code->functions[function_id].bc.data();

			swap(frame->args, frame->next_args);
		}
		FUNCTION_CONT_CHECK_STRATEGY(0);
	}

	// @TODO: refactor op_rets to reduce code duplication
	RETURN_TYPE OpFuns::OPCODE_NAME(ret_l64)(FUNCTION_ARGS) {
		{
			while (!frame->block_stack.empty()) {
				auto block = frame->block_stack.back();
				thread.process.getMemory().freeBlock(block);
				frame->block_stack.pop_back();
			}

			// We have to update values passed in arguments.
			// Old `instr` and `local_stack` are stored on the previous frame.
			// Previous frame is just before current frame in the array, so that
			// substracting one from the pointer will give us the previous frame.
			// The `instr`, `local_stack` and `frame` values should be restored from the previous
			// call stack frame.
			auto ret_val = derefStack<i64>(local_stack, instr->arg0);

			frame--;

			frame->regs.p64_reg_0               = ret_val;
			thread.runtime_data.local_stack_top = local_stack;

			// Load previous frame
			instr       = frame->instr;  // This is already a pointer to next instr
			local_stack = frame->local_stack;
		}
		// Here the argument is `0` becasue of the convention defined in the op_call_func.
		FUNCTION_CONT_CHECK_STRATEGY(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ret_l32)(FUNCTION_ARGS) {
		{
			while (!frame->block_stack.empty()) {
				auto block = frame->block_stack.back();
				thread.process.getMemory().freeBlock(block);
				frame->block_stack.pop_back();
			}

			i32 ret_val = derefStack<i32>(local_stack, instr->arg0);

			frame--;

			frame->regs.p64_reg_0               = ret_val;
			thread.runtime_data.local_stack_top = local_stack;

			// Load previous frame
			instr       = frame->instr;  // This is already a pointer to next instr
			local_stack = frame->local_stack;
		}
		// Here the argument is `0` becasue of the convention defined in the op_call_func.
		FUNCTION_CONT_CHECK_STRATEGY(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ret_imm)(FUNCTION_ARGS) {
		{
			while (!frame->block_stack.empty()) {
				auto block = frame->block_stack.back();
				thread.process.getMemory().freeBlock(block);
				frame->block_stack.pop_back();
			}

			// For explanation go to op_ret_l64.
			frame--;

			frame->regs.p64_reg_0               = instr->arg0;
			thread.runtime_data.local_stack_top = local_stack;

			// Load previous frame
			instr       = frame->instr;  // This is already a pointer to next instr
			local_stack = frame->local_stack;
		}
		// Here the argument is `0` becasue of the convention defined in the op_call_func.
		FUNCTION_CONT_CHECK_STRATEGY(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(ret)(FUNCTION_ARGS) {
		{
			while (!frame->block_stack.empty()) {
				auto block = frame->block_stack.back();
				thread.process.getMemory().freeBlock(block);
				frame->block_stack.pop_back();
			}

			// For explanation go to op_ret_l64.
			frame--;

			thread.runtime_data.local_stack_top = local_stack;

			// Load previous frame
			instr       = frame->instr;  // This is already a pointer to next instr
			local_stack = frame->local_stack;
		}
		// Here the argument is `0` becasue of the convention defined in the op_call_func.
		FUNCTION_CONT_CHECK_STRATEGY(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(init_type)(FUNCTION_ARGS) {
		{
			auto type     = thread.process_types.getType(vm::TypeID(static_cast<u32>(instr->arg0)));
			auto data_ptr = local_stack + frame->local_stack_head;
			auto block    = thread.process_memory.allocateStack(type, data_ptr);
			frame->block_stack.push_back(block);
			frame->local_stack_head += type->getSize();
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(deinit)(FUNCTION_ARGS) {
		{
			auto block = frame->block_stack.back();
			auto type  = thread.process_memory.getBlockType(block);
			frame->block_stack.pop_back();
			thread.process_memory.freeBlock(block);
			frame->local_stack_head -= type->getSize();
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(input_l64)(FUNCTION_ARGS) {
		{
			thread.setProcessStatus(api::WaitingForInput{});
			derefStack<i64>(local_stack, instr->arg0)
				= thread.process.getIO().getInput<i64>(thread);
			thread.setProcessStatus(api::Running{});
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(output_l64)(FUNCTION_ARGS) {
		{ thread.process.getIO().writeOutput(derefStack<u64>(local_stack, instr->arg0)); }
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(nop)(FUNCTION_ARGS) { FUNCTION_CONT(1); }

	RETURN_TYPE OpFuns::OPCODE_NAME(ext_l64)(FUNCTION_ARGS) {
		CORE_PANIC("ext_l64 not consumed by previous instruction");
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(alloc_lptr_type)(FUNCTION_ARGS) {
		{
			auto type  = thread.process_types.getType(vm::TypeID(static_cast<u32>(instr->arg1)));
			auto block = thread.process_memory.allocateHeap(type);
			derefStack<Pointer>(local_stack, instr->arg0) = Memory::getPointer(block);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(free_lptr)(FUNCTION_ARGS) {
		{
			thread.process_memory.freeBlock(derefStack<Pointer>(local_stack, instr->arg0).getBlock()
			);
		}
		FUNCTION_CONT(1);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(load_l64_lptr_ofs)(FUNCTION_ARGS) {
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
		FUNCTION_CONT(next);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(store_lptr_l64_ofs)(FUNCTION_ARGS) {
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
		FUNCTION_CONT(next);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(breakpoint)(FUNCTION_ARGS) {
		{
			instr += 1;
			save_execution_state(instr, local_stack, frame, thread);

			thread.handleBreakpoint();

			// Restore current registers and flow.
			// They can be changed when doing "step by step" execution.
			frame       = thread.runtime_data.frame_stack_current;
			instr       = frame->instr;
			local_stack = frame->local_stack;
		}
		FUNCTION_CONT(0);
	}

	RETURN_TYPE OpFuns::OPCODE_NAME(label)(FUNCTION_ARGS) {
		CORE_PANIC("Handling label should not be possible.");
	}
}

#undef OPCODE_NAME
#undef FUNCTION_ARGS
#undef FUNCTION_CONT
#undef FUNCTION_CONT_CHECK_STRATEGY
