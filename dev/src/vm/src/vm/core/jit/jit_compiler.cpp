/* #include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
LLVM_INCLUDE_END()

#include <memory>

using namespace llvm;
using namespace llvm::orc;

std::unique_ptr<LLJIT> jit;

class DVMJit {
    void initJIT() {
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();

        auto jit_ex = LLJITBuilder().create();
        jit = std::move(*jit_ex);
    }

    void setupDynamicLookup(llvm::orc::LLJIT &jit) {
        auto &main_jd = jit.getMainJITDylib();
        auto const& data_layout = jit.getDataLayout();

        auto search_generator =
cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
            data_layout.getGlobalPrefix()
        ));

        main_jd.addGenerator(std::move(search_generator));
    }
};*/
#ifdef ENABLE_JIT
	#include "jit_compiler.hpp"

	#include "opcode_definitions.hpp"

	#include <llvm_helpers/llvm_helpers.hpp>

	#include <vm/core/thread/low_program/instruction.hpp>

	#include <iostream>

LLVM_INCLUDE_BEGIN()

	#include <llvm/ExecutionEngine/Orc/LLJIT.h>
	#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
	#include <llvm/IR/DerivedTypes.h>
	#include <llvm/IR/Function.h>
	#include <llvm/IR/IRBuilder.h>
	#include <llvm/IR/LLVMContext.h>
	#include <llvm/IR/Module.h>
	#include <llvm/IR/Type.h>
	#include <llvm/IR/Verifier.h>

LLVM_INCLUDE_END()

vm::JitOpFun* compileJit(const vm::low::LowFuncData& func_data) {
	auto  lljit_ptr = llvmGetLljit();
	auto& lljit     = *lljit_ptr;

	auto               ctx = std::make_unique<llvm::LLVMContext>();
	llvm::LLVMContext& C   = *ctx;
	auto new_module        = std::make_unique<llvm::Module>(base::toString(func_data.name), C);

	llvm::Type* voidTy = llvm::Type::getVoidTy(C);

	llvm::StructType* miTy       = llvm::StructType::create(C, "vm::MicroInstruction");
	llvm::StructType* frameTy    = llvm::StructType::create(C, "vm::Frame");
	llvm::StructType* vmThreadTy = llvm::StructType::create(C, "vm::VMThread");

	llvm::PointerType* miPtrPtrTy
		= llvm::PointerType::getUnqual(llvm::PointerType::getUnqual(miTy));
	llvm::PointerType* bytePtrPtrTy
		= llvm::PointerType::getUnqual(llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(C)));
	llvm::PointerType* framePtrPtrTy
		= llvm::PointerType::getUnqual(llvm::PointerType::getUnqual(frameTy));
	llvm::PointerType* vmThreadPtrTy = llvm::PointerType::getUnqual(vmThreadTy);

	llvm::FunctionType* opFunTy = llvm::FunctionType::get(
		voidTy, { miPtrPtrTy, bytePtrPtrTy, framePtrPtrTy, vmThreadPtrTy }, false
	);

	llvm::Function* user_func_wrapper = llvm::Function::Create(
		opFunTy, llvm::Function::ExternalLinkage, base::toString(func_data.name), new_module.get()
	);

	llvm::BasicBlock* entry = llvm::BasicBlock::Create(C, "entry", user_func_wrapper);
	llvm::IRBuilder<> B(entry);

	auto         argIt         = user_func_wrapper->arg_begin();
	llvm::Value* v_instr       = &*argIt++;
	llvm::Value* v_local_stack = &*argIt++;
	llvm::Value* v_frame       = &*argIt++;
	llvm::Value* v_thread      = &*argIt++;
	v_instr->setName("instr");
	v_local_stack->setName("local_stack");
	v_frame->setName("frame");
	v_thread->setName("thread");

	for (const vm::MicroInstruction& mi: func_data.bc) {
		auto num = static_cast<uint64_t>(vm::getInstructionOpcode(mi));
		llvm::Function* opfun      = llvmGetFun(vm::getInstructionOpcode(mi));
		std::string     opfun_name = opfun->getName().str();

		llvm::Function* callee = new_module->getFunction(opfun_name);
		if (!callee) {
			callee = llvm::Function::Create(
				opFunTy, llvm::Function::ExternalLinkage, opfun_name, new_module.get()
			);
		}

		B.CreateCall(opFunTy, callee, { v_instr, v_local_stack, v_frame, v_thread });
	}

	B.CreateRetVoid();

	llvm::orc::ThreadSafeModule tsm(std::move(new_module), std::move(ctx));
	if (auto err = lljit.addIRModule(std::move(tsm)))
		llvm::logAllUnhandledErrors(std::move(err), llvm::errs(), "Error adding module to JIT: ");
	auto addr_or_err = lljit.lookup(base::toString(func_data.name));
	if (!addr_or_err) {
		llvm::handleAllErrors(addr_or_err.takeError(), [&](const llvm::ErrorInfoBase& EIB) {
			llvm::errs() << "JIT lookup failed: " << EIB.message() << '\n';
		});
		return nullptr;
	}

	llvm::orc::ExecutorAddr addr = *addr_or_err;

	auto compiled_fn = addr.toPtr<vm::JitOpFun>();

	return compiled_fn;
}

#endif
