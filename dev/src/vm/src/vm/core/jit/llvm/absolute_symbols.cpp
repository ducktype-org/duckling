// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "absolute_symbols.hpp"

#include "../non_jittable.hpp"

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
	for (auto [name, address]: vm::jit::HARD_SYMBOLS) {
		host_symbols[lljit.mangleAndIntern(name)] = llvm::orc::ExecutorSymbolDef(
			llvm::orc::ExecutorAddr::fromPtr(address),
			llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable
		);
	}
	// NOLINTEND(clang-analyzer-optin.core.EnumCastOutOfRange)

	cantFail(jd.define(llvm::orc::absoluteSymbols(std::move(host_symbols))));
}
