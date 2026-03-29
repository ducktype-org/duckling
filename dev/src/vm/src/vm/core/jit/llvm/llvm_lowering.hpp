#pragma once

#include "../block_detection.hpp"
#include "../jit_compiler.hpp"
#include "opcodes_bitcode_source.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <vm/core/thread/low_program/instruction.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>

#include  <base/collections/optional.hpp>

#include <algorithm>
#include <string>
#include <vector>

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
	struct LLVMBuilder {
		llvm::LLVMContext& llvm_ctx;
		llvm::Module*      module;

		llvm::StructType*   frame_ty;
		llvm::PointerType*  frame_ptr_ty;
		llvm::StructType*   flag_data_ty;
		llvm::FunctionType* opfun_ty;
		llvm::Function*     user_func_wrapper;

		llvm::Value* instr_arg;
		llvm::Value* locals_arg;
		llvm::Value* frame_arg;
		llvm::Value* thread_arg;

		std::vector<usize>             block_beginnings;
		std::vector<llvm::BasicBlock*> llvm_blocks;

		LLVMBuilder(llvm::Module* module, llvm::LLVMContext& ctx):
			  llvm_ctx(ctx),
			  module(module),
			  frame_ty{ llvm::StructType::getTypeByName(llvm_ctx, "struct.vm::Frame") },
			  frame_ptr_ty{ llvm::PointerType::getUnqual(frame_ty) },
			  flag_data_ty{ llvm::StructType::getTypeByName(llvm_ctx, "struct.vm::FlagData") } {
			if (!flag_data_ty) {
				// Define FlagData struct type if it hasn't been defined yet
				flag_data_ty = llvm::StructType::create(llvm_ctx, "struct.vm::FlagData");
				flag_data_ty->setBody(
					{
						llvm::IntegerType::get(llvm_ctx, 1)  // bool flag
					},
					/*isPacked=*/false
				);
			}

			llvm::Type*       void_ty = llvm::Type::getVoidTy(llvm_ctx);
			llvm::StructType* mi_ty   = llvm::StructType::create(llvm_ctx, "vm::MicroInstruction");
			llvm::StructType* vm_thread_ty = llvm::StructType::create(llvm_ctx, "vm::VMThread");

			llvm::PointerType* mi_ptr_ptr_ty
				= llvm::PointerType::getUnqual(llvm::PointerType::getUnqual(mi_ty));
			llvm::PointerType* byte_ptr_ptr_ty = llvm::PointerType::getUnqual(
				llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(llvm_ctx))
			);

			llvm::PointerType* frame_ptr_ptr_ty = llvm::PointerType::getUnqual(frame_ptr_ty);
			llvm::PointerType* vm_thread_ptr_ty = llvm::PointerType::getUnqual(vm_thread_ty);

			opfun_ty = llvm::FunctionType::get(
				void_ty,
				{ mi_ptr_ptr_ty, byte_ptr_ptr_ty, frame_ptr_ptr_ty, vm_thread_ptr_ty },
				false
			);

			user_func_wrapper = llvm::Function::Create(
				opfun_ty, llvm::Function::ExternalLinkage, module->getName(), module
			);

			auto arg_it = user_func_wrapper->arg_begin();
			instr_arg   = &*arg_it++;
			locals_arg  = &*arg_it++;
			frame_arg   = &*arg_it++;
			thread_arg  = &*arg_it++;

			instr_arg->setName("instr");
			locals_arg->setName("local_stack");
			frame_arg->setName("frame");
			thread_arg->setName("thread");
		}

		llvm::Function* getOrCreateOpcodeFunction(std::string_view opfun_name) {
			llvm::Function* callee = module->getFunction(opfun_name.data());
			if (!callee) {
				callee = llvm::Function::Create(
					opfun_ty, llvm::Function::ExternalLinkage, opfun_name, module
				);
			}

			return callee;
		}

		void lowerBasicBlock(
			const low::LowFuncData& function_to_compile,
			llvm::IRBuilder<>&      ir_builder,
			usize                   start,
			usize                   end
		) {
			for (usize instr_idx = start; instr_idx < end; ++instr_idx) {
				const vm::MicroInstruction& mi = function_to_compile.bc[instr_idx];
				std::string opfun_name;
				auto opcode = vm::getInstructionOpcode(mi);

				if (isOpcodeNonExecutable(opcode)) {
					continue;
				}
				
				// TODO: This only works for switch-case; generalise it so it works with both execution modes.
				match_optional(llvmGetFunName(opcode)) {
					opt_some(op_name) {
						opfun_name = op_name;
					}
					opt_none {
						opfun_name = vm::low::OPCODE_NAMES.at(static_cast<u64>(opcode));
					}
				}

				ir_builder.CreateCall(
					opfun_ty,
					getOrCreateOpcodeFunction(opfun_name),
					{ instr_arg, locals_arg, frame_arg, thread_arg }
				);
			}
		}

		[[nodiscard]] usize instrToBlock(usize instr_index) const {
			auto it = std::ranges::lower_bound(block_beginnings, instr_index);
			if (it == block_beginnings.end()) return llvm_blocks.size() - 1;

			usize block_idx = std::distance(block_beginnings.begin(), it);
			if (block_idx == llvm_blocks.size()) return llvm_blocks.size() - 1;

			return block_idx;
		}

		void lowerConditionalJump(
			const vm::MicroInstruction& last_instr,
			low::MicroOpcode            last_opcode,
			usize                       end,
			usize                       block_idx,
			llvm::IRBuilder<>&          ir_builder
		) {
			CORE_ASSERT(frame_ty, "Frame struct type should be defined in the module");
			CORE_ASSERT(!flag_data_ty->isOpaque(), "FlagData struct type should be defined by now");

			usize target_block_idx  = instrToBlock(end + last_instr.arg0);
			u32   flags_field_index = 0;

			llvm::Value* frame_ptr = ir_builder.CreateLoad(frame_ptr_ty, frame_arg);
			llvm::Value* flags_ptr
				= ir_builder.CreateStructGEP(frame_ty, frame_ptr, flags_field_index);

			llvm::Value* flag_ptr   = ir_builder.CreateStructGEP(flag_data_ty, flags_ptr, 0);
			llvm::Value* flag_value = ir_builder.CreateLoad(ir_builder.getInt1Ty(), flag_ptr);

			if (last_opcode == low::MicroOpcode::jmpIfNot_label)
				flag_value = ir_builder.CreateNot(flag_value);

			ir_builder.CreateCondBr(
				flag_value, llvm_blocks[target_block_idx], llvm_blocks[block_idx + 1]
			);
		}

		void lowerBlock(const low::LowFuncData& function_to_compile, usize block_idx) {
			usize start = block_beginnings[block_idx];
			usize end   = block_beginnings[block_idx + 1];

			llvm::IRBuilder<> ir_builder(llvm_blocks[block_idx]);
			lowerBasicBlock(function_to_compile, ir_builder, start, end);

			const vm::MicroInstruction& last_instr  = function_to_compile.bc[end - 1];
			low::MicroOpcode            last_opcode = getInstructionOpcode(last_instr);

			switch (last_opcode) {
			case low::MicroOpcode::jmp_label: {
				usize target_block_idx = instrToBlock(end + last_instr.arg0);
				ir_builder.CreateBr(llvm_blocks[target_block_idx]);
				break;
			}
			case low::MicroOpcode::jmpIf_label:
			case low::MicroOpcode::jmpIfNot_label: {
				if (block_idx + 1 < llvm_blocks.size())
					lowerConditionalJump(last_instr, last_opcode, end, block_idx, ir_builder);
				break;
			}
			case low::MicroOpcode::ret:
			case low::MicroOpcode::ret_tailcall_func: {
				ir_builder.CreateRetVoid();
				break;
			}
			default: {
				if (block_idx + 1 < llvm_blocks.size())
					ir_builder.CreateBr(llvm_blocks[block_idx + 1]);
				else
					ir_builder.CreateRetVoid();
				break;
			}
			}
		}

		void lowerFunction(const low::LowFuncData& function_to_compile) {
			// Get basic block boundaries
			block_beginnings = collectBasicBlockBeginnings(function_to_compile);

			// Create LLVM basic blocks for each VM block
			for (usize block_idx = 0; block_idx < block_beginnings.size(); ++block_idx) {
				llvm::BasicBlock* block = llvm::BasicBlock::Create(
					llvm_ctx, "block_at_" + std::to_string(block_idx), user_func_wrapper
				);
				llvm_blocks.push_back(block);
			}

			if (function_to_compile.bc.empty()) {
				llvm::IRBuilder<> ir_builder(
					llvm::BasicBlock::Create(llvm_ctx, "entry", user_func_wrapper)
				);
				ir_builder.CreateRetVoid();
				return;
			}

			for (usize block_idx = 0; block_idx < llvm_blocks.size(); ++block_idx)
				lowerBlock(function_to_compile, block_idx);
		}
	};
}  // namespace vm::jit
