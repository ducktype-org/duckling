#include "jit_data.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <memory>

LLVM_INCLUDE_BEGIN()

#include <llvm/IR/Module.h>

LLVM_INCLUDE_END()

std::unique_ptr<llvm::Module> setupModule(const std::string& module_name, llvm::LLVMContext& ctx) {
	auto&             llvm_data     = llvmData();
	Ref<llvm::Module> master_module = llvm_data.g_module.get();
	auto              new_mod       = std::make_unique<llvm::Module>(module_name, ctx);
	new_mod->setDataLayout(master_module->getDataLayout());
	new_mod->setTargetTriple(master_module->getTargetTriple());
	return new_mod;
}
