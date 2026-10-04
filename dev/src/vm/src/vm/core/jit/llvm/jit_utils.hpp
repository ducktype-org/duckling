#pragma once

#include <llvm_helpers/llvm_helpers.hpp>

#include <functional>
#include <memory>
#include <string>

LLVM_INCLUDE_BEGIN()

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>

LLVM_INCLUDE_END()
/**
 * @brief Creates a new empty llvm::Module with the same data layout and target triple as in the
 * master module.
 */
std::unique_ptr<llvm::Module> setupModule(const std::string& module_name, llvm::LLVMContext& ctx);

/**
 * @brief "Exports" all LLVM global values in a module (functions, global vars, metadata etc.)
 * to make them visible to other modules.
 * @note This is necessary for proper linking of the user function module with opfunction modules.
 */
void externalizeAllGlobalValues(llvm::Module& module);

/**
 * @brief Creates a new module that contains cloned definitions from `src` based on `filter` and
 * adds it to `lljit`.
 * @details ValueToValueMapTy indicates which values had already been cloned earlier - it is
 * here just to satisfy LLVM's API.
 */
void cloneAndRegisterModule(
	llvm::Module&                                        src,
	llvm::orc::LLJIT&                                    lljit,
	const std::function<bool(const llvm::GlobalValue*)>& filter,
	llvm::orc::ThreadSafeContext&                        tsctx,
	llvm::ExitOnError&                                   exit_on_err
);
