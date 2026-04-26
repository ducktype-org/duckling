#pragma once

#include <llvm_helpers/llvm_helpers.hpp>

#include <vm/core/safe/low_program/opcodes.hpp>

// TODO usunac zbedne
LLVM_INCLUDE_BEGIN()
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ValueMapper.h>
LLVM_INCLUDE_END()

using namespace llvm;
using namespace llvm::orc;

struct LlvmData {
	/// @brief Context of llvmInit.
	/// @note We need to use ThreadSafeContext instead of LLVMContext to be able to use a single
	/// shared context for the JIT instance.
	std::unique_ptr<ThreadSafeContext> g_context;

	/**
	 * @brief LLVM master module containing the parsed microinstruction bitcode.
	 * @note Acts as an IR cache for opfun body cloning, to enable interprocedural optimizations.
	 */
	std::unique_ptr<Module> g_module;

	/// @brief Active LLjit instance.
	std::unique_ptr<LLJIT> lljit_instance;

	/// @brief LLVM helper object used for errors.
	ExitOnError exit_on_err;

	/// @brief For each MicroOpcode stores the name of its corresponding llvm::Function*.
	std::unordered_map<vm::low::MicroOpcode, std::string> lfunc_name_map;

	struct LlvmTypes {
		Ref<llvm::StructType>   frame;
		Ref<llvm::StructType>   flag_data;
		Ref<llvm::StructType>   microinstruction;
		Ref<llvm::StructType>   vm_thread;
		Ref<llvm::FunctionType> opfun;
	};

	/// @brief pointers to LLVM types used in opcode function definitions.
	LlvmTypes types;

	/*llvm::StructType*   frame_ty;
	llvm::StructType*   flag_data_ty;
	llvm::StructType*   microinstruction_ty;
	llvm::StructType*   vm_thread_ty;
	llvm::FunctionType* opfun_ty;*/
};

LlvmData& llvmData();

inline base::Optional<std::string> llvmGetFunName(const vm::low::MicroOpcode& fun) {
	auto& llvm_data     = llvmData();
	auto  fun_name_iter = llvm_data.lfunc_name_map.find(fun);
	if (fun_name_iter != llvm_data.lfunc_name_map.end()) return fun_name_iter->second;
	return {};
}
