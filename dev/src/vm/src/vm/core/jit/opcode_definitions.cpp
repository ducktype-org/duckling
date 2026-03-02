#include "opcode_definitions.hpp"

#include "jit_init.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <vm/core/thread/low_program/opcodes.hpp>

#include <cstddef>
#include <cstring>
#include <iostream>

LLVM_INCLUDE_BEGIN()
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
LLVM_INCLUDE_END()

using namespace llvm;
using namespace llvm::orc;

inline constexpr char OPCODES[] = {
#ifdef USE_TAIL_CALLS
	#embed "src/vm/common_tc.bc"
#else
	#embed "src/vm/common_sc.bc"
#endif
};

static std::unique_ptr<LLVMContext>                              g_context;
static std::unique_ptr<Module>                                   g_module;
static std::unordered_map<vm::low::MicroOpcode, llvm::Function*> func_map;
static std::unordered_map<vm::low::MicroOpcode, std::string>     lfunc_sym_map;
static std::unique_ptr<LLJIT>                                    lljit_instance;
static ExitOnError                                               exit_on_err;

namespace {
	std::string extractFunctionName(const std::string& full) {
		size_t paren_pos = full.find('(');
		if (paren_pos == std::string::npos) paren_pos = full.length();

		size_t colons_pos = full.rfind("::", paren_pos);
		size_t start      = (colons_pos == std::string::npos) ? 0 : colons_pos + 2;

		return full.substr(start, paren_pos - start);
	}

	vm::low::MicroOpcode getOpcode(const std::string& func_name) {
		for (size_t i = 0; i < sizeof(vm::low::OPCODE_NAMES) / sizeof(vm::low::OPCODE_NAMES[0]);
		     ++i) {
			if (strcmp(func_name.c_str(), vm::low::OPCODE_NAMES[i]) == 0)
				return static_cast<vm::low::MicroOpcode>(i);
		}
		CORE_PANIC("Function name does not correspond to any MicroOpcode", func_name);
	}
}

// Pewnie powinien przyjmowac context w argumencie, ale na razie ta funkcje idzie do api i ma byc
// niezalenza od llvm
void llvmInit() {
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();

	if (g_context) return;  // already initialized
	g_context = std::make_unique<LLVMContext>();

	lljit_instance = exit_on_err(LLJITBuilder().create());

	auto& jd = lljit_instance->getMainJITDylib();
	jd.addGenerator(cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
		lljit_instance->getDataLayout().getGlobalPrefix()
	)));

	// Load embedded BC into module
	auto buffer = MemoryBuffer::getMemBuffer(StringRef(OPCODES, sizeof(OPCODES)), "", false);

	auto mod_or_err = parseBitcodeFile(buffer->getMemBufferRef(), *g_context);
	if (!mod_or_err) llvm::report_fatal_error("Aborting due to parse error");

	g_module = std::move(*mod_or_err);

	for (auto& F: g_module->functions()) {
		if (!F.isDeclaration()) {
			//  Here I assume that demangling works for opcodes. Maybe use itaniumDemangle?
			auto demangled = llvm::demangle(F.getName().str());
			// if (demangled.starts_with("vm::OpFuns::op_debug")) {
			// 	std::cerr << "WHAT\n";
			// }
			// One opcode doesn't have op prefix but it is marked to be deleted.
			if (demangled.starts_with("vm::OpFuns::op_")
			    and !demangled.starts_with("vm::OpFuns::op_debug")) {
				auto name   = extractFunctionName(demangled);
				name        = name.substr(3);  // delete op_
				auto opcode = getOpcode(name);
				if (func_map.contains(opcode)) {
					// CORE_PANIC("Duplicate opcode function name: ", name);
				} else {
					// Sanity check, that instructions sizes make sense.
					std::cerr << "found " << name << " number: " << static_cast<uint64_t>(opcode)
							  << " with " << F.getInstructionCount() << " instructions\n";
					func_map[opcode] = &F;
					lfunc_sym_map[opcode] = F.getName().str();
				}
			}
		}
	}
	CORE_ASSERT(!func_map.empty(), "Opfuns not found!");

	exit_on_err(lljit_instance->addIRModule(ThreadSafeModule(std::move(g_module), std::move(g_context))));
	std::cerr << "JIT initialised successfully\n";
}

llvm::Function* llvmGetFun(const vm::low::MicroOpcode& fun) {
	CORE_ASSERT(func_map.contains(fun), "Opcode function not found in LLVM module");
	return func_map.at(fun);
}

std::string llvmGetFunSym(const vm::low::MicroOpcode& fun) {
	CORE_ASSERT(func_map.contains(fun), "LLVM opcode function symbol not found");
	return lfunc_sym_map.at(fun);
}

llvm::orc::LLJIT* llvmGetLljit() { return lljit_instance.get(); }
