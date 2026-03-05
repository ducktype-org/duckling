#include "jit_compiler.hpp"

#include "opcodes_bitcode_source.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <vm/core/thread/low_program/instruction.hpp>

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

namespace vm::jit {
	void lowerBasicBlock(
		const low::LowFuncData&      function_to_compile,
		llvm::Module*                module,
		llvm::ArrayRef<llvm::Value*> args,
		llvm::IRBuilder<>&           ir_builder,
		llvm::FunctionType*          opfun_ty
	) {
		for (const vm::MicroInstruction& mi: function_to_compile.bc) {
			llvm::Function* opfun      = llvmGetFun(vm::getInstructionOpcode(mi));
			std::string     opfun_name = opfun->getName().str();

			llvm::Function* callee = module->getFunction(opfun_name);
			if (!callee) {
				callee = llvm::Function::Create(
					opfun_ty, llvm::Function::ExternalLinkage, opfun_name, module
				);
			}

			ir_builder.CreateCall(opfun_ty, callee, args);
		}
	}

	llvm::FunctionType* opFunType(llvm::LLVMContext& ctx) {
		llvm::Type* void_ty = llvm::Type::getVoidTy(ctx);

		llvm::StructType* mi_ty        = llvm::StructType::create(ctx, "vm::MicroInstruction");
		llvm::StructType* frame_ty     = llvm::StructType::create(ctx, "vm::Frame");
		llvm::StructType* vm_thread_ty = llvm::StructType::create(ctx, "vm::VMThread");

		llvm::PointerType* mi_ptr_ptr_ty
			= llvm::PointerType::getUnqual(llvm::PointerType::getUnqual(mi_ty));
		llvm::PointerType* byte_ptr_ptr_ty
			= llvm::PointerType::getUnqual(llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx)));
		llvm::PointerType* frame_ptr_ptr_ty
			= llvm::PointerType::getUnqual(llvm::PointerType::getUnqual(frame_ty));
		llvm::PointerType* vm_thread_ptr_ty = llvm::PointerType::getUnqual(vm_thread_ty);

		llvm::FunctionType* opfun_ty = llvm::FunctionType::get(
			void_ty, { mi_ptr_ptr_ty, byte_ptr_ptr_ty, frame_ptr_ptr_ty, vm_thread_ptr_ty }, false
		);

		return opfun_ty;
	}

	MRef<JitOpFun> compileLLVM(const low::LowFuncData& func_data) {
		auto               ctx_ptr = std::make_unique<llvm::LLVMContext>();
		llvm::LLVMContext& ctx     = *ctx_ptr;

		llvm::FunctionType* opfun_ty = opFunType(ctx);

		auto new_module = std::make_unique<llvm::Module>(base::toString(func_data.name), ctx);
		llvm::Function* user_func_wrapper = llvm::Function::Create(
			opfun_ty,
			llvm::Function::ExternalLinkage,
			base::toString(func_data.name),
			new_module.get()
		);
		auto         arg_it        = user_func_wrapper->arg_begin();
		llvm::Value* v_instr       = &*arg_it++;
		llvm::Value* v_local_stack = &*arg_it++;
		llvm::Value* v_frame       = &*arg_it++;
		llvm::Value* v_thread      = &*arg_it++;
		v_instr->setName("instr");
		v_local_stack->setName("local_stack");
		v_frame->setName("frame");
		v_thread->setName("thread");

		llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx, "entry", user_func_wrapper);
		llvm::IRBuilder<> ir_builder(entry);
		lowerBasicBlock(
			func_data,
			new_module.get(),
			{ v_instr, v_local_stack, v_frame, v_thread },
			ir_builder,
			opfun_ty
		);
		ir_builder.CreateRetVoid();

		auto&                       lljit = *llvmGetLljit();
		llvm::orc::ThreadSafeModule tsm(std::move(new_module), std::move(ctx_ptr));
		if (auto err = lljit.addIRModule(std::move(tsm)))
			llvm::logAllUnhandledErrors(
				std::move(err), llvm::errs(), "Error adding module to JIT: "
			);
		auto addr_or_err = lljit.lookup(base::toString(func_data.name));
		if (!addr_or_err) {
			llvm::handleAllErrors(addr_or_err.takeError(), [&](const llvm::ErrorInfoBase& eib) {
				llvm::errs() << "JIT lookup failed: " << eib.message() << '\n';
			});
			return nullptr;
		}

		llvm::orc::ExecutorAddr addr = *addr_or_err;

		auto compiled_fn = addr.toPtr<vm::jit::JitOpFun>();

		return compiled_fn;
	}
}
