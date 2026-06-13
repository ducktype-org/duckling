#pragma once

#include "../jit_compiler.hpp"
#include "jit_data.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

#include <base/collections/optional.hpp>

#include <vm/core/safe/low_program/cfg/cf_graph.hpp>
#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/opcode_functions/opcodes_functions.hpp>

#include <algorithm>
#include <cstdint>
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

		llvm::Function* user_func_wrapper = nullptr;

		llvm::Value* instr_arg;
		llvm::Value* locals_arg;
		llvm::Value* frame_arg;
		llvm::Value* thread_arg;

		std::vector<llvm::BasicBlock*> llvm_blocks;

		LLVMBuilder(llvm::Module* module, llvm::LLVMContext& ctx): llvm_ctx(ctx), module(module) {
			auto& llvm_data   = llvmData();
			user_func_wrapper = llvm::Function::Create(
				llvm_data.types.opfun.get(),
				llvm::Function::ExternalLinkage,
				module->getName(),
				module
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
			auto&           llvm_data = llvmData();
			llvm::Function* callee    = module->getFunction(opfun_name.data());
			if (!callee) {
				// Because opfunctions' definitions can live in a different LLVM module, or even in
				// the executor process, we declare them once in the user function module with
				// external linkage.
				callee = llvm::Function::Create(
					llvm_data.types.opfun.get(), llvm::Function::ExternalLinkage, opfun_name, module
				);
			}

			return callee;
		}

		/**
		 * @brief Returns the number of microinstructions (structures) used to execute
		 * the given opcode.
		 * @details This includes the opcode itself, so the result is equal to
		 * 1 plus the number of exts instructions that follow it.
		 */
		size_t argumentsUsedByOpcodeCnt(const low::MicroBytecode& bc, usize idx) {
			size_t res = 1;
			while (idx + res < bc.size()
			       and isOpcodeNonExecutable(getInstructionOpcode(bc[idx + res]))) {
				++res;
			}
			return res;
		}

		/**
		 * @brief Sets the instruction argument so that it points to a compile-time
		 * constant table of arguments.
		 * @details The template parameter specifies whether the parameter should use
		 * the SC or TC version.
		 */
		template<bool switch_case_instr>
		void setInstructionPtr(
			const low::MicroBytecode& bc,
			llvm::IRBuilder<>&        builder,
			usize                     idx,
			const base::StrID&        func_or_loop_name
		) {
			auto& llvm_data = llvmData();

			usize                        length = argumentsUsedByOpcodeCnt(bc, idx);
			std::vector<llvm::Constant*> elems;

			// Fill table with all needed instruction's arguments.
			for (size_t i = 0; i < length; ++i) {
				const auto&                  inst   = bc.at(idx + i);
				const auto&                  opcode = (u64) getInstructionOpcode(inst);
				std::vector<llvm::Constant*> fields;

				if constexpr (switch_case_instr) {
					fields.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvm_ctx), opcode)
					);
				} else {
					fields.push_back(llvm::ConstantInt::get(
						llvm::Type::getInt64Ty(llvm_ctx),
						reinterpret_cast<u64>(OpFuns::OPFUNS.at(opcode))
					));
				}

				fields.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvm_ctx), inst.arg0)
				);

				fields.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvm_ctx), inst.arg1)
				);

				llvm::Constant* c
					= llvm::ConstantStruct::get(llvm_data.types.microinstruction.get(), fields);
				elems.push_back(c);
			}
			llvm::ArrayType* arr_ty
				= llvm::ArrayType::get(llvm_data.types.microinstruction.get(), elems.size());

			llvm::Constant* arr_const = llvm::ConstantArray::get(arr_ty, elems);

			auto needed_bc = new llvm::GlobalVariable(
				*module,
				arr_ty,
				true,  // isConstant
				llvm::GlobalValue::PrivateLinkage,
				arr_const,
				func_or_loop_name.str() + "_bc"
			);

			llvm::Value* zero = builder.getInt32(0);

			// pointer to bc
			llvm::Value* bc_ptr
				= builder.CreateInBoundsGEP(arr_ty, needed_bc, { zero, zero }, "bc_ptr");

			// instr becomes pointer to pointer to created compile time bc array.
			builder.CreateStore(bc_ptr, instr_arg);
		}

		void lowerBasicBlock(
			const low::MicroBytecode&        bc,
			llvm::IRBuilder<>&               ir_builder,
			usize                            start,
			usize                            end,
			const base::StrID&               func_or_loop_name,
			std::unordered_set<std::string>& used_opfuns
		) {
			auto& llvm_data = llvmData();
			for (usize instr_idx = start; instr_idx < end; ++instr_idx) {
				const vm::MicroInstruction& mi     = bc.at(instr_idx);
				auto                        opcode = vm::getInstructionOpcode(mi);
				switch (opcode) {
				case vm::low::MicroOpcode::jitEntrypoint:
					CORE_ASSERT(false, "We should always compile the original code, without entrypoints.");
					continue;
				case vm::low::MicroOpcode::call_func:
				case vm::low::MicroOpcode::virtual_call_pptr_method: {
					// Trampoline uses VM functions, instructions have to have correct type.
#ifdef USE_SWITCH_CASE
					setInstructionPtr<true>(bc, ir_builder, instr_idx, func_or_loop_name);
#else
					setInstructionPtr<false>(bc, ir_builder, instr_idx, func_or_loop_name);
#endif
					ir_builder.CreateCall(
						llvm_data.types.opfun.get(),
						getOrCreateOpcodeFunction("trampoline"),
						{ instr_arg, locals_arg, frame_arg, thread_arg }
					);
				} break;
				default:
					if (isOpcodeNonExecutable(opcode)) continue;
					std::string opfun_name;
					match_optional(llvm_data.getFunName(opcode)) {
						opt_some(op_name) {
							opfun_name = op_name;
							used_opfuns.insert(opfun_name);
						}
						opt_none { opfun_name = low::OPCODE_NAMES.at(static_cast<u64>(opcode)); }
					}

std::cerr << "About to call opfun: " << opfun_name
          << " isDeclaration=" << getOrCreateOpcodeFunction(opfun_name)->isDeclaration()
          << "\n";
					// Here we are calling instruction originating from bc file or debug
					// instruction. Make instruction* point to switch case version of microinstruction.
					setInstructionPtr<true>(bc, ir_builder, instr_idx, func_or_loop_name);
					ir_builder.CreateCall(
						llvm_data.types.opfun.get(),
						getOrCreateOpcodeFunction(opfun_name),
						{ instr_arg, locals_arg, frame_arg, thread_arg }
					);
				}
			}
		}

		void lowerConditionalJump(
			const vm::low::cf::BasicBlock& block, llvm::IRBuilder<>& ir_builder
		) {
			auto& llvm_data         = llvmData();
			u32   flags_field_index = 0;

			llvm::Value* frame_ptr = ir_builder.CreateLoad(
				llvm::PointerType::getUnqual(llvm_data.types.frame.get()), frame_arg
			);
			llvm::Value* flags_ptr = ir_builder.CreateStructGEP(
				llvm_data.types.frame.get(), frame_ptr, flags_field_index
			);

			llvm::Value* flag_ptr
				= ir_builder.CreateStructGEP(llvm_data.types.flag_data.get(), flags_ptr, 0);
			llvm::Value* flag_value = ir_builder.CreateLoad(ir_builder.getInt1Ty(), flag_ptr);

			if (block.edgeKind() == vm::low::cf::OutEdges::Kind::JmpIfNot)
				flag_value = ir_builder.CreateNot(flag_value);

			ir_builder.CreateCondBr(
				flag_value, llvm_blocks[block.successTarget()], llvm_blocks[block.failTarget()]
			);
		}

		void lowerBlock(
			const base::StrID&               func_or_loop_name,
			const low::MicroBytecode&        bc,
			const vm::low::cf::BasicBlock&   block,
			std::unordered_set<std::string>& used_opfuns
		) {
			llvm::IRBuilder<> ir_builder(llvm_blocks[block.id]);
			lowerBasicBlock(bc, ir_builder, block.start, block.end, func_or_loop_name, used_opfuns);

			switch (block.edgeKind()) {
			case vm::low::cf::OutEdges::Kind::Default: {
				ir_builder.CreateBr(llvm_blocks[block.next()]);
				break;
			}
			case vm::low::cf::OutEdges::Kind::JmpIf:
			case vm::low::cf::OutEdges::Kind::JmpIfNot: {
				lowerConditionalJump(block, ir_builder);
				break;
			}
			case vm::low::cf::OutEdges::Kind::End:
			default: {
				ir_builder.CreateRetVoid();
				break;
			}
			}
		}

		void lowerCFG(
			const low::cf::ControlFlowGraph& cfg,
			const low::MicroBytecode&        bc,
			const base::StrID&               func_or_loop_name
		) {
			auto& llvm_data = llvmData();

			// Create LLVM basic blocks for each VM block
			for (usize block_idx = 0; block_idx < cfg.size(); ++block_idx) {
				llvm::BasicBlock* block = llvm::BasicBlock::Create(
					llvm_ctx, "block_" + std::to_string(block_idx), user_func_wrapper
				);
				llvm_blocks.push_back(block);
			}

			// Declare printf
llvm::FunctionType* printf_type = llvm::FunctionType::get(
    llvm::Type::getInt32Ty(llvm_ctx),
    { llvm::PointerType::getUnqual(llvm_ctx) },
    true  // variadic
);
llvm::Function* printf_func = llvm::Function::Create(
    printf_type,
    llvm::Function::ExternalLinkage,
    "printf",
    module
);

// Create the format string global
llvm::Constant* fmt_str = llvm::ConstantDataArray::getString(llvm_ctx, "JIT compiled function entered\n");
llvm::GlobalVariable* fmt_global = new llvm::GlobalVariable(
    *module,
    fmt_str->getType(),
    true,
    llvm::GlobalValue::PrivateLinkage,
    fmt_str,
    "fmt_str"
);

// Insert the printf call at the start of the first basic block
llvm::IRBuilder<> entry_builder(llvm_blocks[0], llvm_blocks[0]->begin());
llvm::Value* fmt_ptr = entry_builder.CreateInBoundsGEP(
    fmt_str->getType(),
    fmt_global,
    { entry_builder.getInt32(0), entry_builder.getInt32(0) }
);
entry_builder.CreateCall(printf_type, printf_func, { fmt_ptr });

			// Helper structure to track called opfuns.
			std::unordered_set<std::string> used_opfuns;

			for (usize block_idx = 0; block_idx < llvm_blocks.size(); ++block_idx)
				lowerBlock(
					func_or_loop_name, bc, cfg.getBlock(block_idx), used_opfuns
				);  // lowerBlock(function_to_compile, block_idx, used_opfuns);

			auto used_opfuns_filter = [&](const llvm::GlobalValue* gv) -> bool {
				return used_opfuns.contains(gv->getName().str());
			};

			// At this point, `used_opfuns` contains all used opfunctions' names, so we can clone
			// the appropriate definitions and link them against the user function module.

			llvm::ValueToValueMapTy       vmap;
			std::unique_ptr<llvm::Module> used_opfuns_module
				= llvm::CloneModule(*llvm_data.g_module, vmap, used_opfuns_filter);
			llvm::Linker::linkModules(
				*module, std::move(used_opfuns_module), llvm::Linker::Flags::LinkOnlyNeeded
			);
			for (auto& f: module->functions()) {
				if (!f.isDeclaration() && &f != user_func_wrapper) {
					// Set cloned opfuns' linkage to AvailableExternally to avoid double compilation
					// and symbol conflicts.
					f.setLinkage(llvm::GlobalValue::AvailableExternallyLinkage);
				}
			}
		}
	};
}  // namespace vm::jit
