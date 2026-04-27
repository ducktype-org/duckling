#include <memory>
#include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
LLVM_INCLUDE_END()

std::unique_ptr<llvm::Module> parseOpcodesBitcode(llvm::LLVMContext& context);
