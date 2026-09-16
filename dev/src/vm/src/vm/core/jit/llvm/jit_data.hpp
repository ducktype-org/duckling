#pragma once

#include <llvm_helpers/llvm_helpers.hpp>

#include <vm/core/safe/low_program/opcodes.hpp>

#include <memory>
#include <string>
#include <unordered_map>

LLVM_INCLUDE_BEGIN()

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>

LLVM_INCLUDE_END()

struct LlvmData final {
	/**
	 * @brief Context of llvmInit.
	 * @note We need to use ThreadSafeContext instead of LLVMContext to be able to use a single
	 * shared context for the JIT instance.
	 */
	std::unique_ptr<llvm::orc::ThreadSafeContext> g_context;

	/**
	 * @brief LLVM master module containing the parsed microinstruction bitcode.
	 * @note Acts as an IR cache for opfun body cloning, to enable interprocedural optimizations.
	 */
	std::unique_ptr<llvm::Module> g_module;

	/**
	 * @brief Active LLjit instance.
	 */
	std::unique_ptr<llvm::orc::LLJIT> lljit_instance;

	/**
	 * @brief LLVM helper object used for errors.
	 */
	llvm::ExitOnError exit_on_err;

	/**
	 * @brief For each MicroOpcode stores the name of its corresponding llvm::Function*.
	 */
	std::unordered_map<vm::low::MicroOpcode, std::string> opcode_name_map;

	struct LlvmTypes final {
		Ref<llvm::StructType>   frame;
		Ref<llvm::StructType>   flag_data;
		Ref<llvm::StructType>   microinstruction;
		Ref<llvm::StructType>   vm_thread;
		Ref<llvm::FunctionType> opfun;
	};

	/**
	 * @brief pointers to LLVM types used in opcode function definitions.
	 */
	LlvmTypes types;

	/**
	 * @brief Returns mangled name of opcode if it is in module.
	 */
	base::Optional<std::string_view> getFunName(const vm::low::MicroOpcode& fun) const;
};

/**
 * @brief Returns LlvmData containing jit constants.
 */
const LlvmData& llvmData();
