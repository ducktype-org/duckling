#include "opcodes_bitcode_source.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <vm/core/safe/low_program/opcodes.hpp>

#include <array>
#include <cstddef>
#include <cstring>

LLVM_INCLUDE_BEGIN()
#include <llvm/ADT/StringRef.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
LLVM_INCLUDE_END()

using namespace llvm;

/**
 * @brief We can't use std::to_array, because it doesn't compile.
 * If used in lambda uses infinite memory (clang bug).
 * Embed gives raw bytes, which can't be assigned directly to std::array
 */
// NOLINTBEGIN
PUSH_DIAGNOSTIC
ALLOW_EXTENSIONS
inline constexpr char OPCODES[] = {
// Linter doesn't actually build common_sc.bc so it would be unavailable.
#if __has_embed("common_sc.bc")
	#embed "common_sc.bc"
#else
	0
#endif
};
POP_DIAGNOSTIC

// NOLINTEND

std::unique_ptr<Module> parseOpcodesBitcode(LLVMContext& context) {
	//  Load embedded BC into module
	auto buffer = MemoryBuffer::getMemBuffer(
		StringRef(static_cast<const char*>(OPCODES), sizeof(OPCODES)), "", false
	);

	auto mod_or_err = parseBitcodeFile(buffer->getMemBufferRef(), context);
	if (!mod_or_err) llvm::report_fatal_error("Aborting due to parse error");

	auto module = std::move(*mod_or_err);

	// llvm.used markers only protect globals from DCE during the offline bitcode optimization;
	// left in, they surface as unresolvable symbols when the cloned modules are JIT-linked.
	for (auto name: { "llvm.used", "llvm.compiler.used" })
		if (auto* used = module->getGlobalVariable(name, /*AllowInternal=*/true))
			used->eraseFromParent();

	CORE_ASSERT(module->isMaterialized(), "Opfuns module not fully materialized!");

	return module;
}
