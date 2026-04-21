#include "absolute_symbols.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <vm/core/thread/opcode_functions/opcodes_functions.hpp>

#include <ranges>


LLVM_INCLUDE_BEGIN()
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/Error.h>
LLVM_INCLUDE_END()

#include "../jit_helper.hpp"

static constexpr usize UNJITABLE_OPCODES_COUNT = 11;

void registerAbsoluteJITSymbols(llvm::orc::LLJIT& lljit) {
	auto&                jd = lljit.getMainJITDylib();
	llvm::orc::SymbolMap host_symbols;

	std::array<std::string_view, UNJITABLE_OPCODES_COUNT> hard_symbols = {
		"jmp_label",
		"jmpIfNot_label",
		"jmpIf_label",
		"jit_call_entrypoint",
		"call_func",
		"call_builtinfunc",
		"virtual_call_lptr_method",
		"ret_tailcall_func",
		"breakpoint",
		"ret",
		"trampoline",
	};

	std::array<vm::OpFun*, UNJITABLE_OPCODES_COUNT> addresses = {
		&vm::OpFuns::op_jmp_label,
		&vm::OpFuns::op_jmpIfNot_label,
		&vm::OpFuns::op_jmpIf_label,
		&vm::OpFuns::op_jit_call_entrypoint,
		&vm::OpFuns::op_call_func,
		&vm::OpFuns::op_call_builtinfunc,
		&vm::OpFuns::op_virtual_call_pptr_method,
		&vm::OpFuns::op_ret_tailcall_func,
		&vm::OpFuns::op_breakpoint,
		&vm::OpFuns::op_ret,
		&vm::jit::helpers::trampoline,
	};

	// Once Clang 21 is compatible with Ubuntu, this can (and should) be changed
	// to use structured bindings and `std::ranges::views::zip`.
	for (usize i = 0; i < addresses.size(); ++i) {
		host_symbols[lljit.mangleAndIntern(hard_symbols.at(i))] = llvm::orc::ExecutorSymbolDef(
			llvm::orc::ExecutorAddr::fromPtr(addresses.at(i)),
			llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable
		);
	}

	cantFail(jd.define(llvm::orc::absoluteSymbols(std::move(host_symbols))));
}
