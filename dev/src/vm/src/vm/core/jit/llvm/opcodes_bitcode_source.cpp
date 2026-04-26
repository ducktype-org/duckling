#include "opcodes_bitcode_source.hpp"

#include "absolute_symbols.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <vm/core/safe/low_program/opcodes.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <ranges>
#include <tuple>
#include <unordered_set>

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

using namespace llvm;
using namespace llvm::orc;

LLVM_INCLUDE_END()
/**
 * @brief We can't use std::to_array, because it doesn't compile.
 * If used in lambda uses infinite memory (clang bug).
 * Embed gives raw bytes, which can't be assigned directly to std::array
 */
// NOLINTBEGIN
PUSH_DIAGNOSTIC ALLOW_EXTENSIONS inline constexpr char OPCODES[] = {
#if __has_embed("common_sc.bc")
	#embed "common_sc.bc"
#else
	0
#endif
};
POP_DIAGNOSTIC
// NOLINTEND

std::unique_ptr<Module> parseOpcodesBitcode(LLVMContext& context) {

	// std::cout<<OPCODES.size()<<std::endl;
	//  Load embedded BC into module
	auto buffer = MemoryBuffer::getMemBuffer(
		StringRef(static_cast<const char*>(OPCODES), sizeof(OPCODES)), "", false
	);

	auto mod_or_err = parseBitcodeFile(buffer->getMemBufferRef(), context);
	if (!mod_or_err) llvm::report_fatal_error("Aborting due to parse error");

	auto g_module = std::move(*mod_or_err);

	CORE_ASSERT(g_module->isMaterialized(), "Opfuns module not fully materialized!");

	return g_module;
}
