#include "absolute_symbols.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <vm/core/thread/opcode_functions/opcodes_functions.hpp>


LLVM_INCLUDE_BEGIN()

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/Support/Error.h>

LLVM_INCLUDE_END()

void registerAbsoluteJITSymbols(llvm::orc::LLJIT& lljit) {
    auto& jd = lljit.getMainJITDylib();
    llvm::orc::SymbolMap host_symbols;
    host_symbols[lljit.mangleAndIntern("ret")] = llvm::orc::ExecutorSymbolDef (
        llvm::orc::ExecutorAddr::fromPtr(&vm::OpFuns::op_ret),
        llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable
    );
    cantFail(jd.define(llvm::orc::absoluteSymbols(std::move(host_symbols))));
}