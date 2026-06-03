#include "jit_helper.hpp"

#include <vm/core/safe/low_program/opcodes.hpp>
#include <vm/core/safe/opcode_functions/opcodes_functions.hpp>

#include <cstdint>

namespace vm::jit {
	constexpr std::array hard_symbols = {
		std::pair{ "jmp_label", &vm::OpFuns::op_debug_jmp_label },
		std::pair{ "jmpIfNot_label", &vm::OpFuns::op_debug_jmpIfNot_label },
		std::pair{ "jmpIf_label", &vm::OpFuns::op_debug_jmpIf_label },
		std::pair{ "jit_call_entrypoint", &vm::OpFuns::op_debug_jit_call_entrypoint },
		std::pair{ "call_func", &vm::OpFuns::op_debug_call_func },
		std::pair{ "call_builtinfunc", &vm::OpFuns::op_debug_call_builtinfunc },
		std::pair{ "virtual_call_pptr_method", &vm::OpFuns::op_debug_virtual_call_pptr_method },
		std::pair{ "ret_tailcall_func", &vm::OpFuns::op_debug_ret_tailcall_func },
		std::pair{ "breakpoint", &vm::OpFuns::op_debug_breakpoint },
		std::pair{ "ret", &vm::OpFuns::op_debug_ret },
		std::pair{ "trampoline", &vm::jit::helpers::trampoline },
	};

	constexpr size_t helper_functions  = 1;
	constexpr size_t non_jitable_count = hard_symbols.size() - helper_functions;
	constexpr size_t jitable_count     = low::microInstrCount() - non_jitable_count;
}
