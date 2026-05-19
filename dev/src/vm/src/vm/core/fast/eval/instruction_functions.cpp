#include "evaluator.hpp"

#include "vm/core/fast/program/instructions/executable.hpp"
#include <vm/core/fast/utils.hpp>

#define IMPL(NAME, body) \
	constexpr void vm::fast::FastExecutor::Instr_##NAME(INSTRFUN_ARGS(NAME)) { body; }

#define getBytePtrToPlace(ARG) decodePlace(ARG, local_stack, state.global_data_buffer_base)

IMPL(init_imm, {
	std::memset(local_stack, 0, instr.size);
	local_stack += instr.size;
})

IMPL(deinit_imm, local_stack -= instr.size)

IMPL(mov_p64_imm, {
	byte* const dst = getBytePtrToPlace(instr.dst);
	memcpy(dst, &instr.imm, sizeof(u64));
})

IMPL(mov_p64_p64, {
	byte* const dst = getBytePtrToPlace(instr.dst);
	byte* const src = getBytePtrToPlace(instr.src);
	memcpy(dst, src, sizeof(u64));
})

IMPL(add_p64_p64, {
	byte* const dst = getBytePtrToPlace(instr.dst);
	byte* const src = getBytePtrToPlace(instr.src);
	u64         dst_val;
	u64         src_val;
	memcpy(&dst_val, dst, sizeof(u64));
	memcpy(&src_val, src, sizeof(u64));
	dst_val += src_val;
	memcpy(dst, &dst_val, sizeof(u64));
})

IMPL(cmpEq_p64_p64, {
	byte* const a = getBytePtrToPlace(instr.a);
	byte* const b = getBytePtrToPlace(instr.b);
	frame->flag   = memcmp(a, b, sizeof(u64)) == 0;
})

IMPL(jumpIf_dest, if (frame->flag) frame->ip = instr.target)

IMPL(call_func, {
	// We will handle the call in the main loop, so we just need to set the instruction pointer to
	// the called function's instructions. The rest of the call logic, like setting up the new
	// frame and passing arguments, will be handled in the main loop as well.
	state.pushFrame();
	frame = state.top_frame;  // Update the reference to the current frame after pushing a new one.
	frame->ip = instr.func->data.data();
})

IMPL(ret, {
	state.popFrame();
	// After popping the frame, the instruction pointer will be set to the caller's next
	// instruction, so we don't need to do anything else here.
})

constexpr int vm::fast::FastExecutor::eval(INSTRFUN_ARGS_BASE) {
	bool run = true;
	while (run) {
		switch (state.top_frame->ip->id) {
#define HANDLE_INSTR(NAME)                                                    \
	case vm::fast::InstrID::NAME: {                                           \
		if constexpr (std::string_view(#NAME) != "exit") {                    \
			Instr_##NAME(state, local_stack, frame, frame->ip->instr_##NAME); \
		} else {                                                              \
			run = false;                                                      \
		}                                                                     \
		break;                                                                \
	}
#include <vm/core/fast/program/instructions/instruction_definitions.hpp>
#undef HANDLE_INSTR
		default:
			return 1;
			break;
		}
		state.top_frame->ip++;
	}
	return 0;
}
