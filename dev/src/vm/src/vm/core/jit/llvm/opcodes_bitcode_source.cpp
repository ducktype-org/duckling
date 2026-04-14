/**
 * @file opcodes_bitcode_source.cpp
 * @note Tis file does not depend on execution style.
 */

#ifdef ENABLE_JIT  // @TODO: #2312 Remove the #ifdef
	#include "opcodes_bitcode_source.hpp"
	#include "absolute_symbols.hpp"

	#include "llvm_init.hpp"

	#include <llvm_helpers/llvm_helpers.hpp>

	#include <vm/core/thread/low_program/opcodes.hpp>

	#include <cstddef>
	#include <cstring>
	#include <array>

LLVM_INCLUDE_BEGIN()
	#include <llvm/Bitcode/BitcodeReader.h>
	#include <llvm/Demangle/Demangle.h>
	#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
	#include <llvm/ExecutionEngine/Orc/LLJIT.h>
	#include <llvm/ExecutionEngine/Orc/Core.h>
	#include <llvm/ExecutionEngine/JITSymbol.h>
	#include <llvm/IR/Function.h>
	#include <llvm/IR/LLVMContext.h>
	#include <llvm/IR/Module.h>
	#include <llvm/Support/Error.h>
	#include <llvm/Support/MemoryBuffer.h>
	#include <llvm/Support/TargetSelect.h>
LLVM_INCLUDE_END()

using namespace llvm;
using namespace llvm::orc;

/**
 * @brief We can't use std::to_array, because it doesn't compile.
 * If used in lambda uses infinite memory (clang bug).
 * Embed gives raw bytes, which can't be assigned directly to std::array
 */
// NOLINTBEGIN
PUSH_DIAGNOSTIC ALLOW_EXTENSIONS inline constexpr char OPCODES[] = {
	#embed "common_sc.bc"
};
POP_DIAGNOSTIC
// NOLINTEND

/// @brief Context of llvmInit.
/// @note We need to use ThreadSafeContext instead of LLVMContext to be able to use a single shared context for the JIT instance.
static std::unique_ptr<ThreadSafeContext> g_context;

/// @brief LLVM module containing the parsed microinstruction bitcode.
static std::unique_ptr<Module> g_module;

/// @brief Active LLjit instance.
static std::unique_ptr<LLJIT> lljit_instance;

/// @brief LLVM helper object used for errors.
static ExitOnError exit_on_err;

/// @brief For each MicroOpcode stores calculated llvm::Function*.
static std::unordered_map<vm::low::MicroOpcode, llvm::Function*> func_map;

/// @brief For each MicroOpcode stores the name of its corresponding llvm::Function*.
static std::unordered_map<vm::low::MicroOpcode, std::string> lfunc_name_map;

namespace {
	/**
	 * @brief Extracts function name from its mangled version. It should be string
	 * between last "::" (if present) and first "(".
	 */
	std::string extractFunctionName(const std::string& full) {
		size_t paren_pos = full.find('(');
		if (paren_pos == std::string::npos) paren_pos = full.length();

		size_t colons_pos = full.rfind("::", paren_pos);
		size_t start      = (colons_pos == std::string::npos) ? 0 : colons_pos + 2;

		return full.substr(start, paren_pos - start);
	}

	/**
	 * @brief For microinstruction name, returns corresponding MicroOpcode.
	 */
	vm::low::MicroOpcode getOpcode(const std::string& func_name) {
		for (size_t i = 0; i < vm::low::OPCODE_NAMES.size(); ++i)
			if (func_name == vm::low::OPCODE_NAMES[i]) return static_cast<vm::low::MicroOpcode>(i);
		CORE_PANIC("Function name does not correspond to any MicroOpcode", func_name);
	}

	constexpr auto constructNonExecOpcodeArray() {
		std::array<vm::low::MicroOpcode, vm::low::nonExecutableMicroInstrCount()> non_exec_opcodes;
		size_t j = 0;
		for (size_t i = 0; i < vm::low::OPCODE_NAMES.size(); ++i) {
			if (vm::low::OPCODE_NAMES[i].starts_with("ext_")) {
				non_exec_opcodes[j++] = static_cast<vm::low::MicroOpcode>(i);
			}
		}
		return non_exec_opcodes;
	}
}

static constexpr std::array<vm::low::MicroOpcode, vm::low::nonExecutableMicroInstrCount()> NON_EXEC_OPCODES = constructNonExecOpcodeArray();

void llvmInit() {
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();

	if (g_context) return;  // already initialized
	auto initial_context = std::make_unique<LLVMContext>();

	lljit_instance = exit_on_err(LLJITBuilder().create());

	auto& jd = lljit_instance->getMainJITDylib();
	jd.addGenerator(cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
		lljit_instance->getDataLayout().getGlobalPrefix()
	)));

	registerAbsoluteJITSymbols(*lljit_instance);

	// Load embedded BC into module
	auto buffer = MemoryBuffer::getMemBuffer(StringRef(OPCODES, sizeof(OPCODES)), "", false);

	auto mod_or_err = parseBitcodeFile(buffer->getMemBufferRef(), *initial_context);
	if (!mod_or_err) llvm::report_fatal_error("Aborting due to parse error");

	g_module = std::move(*mod_or_err);

	for (auto& F: g_module->functions()) {
		if (!F.isDeclaration()) {
			auto demangled = llvm::demangle(F.getName().str());
			if (demangled.starts_with("vm::OpFuns::op_")
			    and !demangled.starts_with("vm::OpFuns::op_debug")) {
				auto name        = extractFunctionName(demangled);
				name             = name.substr(3);  // delete op_
				auto opcode      = getOpcode(name);
				func_map[opcode] = &F;
				lfunc_name_map[opcode] = F.getName().str();
			}
		}
	}
	CORE_ASSERT(!func_map.empty(), "Opfuns not found!");

	g_context = std::make_unique<ThreadSafeContext>(std::move(initial_context));

	ThreadSafeModule tsm(std::move(g_module), *g_context);

	exit_on_err(lljit_instance->addIRModule(std::move(tsm)));
}

llvm::Function* llvmGetFun(const vm::low::MicroOpcode& fun) {
	CORE_ASSERT(func_map.contains(fun), "Opcode function not found in LLVM module");
	return func_map.at(fun);
}

base::Optional<std::string> llvmGetFunName(const vm::low::MicroOpcode& fun) {
	auto fun_name_iter = lfunc_name_map.find(fun);
	if (fun_name_iter != lfunc_name_map.end()) {
		return fun_name_iter->second;
	}
	return {};
	
}

llvm::orc::ThreadSafeContext* llvmGetTSCtx() { return g_context.get(); }

llvm::orc::LLJIT* llvmGetLljit() { return lljit_instance.get(); }

bool isOpcodeNonExecutable(const vm::low::MicroOpcode& opcode) {
	for (const auto& mo: NON_EXEC_OPCODES) {
		if (mo == opcode) {
			return true;
		}
	}
	return false;
}

#endif
