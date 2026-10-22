// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "jit_data.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <functional>

LLVM_INCLUDE_BEGIN()

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

LLVM_INCLUDE_END()

std::unique_ptr<llvm::Module> setupModule(const std::string& module_name, llvm::LLVMContext& ctx) {
	auto&             llvm_data     = llvmData();
	Ref<llvm::Module> master_module = llvm_data.g_module.get();
	auto              new_mod       = std::make_unique<llvm::Module>(module_name, ctx);
	new_mod->setDataLayout(master_module->getDataLayout());
	new_mod->setTargetTriple(master_module->getTargetTriple());
	return new_mod;
}

namespace {
	/**
	 * @brief "Exports" an LLVM global value so it is visible to other modules.
	 */
	void externalizeGlobalValue(llvm::GlobalValue& gv) {
		if (!gv.isDeclaration()) {
			gv.setLinkage(llvm::GlobalValue::ExternalLinkage);
			gv.setVisibility(llvm::GlobalValue::DefaultVisibility);
		}
	}
}

void externalizeAllGlobalValues(llvm::Module& module) {
	for (auto& gv: module.globals()) externalizeGlobalValue(gv);

	for (auto& ga: module.aliases()) externalizeGlobalValue(ga);

	for (auto& ifunc: module.ifuncs()) externalizeGlobalValue(ifunc);

	for (auto& f: module.functions()) externalizeGlobalValue(f);
}

void cloneAndRegisterModule(
	llvm::Module&                                        src,
	llvm::orc::LLJIT&                                    lljit,
	const std::function<bool(const llvm::GlobalValue*)>& filter,
	llvm::orc::ThreadSafeContext&                        tsctx,
	llvm::ExitOnError&                                   exit_on_err
) {
	llvm::ValueToValueMapTy     vmap;
	auto                        dest = llvm::CloneModule(src, vmap, filter);
	llvm::orc::ThreadSafeModule tsm(std::move(dest), tsctx);
	exit_on_err(lljit.addIRModule(std::move(tsm)));
}
