// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "evaluator.hpp"
#include "helpers.hpp"

#include <vm/api/data/status.hpp>
#include <vm/core/fast/fast_vmthread.hpp>
#include <vm/core/fast/utils.hpp>
#include <vm/core/process/ivmprocess.hpp>

#include <cstring>
#include <utility>

#define IMPL(NAME) void vm::fast::FastExecutor::Instr_##NAME(INSTRFUN_ARGS(NAME)) noexcept

// Increments instruction pointer.
// In tailcalls, the dispatcher calls the next instruction
#define PROGRESS_BY(progress_count) state.incrementInstruction(progress_count)

// Ends execution with `trap`; the dispatch loop stops at the start function's `exit`.
#define FAST_TRAP(trap_kind)                 \
	do {                                     \
		state.trap = (trap_kind);            \
		frame->ip  = state.exit_instruction; \
		return;                              \
	} while (0)

// Calls a helper and traps when it reports one.
#define CALL_HELPER(...)                                                                  \
	do {                                                                                  \
		if (const Trap helper_trap = FastHelpers::__VA_ARGS__; helper_trap != Trap::None) \
			[[unlikely]]                                                                  \
			FAST_TRAP(helper_trap);                                                       \
	} while (0)

// Checks a VM invariant in development builds, trapping instead of throwing.
#if defined(BUILD_TYPE_DEV)
	#define FAST_DEV_CHECK(cond)                \
		do {                                    \
			if (!(cond)) [[unlikely]]           \
				FAST_TRAP(Trap::InternalError); \
		} while (0)
#else
	#define FAST_DEV_CHECK(cond) \
		do {                     \
		} while (0)
#endif

#define getBytePtrToPlace(ARG) decodePlace(ARG, local_stack, state.global_data_buffer_base)
#define READ_PLACE(type, arg)  safeReadPointerBytes<type>(getBytePtrToPlace(arg))
#define WRITE_PLACE(arg, val)  safeWriteBytes(getBytePtrToPlace(arg), val)

IMPL(init_pany_imm) {
	// @note init will never happen for global
	memset(local_stack + instr.dst, 0, instr.size);
	// LLVM does not optimize call to memset, even with LTO for some reason.
	// But call using standard abi is still for some reason faster than
	// the loop below.
	// for (u64* p = (u64*) (local_stack + instr.dst);
	//      p < (u64*) (local_stack + instr.dst + instr.size);
	//      p++)
	// 	*p = 0ULL;

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
	CALL_HELPER(outputU64(state, thread, READ_PLACE(u64, instr.src)));
	PROGRESS_BY(1);
}

IMPL(input_p64) {
	i64 io_value = 0;
	CALL_HELPER(inputI64(state, thread, io_value));
	WRITE_PLACE(instr.dst, io_value);
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
	FAST_DEV_CHECK(state.canPopFrame());
	frame       = state.popFrame();
	local_stack = frame->local_stack_base;
	// After popping the frame, the instruction pointer will be set to the caller's next
	// instruction, so we don't need to do anything else here.
	PROGRESS_BY(0);
}

IMPL(exit) {}

void vm::fast::FastExecutor::eval(INSTRFUN_ARGS_BASE) IF_NOT_TC(noexcept) {
#ifdef USE_TAIL_CALLS
	frame->ip->id(state, local_stack, frame, thread, *frame->ip);
#else
	bool run = true;
	while (run) {
		switch (frame->ip->id) {
	#define HANDLE_INSTR(NAME)                                                        \
	case vm::fast::InstrID::NAME: {                                                   \
		if constexpr (std::string_view(#NAME) != "exit") {                            \
			Instr_##NAME(state, local_stack, frame, thread, frame->ip->instr_##NAME); \
		} else {                                                                      \
			run = false;                                                              \
		}                                                                             \
		break;                                                                        \
	}
	#include <vm/core/fast/program/instructions/instruction_definitions.def.hpp>
	#undef HANDLE_INSTR
		default:
	#if defined(BUILD_TYPE_DEV)
			state.trap = Trap::InternalError;
			run        = false;
	#else
			// Lets the compiler drop the jump table's range check.
			std::unreachable();
	#endif
		}
	}
#endif
}
