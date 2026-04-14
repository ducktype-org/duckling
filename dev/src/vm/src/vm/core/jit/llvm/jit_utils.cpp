#include "opcodes_bitcode_source.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <memory>

LLVM_INCLUDE_BEGIN()

#include <llvm/IR/Module.h>

LLVM_INCLUDE_END()

std::unique_ptr<llvm::Module> setupModule(const std::string& module_name, llvm::LLVMContext &ctx) {
    llvm::Module* master_module = llvmGetMasterModule();
    auto new_mod = std::make_unique<llvm::Module>(module_name, ctx);
    new_mod->setDataLayout(master_module->getDataLayout());
    new_mod->setTargetTriple(master_module->getTargetTriple());
    return new_mod;
}
