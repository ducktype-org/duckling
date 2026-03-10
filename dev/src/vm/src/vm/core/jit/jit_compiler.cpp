#include "jit_compiler.hpp"

#include <iostream>

#include "block_detection.hpp"

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
		const auto&                  bitcode,
		llvm::Module*                module,
		llvm::ArrayRef<llvm::Value*> args,
		llvm::IRBuilder<>&           ir_builder,
		llvm::FunctionType*          opfun_ty
	) {
		for (const vm::MicroInstruction& mi: bitcode) {
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

	void lowerFunction(
		const low::LowFuncData& function_to_compile,
		llvm::Module*           module,
		llvm::FunctionType*     opfun_ty,
		llvm::LLVMContext&      llvm_ctx
	) {
		llvm::Function* user_func_wrapper = llvm::Function::Create(
			opfun_ty,
			llvm::Function::ExternalLinkage,
			base::toString(function_to_compile.name),
			module
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

		// Get basic block boundaries
		std::vector<usize> block_beginnings = collectBasicBlockBeginnings(function_to_compile);

		// Create LLVM basic blocks for each VM block
		std::vector<llvm::BasicBlock*> llvm_blocks;
		for (usize i = 0; i < block_beginnings.size(); ++i) {
			llvm::BasicBlock* block = llvm::BasicBlock::Create(
				llvm_ctx,
				"block_at_" + std::to_string(i),
				user_func_wrapper
			);
			llvm_blocks.push_back(block);
		}

		auto instr_to_block = [&](usize instr_index) {
			auto it = std::lower_bound(block_beginnings.begin(), block_beginnings.end(), instr_index);
			return it != block_beginnings.end() ? std::distance(block_beginnings.begin(), it) : block_beginnings.size() - 1;
		};

		for (usize block_idx = 0; block_idx < llvm_blocks.size() - 1; ++block_idx) {
			llvm::IRBuilder<> ir_builder(llvm_blocks[block_idx]);
			usize start = block_beginnings[block_idx];
			usize end = block_beginnings[block_idx + 1];

			low::MicroBytecode block_bc(
				function_to_compile.bc.begin() + start,
				function_to_compile.bc.begin() + end
			);

			lowerBasicBlock(
				block_bc,
				module,
				{ v_instr, v_local_stack, v_frame, v_thread },
				ir_builder,
				opfun_ty
			);

			std::cerr << "Lowered block " << block_idx << " with instructions [" << start << ", " << end << ")\n";

			// Determine the kind of terminator needed for the block
			const vm::MicroInstruction& last_instr = function_to_compile.bc[end - 1];
			low::MicroOpcode last_opcode = getInstructionOpcode(last_instr);
			switch (last_opcode) {
				case low::MicroOpcode::jmp_label: {
					usize target_block_idx = instr_to_block(end + last_instr.arg0);
					std::cerr << "Connecting block " << block_idx << " with an unconditional jump to block " << target_block_idx << "\n";
					ir_builder.CreateBr(llvm_blocks[target_block_idx]);
					break;
				}
				case low::MicroOpcode::jmpIf_label:
				case low::MicroOpcode::jmpIfNot_label: {
					if (block_idx + 1 < llvm_blocks.size()) {
						usize target_block_idx = instr_to_block(end + last_instr.arg0);

						// Get pointer to flags field in frame
						llvm::StructType* frame_ty = llvm::StructType::create(llvm_ctx, "vm::Frame");
						llvm::PointerType* frame_ptr_ty = llvm::PointerType::getUnqual(frame_ty);
						llvm::Value* frame_ptr = ir_builder.CreateLoad(frame_ptr_ty, v_frame);
						llvm::Value* flags_ptr = ir_builder.CreateStructGEP(frame_ty, frame_ptr, 0);

						// Access flags.flag (field index 0 in FlagData)
						llvm::StructType* flag_data_ty = llvm::StructType::create(llvm_ctx, "vm::FlagData");
						llvm::Value* flag_ptr = ir_builder.CreateStructGEP(flag_data_ty, flags_ptr, 0);

						// Load the flag value
						llvm::Value* flag_value = ir_builder.CreateLoad(
							ir_builder.getInt1Ty(),
							flag_ptr
						);

						if (last_opcode == low::MicroOpcode::jmpIfNot_label) {
							flag_value = ir_builder.CreateNot(flag_value);
						}

						// Create conditional branch
						ir_builder.CreateCondBr(
							flag_value,
							llvm_blocks[target_block_idx],
							llvm_blocks[block_idx + 1]
						);
					}
					break;
				}
				case low::MicroOpcode::ret:
				case low::MicroOpcode::ret_tailcall_func: {
					ir_builder.CreateRetVoid();
					break;
				}
				default: {
					// For other opcodes, we assume the block falls through to the next one
					if (block_idx + 1 < llvm_blocks.size()) {
						ir_builder.CreateBr(llvm_blocks[block_idx + 1]);
					} else {
						ir_builder.CreateRetVoid();
					}
					break;
				}
			}
			std::cerr << "Connected block " << block_idx << " with instructions [" << start << ", " << end << ")\n";
		}
		std::cerr << "Finished lowering function " << base::toString(function_to_compile.name) << "\n";
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

	MRef<JitOpFun> compileLLVM(const low::LowFuncData& function_to_compile) {
		auto               ctx_ptr = std::make_unique<llvm::LLVMContext>();
		llvm::LLVMContext& ctx     = *ctx_ptr;

		llvm::FunctionType* opfun_ty = opFunType(ctx);

		auto new_module
			= std::make_unique<llvm::Module>(base::toString(function_to_compile.name), ctx);

		lowerFunction(function_to_compile, new_module.get(), opfun_ty, ctx);

		auto&                       lljit = *llvmGetLljit();
		std::cerr << "Adding module for function " << base::toString(function_to_compile.name) << " to JIT...\n";

		llvm::orc::ThreadSafeModule tsm(std::move(new_module), std::move(ctx_ptr));
		if (auto err = lljit.addIRModule(std::move(tsm)))
			llvm::logAllUnhandledErrors(
				std::move(err), llvm::errs(), "Error adding module to JIT: "
			);
		std::cerr << "Module added.\n";
		auto addr_or_err = lljit.lookup(base::toString(function_to_compile.name));
		std::cerr << "Looked up JIT-compiled function " << base::toString(function_to_compile.name) << "\n";
		if (!addr_or_err) {
			llvm::handleAllErrors(addr_or_err.takeError(), [&](const llvm::ErrorInfoBase& eib) {
				llvm::errs() << "JIT lookup failed: " << eib.message() << '\n';
			});
			return nullptr;
		}

		llvm::orc::ExecutorAddr addr = *addr_or_err;

		auto compiled_fn = addr.toPtr<vm::jit::JitOpFun>();
		std::cerr << "Successfully compiled function " << base::toString(function_to_compile.name) << "\n";

		return compiled_fn;
	}
}
