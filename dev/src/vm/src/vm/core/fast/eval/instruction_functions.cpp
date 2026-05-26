#include "evaluator.hpp"

#include <vm/api/data/status.hpp>
#include <vm/core/fast/fast_vmthread.hpp>
#include <vm/core/fast/utils.hpp>
#include <vm/core/process/vmprocess.hpp>

#define IMPL(NAME) void vm::fast::FastExecutor::Instr_##NAME(INSTRFUN_ARGS(NAME))

#define PROGRESS_BY(progress_count) state.incrementInstruction(progress_count)

#define getBytePtrToPlace(ARG) local_stack + ARG

#define READ_PLACE(type, arg) *reinterpret_cast<type*>(getBytePtrToPlace(arg))
#define WRITE_PLACE(arg, val) \
	*reinterpret_cast<std::remove_cvref_t<decltype(val)>*>(getBytePtrToPlace(arg)) = val

// #define getBytePtrToPlace(ARG) \
// 	decodePlace(ARG, local_stack, state.global_data_buffer_base)
// #define READ_PLACE(type, arg) safeReadPointerBytes<type>(getBytePtrToPlace(arg))
// #define WRITE_PLACE(arg, val) safeWriteBytes(getBytePtrToPlace(arg), val)

// !TODO: Make local_stack be the stack_base_pointer

IMPL(init_pany_imm) {
	// Init will never happen for global
	std::memset(local_stack + instr.dst, 0, instr.size);
	PROGRESS_BY(1);
}

IMPL(mov_p64_imm) {
	WRITE_PLACE(instr.dst, instr.imm);
	PROGRESS_BY(1);
}

IMPL(mov_p64_p64) {
	WRITE_PLACE(instr.dst, READ_PLACE(u64, instr.src));
	PROGRESS_BY(1);
}

IMPL(add_p64_p64) {
	u64 dst_val = READ_PLACE(u64, instr.dst);
	u64 src_val = READ_PLACE(u64, instr.src);
	WRITE_PLACE(instr.dst, dst_val + src_val);
	PROGRESS_BY(1);
}

IMPL(add_p64_imm) {
	u64 dst_val = READ_PLACE(u64, instr.dst);
	WRITE_PLACE(instr.dst, dst_val + instr.imm);
	PROGRESS_BY(1);
}

IMPL(sub_p64_p64) {
	u64 dst_val = READ_PLACE(u64, instr.dst);
	u64 src_val = READ_PLACE(u64, instr.src);
	WRITE_PLACE(instr.dst, dst_val - src_val);
	PROGRESS_BY(1);
}

IMPL(sub_p64_imm) {
	u64 dst_val = READ_PLACE(u64, instr.dst);
	WRITE_PLACE(instr.dst, dst_val - instr.imm);
	PROGRESS_BY(1);
}

IMPL(mod_p64_p64) {
	u64 dst_val = READ_PLACE(u64, instr.dst);
	u64 src_val = READ_PLACE(u64, instr.src);
	WRITE_PLACE(instr.dst, dst_val % src_val);
	PROGRESS_BY(1);
}

IMPL(mod_p64_imm) {
	u64 dst_val = READ_PLACE(u64, instr.dst);
	WRITE_PLACE(instr.dst, dst_val % instr.imm);
	PROGRESS_BY(1);
}

IMPL(output_p64) {
	thread.getMyProcess().getIO().writeOutput(READ_PLACE(u64, instr.src));
	PROGRESS_BY(1);
}

IMPL(input_p64) {
	thread.getMyProcess().setStatus(api::Sleeping{});
	i64 io_value = thread.getMyProcess().getIO().getInput<i64>(thread);
	WRITE_PLACE(instr.dst, io_value);
	thread.getMyProcess().setStatus(api::Running{});
	PROGRESS_BY(1);
}

IMPL(cmpEq_p64_p64) {
	byte* const a = getBytePtrToPlace(instr.a);
	byte* const b = getBytePtrToPlace(instr.b);
	frame->flag   = memcmp(a, b, sizeof(u64)) == 0;
	PROGRESS_BY(1);
}

IMPL(cmpEq_p64_imm) {
	u64 a       = READ_PLACE(u64, instr.a);
	frame->flag = a == instr.b;
	PROGRESS_BY(1);
}

IMPL(cmpGt_p64_p64) {
	frame->flag = READ_PLACE(i64, instr.a) > READ_PLACE(i64, instr.b);
	PROGRESS_BY(1);
}

IMPL(cmpGt_p64_imm) {
	u64 a       = READ_PLACE(u64, instr.a);
	frame->flag = a > i64(instr.b);
	PROGRESS_BY(1);
}

IMPL(jmp_dest) {
	frame->ip += instr.target;
	PROGRESS_BY(0);
}

IMPL(jmpIf_dest) {
	frame->ip += frame->flag ? instr.target : 1;
	PROGRESS_BY(0);
}

IMPL(jmpIfNot_dest) {
	frame->ip += !frame->flag ? instr.target : 1;
	PROGRESS_BY(0);
}

IMPL(call_func_imm) {
	// First increase the instruction pointer to point to the next instruction,
	// so that when function returns, it will return to the correct place.
	frame->ip++;
	// Next, adjust the local stack so that local_stack begins
	// at the first return value of the called function.
	local_stack += instr.stack_diff;
	// Lastly, push a new frame for the called function. The instruction pointer of the new frame
	// will be set to the start of the called function.
	frame = state.pushFrame(instr.func, local_stack);

	// Note that we do not progress the instruction pointer here
	// because frame is already set to the called function's first instruction.
	PROGRESS_BY(0);
}

IMPL(ret_imm) {
	frame       = state.popFrame();
	local_stack = frame->local_stack_base;
	// After popping the frame, the instruction pointer will be set to the caller's next
	// instruction, so we don't need to do anything else here.
	PROGRESS_BY(0);
}
