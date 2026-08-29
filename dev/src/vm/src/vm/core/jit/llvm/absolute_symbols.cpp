#include "absolute_symbols.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <vm/core/safe/opcode_functions/opcodes_functions.hpp>

#include <ranges>

LLVM_INCLUDE_BEGIN()
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/Error.h>
LLVM_INCLUDE_END()

#include "../jit_helper.hpp"

void registerAbsoluteJITSymbols(llvm::orc::LLJIT& lljit) {
	auto&                jd = lljit.getMainJITDylib();
	llvm::orc::SymbolMap host_symbols;

	std::array hard_symbols = {
		std::pair{ "jitEntrypoint", &vm::OpFuns::op_debug_jitEntrypoint },
		std::pair{ "call_func", &vm::OpFuns::op_debug_call_func },
		std::pair{ "call_builtinfunc", &vm::OpFuns::op_debug_call_builtinfunc },
		std::pair{ "virtual_call_pptr_method", &vm::OpFuns::op_debug_virtual_call_pptr_method },
		std::pair{ "ret_tailcall_func", &vm::OpFuns::op_debug_ret_tailcall_func },
		std::pair{ "trampoline", &vm::jit::helpers::trampoline },
#define HANDLE_MICRO_INSTR(opcode) std::pair{ #opcode, &vm::OpFuns::opcode },
#include <vm/core/safe/low_program/micro_instruction_definitions.hpp>
#undef HANDLE_MICRO_INSTR
	};

	// NOLINTBEGIN(clang-analyzer-optin.core.EnumCastOutOfRange)
	for (auto [name, address]: hard_symbols) {
		host_symbols[lljit.mangleAndIntern(name)] = llvm::orc::ExecutorSymbolDef(
			llvm::orc::ExecutorAddr::fromPtr(address),
			llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable
		);
	}
	// NOLINTEND(clang-analyzer-optin.core.EnumCastOutOfRange)

	cantFail(jd.define(llvm::orc::absoluteSymbols(std::move(host_symbols))));
}
