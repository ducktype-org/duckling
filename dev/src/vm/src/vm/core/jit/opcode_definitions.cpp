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

inline constexpr char opcodes[] = {
#ifdef USE_TAIL_CALLS
	#embed "src/vm/common_tc.bc"
#else
	#embed "src/vm/common_sc.bc"
#endif
};

static std::unique_ptr<LLVMContext>                              gContext;
static std::unique_ptr<Module>                                   gModule;
static std::unordered_map<vm::low::MicroOpcode, llvm::Function*> FuncMap;
static std::unique_ptr<LLJIT>                                    lljitInstance;
static ExitOnError                                               ExitOnErr;

namespace {
	std::string extract_function_name(const std::string& full) {
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

	if (gContext) return;  // already initialized
	gContext = std::make_unique<LLVMContext>();

	lljitInstance = ExitOnErr(LLJITBuilder().create());

	auto& jd = lljitInstance->getMainJITDylib();
	jd.addGenerator(cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
		lljitInstance->getDataLayout().getGlobalPrefix()
	)));

	// Load embedded BC into module
	auto buffer = MemoryBuffer::getMemBuffer(StringRef(opcodes, sizeof(opcodes)), "", false);

	auto modOrErr = parseBitcodeFile(buffer->getMemBufferRef(), *gContext);
	if (!modOrErr) llvm::report_fatal_error("Aborting due to parse error");

	gModule = std::move(*modOrErr);

	for (auto& F: gModule->functions()) {
		if (!F.isDeclaration()) {
			//  Here I assume that demangling works for opcodes. Maybe use itaniumDemangle?
			auto demangled = llvm::demangle(F.getName().str());
			// if (demangled.starts_with("vm::OpFuns::op_debug")) {
			// 	std::cerr << "WHAT\n";
			// }
			// One opcode doesn't have op prefix but it is marked to be deleted.
			if (demangled.starts_with("vm::OpFuns::op_")
			    and !demangled.starts_with("vm::OpFuns::op_debug")) {
				auto name   = extract_function_name(demangled);
				name        = name.substr(3);  // delete op_
				auto opcode = getOpcode(name);
				if (FuncMap.contains(opcode)) {
					// CORE_PANIC("Duplicate opcode function name: ", name);
				} else {
					// Sanity check, that instructions sizes make sense.
					std::cerr << "found " << name << " number: " << static_cast<uint64_t>(opcode)
							  << " with " << F.getInstructionCount() << " instructions\n";
					FuncMap[opcode] = &F;
				}
			}
		}
	}
	CORE_ASSERT(!FuncMap.empty(), "Opfuns not found!");

	ExitOnErr(lljitInstance->addIRModule(ThreadSafeModule(std::move(gModule), std::move(gContext))));
	std::cerr << "JIT initialised successfully\n";
}

llvm::Function* llvm_get_fun(const vm::low::MicroOpcode& fun) {
	CORE_ASSERT(FuncMap.contains(fun), "Opcode function not found in LLVM module");
	return FuncMap.at(fun);
}

llvm::orc::LLJIT* llvm_get_lljit() { return lljitInstance.get(); }
