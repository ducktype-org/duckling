#include "evaluator.hpp"

#include "vm/core/fast/program/instructions/executable.hpp"
#include <vm/core/fast/program/program.hpp>
#include <vm/core/fast/utils.hpp>

#include <iostream>

#define IMPL(NAME, body, progress_count)                             \
	void vm::fast::FastExecutor::Instr_##NAME(INSTRFUN_ARGS(NAME)) { \
		body;                                                        \
		state.incrementInstruction(progress_count);                  \
	}

#define getBytePtrToPlace(ARG) \
	decodePlace(ARG, frame->local_stack_base, state.global_data_buffer_base)

IMPL(
	init_imm,
	{
		std::memset(local_stack, 0, instr.size);
		local_stack += instr.size;
	},
	1
)

IMPL(deinit_imm, local_stack -= instr.size, 1)

IMPL(
	mov_p64_imm,
	{
		byte* const dst = getBytePtrToPlace(instr.dst);
		std::cout << "Moving immediate value " << instr.imm << " to destination: " << dst << '\n';
		memcpy(dst, &instr.imm, sizeof(u64));
	},
	1
)

IMPL(
	mov_p64_p64,
	{
		byte* const dst = getBytePtrToPlace(instr.dst);
		byte* const src = getBytePtrToPlace(instr.src);
		memcpy(dst, src, sizeof(u64));
	},
	1
)

IMPL(
	add_p64_p64,
	{
		byte* const dst = getBytePtrToPlace(instr.dst);
		byte* const src = getBytePtrToPlace(instr.src);
		u64         dst_val;
		u64         src_val;
		memcpy(&dst_val, dst, sizeof(u64));
		memcpy(&src_val, src, sizeof(u64));
		dst_val += src_val;
		memcpy(dst, &dst_val, sizeof(u64));
	},
	1
)

IMPL(
	cmpEq_p64_p64,
	{
		byte* const a = getBytePtrToPlace(instr.a);
		byte* const b = getBytePtrToPlace(instr.b);
		frame->flag   = memcmp(a, b, sizeof(u64)) == 0;
	},
	1
)

IMPL(jumpIf_dest, if (frame->flag) frame->ip = instr.target, 0)

IMPL(
	call_func_imm,
	{
		// First increase the instruction pointer to point to the next instruction,
	    // so that when function returns, it will return to the correct place.
		frame->ip++;
		// Next, push a new frame for the called function. The instruction pointer of the new frame
	    // will be set to the start of the called function. The local stack for the new frame should
	    // be set to the address of the first return type.
		frame = state.pushFrame(instr.func, local_stack - instr.stack_diff);
	},
	0
)

IMPL(
	ret_imm,
	{
		local_stack -= instr.stack_cleanup_size;
		state.popFrame();
		// After popping the frame, the instruction pointer will be set to the caller's next
	    // instruction, so we don't need to do anything else here.
	},
	0
)
