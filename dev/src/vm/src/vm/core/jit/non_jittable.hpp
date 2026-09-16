#pragma once

#include "jit_helper.hpp"

#include <vm/core/safe/low_program/opcodes.hpp>
#include <vm/core/safe/opcode_functions/opcodes_functions.hpp>

#include <cstdint>

#ifdef ENABLE_JIT
namespace vm::jit {
	constexpr std::array HARD_SYMBOLS = {
	#define HANDLE_NONJITTABLE_INSTR(instr) std::pair{ #instr, &vm::OpFuns::op_debug_##instr },
	#include "non_jittable.def.hpp"
	#undef HANDLE_NONJITTABLE_INSTR
		std::pair{ "trampoline", &vm::jit::helpers::trampoline },
	};

	// Currently unused; kept for sizing jittable/non-jittable-indexed arrays.
	constexpr size_t HELPER_FUNCTIONS  = 1;
	constexpr size_t NON_JITTABLE_COUNT = HARD_SYMBOLS.size() - HELPER_FUNCTIONS;
	constexpr size_t JITTABLE_COUNT     = low::microInstrCount() - NON_JITTABLE_COUNT;
}
#endif  // ENABLE_JIT
