#include "absolute_symbols.hpp"

#include "../non_jitable.hpp"
#include <llvm_helpers/llvm_helpers.hpp>

#include <ranges>

LLVM_INCLUDE_BEGIN()
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/Error.h>
LLVM_INCLUDE_END()

void registerAbsoluteJITSymbols(llvm::orc::LLJIT& lljit) {
	auto&                jd = lljit.getMainJITDylib();
	llvm::orc::SymbolMap host_symbols;

	// NOLINTBEGIN(clang-analyzer-optin.core.EnumCastOutOfRange)
	for (auto [name, address]: hard_symbols) {
		host_symbols[lljit.mangleAndIntern(name)] = llvm::orc::ExecutorSymbolDef(
			llvm::orc::ExecutorAddr::fromPtr(address),
			llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable
		);
	}
	// NOLINTEND

	cantFail(jd.define(llvm::orc::absoluteSymbols(std::move(host_symbols))));
}
