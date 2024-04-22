#pragma once

#include "frame.hpp"
#include "config.hpp"
#include <base/ints.hpp>
#include <array>

// #define USE_COMPACT_INSTRUCTION

#define OPFUN_ARGS                                                    \
	const Fix8Instruction *IF_NOT_TC(&) instr [[maybe_unused]],       \
		std::byte *        IF_NOT_TC(&) local_stack [[maybe_unused]], \
		Frame *IF_NOT_TC(&) frame [[maybe_unused]], Executor &executor [[maybe_unused]]

#define RETURN_TYPE IF_NOT_TC([[gnu::always_inline]]) void

namespace vm {
	class Executor;
	struct Fix8Instruction;

	class OpFuns;
	using OpFun = void(OPFUN_ARGS);

	// Describes number of RiftBC opcodes + meta-opcodes recognized by Executor.
	// This constant is relevant for `vm::Opfuns::opfuns[]` (instructions.hpp) and `opcode_label[]`
	// (CG, executor.cpp)
	constexpr const u16 OpCasesCount = 39;

#ifdef USE_TAIL_CALLS
	struct Fix8Instruction {
		OpFun* opfun;
		i32    arg0;
		i32    arg1;
	};
#else
	#ifdef USE_COMPACT_INSTRUCTION
	struct Fix8Instruction {
		i64 opcode: 16, arg0: 24, arg1: 24;
	};
	#else
	struct Fix8Instruction {
		u16 opcode;
		i32 arg0;
		i32 arg1;
	};
	#endif
#endif

	class OpFuns {
	public:
		static OpFun op_mov_l64_imm;

		static OpFun op_mov_l64_l64;
		static OpFun op_cmov_l64_l64;

		static OpFun op_mov_l64_r0;
		static OpFun op_mov_r0_l64;

		static OpFun op_mov_l64_pFuncArg;
		static OpFun op_mov_lptr_ptrFuncArg;

		static OpFun op_add_l64_l64;
		static OpFun op_add_l64_imm;

		static OpFun op_sub_l64_l64;
		static OpFun op_sub_l64_imm;

		static OpFun op_mul_l64_imm;

		static OpFun op_mod_l64_l64;
		static OpFun op_mod_l64_imm;

		static OpFun op_div_l64_imm;

		static OpFun op_cmpEq_l64_l64;
		static OpFun op_cmpEq_l64_imm;
		static OpFun op_cmpG_l64_l64;
		static OpFun op_cmpG_l64_imm;

		static OpFun op_jmpRel_label;
		static OpFun op_jmpRelIf_label;
		static OpFun op_jmpRelNotIf_label;

		static OpFun op_setPArg_l64;
		static OpFun op_setPtrArg_lptr;
		static OpFun op_call_func;

		static OpFun op_ret_l64;
		static OpFun op_ret_imm;

		static OpFun op_init_type;
		static OpFun op_deinit;

		static OpFun op_input_l64;
		static OpFun op_output_l64;

		static OpFun op_nop;

		static OpFun op_alloc_lptr_type;
		static OpFun op_free_lptr;
		static OpFun op_load_l64_lptr_ofs;
		static OpFun op_store_lptr_l64_ofs;

		static OpFun op_ext_l64;
		static OpFun op_exit;

		// Meta-opcodes, that are not a part of BC
		static OpFun op_handle_strategy;

		// A mapping between opcode ids and function pointers.
		// WARN: Ordering of elements must stay the same as in vm::OpcodeFix8
		static constexpr std::array<OpFun*, OpCasesCount> opfuns{
			op_mov_l64_imm,

			op_mov_l64_l64,
			op_cmov_l64_l64,

			op_mov_l64_r0,
			op_mov_r0_l64,

			op_mov_l64_pFuncArg,
			op_mov_lptr_ptrFuncArg,

			op_add_l64_l64,
			op_add_l64_imm,

			op_sub_l64_l64,
			op_sub_l64_imm,

			op_mul_l64_imm,

			op_mod_l64_l64,
			op_mod_l64_imm,

			op_div_l64_imm,

			op_cmpEq_l64_l64,
			op_cmpEq_l64_imm,
			op_cmpG_l64_l64,
			op_cmpG_l64_imm,

			op_jmpRel_label,
			op_jmpRelIf_label,
			op_jmpRelNotIf_label,

			op_setPArg_l64,
			op_setPtrArg_lptr,
			op_call_func,

			op_ret_l64,
			op_ret_imm,

			op_init_type,
			op_deinit,

			op_input_l64,
			op_output_l64,

			op_nop,

			op_alloc_lptr_type,
			op_free_lptr,
			op_load_l64_lptr_ofs,
			op_store_lptr_l64_ofs,

			op_ext_l64,
			op_exit,

			op_handle_strategy,
		};
	};
}  // namespace vm
