#include "evaluator.hpp"

#include "vm/api/data/status.hpp"
#include <vm/core/fast/fast_vmthread.hpp>
#include <vm/core/fast/utils.hpp>
#include <vm/core/process/vmprocess.hpp>

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
	sub_p64_p64,
	{
		byte* const dst = getBytePtrToPlace(instr.dst);
		byte* const src = getBytePtrToPlace(instr.src);
		u64         dst_val;
		u64         src_val;
		memcpy(&dst_val, dst, sizeof(u64));
		memcpy(&src_val, src, sizeof(u64));
		dst_val -= src_val;
		memcpy(dst, &dst_val, sizeof(u64));
	},
	1
)

IMPL(
	output_p64,
	{
		byte* const src = getBytePtrToPlace(instr.src);
		u64         value;
		memcpy(&value, src, sizeof(u64));
		thread.getMyProcess().getIO().writeOutput(value);
	},
	1
)

IMPL(
	input_p64,
	{
		thread.getMyProcess().setStatus(api::Sleeping{});
		i64   io_value = thread.getMyProcess().getIO().getInput<i64>(thread);
		byte* dst      = getBytePtrToPlace(instr.dst);
		memcpy(dst, &io_value, sizeof(u64));
		thread.getMyProcess().setStatus(api::Running{});
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
		local_stack = frame->local_stack_base + instr.function_return_size;
		state.popFrame();
		// After popping the frame, the instruction pointer will be set to the caller's next
	    // instruction, so we don't need to do anything else here.
	},
	0
)
