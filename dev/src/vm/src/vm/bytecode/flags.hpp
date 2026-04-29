#pragma once
#include <base/extend_cpp/flag.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/core/builtin_functions.hpp>
#include "instructions.hpp"

MAKE_FLAG_TYPE(vm::code, InstructionFlagOptions, InstructionFlag,
	IORead,
	IOWrite,
	GlobalRead,
	GlobalWrite,
	Call,
	CallExternal,
	Mutlithread,

	RequiresGIL,           // Instrukcja WYMAGA GIL
    ReleaseGIL,            // Instrukcja MOŻE puścić GIL (długa operacja)
    ControlFlowModifying,  // Zmienia przepływ kontroli (jmp, branch, call, ret)
    MayBlock
)

MAKE_FLAG_TYPE(vm::code, FunctionFlagOptions, FunctionFlag,
	IORead,
	IOWrite,
	GlobalRead,
	GlobalWrite,
	Call,
	CallExternal,
	Mutlithread,

	RequiresGIL,           // Instrukcja WYMAGA GIL
    ReleaseGIL,            // Instrukcja MOŻE puścić GIL (długa operacja)
    ControlFlowModifying,  // Zmienia przepływ kontroli (jmp, branch, call, ret)
    MayBlock
)

namespace vm::code {
	/**
	 * @brief Returns the flags describing observable effects of a builtin function.
	 */
	inline FunctionFlag getFlagsForBuiltinFunction(base::StrID name) {
		using enum FunctionFlagOptions;

		auto builtin_func_opt = vm::builtins::getBuiltinFunctionID(name);
		if (!builtin_func_opt) {
			CORE_PANIC("Function name ", name, " is not a builtin function");
		}
		switch (builtin_func_opt.value()) {
			case vm::builtins::BuiltinFunctionID::InputI64:
				// reads from stdin, blocks waiting for the user
				return FunctionFlag(IORead) | MayBlock | ReleaseGIL;
			case vm::builtins::BuiltinFunctionID::OutputI64:
			case vm::builtins::BuiltinFunctionID::OutputString:
				return FunctionFlag(IOWrite) | RequiresGIL;
			case vm::builtins::BuiltinFunctionID::Stoi:
				// pure conversion, no observable effects
				return FunctionFlag();
			case vm::builtins::BuiltinFunctionID::StartThread:
				return FunctionFlag(Mutlithread) | ControlFlowModifying;
			case vm::builtins::BuiltinFunctionID::JoinThread:
				return FunctionFlag(Mutlithread) | MayBlock | ReleaseGIL;
			case vm::builtins::BuiltinFunctionID::CreateMutex:
			case vm::builtins::BuiltinFunctionID::UnlockMutex:
			case vm::builtins::BuiltinFunctionID::DestroyMutex:
			case vm::builtins::BuiltinFunctionID::CreateCV:
			case vm::builtins::BuiltinFunctionID::NotifyCV:
			case vm::builtins::BuiltinFunctionID::NotifyAllCV:
			case vm::builtins::BuiltinFunctionID::DestroyCV:
				return FunctionFlag(Mutlithread);
			case vm::builtins::BuiltinFunctionID::LockMutex:
			case vm::builtins::BuiltinFunctionID::WaitCV:
				return FunctionFlag(Mutlithread) | MayBlock | ReleaseGIL;
		}
		CORE_PANIC("Invalid builtin function ID");
	}

	namespace internal {
		/** @brief Translates a FunctionFlag to the corresponding InstructionFlag. */
		inline InstructionFlag functionFlagToInstructionFlag(FunctionFlag f) {
			using FFO = FunctionFlagOptions;
			using IFO = InstructionFlagOptions;
			InstructionFlag out;
			if (f.contains(FFO::IORead))               out |= IFO::IORead;
			if (f.contains(FFO::IOWrite))              out |= IFO::IOWrite;
			if (f.contains(FFO::GlobalRead))           out |= IFO::GlobalRead;
			if (f.contains(FFO::GlobalWrite))          out |= IFO::GlobalWrite;
			if (f.contains(FFO::Call))                 out |= IFO::Call;
			if (f.contains(FFO::CallExternal))         out |= IFO::CallExternal;
			if (f.contains(FFO::Mutlithread))          out |= IFO::Mutlithread;
			if (f.contains(FFO::RequiresGIL))          out |= IFO::RequiresGIL;
			if (f.contains(FFO::ReleaseGIL))           out |= IFO::ReleaseGIL;
			if (f.contains(FFO::ControlFlowModifying)) out |= IFO::ControlFlowModifying;
			if (f.contains(FFO::MayBlock))             out |= IFO::MayBlock;
			return out;
		}
	}

	/**
	 * @brief Returns the flags describing observable effects of an instruction.
	 *
	 * Place arguments are inspected against @p globals to decide if their access
	 * counts as a global read or a global write. Calls to builtin functions
	 * propagate the builtin's effect flags. Calls to external C functions are
	 * marked with CallExternal and treated as opaque (assumed to touch IO).
	 */
	inline InstructionFlag getFlagsForInstruction(
		Instruction instruction,
		const ObjIdNameMap<GlobalData>&        globals,
		const ObjIdNameMap<ExternalCFunction>& ext_c_functions
	) {
		using enum InstructionFlagOptions;
		namespace ins = instructions;

		InstructionFlag flags;

		auto isGlobal = [&](base::StrID n) { return globals.contains(n); };
		auto rd = [&](auto const& place) {
			if (isGlobal(place.var_name)) flags |= GlobalRead;
		};
		auto wr = [&](auto const& place) {
			if (isGlobal(place.var_name)) flags |= GlobalWrite;
		};
		// dst that is read and then written (arith/cmov/cast in-place)
		auto rdwr = [&](auto const& place) {
			if (isGlobal(place.var_name)) flags |= GlobalRead | InstructionFlag(GlobalWrite);
		};
		// Conservative pointer-deref marking. We cannot statically determine what a
		// pointer aliases (no points-to analysis exists), so any read/write through
		// a pointer is conservatively assumed to potentially touch a global. This
		// keeps the analysis sound for correctness proofs (e.g. data-race freedom).
		auto derefRead  = [&] { flags |= GlobalRead;  };
		auto derefWrite = [&] { flags |= GlobalWrite; };

		PUSH_DIAGNOSTIC
		UNHANDLED_ENUM
		instr_match(instruction) {
			// ===== Pure moves: dst written, src (place) read =====
			instr_case(ins::Op_mov_p8_imm,    i) { wr(i.dst); }
			instr_case(ins::Op_mov_p16_imm,   i) { wr(i.dst); }
			instr_case(ins::Op_mov_p32_imm,   i) { wr(i.dst); }
			instr_case(ins::Op_mov_p64_imm,   i) { wr(i.dst); }
			instr_case(ins::Op_mov_p8_p8,     i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_mov_p16_p16,   i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_mov_p32_p32,   i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_mov_p64_p64,   i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_mov_pptr_pptr, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_mov_pste_pste, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_mov_pfst_pfst, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_mov_popq_popq, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_mov_popq_imm,  i) { wr(i.dst); }
			instr_case(ins::Op_setNull_pptr,  i) { wr(i.dst); }

			// ===== Conditional moves: dst is read (kept conditionally) and written =====
			instr_case(ins::Op_cmov_p8_p8,    i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_cmov_p16_p16,  i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_cmov_p32_p32,  i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_cmov_p64_p64,  i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_cmov_p8_imm,   i) { rdwr(i.dst); }
			instr_case(ins::Op_cmov_p16_imm,  i) { rdwr(i.dst); }
			instr_case(ins::Op_cmov_p32_imm,  i) { rdwr(i.dst); }
			instr_case(ins::Op_cmov_p64_imm,  i) { rdwr(i.dst); }

			// ===== Binary arithmetic: dst = dst op src =====
			instr_case(ins::Op_add_p64_p64, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_add_p64_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_add_p32_p32, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_add_p32_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_add_p16_p16, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_add_p16_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_add_p8_p8,   i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_add_p8_imm,  i) { rdwr(i.dst); }
			instr_case(ins::Op_sub_p64_p64, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_sub_p64_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_sub_p32_p32, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_sub_p32_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_sub_p16_p16, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_sub_p16_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_sub_p8_p8,   i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_sub_p8_imm,  i) { rdwr(i.dst); }
			instr_case(ins::Op_mul_p64_p64, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_mul_p64_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_mul_p32_p32, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_mul_p32_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_mul_p16_p16, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_mul_p16_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_mul_p8_p8,   i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_mul_p8_imm,  i) { rdwr(i.dst); }
			instr_case(ins::Op_div_p64_p64, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_div_p64_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_div_p32_p32, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_div_p32_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_div_p16_p16, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_div_p16_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_div_p8_p8,   i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_div_p8_imm,  i) { rdwr(i.dst); }
			instr_case(ins::Op_mod_p64_p64, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_mod_p64_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_mod_p32_p32, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_mod_p32_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_mod_p16_p16, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_mod_p16_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_mod_p8_p8,   i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_mod_p8_imm,  i) { rdwr(i.dst); }

			// Unsigned arith
			instr_case(ins::Op_umul_p64_p64, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_umul_p64_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_umul_p32_p32, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_umul_p32_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_umul_p16_p16, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_umul_p16_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_umul_p8_p8,   i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_umul_p8_imm,  i) { rdwr(i.dst); }
			instr_case(ins::Op_umod_p64_p64, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_umod_p64_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_umod_p32_p32, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_umod_p32_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_umod_p16_p16, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_umod_p16_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_umod_p8_p8,   i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_umod_p8_imm,  i) { rdwr(i.dst); }
			instr_case(ins::Op_udiv_p64_p64, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_udiv_p64_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_udiv_p32_p32, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_udiv_p32_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_udiv_p16_p16, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_udiv_p16_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_udiv_p8_p8,   i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_udiv_p8_imm,  i) { rdwr(i.dst); }

			// Floating point
			instr_case(ins::Op_fadd_p64_p64, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_fadd_p64_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_fadd_p32_p32, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_fadd_p32_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_fsub_p64_p64, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_fsub_p64_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_fsub_p32_p32, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_fsub_p32_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_fmul_p64_p64, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_fmul_p64_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_fmul_p32_p32, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_fmul_p32_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_fdiv_p64_p64, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_fdiv_p64_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_fdiv_p32_p32, i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_fdiv_p32_imm, i) { rdwr(i.dst); }

			// Unary arith / negation
			instr_case(ins::Op_neg_p64,  i) { rdwr(i.dst); }
			instr_case(ins::Op_neg_p32,  i) { rdwr(i.dst); }
			instr_case(ins::Op_neg_p16,  i) { rdwr(i.dst); }
			instr_case(ins::Op_neg_p8,   i) { rdwr(i.dst); }
			instr_case(ins::Op_fneg_p64, i) { rdwr(i.dst); }
			instr_case(ins::Op_fneg_p32, i) { rdwr(i.dst); }

			// Logical
			instr_case(ins::Op_log_and_p8_p8,  i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_log_and_p8_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_log_or_p8_p8,   i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_log_or_p8_imm,  i) { rdwr(i.dst); }
			instr_case(ins::Op_log_xor_p8_p8,  i) { rdwr(i.dst); rd(i.src); }
			instr_case(ins::Op_log_xor_p8_imm, i) { rdwr(i.dst); }
			instr_case(ins::Op_log_not_p8,     i) { rdwr(i.dst); }

			// ===== Comparisons: lhs/rhs are read =====
			instr_case(ins::Op_cmpEq_p64_p64,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpEq_p64_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_cmpNeq_p64_p64, i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpNeq_p64_imm, i) { rd(i.lhs); }
			instr_case(ins::Op_cmpGt_p64_p64,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpGt_p64_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_cmpGe_p64_p64,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpGe_p64_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_ucmpGt_p64_p64, i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_ucmpGt_p64_imm, i) { rd(i.lhs); }
			instr_case(ins::Op_ucmpGe_p64_p64, i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_ucmpGe_p64_imm, i) { rd(i.lhs); }
			instr_case(ins::Op_cmpLt_p64_p64,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpLt_p64_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_cmpLe_p64_p64,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpLe_p64_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_ucmpLt_p64_p64, i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_ucmpLt_p64_imm, i) { rd(i.lhs); }
			instr_case(ins::Op_ucmpLe_p64_p64, i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_ucmpLe_p64_imm, i) { rd(i.lhs); }
			instr_case(ins::Op_cmpEq_p32_p32,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpEq_p32_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_cmpNeq_p32_p32, i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpNeq_p32_imm, i) { rd(i.lhs); }
			instr_case(ins::Op_cmpGt_p32_p32,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpGt_p32_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_cmpGe_p32_p32,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpGe_p32_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_ucmpGt_p32_p32, i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_ucmpGt_p32_imm, i) { rd(i.lhs); }
			instr_case(ins::Op_ucmpGe_p32_p32, i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_ucmpGe_p32_imm, i) { rd(i.lhs); }
			instr_case(ins::Op_cmpLt_p32_p32,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpLt_p32_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_cmpLe_p32_p32,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpLe_p32_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_ucmpLt_p32_p32, i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_ucmpLt_p32_imm, i) { rd(i.lhs); }
			instr_case(ins::Op_ucmpLe_p32_p32, i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_ucmpLe_p32_imm, i) { rd(i.lhs); }
			instr_case(ins::Op_cmpEq_p16_p16,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpEq_p16_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_cmpNeq_p16_p16, i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpNeq_p16_imm, i) { rd(i.lhs); }
			instr_case(ins::Op_cmpGt_p16_p16,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpGt_p16_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_cmpGe_p16_p16,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpGe_p16_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_ucmpGt_p16_p16, i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_ucmpGt_p16_imm, i) { rd(i.lhs); }
			instr_case(ins::Op_ucmpGe_p16_p16, i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_ucmpGe_p16_imm, i) { rd(i.lhs); }
			instr_case(ins::Op_cmpLt_p16_p16,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpLt_p16_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_cmpLe_p16_p16,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpLe_p16_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_ucmpLt_p16_p16, i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_ucmpLt_p16_imm, i) { rd(i.lhs); }
			instr_case(ins::Op_ucmpLe_p16_p16, i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_ucmpLe_p16_imm, i) { rd(i.lhs); }
			instr_case(ins::Op_cmpEq_p8_p8,    i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpEq_p8_imm,   i) { rd(i.lhs); }
			instr_case(ins::Op_cmpNeq_p8_p8,   i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpNeq_p8_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_cmpGt_p8_p8,    i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpGt_p8_imm,   i) { rd(i.lhs); }
			instr_case(ins::Op_cmpGe_p8_p8,    i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpGe_p8_imm,   i) { rd(i.lhs); }
			instr_case(ins::Op_ucmpGt_p8_p8,   i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_ucmpGt_p8_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_ucmpGe_p8_p8,   i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_ucmpGe_p8_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_cmpLt_p8_p8,    i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpLt_p8_imm,   i) { rd(i.lhs); }
			instr_case(ins::Op_cmpLe_p8_p8,    i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_cmpLe_p8_imm,   i) { rd(i.lhs); }
			instr_case(ins::Op_ucmpLt_p8_p8,   i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_ucmpLt_p8_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_ucmpLe_p8_p8,   i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_ucmpLe_p8_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_fcmpEq_p64_p64,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_fcmpEq_p64_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_fcmpNeq_p64_p64, i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_fcmpNeq_p64_imm, i) { rd(i.lhs); }
			instr_case(ins::Op_fcmpGt_p64_p64,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_fcmpGt_p64_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_fcmpGe_p64_p64,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_fcmpGe_p64_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_fcmpLt_p64_p64,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_fcmpLt_p64_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_fcmpLe_p64_p64,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_fcmpLe_p64_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_fcmpEq_p32_p32,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_fcmpEq_p32_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_fcmpNeq_p32_p32, i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_fcmpNeq_p32_imm, i) { rd(i.lhs); }
			instr_case(ins::Op_fcmpGt_p32_p32,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_fcmpGt_p32_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_fcmpGe_p32_p32,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_fcmpGe_p32_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_fcmpLt_p32_p32,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_fcmpLt_p32_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_fcmpLe_p32_p32,  i) { rd(i.lhs); rd(i.rhs); }
			instr_case(ins::Op_fcmpLe_p32_imm,  i) { rd(i.lhs); }
			instr_case(ins::Op_cmpNull_pptr,    i) { rd(i.ptr); }

			// ===== Variants =====
			instr_case(ins::Op_variantSetInner_pvnt_type, i) { rdwr(i.variant); }
			instr_case(ins::Op_variantGetInner_pptr_pvnt_type, i) {
				wr(i.dst_ptr); rd(i.variant);
			}
			instr_case(ins::Op_variantSetInner_pptr_type,      i) {
				rd(i.variant_ptr); derefWrite();
			}
			instr_case(ins::Op_variantGetInner_pptr_pptr_type, i) {
				wr(i.dst_ptr); rd(i.variant_ptr); derefRead();
			}

			// ===== Labels & jumps =====
			instr_case(ins::Op_label,        i) { (void)i; }
			instr_case(ins::Op_jmp_label,    i) { (void)i; flags |= ControlFlowModifying; }
			instr_case(ins::Op_jmpIf_label,  i) { (void)i; flags |= ControlFlowModifying; }
			instr_case(ins::Op_jmpIfNot_label, i) { (void)i; flags |= ControlFlowModifying; }

			// ===== Calls =====
			instr_case(ins::Op_call_func, i) {
				(void)i;
				flags |= Call | InstructionFlag(ControlFlowModifying);
			}
			instr_case(ins::Op_call_builtinfunc, i) {
				flags |= Call | InstructionFlag(ControlFlowModifying);
				flags |= internal::functionFlagToInstructionFlag(
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
			instr_case(ins::Op_set_threadctx, i) {
				(void)i;
				flags |= Call | InstructionFlag(Mutlithread)
				       | InstructionFlag(ControlFlowModifying);
			}
			instr_case(ins::Op_ret_tailcall_func, i) {
				(void)i;
				flags |= Call | InstructionFlag(ControlFlowModifying);
			}
			instr_case(ins::Op_ret, i) { (void)i; flags |= ControlFlowModifying; }

			// ===== Stack lifecycle =====
			instr_case(ins::Op_init_pany_type, i) { wr(i.var); }
			instr_case(ins::Op_deinit, i)          { (void)i; }

			// ===== IO =====
			instr_case(ins::Op_input_p64,  i) {
				wr(i.dst);
				flags |= IORead | InstructionFlag(MayBlock) | InstructionFlag(ReleaseGIL);
			}
			instr_case(ins::Op_input_p32,  i) {
				wr(i.dst);
				flags |= IORead | InstructionFlag(MayBlock) | InstructionFlag(ReleaseGIL);
			}
			instr_case(ins::Op_output_p64, i) {
				rd(i.src); flags |= IOWrite | InstructionFlag(RequiresGIL);
			}
			instr_case(ins::Op_output_p32, i) {
				rd(i.src); flags |= IOWrite | InstructionFlag(RequiresGIL);
			}
			instr_case(ins::Op_strOutput_pptr, i) {
				rd(i.string_ptr); derefRead();
				flags |= IOWrite | InstructionFlag(RequiresGIL);
			}

			// ===== VTable / casts =====
			// setVTable / resetVTable write the vtable slot through the pointer
			instr_case(ins::Op_setVTable_pptr_type, i) { rd(i.object_ptr); derefWrite(); }
			instr_case(ins::Op_resetVTable_pptr,    i) { rd(i.object_ptr); derefWrite(); }
			// upcast/downcast operate on the pointer value itself; downcast peeks at the vtable
			instr_case(ins::Op_upcast_pptr_pptr,    i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_downcast_pptr_pptr,  i) { wr(i.dst); rd(i.src); derefRead(); }
			instr_case(ins::Op_virtual_call_pptr_method, i) {
				rd(i.object_ptr); derefRead();  // vtable lookup through ptr
				flags |= Call | InstructionFlag(ControlFlowModifying);
			}

			// ===== Allocation / deref / refs =====
			instr_case(ins::Op_alloc_pptr_type, i) { wr(i.ptr); flags |= MayBlock; }
			// free modifies the pointed-to memory (deallocation)
			instr_case(ins::Op_free_pptr,       i) { rd(i.ptr); derefWrite(); }
			instr_case(ins::Op_store_pptr_pany, i) {
				rd(i.dst_ptr); rd(i.src); derefWrite();
			}
			instr_case(ins::Op_load_pany_pptr,  i) {
				wr(i.dst); rd(i.src_ptr); derefRead();
			}
			// ref/lea-style ops just compute or take an address — no actual deref
			instr_case(ins::Op_ref_pptr_pany,   i) { wr(i.dst_ptr); rd(i.src); }
			instr_case(ins::Op_ref_pptr_pvnt,   i) { wr(i.dst_ptr); rd(i.src); }

			// ===== Structs =====
			// Lea = pure address arithmetic, no memory access through src_data_ptr
			instr_case(ins::Op_structLea_pptr_pptr_field,   i) { wr(i.dst_ptr); rd(i.src_data_ptr); }
			instr_case(ins::Op_structLoad_pany_pptr_field,  i) {
				wr(i.dst); rd(i.src_data_ptr); derefRead();
			}
			instr_case(ins::Op_structStore_pptr_pany_field, i) {
				rd(i.dst_data_ptr); rd(i.src); derefWrite();
			}
			// pste-based: the struct lives in the named place itself
			instr_case(ins::Op_structLea_pptr_pste_field,   i) { wr(i.dst_ptr); rd(i.src_data_struct); }
			instr_case(ins::Op_structLoad_pany_pste_field,  i) { wr(i.dst); rd(i.src_data_struct); }
			instr_case(ins::Op_structStore_pste_pany_field, i) { rdwr(i.dst_data_struct); rd(i.src); }

			// ===== Tables =====
			// pptr-based fixed-size
			instr_case(ins::Op_fixedSizeTableLea_pptr_pptr_p64,   i) {
				wr(i.dst_ptr); rd(i.src_table_ptr); rd(i.index);
			}
			instr_case(ins::Op_fixedSizeTableLoad_pany_pptr_p64,  i) {
				wr(i.dst); rd(i.src_table_ptr); rd(i.index); derefRead();
			}
			instr_case(ins::Op_fixedSizeTableStore_pptr_pany_p64, i) {
				rd(i.dst_table_ptr); rd(i.src); rd(i.index); derefWrite();
			}
			// pfst-based (table lives in the place itself)
			instr_case(ins::Op_fixedSizeTableLea_pptr_pfst_p64,   i) {
				wr(i.dst_ptr); rd(i.src_table); rd(i.index);
			}
			instr_case(ins::Op_fixedSizeTableLoad_pany_pfst_p64,  i) {
				wr(i.dst); rd(i.src_table); rd(i.index);
			}
			instr_case(ins::Op_fixedSizeTableStore_pfst_pany_p64, i) {
				rdwr(i.dst_table); rd(i.src); rd(i.index);
			}
			// dyn (always pptr-based)
			instr_case(ins::Op_dynTableLea_pptr_pptr_p64,   i) {
				wr(i.dst_ptr); rd(i.src_table_ptr); rd(i.index);
			}
			instr_case(ins::Op_dynTableLoad_pany_pptr_p64,  i) {
				wr(i.dst); rd(i.src_table_ptr); rd(i.index); derefRead();
			}
			instr_case(ins::Op_dynTableStore_pptr_pany_p64, i) {
				rd(i.dst_table_ptr); rd(i.src); rd(i.index); derefWrite();
			}
			instr_case(ins::Op_dynTableReAlloc_pptr_type_p64, i) {
				rd(i.dst_table_ptr); rd(i.new_elem_count);
				derefWrite();  // realloc rewrites the table memory
				flags |= MayBlock;
			}

			// ===== Casts (in-place primitive casts) =====
			instr_case(ins::Op_cast_p8_type,  i) { rdwr(i.value); }
			instr_case(ins::Op_cast_p16_type, i) { rdwr(i.value); }
			instr_case(ins::Op_cast_p32_type, i) { rdwr(i.value); }
			instr_case(ins::Op_cast_p64_type, i) { rdwr(i.value); }

			// ===== Sign / zero extension =====
			instr_case(ins::Op_sext_p16_p8,  i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_sext_p32_p8,  i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_sext_p64_p8,  i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_sext_p32_p16, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_sext_p64_p16, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_sext_p64_p32, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_zext_p16_p8,  i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_zext_p32_p8,  i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_zext_p64_p8,  i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_zext_p32_p16, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_zext_p64_p16, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_zext_p64_p32, i) { wr(i.dst); rd(i.src); }

			// ===== Truncation =====
			instr_case(ins::Op_trunc_p8_p16,  i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_trunc_p8_p32,  i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_trunc_p8_p64,  i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_trunc_p16_p32, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_trunc_p16_p64, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_trunc_p32_p64, i) { wr(i.dst); rd(i.src); }

			// ===== Int/Float conversions =====
			instr_case(ins::Op_sitofp_p32_p8,  i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_sitofp_p64_p8,  i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_uitofp_p32_p8,  i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_uitofp_p64_p8,  i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_sitofp_p32_p16, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_sitofp_p64_p16, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_uitofp_p32_p16, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_uitofp_p64_p16, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_sitofp_p32_p32, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_sitofp_p64_p32, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_uitofp_p32_p32, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_uitofp_p64_p32, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_sitofp_p32_p64, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_sitofp_p64_p64, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_uitofp_p32_p64, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_uitofp_p64_p64, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fptosi_p8_p32,  i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fptoui_p8_p32,  i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fptosi_p16_p32, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fptoui_p16_p32, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fptosi_p32_p32, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fptoui_p32_p32, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fptosi_p64_p32, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fptoui_p64_p32, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fptosi_p8_p64,  i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fptoui_p8_p64,  i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fptosi_p16_p64, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fptoui_p16_p64, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fptosi_p32_p64, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fptoui_p32_p64, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fptosi_p64_p64, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fptoui_p64_p64, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fptrunc_p32_p64, i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_fpext_p64_p32,   i) { wr(i.dst); rd(i.src); }

			// ===== Misc =====
			instr_case(ins::Op_nop,             i) { (void)i; }
			instr_case(ins::Op_exit,            i) { (void)i; flags |= ControlFlowModifying; }
			instr_case(ins::Op_breakpoint,      i) { (void)i; flags |= MayBlock; }
			instr_case(ins::Op_initFromVmValue, i) { (void)i; }
			instr_case(ins::Comment,            i) { (void)i; }
			instr_default {
				CORE_PANIC("Unhandled instruction");
			}
		}
		POP_DIAGNOSTIC

		return flags;
	}
}
