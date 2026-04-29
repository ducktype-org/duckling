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
				return {};
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
				return {Mutlithread};
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

		auto is_global = [&](base::StrID n) { return globals.contains(n); };
		auto rd = [&](auto const& place) {
			if (is_global(place.var_name)) flags |= GlobalRead;
		};
		auto wr = [&](auto const& place) {
			if (is_global(place.var_name)) flags |= GlobalWrite;
		};
		// dst that is read and then written (arith/cmov/cast in-place)
		auto rdwr = [&](auto const& place) {
			if (is_global(place.var_name)) flags |= GlobalRead | InstructionFlag(GlobalWrite);
		};

		// We can't have a pointers to global values so do nothing here
		auto deref_read  = [&] {  };
		auto deref_write = [&] {  };

		// Per-shape `instr_case` shorthands. Cover the common patterns where the
		// instruction's name and arg shape uniquely determine the read/write set.
		// Defined locally so they don't leak to other translation units.
#define FLAGS_WR(NAME)       instr_case(ins::Op_##NAME, i) { wr(i.dst); }
#define FLAGS_WR_RD(NAME)    instr_case(ins::Op_##NAME, i) { wr(i.dst); rd(i.src); }
#define FLAGS_RDWR(NAME)     instr_case(ins::Op_##NAME, i) { rdwr(i.dst); }
#define FLAGS_RDWR_RD(NAME)  instr_case(ins::Op_##NAME, i) { rdwr(i.dst); rd(i.src); }
#define FLAGS_CMP(NAME)      instr_case(ins::Op_##NAME, i) { rd(i.lhs); rd(i.rhs); }
#define FLAGS_CMP_IMM(NAME)  instr_case(ins::Op_##NAME, i) { rd(i.lhs); }

		PUSH_DIAGNOSTIC
		UNHANDLED_ENUM
		instr_match(instruction) {
			// ===== Pure moves: dst written, src (place) read =====
			FLAGS_WR(mov_p8_imm)        FLAGS_WR(mov_p16_imm)
			FLAGS_WR(mov_p32_imm)       FLAGS_WR(mov_p64_imm)
			FLAGS_WR(mov_popq_imm)      FLAGS_WR(setNull_pptr)

			FLAGS_WR_RD(mov_p8_p8)      FLAGS_WR_RD(mov_p16_p16)
			FLAGS_WR_RD(mov_p32_p32)    FLAGS_WR_RD(mov_p64_p64)
			FLAGS_WR_RD(mov_pptr_pptr)  FLAGS_WR_RD(mov_pste_pste)
			FLAGS_WR_RD(mov_pfst_pfst)  FLAGS_WR_RD(mov_popq_popq)

			// ===== Conditional moves: dst is read (kept conditionally) and written =====
			FLAGS_RDWR_RD(cmov_p8_p8)   FLAGS_RDWR_RD(cmov_p16_p16)
			FLAGS_RDWR_RD(cmov_p32_p32) FLAGS_RDWR_RD(cmov_p64_p64)
			FLAGS_RDWR(cmov_p8_imm)     FLAGS_RDWR(cmov_p16_imm)
			FLAGS_RDWR(cmov_p32_imm)    FLAGS_RDWR(cmov_p64_imm)

			// ===== Binary arithmetic: dst = dst op src =====
			FLAGS_RDWR_RD(add_p64_p64)  FLAGS_RDWR(add_p64_imm)
			FLAGS_RDWR_RD(add_p32_p32)  FLAGS_RDWR(add_p32_imm)
			FLAGS_RDWR_RD(add_p16_p16)  FLAGS_RDWR(add_p16_imm)
			FLAGS_RDWR_RD(add_p8_p8)    FLAGS_RDWR(add_p8_imm)
			FLAGS_RDWR_RD(sub_p64_p64)  FLAGS_RDWR(sub_p64_imm)
			FLAGS_RDWR_RD(sub_p32_p32)  FLAGS_RDWR(sub_p32_imm)
			FLAGS_RDWR_RD(sub_p16_p16)  FLAGS_RDWR(sub_p16_imm)
			FLAGS_RDWR_RD(sub_p8_p8)    FLAGS_RDWR(sub_p8_imm)
			FLAGS_RDWR_RD(mul_p64_p64)  FLAGS_RDWR(mul_p64_imm)
			FLAGS_RDWR_RD(mul_p32_p32)  FLAGS_RDWR(mul_p32_imm)
			FLAGS_RDWR_RD(mul_p16_p16)  FLAGS_RDWR(mul_p16_imm)
			FLAGS_RDWR_RD(mul_p8_p8)    FLAGS_RDWR(mul_p8_imm)
			FLAGS_RDWR_RD(div_p64_p64)  FLAGS_RDWR(div_p64_imm)
			FLAGS_RDWR_RD(div_p32_p32)  FLAGS_RDWR(div_p32_imm)
			FLAGS_RDWR_RD(div_p16_p16)  FLAGS_RDWR(div_p16_imm)
			FLAGS_RDWR_RD(div_p8_p8)    FLAGS_RDWR(div_p8_imm)
			FLAGS_RDWR_RD(mod_p64_p64)  FLAGS_RDWR(mod_p64_imm)
			FLAGS_RDWR_RD(mod_p32_p32)  FLAGS_RDWR(mod_p32_imm)
			FLAGS_RDWR_RD(mod_p16_p16)  FLAGS_RDWR(mod_p16_imm)
			FLAGS_RDWR_RD(mod_p8_p8)    FLAGS_RDWR(mod_p8_imm)

			// Unsigned arith
			FLAGS_RDWR_RD(umul_p64_p64) FLAGS_RDWR(umul_p64_imm)
			FLAGS_RDWR_RD(umul_p32_p32) FLAGS_RDWR(umul_p32_imm)
			FLAGS_RDWR_RD(umul_p16_p16) FLAGS_RDWR(umul_p16_imm)
			FLAGS_RDWR_RD(umul_p8_p8)   FLAGS_RDWR(umul_p8_imm)
			FLAGS_RDWR_RD(umod_p64_p64) FLAGS_RDWR(umod_p64_imm)
			FLAGS_RDWR_RD(umod_p32_p32) FLAGS_RDWR(umod_p32_imm)
			FLAGS_RDWR_RD(umod_p16_p16) FLAGS_RDWR(umod_p16_imm)
			FLAGS_RDWR_RD(umod_p8_p8)   FLAGS_RDWR(umod_p8_imm)
			FLAGS_RDWR_RD(udiv_p64_p64) FLAGS_RDWR(udiv_p64_imm)
			FLAGS_RDWR_RD(udiv_p32_p32) FLAGS_RDWR(udiv_p32_imm)
			FLAGS_RDWR_RD(udiv_p16_p16) FLAGS_RDWR(udiv_p16_imm)
			FLAGS_RDWR_RD(udiv_p8_p8)   FLAGS_RDWR(udiv_p8_imm)

			// Floating point
			FLAGS_RDWR_RD(fadd_p64_p64) FLAGS_RDWR(fadd_p64_imm)
			FLAGS_RDWR_RD(fadd_p32_p32) FLAGS_RDWR(fadd_p32_imm)
			FLAGS_RDWR_RD(fsub_p64_p64) FLAGS_RDWR(fsub_p64_imm)
			FLAGS_RDWR_RD(fsub_p32_p32) FLAGS_RDWR(fsub_p32_imm)
			FLAGS_RDWR_RD(fmul_p64_p64) FLAGS_RDWR(fmul_p64_imm)
			FLAGS_RDWR_RD(fmul_p32_p32) FLAGS_RDWR(fmul_p32_imm)
			FLAGS_RDWR_RD(fdiv_p64_p64) FLAGS_RDWR(fdiv_p64_imm)
			FLAGS_RDWR_RD(fdiv_p32_p32) FLAGS_RDWR(fdiv_p32_imm)

			// Unary arith / negation / logical-not
			FLAGS_RDWR(neg_p64)  FLAGS_RDWR(neg_p32)
			FLAGS_RDWR(neg_p16)  FLAGS_RDWR(neg_p8)
			FLAGS_RDWR(fneg_p64) FLAGS_RDWR(fneg_p32)
			FLAGS_RDWR(log_not_p8)

			// Logical (and/or/xor)
			FLAGS_RDWR_RD(log_and_p8_p8) FLAGS_RDWR(log_and_p8_imm)
			FLAGS_RDWR_RD(log_or_p8_p8)  FLAGS_RDWR(log_or_p8_imm)
			FLAGS_RDWR_RD(log_xor_p8_p8) FLAGS_RDWR(log_xor_p8_imm)

			// ===== Comparisons: lhs/rhs are read =====
			FLAGS_CMP(cmpEq_p64_p64)   FLAGS_CMP_IMM(cmpEq_p64_imm)
			FLAGS_CMP(cmpNeq_p64_p64)  FLAGS_CMP_IMM(cmpNeq_p64_imm)
			FLAGS_CMP(cmpGt_p64_p64)   FLAGS_CMP_IMM(cmpGt_p64_imm)
			FLAGS_CMP(cmpGe_p64_p64)   FLAGS_CMP_IMM(cmpGe_p64_imm)
			FLAGS_CMP(ucmpGt_p64_p64)  FLAGS_CMP_IMM(ucmpGt_p64_imm)
			FLAGS_CMP(ucmpGe_p64_p64)  FLAGS_CMP_IMM(ucmpGe_p64_imm)
			FLAGS_CMP(cmpLt_p64_p64)   FLAGS_CMP_IMM(cmpLt_p64_imm)
			FLAGS_CMP(cmpLe_p64_p64)   FLAGS_CMP_IMM(cmpLe_p64_imm)
			FLAGS_CMP(ucmpLt_p64_p64)  FLAGS_CMP_IMM(ucmpLt_p64_imm)
			FLAGS_CMP(ucmpLe_p64_p64)  FLAGS_CMP_IMM(ucmpLe_p64_imm)
			FLAGS_CMP(cmpEq_p32_p32)   FLAGS_CMP_IMM(cmpEq_p32_imm)
			FLAGS_CMP(cmpNeq_p32_p32)  FLAGS_CMP_IMM(cmpNeq_p32_imm)
			FLAGS_CMP(cmpGt_p32_p32)   FLAGS_CMP_IMM(cmpGt_p32_imm)
			FLAGS_CMP(cmpGe_p32_p32)   FLAGS_CMP_IMM(cmpGe_p32_imm)
			FLAGS_CMP(ucmpGt_p32_p32)  FLAGS_CMP_IMM(ucmpGt_p32_imm)
			FLAGS_CMP(ucmpGe_p32_p32)  FLAGS_CMP_IMM(ucmpGe_p32_imm)
			FLAGS_CMP(cmpLt_p32_p32)   FLAGS_CMP_IMM(cmpLt_p32_imm)
			FLAGS_CMP(cmpLe_p32_p32)   FLAGS_CMP_IMM(cmpLe_p32_imm)
			FLAGS_CMP(ucmpLt_p32_p32)  FLAGS_CMP_IMM(ucmpLt_p32_imm)
			FLAGS_CMP(ucmpLe_p32_p32)  FLAGS_CMP_IMM(ucmpLe_p32_imm)
			FLAGS_CMP(cmpEq_p16_p16)   FLAGS_CMP_IMM(cmpEq_p16_imm)
			FLAGS_CMP(cmpNeq_p16_p16)  FLAGS_CMP_IMM(cmpNeq_p16_imm)
			FLAGS_CMP(cmpGt_p16_p16)   FLAGS_CMP_IMM(cmpGt_p16_imm)
			FLAGS_CMP(cmpGe_p16_p16)   FLAGS_CMP_IMM(cmpGe_p16_imm)
			FLAGS_CMP(ucmpGt_p16_p16)  FLAGS_CMP_IMM(ucmpGt_p16_imm)
			FLAGS_CMP(ucmpGe_p16_p16)  FLAGS_CMP_IMM(ucmpGe_p16_imm)
			FLAGS_CMP(cmpLt_p16_p16)   FLAGS_CMP_IMM(cmpLt_p16_imm)
			FLAGS_CMP(cmpLe_p16_p16)   FLAGS_CMP_IMM(cmpLe_p16_imm)
			FLAGS_CMP(ucmpLt_p16_p16)  FLAGS_CMP_IMM(ucmpLt_p16_imm)
			FLAGS_CMP(ucmpLe_p16_p16)  FLAGS_CMP_IMM(ucmpLe_p16_imm)
			FLAGS_CMP(cmpEq_p8_p8)     FLAGS_CMP_IMM(cmpEq_p8_imm)
			FLAGS_CMP(cmpNeq_p8_p8)    FLAGS_CMP_IMM(cmpNeq_p8_imm)
			FLAGS_CMP(cmpGt_p8_p8)     FLAGS_CMP_IMM(cmpGt_p8_imm)
			FLAGS_CMP(cmpGe_p8_p8)     FLAGS_CMP_IMM(cmpGe_p8_imm)
			FLAGS_CMP(ucmpGt_p8_p8)    FLAGS_CMP_IMM(ucmpGt_p8_imm)
			FLAGS_CMP(ucmpGe_p8_p8)    FLAGS_CMP_IMM(ucmpGe_p8_imm)
			FLAGS_CMP(cmpLt_p8_p8)     FLAGS_CMP_IMM(cmpLt_p8_imm)
			FLAGS_CMP(cmpLe_p8_p8)     FLAGS_CMP_IMM(cmpLe_p8_imm)
			FLAGS_CMP(ucmpLt_p8_p8)    FLAGS_CMP_IMM(ucmpLt_p8_imm)
			FLAGS_CMP(ucmpLe_p8_p8)    FLAGS_CMP_IMM(ucmpLe_p8_imm)
			FLAGS_CMP(fcmpEq_p64_p64)  FLAGS_CMP_IMM(fcmpEq_p64_imm)
			FLAGS_CMP(fcmpNeq_p64_p64) FLAGS_CMP_IMM(fcmpNeq_p64_imm)
			FLAGS_CMP(fcmpGt_p64_p64)  FLAGS_CMP_IMM(fcmpGt_p64_imm)
			FLAGS_CMP(fcmpGe_p64_p64)  FLAGS_CMP_IMM(fcmpGe_p64_imm)
			FLAGS_CMP(fcmpLt_p64_p64)  FLAGS_CMP_IMM(fcmpLt_p64_imm)
			FLAGS_CMP(fcmpLe_p64_p64)  FLAGS_CMP_IMM(fcmpLe_p64_imm)
			FLAGS_CMP(fcmpEq_p32_p32)  FLAGS_CMP_IMM(fcmpEq_p32_imm)
			FLAGS_CMP(fcmpNeq_p32_p32) FLAGS_CMP_IMM(fcmpNeq_p32_imm)
			FLAGS_CMP(fcmpGt_p32_p32)  FLAGS_CMP_IMM(fcmpGt_p32_imm)
			FLAGS_CMP(fcmpGe_p32_p32)  FLAGS_CMP_IMM(fcmpGe_p32_imm)
			FLAGS_CMP(fcmpLt_p32_p32)  FLAGS_CMP_IMM(fcmpLt_p32_imm)
			FLAGS_CMP(fcmpLe_p32_p32)  FLAGS_CMP_IMM(fcmpLe_p32_imm)
			instr_case(ins::Op_cmpNull_pptr, i) { rd(i.ptr); }

			// ===== Variants =====
			instr_case(ins::Op_variantSetInner_pvnt_type, i) { rdwr(i.variant); }
			instr_case(ins::Op_variantGetInner_pptr_pvnt_type, i) {
				wr(i.dst_ptr); rd(i.variant);
			}
			instr_case(ins::Op_variantSetInner_pptr_type,      i) {
				rd(i.variant_ptr); deref_write();
			}
			instr_case(ins::Op_variantGetInner_pptr_pptr_type, i) {
				wr(i.dst_ptr); rd(i.variant_ptr); deref_read();
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
				rd(i.string_ptr); deref_read();
				flags |= IOWrite | InstructionFlag(RequiresGIL);
			}

			// ===== VTable / casts =====
			// setVTable / resetVTable write the vtable slot through the pointer
			instr_case(ins::Op_setVTable_pptr_type, i) { rd(i.object_ptr); deref_write(); }
			instr_case(ins::Op_resetVTable_pptr,    i) { rd(i.object_ptr); deref_write(); }
			// upcast/downcast operate on the pointer value itself; downcast peeks at the vtable
			instr_case(ins::Op_upcast_pptr_pptr,    i) { wr(i.dst); rd(i.src); }
			instr_case(ins::Op_downcast_pptr_pptr,  i) { wr(i.dst); rd(i.src); deref_read(); }
			instr_case(ins::Op_virtual_call_pptr_method, i) {
				rd(i.object_ptr); deref_read();  // vtable lookup through ptr
				flags |= Call | InstructionFlag(ControlFlowModifying);
			}

			// ===== Allocation / deref / refs =====
			instr_case(ins::Op_alloc_pptr_type, i) { wr(i.ptr); flags |= MayBlock; }
			// free modifies the pointed-to memory (deallocation)
			instr_case(ins::Op_free_pptr,       i) { rd(i.ptr); deref_write(); }
			instr_case(ins::Op_store_pptr_pany, i) {
				rd(i.dst_ptr); rd(i.src); deref_write();
			}
			instr_case(ins::Op_load_pany_pptr,  i) {
				wr(i.dst); rd(i.src_ptr); deref_read();
			}
			// ref/lea-style ops just compute or take an address — no actual deref
			instr_case(ins::Op_ref_pptr_pany,   i) { wr(i.dst_ptr); rd(i.src); }
			instr_case(ins::Op_ref_pptr_pvnt,   i) { wr(i.dst_ptr); rd(i.src); }

			// ===== Structs =====
			// Lea = pure address arithmetic, no memory access through src_data_ptr
			instr_case(ins::Op_structLea_pptr_pptr_field,   i) { wr(i.dst_ptr); rd(i.src_data_ptr); }
			instr_case(ins::Op_structLoad_pany_pptr_field,  i) {
				wr(i.dst); rd(i.src_data_ptr); deref_read();
			}
			instr_case(ins::Op_structStore_pptr_pany_field, i) {
				rd(i.dst_data_ptr); rd(i.src); deref_write();
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
				wr(i.dst); rd(i.src_table_ptr); rd(i.index); deref_read();
			}
			instr_case(ins::Op_fixedSizeTableStore_pptr_pany_p64, i) {
				rd(i.dst_table_ptr); rd(i.src); rd(i.index); deref_write();
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
				wr(i.dst); rd(i.src_table_ptr); rd(i.index); deref_read();
			}
			instr_case(ins::Op_dynTableStore_pptr_pany_p64, i) {
				rd(i.dst_table_ptr); rd(i.src); rd(i.index); deref_write();
			}
			instr_case(ins::Op_dynTableReAlloc_pptr_type_p64, i) {
				rd(i.dst_table_ptr); rd(i.new_elem_count);
				deref_write();  // realloc rewrites the table memory
				flags |= MayBlock;
			}

			// ===== Casts (in-place primitive casts) =====
			instr_case(ins::Op_cast_p8_type,  i) { rdwr(i.value); }
			instr_case(ins::Op_cast_p16_type, i) { rdwr(i.value); }
			instr_case(ins::Op_cast_p32_type, i) { rdwr(i.value); }
			instr_case(ins::Op_cast_p64_type, i) { rdwr(i.value); }

			// ===== Sign / zero extension =====
			FLAGS_WR_RD(sext_p16_p8)  FLAGS_WR_RD(sext_p32_p8)  FLAGS_WR_RD(sext_p64_p8)
			FLAGS_WR_RD(sext_p32_p16) FLAGS_WR_RD(sext_p64_p16) FLAGS_WR_RD(sext_p64_p32)
			FLAGS_WR_RD(zext_p16_p8)  FLAGS_WR_RD(zext_p32_p8)  FLAGS_WR_RD(zext_p64_p8)
			FLAGS_WR_RD(zext_p32_p16) FLAGS_WR_RD(zext_p64_p16) FLAGS_WR_RD(zext_p64_p32)

			// ===== Truncation =====
			FLAGS_WR_RD(trunc_p8_p16)  FLAGS_WR_RD(trunc_p8_p32)  FLAGS_WR_RD(trunc_p8_p64)
			FLAGS_WR_RD(trunc_p16_p32) FLAGS_WR_RD(trunc_p16_p64) FLAGS_WR_RD(trunc_p32_p64)

			// ===== Int/Float conversions =====
			FLAGS_WR_RD(sitofp_p32_p8)  FLAGS_WR_RD(sitofp_p64_p8)
			FLAGS_WR_RD(uitofp_p32_p8)  FLAGS_WR_RD(uitofp_p64_p8)
			FLAGS_WR_RD(sitofp_p32_p16) FLAGS_WR_RD(sitofp_p64_p16)
			FLAGS_WR_RD(uitofp_p32_p16) FLAGS_WR_RD(uitofp_p64_p16)
			FLAGS_WR_RD(sitofp_p32_p32) FLAGS_WR_RD(sitofp_p64_p32)
			FLAGS_WR_RD(uitofp_p32_p32) FLAGS_WR_RD(uitofp_p64_p32)
			FLAGS_WR_RD(sitofp_p32_p64) FLAGS_WR_RD(sitofp_p64_p64)
			FLAGS_WR_RD(uitofp_p32_p64) FLAGS_WR_RD(uitofp_p64_p64)
			FLAGS_WR_RD(fptosi_p8_p32)  FLAGS_WR_RD(fptoui_p8_p32)
			FLAGS_WR_RD(fptosi_p16_p32) FLAGS_WR_RD(fptoui_p16_p32)
			FLAGS_WR_RD(fptosi_p32_p32) FLAGS_WR_RD(fptoui_p32_p32)
			FLAGS_WR_RD(fptosi_p64_p32) FLAGS_WR_RD(fptoui_p64_p32)
			FLAGS_WR_RD(fptosi_p8_p64)  FLAGS_WR_RD(fptoui_p8_p64)
			FLAGS_WR_RD(fptosi_p16_p64) FLAGS_WR_RD(fptoui_p16_p64)
			FLAGS_WR_RD(fptosi_p32_p64) FLAGS_WR_RD(fptoui_p32_p64)
			FLAGS_WR_RD(fptosi_p64_p64) FLAGS_WR_RD(fptoui_p64_p64)
			FLAGS_WR_RD(fptrunc_p32_p64) FLAGS_WR_RD(fpext_p64_p32)

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

#undef FLAGS_WR
#undef FLAGS_WR_RD
#undef FLAGS_RDWR
#undef FLAGS_RDWR_RD
#undef FLAGS_CMP
#undef FLAGS_CMP_IMM

		return flags;
	}
}
