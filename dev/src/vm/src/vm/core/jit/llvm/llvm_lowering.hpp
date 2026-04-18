#pragma once

#include "../cf_analyzer.hpp"
#include "../jit_compiler.hpp"
#include "opcodes_bitcode_source.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <base/collections/optional.hpp>

#include <vm/core/thread/low_program/instruction.hpp>

#include <algorithm>
#include <string>
#include <unordered_set>
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
#include <llvm/Linker/Linker.h>
#include <llvm/Transforms/Utils/Cloning.h>

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

		cf::ControlFlowGraph           cfg;
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
			const low::LowFuncData&          function_to_compile,
			llvm::IRBuilder<>&               ir_builder,
			usize                            start,
			usize                            end,
			std::unordered_set<std::string>& used_opfuns
		) {
			for (usize instr_idx = start; instr_idx < end; ++instr_idx) {
				const vm::MicroInstruction& mi     = function_to_compile.bc.at(instr_idx);
				auto                        opcode = vm::getInstructionOpcode(mi);
				switch (opcode) {
				case vm::low::MicroOpcode::jit_call_entrypoint:
				case vm::low::MicroOpcode::call_func:
				case vm::low::MicroOpcode::virtual_call_lptr_method:{
					ir_builder.CreateCall(
						opfun_ty,
						getOrCreateOpcodeFunction("trampoline"),
						{ instr_arg, locals_arg, frame_arg, thread_arg }
					);
				} break;
				default:
					if (isOpcodeNonExecutable(opcode)) continue;
					std::string opfun_name;
					match_optional(llvmGetFunName(opcode)) {
						opt_some(op_name) {
							opfun_name = op_name;
							used_opfuns.insert(opfun_name);
						}
						opt_none { opfun_name = low::OPCODE_NAMES.at(static_cast<u64>(opcode)); }
					}
					ir_builder.CreateCall(
						opfun_ty,
						getOrCreateOpcodeFunction(opfun_name),
						{ instr_arg, locals_arg, frame_arg, thread_arg }
					);
				}
			}
		}

		void lowerConditionalJump(const cf::BasicBlock& block, llvm::IRBuilder<>& ir_builder) {
			CORE_ASSERT(frame_ty, "Frame struct type should be defined in the module");
			CORE_ASSERT(!flag_data_ty->isOpaque(), "FlagData struct type should be defined by now");

			u32 flags_field_index = 0;

			llvm::Value* frame_ptr = ir_builder.CreateLoad(frame_ptr_ty, frame_arg);
			llvm::Value* flags_ptr
				= ir_builder.CreateStructGEP(frame_ty, frame_ptr, flags_field_index);

			llvm::Value* flag_ptr   = ir_builder.CreateStructGEP(flag_data_ty, flags_ptr, 0);
			llvm::Value* flag_value = ir_builder.CreateLoad(ir_builder.getInt1Ty(), flag_ptr);

			if (block.edgeKind() == cf::OutEdges::Kind::JmpIfNot)
				flag_value = ir_builder.CreateNot(flag_value);

			ir_builder.CreateCondBr(
				flag_value, llvm_blocks[block.successTarget()], llvm_blocks[block.failTarget()]
			);
		}

		void lowerBlock(
			const low::LowFuncData&          function_to_compile,
			const cf::BasicBlock&            block,
			std::unordered_set<std::string>& used_opfuns
		) {
			llvm::IRBuilder<> ir_builder(llvm_blocks[block.id]);
			lowerBasicBlock(function_to_compile, ir_builder, block.start, block.end, used_opfuns);

			switch (block.edgeKind()) {
			case cf::OutEdges::Kind::Default: {
				ir_builder.CreateBr(llvm_blocks[block.next()]);
				break;
			}
			case cf::OutEdges::Kind::JmpIf:
			case cf::OutEdges::Kind::JmpIfNot: {
				lowerConditionalJump(block, ir_builder);
				break;
			}
			case cf::OutEdges::Kind::End:
			default: {
				ir_builder.CreateRetVoid();
				break;
			}
			}
		}


		// Debug function to print IR - remove.
		void printModuleIR(llvm::Module& M) {
			M.print(llvm::errs(), nullptr);
		}

		void lowerFunction(const low::LowFuncData& function_to_compile) {
			cf::ControlFlowAnalyzer cf_analyzer{};
			cfg = cf_analyzer.controlFlowGraph(function_to_compile);

			// Create LLVM basic blocks for each VM block
			for (usize block_idx = 0; block_idx < cfg.size(); ++block_idx) {
				llvm::BasicBlock* block = llvm::BasicBlock::Create(
					llvm_ctx, "block_" + std::to_string(block_idx), user_func_wrapper
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

			// Helper structure to track called opfuns.
			std::unordered_set<std::string> used_opfuns;

			for (usize block_idx = 0; block_idx < llvm_blocks.size(); ++block_idx)
				lowerBlock(
					function_to_compile, cfg.getBlock(block_idx), used_opfuns
				);  // lowerBlock(function_to_compile, block_idx, used_opfuns);

			auto used_opfuns_filter = [&](const llvm::GlobalValue* GV) -> bool {
				return used_opfuns.contains(GV->getName().str());
			};

			// At this point, `used_opfuns` contains all used opfunctions' names, so we can clone
			// the appropriate definitions and link them against the user function module.

			llvm::ValueToValueMapTy       vmap;
			std::unique_ptr<llvm::Module> used_opfuns_module
				= llvm::CloneModule(*llvmGetMasterModule(), vmap, used_opfuns_filter);
			llvm::Linker::linkModules(
				*module, std::move(used_opfuns_module), llvm::Linker::Flags::LinkOnlyNeeded
			);
			for (auto& F: module->functions()) {
				if (!F.isDeclaration()
				    && F.getName().str() != base::toString(function_to_compile.name)) {
					// Set cloned opfuns' linkage to AvailableExternally to avoid double compilation
					// and symbol conflicts.
					F.setLinkage(llvm::GlobalValue::AvailableExternallyLinkage);
				}
			}
		}
	};
}  // namespace vm::jit
