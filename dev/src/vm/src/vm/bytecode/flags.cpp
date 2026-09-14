#include "flags.hpp"

#include "validator/errors.hpp"

#include <base/preproc/for_each.hpp>

#include <vm/core/builtin_functions.hpp>

namespace vm::code {
	FunctionFlag getFlagsForBuiltinFunction(base::StrID name) {
		using enum FunctionFlagOptions;

		auto builtin_func_opt = builtins::getBuiltinFunctionID(name);
		if (!builtin_func_opt) throw InvalidBuiltinFunctionError(opargs::BuiltinFunctionName(name));
		switch (builtin_func_opt.value()) {
		case builtins::BuiltinFunctionID::Abort:
			// terminates the current execution
			return { ControlFlowModifying };
		case builtins::BuiltinFunctionID::InputI64:
		case builtins::BuiltinFunctionID::InputChar:
			// reads from stdin, blocks waiting for the user
			return FunctionFlag(IORead) | MayBlock | ReleaseGIL;
		case builtins::BuiltinFunctionID::OutputI64:
		case builtins::BuiltinFunctionID::OutputI32:
		case builtins::BuiltinFunctionID::OutputChar:
		case builtins::BuiltinFunctionID::OutputString:
			return FunctionFlag(IOWrite) | RequiresGIL;
		case builtins::BuiltinFunctionID::Stoi:
		case builtins::BuiltinFunctionID::Strtod:
			// pure conversion, no observable effects
			return {};
		case builtins::BuiltinFunctionID::FloatToString:
		case builtins::BuiltinFunctionID::U64ToString:
		case builtins::BuiltinFunctionID::I64ToString:
			// Number-to-string conversions that write the result through a pointer operand.
			// No console I/O and no threading; like every other pointer-deref write (see the
			// `deref_write` no-op in getFlagsForInstruction) they are not classified as
			// GlobalRead/GlobalWrite.
			return {};
		case builtins::BuiltinFunctionID::StartThread:
			return FunctionFlag(Multithread) | ControlFlowModifying;
		case builtins::BuiltinFunctionID::JoinThread:
			return FunctionFlag(Multithread) | MayBlock | ReleaseGIL;
		case builtins::BuiltinFunctionID::CreateMutex:
		case builtins::BuiltinFunctionID::UnlockMutex:
		case builtins::BuiltinFunctionID::DestroyMutex:
		case builtins::BuiltinFunctionID::CreateCV:
		case builtins::BuiltinFunctionID::NotifyCV:
		case builtins::BuiltinFunctionID::NotifyAllCV:
		case builtins::BuiltinFunctionID::DestroyCV:
			return { Multithread };
		case builtins::BuiltinFunctionID::LockMutex:
		case builtins::BuiltinFunctionID::WaitCV:
			return FunctionFlag(Multithread) | MayBlock | ReleaseGIL;
		}
		CORE_PANIC("Invalid builtin function ID");
	}

	namespace {
		/** @brief Translates a FunctionFlag to the corresponding InstructionFlag. */
		InstructionFlag functionFlagToInstructionFlag(FunctionFlag f) {
			using FFO = FunctionFlagOptions;
			using IFO = InstructionFlagOptions;
			InstructionFlag out;
#define FUNCTION_TO_INSTRUCTION_FLAG(FLAG) \
	if (f.contains(FFO::FLAG)) out |= IFO::FLAG;
			FOR_EACH(FUNCTION_TO_INSTRUCTION_FLAG, FUNCTION_FLAG_OPTIONS)
			return out;
		}
	}

	InstructionFlag getFlagsForInstruction(
		Instruction                            instruction,
		const ObjIdNameMap<GlobalData>&        globals,
		const ObjIdNameMap<ExternalCFunction>& ext_c_functions
	) {
		using enum InstructionFlagOptions;
		namespace ins = instructions;

		InstructionFlag flags;

		auto is_global = [&](base::StrID n) { return globals.contains(n); };
		auto rd        = [&](const auto& place) {
            if (is_global(place.var_name)) flags |= GlobalRead;
		};
		auto wr = [&](const auto& place) {
			if (is_global(place.var_name)) flags |= GlobalWrite;
		};
		// dst that is read and then written (arith/cmov/cast in-place)
		auto rdwr = [&](const auto& place) {
			if (is_global(place.var_name)) flags |= GlobalRead | GlobalWrite;
		};

		// Dereferencing a pointer contributes no global read/write flags for now.
		//
		// Treating every pointer deref as a global access is too restrictive: we cannot yet
		// tell whether a pointer aliases a global. Once const pointers exist as a dedicated
		// instruction, taking a mutable pointer to a global will imply GlobalWrite and taking a
		// const pointer to a global will imply GlobalRead. Until then, taking a pointer to a
		// global yields only GlobalRead (through the `rd` on the ref source) and dereferencing
		// does nothing with globals.
		auto deref_read  = [] {};
		auto deref_write = [] {};


		// Per-shape `instr_case` shorthands. Cover the common patterns where the
		// instruction's name and arg shape uniquely determine the read/write set.
		// Defined locally so they don't leak to other translation units.
#define FLAGS_RW(op)                                         \
	auto dst = VISIT(op, instr, return instr->dst.var_name); \
	if (is_global(dst)) flags |= GlobalRead | GlobalWrite;

#define FLAGS_RW_R(op)                                       \
	auto dst = VISIT(op, instr, return instr->dst.var_name); \
	auto src = VISIT(op, instr, return instr->src.var_name); \
	if (is_global(dst)) flags |= GlobalRead | GlobalWrite;   \
	if (is_global(src)) flags |= GlobalRead;

#define FLAGS_CMP(op)                                        \
	auto lhs = VISIT(op, instr, return instr->lhs.var_name); \
	auto rhs = VISIT(op, instr, return instr->rhs.var_name); \
	if (is_global(lhs)) flags |= GlobalRead;                 \
	if (is_global(rhs)) flags |= GlobalRead;

#define FLAGS_CMP_IMM(op)                                    \
	auto lhs = VISIT(op, instr, return instr->lhs.var_name); \
	if (is_global(lhs)) flags |= GlobalRead;

		// UNHANDLED_ENUM suppresses the switch-exhaustiveness warning, so adding a new
		// instruction still compiles. Any instruction that lacks a case below therefore reaches
		// the `instr_default` CORE_PANIC at load time instead of failing to build: every new
		// instruction must be given an entry in this map.
		PUSH_DIAGNOSTIC
		UNHANDLED_ENUM
		instr_match(instruction) {
			// ===== Pure moves: dst written, src (place) read =====
			instr_case_many(
				mov_p_imm,
				ins::Op_mov_p8_imm,
				ins::Op_mov_p16_imm,
				ins::Op_mov_p32_imm,
				ins::Op_mov_p64_imm,
				ins::Op_setNull_pptr
			) {
				auto dst = VISIT(mov_p_imm, instr, return instr->dst.var_name);
				if (is_global(dst)) flags |= GlobalWrite;
			}

			instr_case_many(
				mov_p_p,
				ins::Op_mov_p8_p8,
				ins::Op_mov_p16_p16,
				ins::Op_mov_p32_p32,
				ins::Op_mov_p64_p64,
				ins::Op_mov_pptr_pptr,
				ins::Op_mov_pcptr_pcptr,
				ins::Op_mov_pste_pste,
				ins::Op_mov_pfst_pfst,
				ins::Op_mov_popq_popq,
				ins::Op_mov_pvnt_pvnt
			) {
				auto dst = VISIT(mov_p_p, instr, return instr->dst.var_name);
				auto src = VISIT(mov_p_p, instr, return instr->src.var_name);
				if (is_global(dst)) flags |= GlobalWrite;
				if (is_global(src)) flags |= GlobalRead;
			}

			// ===== Conditional moves: dst is read (kept conditionally) and written =====
			instr_case_many(
				cmov_p_p,
				ins::Op_cmov_p8_p8,
				ins::Op_cmov_p16_p16,
				ins::Op_cmov_p32_p32,
				ins::Op_cmov_p64_p64
			){ FLAGS_RW_R(cmov_p_p) }

			instr_case_many(
				cmov_p_imm,
				ins::Op_cmov_p8_imm,
				ins::Op_cmov_p16_imm,
				ins::Op_cmov_p32_imm,
				ins::Op_cmov_p64_imm
			){ FLAGS_RW(cmov_p_imm) }

			// ===== Binary arithmetic: dst = dst op src =====
			instr_case_many(
				bin_arith,
				ins::Op_add_p64_p64,
				ins::Op_add_p32_p32,
				ins::Op_add_p16_p16,
				ins::Op_add_p8_p8,
				ins::Op_sub_p64_p64,
				ins::Op_sub_p32_p32,
				ins::Op_sub_p16_p16,
				ins::Op_sub_p8_p8,
				ins::Op_mul_p64_p64,
				ins::Op_mul_p32_p32,
				ins::Op_mul_p16_p16,
				ins::Op_mul_p8_p8,
				ins::Op_div_p64_p64,
				ins::Op_div_p32_p32,
				ins::Op_div_p16_p16,
				ins::Op_div_p8_p8,
				ins::Op_mod_p64_p64,
				ins::Op_mod_p32_p32,
				ins::Op_mod_p16_p16,
				ins::Op_mod_p8_p8
			){ FLAGS_RW_R(bin_arith) }

			instr_case_many(
				arith_imm,
				ins::Op_add_p64_imm,
				ins::Op_add_p32_imm,
				ins::Op_add_p16_imm,
				ins::Op_add_p8_imm,
				ins::Op_sub_p64_imm,
				ins::Op_sub_p32_imm,
				ins::Op_sub_p16_imm,
				ins::Op_sub_p8_imm,
				ins::Op_mul_p64_imm,
				ins::Op_mul_p32_imm,
				ins::Op_mul_p16_imm,
				ins::Op_mul_p8_imm,
				ins::Op_div_p64_imm,
				ins::Op_div_p32_imm,
				ins::Op_div_p16_imm,
				ins::Op_div_p8_imm,
				ins::Op_mod_p64_imm,
				ins::Op_mod_p32_imm,
				ins::Op_mod_p16_imm,
				ins::Op_mod_p8_imm
			) {
				auto dst = VISIT(arith_imm, instr, return instr->dst.var_name);
				if (is_global(dst)) flags |= GlobalRead | GlobalWrite;
			}

			// Unsigned arith
			instr_case_many(
				uarith_p_p,
				ins::Op_umul_p64_p64,
				ins::Op_umul_p32_p32,
				ins::Op_umul_p16_p16,
				ins::Op_umul_p8_p8,
				ins::Op_umod_p64_p64,
				ins::Op_umod_p32_p32,
				ins::Op_umod_p16_p16,
				ins::Op_umod_p8_p8,
				ins::Op_udiv_p64_p64,
				ins::Op_udiv_p32_p32,
				ins::Op_udiv_p16_p16,
				ins::Op_udiv_p8_p8
			){ FLAGS_RW_R(uarith_p_p) }

			instr_case_many(
				uarith_p_imm,
				ins::Op_umul_p64_imm,
				ins::Op_umul_p32_imm,
				ins::Op_umul_p16_imm,
				ins::Op_umul_p8_imm,
				ins::Op_umod_p64_imm,
				ins::Op_umod_p32_imm,
				ins::Op_umod_p16_imm,
				ins::Op_umod_p8_imm,
				ins::Op_udiv_p64_imm,
				ins::Op_udiv_p32_imm,
				ins::Op_udiv_p16_imm,
				ins::Op_udiv_p8_imm
			){ FLAGS_RW(uarith_p_imm) }

			// Floating point
			instr_case_many(
				farith_p_p,
				ins::Op_fadd_p64_p64,
				ins::Op_fadd_p32_p32,
				ins::Op_fsub_p64_p64,
				ins::Op_fsub_p32_p32,
				ins::Op_fmul_p64_p64,
				ins::Op_fmul_p32_p32,
				ins::Op_fdiv_p64_p64,
				ins::Op_fdiv_p32_p32
			){ FLAGS_RW_R(farith_p_p) }

			instr_case_many(
				farith_p_imm,
				ins::Op_fadd_p64_imm,
				ins::Op_fadd_p32_imm,
				ins::Op_fsub_p64_imm,
				ins::Op_fsub_p32_imm,
				ins::Op_fmul_p64_imm,
				ins::Op_fmul_p32_imm,
				ins::Op_fdiv_p64_imm,
				ins::Op_fdiv_p32_imm
			){ FLAGS_RW(farith_p_imm) }

			// Unary arith / negation / logical-not
			instr_case_many(
				unary_p,
				ins::Op_neg_p64,
				ins::Op_neg_p32,
				ins::Op_neg_p16,
				ins::Op_neg_p8,
				ins::Op_fneg_p64,
				ins::Op_fneg_p32,
				ins::Op_log_not_p8
			){ FLAGS_RW(unary_p) }

			// Logical (and/or/xor)
			instr_case_many(
				log_p_p, ins::Op_log_and_p8_p8, ins::Op_log_or_p8_p8, ins::Op_log_xor_p8_p8
			){ FLAGS_RW_R(log_p_p) }

			instr_case_many(
				log_p_imm, ins::Op_log_and_p8_imm, ins::Op_log_or_p8_imm, ins::Op_log_xor_p8_imm
			){ FLAGS_RW(log_p_imm) }

			// ===== Bitwise (and / or / xor / shl / shr / not) =====
			// 64-bit
			instr_case_many(
				bit64_p_p,
				ins::Op_bit_and_p64_p64,
				ins::Op_bit_or_p64_p64,
				ins::Op_bit_xor_p64_p64,
				ins::Op_shl_p64_p64,
				ins::Op_shr_p64_p64
			){ FLAGS_RW_R(bit64_p_p) }

			instr_case_many(
				bit64_p_imm,
				ins::Op_bit_and_p64_imm,
				ins::Op_bit_or_p64_imm,
				ins::Op_bit_xor_p64_imm,
				ins::Op_shl_p64_imm,
				ins::Op_shr_p64_imm,
				ins::Op_bit_not_p64
			){ FLAGS_RW(bit64_p_imm) }

			// 32-bit
			instr_case_many(
				bit32_p_p,
				ins::Op_bit_and_p32_p32,
				ins::Op_bit_or_p32_p32,
				ins::Op_bit_xor_p32_p32,
				ins::Op_shl_p32_p32,
				ins::Op_shr_p32_p32
			){ FLAGS_RW_R(bit32_p_p) }

			instr_case_many(
				bit32_p_imm,
				ins::Op_bit_and_p32_imm,
				ins::Op_bit_or_p32_imm,
				ins::Op_bit_xor_p32_imm,
				ins::Op_shl_p32_imm,
				ins::Op_shr_p32_imm,
				ins::Op_bit_not_p32
			){ FLAGS_RW(bit32_p_imm) }

			// 16-bit
			instr_case_many(
				bit16_p_p,
				ins::Op_bit_and_p16_p16,
				ins::Op_bit_or_p16_p16,
				ins::Op_bit_xor_p16_p16,
				ins::Op_shl_p16_p16,
				ins::Op_shr_p16_p16
			){ FLAGS_RW_R(bit16_p_p) }

			instr_case_many(
				bit16_p_imm,
				ins::Op_bit_and_p16_imm,
				ins::Op_bit_or_p16_imm,
				ins::Op_bit_xor_p16_imm,
				ins::Op_shl_p16_imm,
				ins::Op_shr_p16_imm,
				ins::Op_bit_not_p16
			){ FLAGS_RW(bit16_p_imm) }

			// 8-bit
			instr_case_many(
				bit8_p_p,
				ins::Op_bit_and_p8_p8,
				ins::Op_bit_or_p8_p8,
				ins::Op_bit_xor_p8_p8,
				ins::Op_shl_p8_p8,
				ins::Op_shr_p8_p8
			){ FLAGS_RW_R(bit8_p_p) }

			instr_case_many(
				bit8_p_imm,
				ins::Op_bit_and_p8_imm,
				ins::Op_bit_or_p8_imm,
				ins::Op_bit_xor_p8_imm,
				ins::Op_shl_p8_imm,
				ins::Op_shr_p8_imm,
				ins::Op_bit_not_p8
			){ FLAGS_RW(bit8_p_imm) }

			// ===== Comparisons: lhs/rhs are read =====
			instr_case_many(
				cmp_p_p,
				ins::Op_cmpEq_p64_p64,
				ins::Op_cmpNeq_p64_p64,
				ins::Op_cmpGt_p64_p64,
				ins::Op_cmpGe_p64_p64,
				ins::Op_ucmpGt_p64_p64,
				ins::Op_ucmpGe_p64_p64,
				ins::Op_cmpLt_p64_p64,
				ins::Op_cmpLe_p64_p64,
				ins::Op_ucmpLt_p64_p64,
				ins::Op_ucmpLe_p64_p64,
				ins::Op_cmpEq_p32_p32,
				ins::Op_cmpNeq_p32_p32,
				ins::Op_cmpGt_p32_p32,
				ins::Op_cmpGe_p32_p32,
				ins::Op_ucmpGt_p32_p32,
				ins::Op_ucmpGe_p32_p32,
				ins::Op_cmpLt_p32_p32,
				ins::Op_cmpLe_p32_p32,
				ins::Op_ucmpLt_p32_p32,
				ins::Op_ucmpLe_p32_p32,
				ins::Op_cmpEq_p16_p16,
				ins::Op_cmpNeq_p16_p16,
				ins::Op_cmpGt_p16_p16,
				ins::Op_cmpGe_p16_p16,
				ins::Op_ucmpGt_p16_p16,
				ins::Op_ucmpGe_p16_p16,
				ins::Op_cmpLt_p16_p16,
				ins::Op_cmpLe_p16_p16,
				ins::Op_ucmpLt_p16_p16,
				ins::Op_ucmpLe_p16_p16,
				ins::Op_cmpEq_p8_p8,
				ins::Op_cmpNeq_p8_p8,
				ins::Op_cmpGt_p8_p8,
				ins::Op_cmpGe_p8_p8,
				ins::Op_ucmpGt_p8_p8,
				ins::Op_ucmpGe_p8_p8,
				ins::Op_cmpLt_p8_p8,
				ins::Op_cmpLe_p8_p8,
				ins::Op_ucmpLt_p8_p8,
				ins::Op_ucmpLe_p8_p8,
				ins::Op_fcmpEq_p64_p64,
				ins::Op_fcmpNeq_p64_p64,
				ins::Op_fcmpGt_p64_p64,
				ins::Op_fcmpGe_p64_p64,
				ins::Op_fcmpLt_p64_p64,
				ins::Op_fcmpLe_p64_p64,
				ins::Op_fcmpEq_p32_p32,
				ins::Op_fcmpNeq_p32_p32,
				ins::Op_fcmpGt_p32_p32,
				ins::Op_fcmpGe_p32_p32,
				ins::Op_fcmpLt_p32_p32,
				ins::Op_fcmpLe_p32_p32
			){ FLAGS_CMP(cmp_p_p) }

			instr_case_many(
				cmp_p_imm,
				ins::Op_cmpEq_p64_imm,
				ins::Op_cmpNeq_p64_imm,
				ins::Op_cmpGt_p64_imm,
				ins::Op_cmpGe_p64_imm,
				ins::Op_ucmpGt_p64_imm,
				ins::Op_ucmpGe_p64_imm,
				ins::Op_cmpLt_p64_imm,
				ins::Op_cmpLe_p64_imm,
				ins::Op_ucmpLt_p64_imm,
				ins::Op_ucmpLe_p64_imm,
				ins::Op_cmpEq_p32_imm,
				ins::Op_cmpNeq_p32_imm,
				ins::Op_cmpGt_p32_imm,
				ins::Op_cmpGe_p32_imm,
				ins::Op_ucmpGt_p32_imm,
				ins::Op_ucmpGe_p32_imm,
				ins::Op_cmpLt_p32_imm,
				ins::Op_cmpLe_p32_imm,
				ins::Op_ucmpLt_p32_imm,
				ins::Op_ucmpLe_p32_imm,
				ins::Op_cmpEq_p16_imm,
				ins::Op_cmpNeq_p16_imm,
				ins::Op_cmpGt_p16_imm,
				ins::Op_cmpGe_p16_imm,
				ins::Op_ucmpGt_p16_imm,
				ins::Op_ucmpGe_p16_imm,
				ins::Op_cmpLt_p16_imm,
				ins::Op_cmpLe_p16_imm,
				ins::Op_ucmpLt_p16_imm,
				ins::Op_ucmpLe_p16_imm,
				ins::Op_cmpEq_p8_imm,
				ins::Op_cmpNeq_p8_imm,
				ins::Op_cmpGt_p8_imm,
				ins::Op_cmpGe_p8_imm,
				ins::Op_ucmpGt_p8_imm,
				ins::Op_ucmpGe_p8_imm,
				ins::Op_cmpLt_p8_imm,
				ins::Op_cmpLe_p8_imm,
				ins::Op_ucmpLt_p8_imm,
				ins::Op_ucmpLe_p8_imm,
				ins::Op_fcmpEq_p64_imm,
				ins::Op_fcmpNeq_p64_imm,
				ins::Op_fcmpGt_p64_imm,
				ins::Op_fcmpGe_p64_imm,
				ins::Op_fcmpLt_p64_imm,
				ins::Op_fcmpLe_p64_imm,
				ins::Op_fcmpEq_p32_imm,
				ins::Op_fcmpNeq_p32_imm,
				ins::Op_fcmpGt_p32_imm,
				ins::Op_fcmpGe_p32_imm,
				ins::Op_fcmpLt_p32_imm,
				ins::Op_fcmpLe_p32_imm
			){ FLAGS_CMP_IMM(cmp_p_imm) }

			instr_case(ins::Op_cmpNull_pptr, i) {
				rd(i.ptr);
			}
			instr_case(ins::Op_cmpNull_pcptr, i) { rd(i.ptr); }

			// ===== Variants =====
			instr_case(ins::Op_variantSetInner_pvnt_type, i) { rdwr(i.variant); }
			instr_case(ins::Op_variantGetInner_pptr_pvnt_type, i) {
				wr(i.dst_ptr);
				rd(i.variant);
			}
			instr_case(ins::Op_variantSetInner_pptr_type, i) {
				rd(i.variant_ptr);
				deref_write();
			}
			instr_case(ins::Op_variantGetInner_pptr_pptr_type, i) {
				wr(i.dst_ptr);
				rd(i.variant_ptr);
				deref_read();
			}

			// ===== Labels & jumps =====
			instr_case(ins::Op_label, i) {}
			instr_case(ins::Op_jmp_label, i) { flags |= ControlFlowModifying; }
			instr_case(ins::Op_jmpIf_label, i) { flags |= ControlFlowModifying; }
			instr_case(ins::Op_jmpIfNot_label, i) { flags |= ControlFlowModifying; }

			// ===== Calls =====
			instr_case(ins::Op_call_func, i) {
				flags |= Call | InstructionFlag(ControlFlowModifying);
			}
			instr_case(ins::Op_call_builtinfunc, i) {
				flags |= Call | InstructionFlag(ControlFlowModifying);
				flags |= functionFlagToInstructionFlag(
					getFlagsForBuiltinFunction(i.function.function_name)
				);
			}
			instr_case(ins::Op_call_cfunc, i) {
				CORE_ASSERT(
					ext_c_functions.contains(i.function.function_name),
					"call_cfunc references unknown external C function"
				);
				// External C call is opaque: assume it can do IO and runs outside the VM.
				flags |= CallExternal | InstructionFlag(ControlFlowModifying)
				       | InstructionFlag(IORead) | InstructionFlag(IOWrite)
				       | InstructionFlag(MayBlock) | InstructionFlag(ReleaseGIL);
			}
			instr_case(ins::Op_call_ffifunc, i) {
				// An FFI call dispatches through libffi into a shared object: like an external C
				// call it is opaque, may do IO, runs outside the VM, and can block.
				flags |= CallExternal | InstructionFlag(ControlFlowModifying)
				       | InstructionFlag(IORead) | InstructionFlag(IOWrite)
				       | InstructionFlag(MayBlock) | InstructionFlag(ReleaseGIL);
			}
			instr_case(ins::Op_set_threadctx, i) {
				flags
					|= Call | InstructionFlag(Multithread) | InstructionFlag(ControlFlowModifying);
			}
			instr_case(ins::Op_ret_tailcall_func, i) {
				flags |= Call | InstructionFlag(ControlFlowModifying);
			}
			instr_case(ins::Op_ret, i) { flags |= ControlFlowModifying; }

			// ===== Stack lifecycle =====
			instr_case(ins::Op_init_pany_type, i) { wr(i.var); }
			instr_case(ins::Op_deinit, i) {}

			// ===== IO =====
			instr_case(ins::Op_input_p64, i) {
				wr(i.dst);
				flags |= IORead | InstructionFlag(MayBlock) | InstructionFlag(ReleaseGIL);
			}
			instr_case(ins::Op_input_p32, i) {
				wr(i.dst);
				flags |= IORead | InstructionFlag(MayBlock) | InstructionFlag(ReleaseGIL);
			}
			instr_case(ins::Op_output_p64, i) {
				rd(i.src);
				flags |= IOWrite | InstructionFlag(RequiresGIL);
			}
			instr_case(ins::Op_output_p32, i) {
				rd(i.src);
				flags |= IOWrite | InstructionFlag(RequiresGIL);
			}
			instr_case(ins::Op_strOutput_pptr, i) {
				rd(i.string_ptr);
				deref_read();
				flags |= IOWrite | InstructionFlag(RequiresGIL);
			}

			// ===== VTable / casts =====
			// setVTable / resetVTable write the vtable slot through the pointer
			instr_case(ins::Op_setVTable_pptr_type, i) {
				rd(i.object_ptr);
				deref_write();
			}
			instr_case(ins::Op_resetVTable_pptr, i) {
				rd(i.object_ptr);
				deref_write();
			}
			// upcast/downcast operate on the pointer value itself; downcast peeks at the vtable
			instr_case(ins::Op_upcast_pptr_pptr, i) {
				wr(i.dst);
				rd(i.src);
			}
			instr_case(ins::Op_downcast_pptr_pptr, i) {
				wr(i.dst);
				rd(i.src);
				deref_read();
			}
			instr_case(ins::Op_virtual_call_pptr_method, i) {
				// Known limitation: a virtual call is opaque, so we conservatively raise every
				// flag. As a result no execution config (no_io / read_only / single_thread) can
				// admit code that performs a dynamic dispatch. Lifting this needs per-callsite
				// devirtualization or a vtable-level effect summary.
				flags |= IORead | IOWrite | GlobalRead | GlobalWrite | Call | CallExternal
				       | Multithread | RequiresGIL | ReleaseGIL | ControlFlowModifying | MayBlock;
			}

			// ===== Allocation / deref / refs =====
			instr_case(ins::Op_alloc_pptr_type, i) {
				wr(i.ptr);
				flags |= MayBlock;
			}
			// free modifies the pointed-to memory (deallocation)
			instr_case(ins::Op_free_pptr, i) {
				rd(i.ptr);
				deref_write();
			}
			instr_case(ins::Op_store_pptr_pany, i) {
				rd(i.dst_ptr);
				rd(i.src);
				deref_write();
			}
			instr_case(ins::Op_load_pany_pptr, i) {
				wr(i.dst);
				rd(i.src_ptr);
				deref_read();
			}
			// ref/lea-style ops just compute or take an address — no actual deref
			instr_case(ins::Op_ref_pptr_pany, i) {
				wr(i.dst_ptr);
				rd(i.src);
			}
			instr_case(ins::Op_ref_pptr_pvnt, i) {
				wr(i.dst_ptr);
				rd(i.src);
			}

			// ===== C pointers =====
			instr_case(ins::Op_load_pany_pcptr, i) {
				wr(i.dst);
				rd(i.src_ptr);
			}
			instr_case(ins::Op_store_pcptr_pany, i) {
				rd(i.dst_ptr);
				rd(i.src);
			}
			instr_case(ins::Op_read_pptr_pcptr, i) {
				rd(i.dst_ptr);
				rd(i.src_ptr);
				deref_write();
			}
			instr_case(ins::Op_write_pcptr_pptr, i) {
				rd(i.dst_ptr);
				rd(i.src_ptr);
				deref_read();
			}
			instr_case(ins::Op_cast_pcptr_pcptr, i) {
				wr(i.dst);
				rd(i.src);
			}
			// Taking an address only reads the pointer operand, like the ref/lea ops above.
			instr_case(ins::Op_cast_pcptr_pptr, i) {
				wr(i.dst);
				rd(i.src_ptr);
			}
			// Decomposing a pointer only reads it, like the cast above.
			instr_case(ins::Op_ptrParts_p64_p64_pptr, i) {
				wr(i.dst_id);
				wr(i.dst_offset);
				rd(i.src_ptr);
			}
			instr_case(ins::Op_add_pcptr_p64, i) {
				rdwr(i.dst);
				rd(i.offset);
			}
			instr_case(ins::Op_add_pcptr_imm, i) { rdwr(i.dst); }

			// ===== Structs =====
			// Lea = pure address arithmetic, no memory access through src_data_ptr
			instr_case(ins::Op_structLea_pptr_pptr_field, i) {
				wr(i.dst_ptr);
				rd(i.src_data_ptr);
			}
			instr_case(ins::Op_structLoad_pany_pptr_field, i) {
				wr(i.dst);
				rd(i.src_data_ptr);
				deref_read();
			}
			instr_case(ins::Op_structStore_pptr_pany_field, i) {
				rd(i.dst_data_ptr);
				rd(i.src);
				deref_write();
			}
			// pste-based: the struct lives in the named place itself
			instr_case(ins::Op_structLea_pptr_pste_field, i) {
				wr(i.dst_ptr);
				rd(i.src_data_struct);
			}
			instr_case(ins::Op_structLoad_pany_pste_field, i) {
				wr(i.dst);
				rd(i.src_data_struct);
			}
			instr_case(ins::Op_structStore_pste_pany_field, i) {
				rdwr(i.dst_data_struct);
				rd(i.src);
			}

			// ===== Tables =====
			// pptr-based fixed-size
			instr_case(ins::Op_fixedSizeTableLea_pptr_pptr_p64, i) {
				wr(i.dst_ptr);
				rd(i.src_table_ptr);
				rd(i.index);
			}
			instr_case(ins::Op_fixedSizeTableLoad_pany_pptr_p64, i) {
				wr(i.dst);
				rd(i.src_table_ptr);
				rd(i.index);
				deref_read();
			}
			instr_case(ins::Op_fixedSizeTableStore_pptr_pany_p64, i) {
				rd(i.dst_table_ptr);
				rd(i.src);
				rd(i.index);
				deref_write();
			}
			// pfst-based (table lives in the place itself)
			instr_case(ins::Op_fixedSizeTableLea_pptr_pfst_p64, i) {
				wr(i.dst_ptr);
				rd(i.src_table);
				rd(i.index);
			}
			instr_case(ins::Op_fixedSizeTableLoad_pany_pfst_p64, i) {
				wr(i.dst);
				rd(i.src_table);
				rd(i.index);
			}
			instr_case(ins::Op_fixedSizeTableStore_pfst_pany_p64, i) {
				rdwr(i.dst_table);
				rd(i.src);
				rd(i.index);
			}
			// dyn (always pptr-based)
			instr_case(ins::Op_dynTableLea_pptr_pptr_p64, i) {
				wr(i.dst_ptr);
				rd(i.src_table_ptr);
				rd(i.index);
			}
			instr_case(ins::Op_dynTableLoad_pany_pptr_p64, i) {
				wr(i.dst);
				rd(i.src_table_ptr);
				rd(i.index);
				deref_read();
			}
			instr_case(ins::Op_dynTableStore_pptr_pany_p64, i) {
				rd(i.dst_table_ptr);
				rd(i.src);
				rd(i.index);
				deref_write();
			}
			instr_case(ins::Op_dynTableReAlloc_pptr_type_p64, i) {
				rd(i.dst_table_ptr);
				rd(i.new_elem_count);
				// realloc rewrites the table memory
				deref_write();
				flags |= MayBlock;
			}
			// fst->dyn is a type-only pointer reinterpretation: the value is copied, no deref
			instr_case(ins::Op_fstToDynTable_pptr_pptr, i) {
				wr(i.dst_table_ptr);
				rd(i.src_table_ptr);
			}

			// ===== Casts (in-place primitive casts) =====
			instr_case(ins::Op_cast_p8_type, i) { rdwr(i.value); }
			instr_case(ins::Op_cast_p16_type, i) { rdwr(i.value); }
			instr_case(ins::Op_cast_p32_type, i) { rdwr(i.value); }
			instr_case(ins::Op_cast_p64_type, i) { rdwr(i.value); }

			// ===== Sign / zero extension =====
			instr_case_many(
				sext_p_p,
				ins::Op_sext_p16_p8,
				ins::Op_sext_p32_p8,
				ins::Op_sext_p64_p8,
				ins::Op_sext_p32_p16,
				ins::Op_sext_p64_p16,
				ins::Op_sext_p64_p32,
				ins::Op_zext_p16_p8,
				ins::Op_zext_p32_p8,
				ins::Op_zext_p64_p8,
				ins::Op_zext_p32_p16,
				ins::Op_zext_p64_p16,
				ins::Op_zext_p64_p32
			) {
				FLAGS_RW_R(sext_p_p);
			}

			// ===== Truncation =====
			instr_case_many(
				trunc_p_p,
				ins::Op_trunc_p8_p16,
				ins::Op_trunc_p8_p32,
				ins::Op_trunc_p8_p64,
				ins::Op_trunc_p16_p32,
				ins::Op_trunc_p16_p64,
				ins::Op_trunc_p32_p64
			) {
				FLAGS_RW_R(trunc_p_p);
			}

			// ===== Int/Float conversions =====
			instr_case_many(
				itofp_p_p,
				ins::Op_sitofp_p32_p8,
				ins::Op_sitofp_p64_p8,
				ins::Op_uitofp_p32_p8,
				ins::Op_uitofp_p64_p8,
				ins::Op_sitofp_p32_p16,
				ins::Op_sitofp_p64_p16,
				ins::Op_uitofp_p32_p16,
				ins::Op_uitofp_p64_p16,
				ins::Op_sitofp_p32_p32,
				ins::Op_sitofp_p64_p32,
				ins::Op_uitofp_p32_p32,
				ins::Op_uitofp_p64_p32,
				ins::Op_sitofp_p32_p64,
				ins::Op_sitofp_p64_p64,
				ins::Op_uitofp_p32_p64,
				ins::Op_uitofp_p64_p64,
				ins::Op_fptosi_p8_p32,
				ins::Op_fptoui_p8_p32,
				ins::Op_fptosi_p16_p32,
				ins::Op_fptoui_p16_p32,
				ins::Op_fptosi_p32_p32,
				ins::Op_fptoui_p32_p32,
				ins::Op_fptosi_p64_p32,
				ins::Op_fptoui_p64_p32,
				ins::Op_fptosi_p8_p64,
				ins::Op_fptoui_p8_p64,
				ins::Op_fptosi_p16_p64,
				ins::Op_fptoui_p16_p64,
				ins::Op_fptosi_p32_p64,
				ins::Op_fptoui_p32_p64,
				ins::Op_fptosi_p64_p64,
				ins::Op_fptoui_p64_p64,
				ins::Op_fptrunc_p32_p64,
				ins::Op_fpext_p64_p32
			) {
				FLAGS_RW_R(itofp_p_p);
			}

			// ===== Misc =====
			instr_case(ins::Op_nop, i) {}
			instr_case(ins::Op_exit, i) { flags |= ControlFlowModifying; }
			instr_case(ins::Op_initFromVMValue, i) {}
			instr_case(ins::Comment, i) {}
			instr_default { CORE_PANIC("Unhandled instruction: ", internal_value.name()); }
		}
		POP_DIAGNOSTIC

#undef FLAGS_RW
#undef FLAGS_RW_R
#undef FLAGS_CMP
#undef FLAGS_CMP_IMM

		return flags;
	}
}
