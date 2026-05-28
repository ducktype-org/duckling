#pragma once

#include <llvm_helpers/llvm_helpers.hpp>

#include <base/except/exceptions.hpp>

#include <filesystem>

LLVM_INCLUDE_BEGIN()
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
LLVM_INCLUDE_END()

namespace pg {
	class LlvmProgramModule final {
	public:
		LlvmProgramModule();

		void inCtxDo(const std::function<void(llvm::LLVMContext&, llvm::Module&)>& func);

		void buildMainReturningZero();

		void verify() const;

		void emitObjectFile(const std::filesystem::path& output_path);

	private:
		void initializeTargetMachine();

		llvm::LLVMContext                    context;
		std::unique_ptr<llvm::Module>        module;
		std::unique_ptr<llvm::TargetMachine> target_machine;
	};
}
