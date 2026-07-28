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
			// CptrRead/CptrWrite note below) they are not classified as GlobalRead/GlobalWrite.
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
		case builtins::BuiltinFunctionID::CptrRead:
		case builtins::BuiltinFunctionID::CptrWrite:
			// Raw memory copies between VM memory and C memory addressed by a `cptr`. They perform
			// no console I/O and do not spawn threads. The VM side is accessed through a pointer
			// operand, and like every other pointer-deref write in this module (see the
			// `deref_write` no-op in getFlagsForInstruction) such accesses are not classified as
			// GlobalRead/GlobalWrite: the analysis cannot tell whether the pointer aliases a
			// global. So no config restriction applies here.
			return {};
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
			if (is_global(place.var_name)) flags |= InstructionFlag(GlobalRead) | GlobalWrite;
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
#define FLAGS_W(NAME) \
	instr_case(ins::Op_##NAME, i) { wr(i.dst); }
#define FLAGS_W_R(NAME)             \
	instr_case(ins::Op_##NAME, i) { \
		wr(i.dst);                  \
		rd(i.src);                  \
	}
#define FLAGS_RW(NAME) \
	instr_case(ins::Op_##NAME, i) { rdwr(i.dst); }
#define FLAGS_RW_R(NAME)            \
	instr_case(ins::Op_##NAME, i) { \
		rdwr(i.dst);                \
		rd(i.src);                  \
	}
#define FLAGS_CMP(NAME)             \
	instr_case(ins::Op_##NAME, i) { \
		rd(i.lhs);                  \
		rd(i.rhs);                  \
	}
#define FLAGS_CMP_IMM(NAME) \
	instr_case(ins::Op_##NAME, i) { rd(i.lhs); }

		// UNHANDLED_ENUM suppresses the switch-exhaustiveness warning, so adding a new
		// instruction still compiles. Any instruction that lacks a case below therefore reaches
		// the `instr_default` CORE_PANIC at load time instead of failing to build: every new
		// instruction must be given an entry in this map.
		PUSH_DIAGNOSTIC
		UNHANDLED_ENUM
		instr_match(instruction) {
			// ===== Pure moves: dst written, src (place) read =====
			FLAGS_W(mov_p8_imm)
			FLAGS_W(mov_p16_imm)
			FLAGS_W(mov_p32_imm)
			FLAGS_W(mov_p64_imm)
			FLAGS_W(setNull_pptr)

			FLAGS_W_R(mov_p8_p8)
			FLAGS_W_R(mov_p16_p16)
			FLAGS_W_R(mov_p32_p32)
			FLAGS_W_R(mov_p64_p64)
			FLAGS_W_R(mov_pptr_pptr)
			FLAGS_W_R(mov_pcpt_pcpt)
			FLAGS_W_R(mov_pste_pste)
			FLAGS_W_R(mov_pfst_pfst)
			FLAGS_W_R(mov_popq_popq)

			// ===== Conditional moves: dst is read (kept conditionally) and written =====
			FLAGS_RW_R(cmov_p8_p8)
			FLAGS_RW_R(cmov_p16_p16)
			FLAGS_RW_R(cmov_p32_p32)
			FLAGS_RW_R(cmov_p64_p64)
			FLAGS_RW(cmov_p8_imm)
			FLAGS_RW(cmov_p16_imm)
			FLAGS_RW(cmov_p32_imm)
			FLAGS_RW(cmov_p64_imm)

			// ===== Binary arithmetic: dst = dst op src =====
			FLAGS_RW_R(add_p64_p64)
			FLAGS_RW(add_p64_imm)
			FLAGS_RW_R(add_p32_p32)
			FLAGS_RW(add_p32_imm)
			FLAGS_RW_R(add_p16_p16)
			FLAGS_RW(add_p16_imm)
			FLAGS_RW_R(add_p8_p8)
			FLAGS_RW(add_p8_imm)
			FLAGS_RW_R(sub_p64_p64)
			FLAGS_RW(sub_p64_imm)
			FLAGS_RW_R(sub_p32_p32)
			FLAGS_RW(sub_p32_imm)
			FLAGS_RW_R(sub_p16_p16)
			FLAGS_RW(sub_p16_imm)
			FLAGS_RW_R(sub_p8_p8)
			FLAGS_RW(sub_p8_imm)
			FLAGS_RW_R(mul_p64_p64)
			FLAGS_RW(mul_p64_imm)
			FLAGS_RW_R(mul_p32_p32)
			FLAGS_RW(mul_p32_imm)
			FLAGS_RW_R(mul_p16_p16)
			FLAGS_RW(mul_p16_imm)
			FLAGS_RW_R(mul_p8_p8)
			FLAGS_RW(mul_p8_imm)
			FLAGS_RW_R(div_p64_p64)
			FLAGS_RW(div_p64_imm)
			FLAGS_RW_R(div_p32_p32)
			FLAGS_RW(div_p32_imm)
			FLAGS_RW_R(div_p16_p16)
			FLAGS_RW(div_p16_imm)
			FLAGS_RW_R(div_p8_p8)
			FLAGS_RW(div_p8_imm)
			FLAGS_RW_R(mod_p64_p64)
			FLAGS_RW(mod_p64_imm)
			FLAGS_RW_R(mod_p32_p32)
			FLAGS_RW(mod_p32_imm)
			FLAGS_RW_R(mod_p16_p16)
			FLAGS_RW(mod_p16_imm)
			FLAGS_RW_R(mod_p8_p8)
			FLAGS_RW(mod_p8_imm)

			// Unsigned arith
			FLAGS_RW_R(umul_p64_p64)
			FLAGS_RW(umul_p64_imm)
			FLAGS_RW_R(umul_p32_p32)
			FLAGS_RW(umul_p32_imm)
			FLAGS_RW_R(umul_p16_p16)
			FLAGS_RW(umul_p16_imm)
			FLAGS_RW_R(umul_p8_p8)
			FLAGS_RW(umul_p8_imm)
			FLAGS_RW_R(umod_p64_p64)
			FLAGS_RW(umod_p64_imm)
			FLAGS_RW_R(umod_p32_p32)
			FLAGS_RW(umod_p32_imm)
			FLAGS_RW_R(umod_p16_p16)
			FLAGS_RW(umod_p16_imm)
			FLAGS_RW_R(umod_p8_p8)
			FLAGS_RW(umod_p8_imm)
			FLAGS_RW_R(udiv_p64_p64)
			FLAGS_RW(udiv_p64_imm)
			FLAGS_RW_R(udiv_p32_p32)
			FLAGS_RW(udiv_p32_imm)
			FLAGS_RW_R(udiv_p16_p16)
			FLAGS_RW(udiv_p16_imm)
			FLAGS_RW_R(udiv_p8_p8)
			FLAGS_RW(udiv_p8_imm)

			// Floating point
			FLAGS_RW_R(fadd_p64_p64)
			FLAGS_RW(fadd_p64_imm)
			FLAGS_RW_R(fadd_p32_p32)
			FLAGS_RW(fadd_p32_imm)
			FLAGS_RW_R(fsub_p64_p64)
			FLAGS_RW(fsub_p64_imm)
			FLAGS_RW_R(fsub_p32_p32)
			FLAGS_RW(fsub_p32_imm)
			FLAGS_RW_R(fmul_p64_p64)
			FLAGS_RW(fmul_p64_imm)
			FLAGS_RW_R(fmul_p32_p32)
			FLAGS_RW(fmul_p32_imm)
			FLAGS_RW_R(fdiv_p64_p64)
			FLAGS_RW(fdiv_p64_imm)
			FLAGS_RW_R(fdiv_p32_p32)
			FLAGS_RW(fdiv_p32_imm)

			// Unary arith / negation / logical-not
			FLAGS_RW(neg_p64)
			FLAGS_RW(neg_p32)
			FLAGS_RW(neg_p16)
			FLAGS_RW(neg_p8)
			FLAGS_RW(fneg_p64)
			FLAGS_RW(fneg_p32)
			FLAGS_RW(log_not_p8)

			// Logical (and/or/xor)
			FLAGS_RW_R(log_and_p8_p8)
			FLAGS_RW(log_and_p8_imm)
			FLAGS_RW_R(log_or_p8_p8)
			FLAGS_RW(log_or_p8_imm)
			FLAGS_RW_R(log_xor_p8_p8)
			FLAGS_RW(log_xor_p8_imm)

			// ===== Comparisons: lhs/rhs are read =====
			FLAGS_CMP(cmpEq_p64_p64)
			FLAGS_CMP_IMM(cmpEq_p64_imm)
			FLAGS_CMP(cmpNeq_p64_p64)
			FLAGS_CMP_IMM(cmpNeq_p64_imm)
			FLAGS_CMP(cmpGt_p64_p64)
			FLAGS_CMP_IMM(cmpGt_p64_imm)
			FLAGS_CMP(cmpGe_p64_p64)
			FLAGS_CMP_IMM(cmpGe_p64_imm)
			FLAGS_CMP(ucmpGt_p64_p64)
			FLAGS_CMP_IMM(ucmpGt_p64_imm)
			FLAGS_CMP(ucmpGe_p64_p64)
			FLAGS_CMP_IMM(ucmpGe_p64_imm)
			FLAGS_CMP(cmpLt_p64_p64)
			FLAGS_CMP_IMM(cmpLt_p64_imm)
			FLAGS_CMP(cmpLe_p64_p64)
			FLAGS_CMP_IMM(cmpLe_p64_imm)
			FLAGS_CMP(ucmpLt_p64_p64)
			FLAGS_CMP_IMM(ucmpLt_p64_imm)
			FLAGS_CMP(ucmpLe_p64_p64)
			FLAGS_CMP_IMM(ucmpLe_p64_imm)
			FLAGS_CMP(cmpEq_p32_p32)
			FLAGS_CMP_IMM(cmpEq_p32_imm)
			FLAGS_CMP(cmpNeq_p32_p32)
			FLAGS_CMP_IMM(cmpNeq_p32_imm)
			FLAGS_CMP(cmpGt_p32_p32)
			FLAGS_CMP_IMM(cmpGt_p32_imm)
			FLAGS_CMP(cmpGe_p32_p32)
			FLAGS_CMP_IMM(cmpGe_p32_imm)
			FLAGS_CMP(ucmpGt_p32_p32)
			FLAGS_CMP_IMM(ucmpGt_p32_imm)
			FLAGS_CMP(ucmpGe_p32_p32)
			FLAGS_CMP_IMM(ucmpGe_p32_imm)
			FLAGS_CMP(cmpLt_p32_p32)
			FLAGS_CMP_IMM(cmpLt_p32_imm)
			FLAGS_CMP(cmpLe_p32_p32)
			FLAGS_CMP_IMM(cmpLe_p32_imm)
			FLAGS_CMP(ucmpLt_p32_p32)
			FLAGS_CMP_IMM(ucmpLt_p32_imm)
			FLAGS_CMP(ucmpLe_p32_p32)
			FLAGS_CMP_IMM(ucmpLe_p32_imm)
			FLAGS_CMP(cmpEq_p16_p16)
			FLAGS_CMP_IMM(cmpEq_p16_imm)
			FLAGS_CMP(cmpNeq_p16_p16)
			FLAGS_CMP_IMM(cmpNeq_p16_imm)
			FLAGS_CMP(cmpGt_p16_p16)
			FLAGS_CMP_IMM(cmpGt_p16_imm)
			FLAGS_CMP(cmpGe_p16_p16)
			FLAGS_CMP_IMM(cmpGe_p16_imm)
			FLAGS_CMP(ucmpGt_p16_p16)
			FLAGS_CMP_IMM(ucmpGt_p16_imm)
			FLAGS_CMP(ucmpGe_p16_p16)
			FLAGS_CMP_IMM(ucmpGe_p16_imm)
			FLAGS_CMP(cmpLt_p16_p16)
			FLAGS_CMP_IMM(cmpLt_p16_imm)
			FLAGS_CMP(cmpLe_p16_p16)
			FLAGS_CMP_IMM(cmpLe_p16_imm)
			FLAGS_CMP(ucmpLt_p16_p16)
			FLAGS_CMP_IMM(ucmpLt_p16_imm)
			FLAGS_CMP(ucmpLe_p16_p16)
			FLAGS_CMP_IMM(ucmpLe_p16_imm)
			FLAGS_CMP(cmpEq_p8_p8)
			FLAGS_CMP_IMM(cmpEq_p8_imm)
			FLAGS_CMP(cmpNeq_p8_p8)
			FLAGS_CMP_IMM(cmpNeq_p8_imm)
			FLAGS_CMP(cmpGt_p8_p8)
			FLAGS_CMP_IMM(cmpGt_p8_imm)
			FLAGS_CMP(cmpGe_p8_p8)
			FLAGS_CMP_IMM(cmpGe_p8_imm)
			FLAGS_CMP(ucmpGt_p8_p8)
			FLAGS_CMP_IMM(ucmpGt_p8_imm)
			FLAGS_CMP(ucmpGe_p8_p8)
			FLAGS_CMP_IMM(ucmpGe_p8_imm)
			FLAGS_CMP(cmpLt_p8_p8)
			FLAGS_CMP_IMM(cmpLt_p8_imm)
			FLAGS_CMP(cmpLe_p8_p8)
			FLAGS_CMP_IMM(cmpLe_p8_imm)
			FLAGS_CMP(ucmpLt_p8_p8)
			FLAGS_CMP_IMM(ucmpLt_p8_imm)
			FLAGS_CMP(ucmpLe_p8_p8)
			FLAGS_CMP_IMM(ucmpLe_p8_imm)
			FLAGS_CMP(fcmpEq_p64_p64)
			FLAGS_CMP_IMM(fcmpEq_p64_imm)
			FLAGS_CMP(fcmpNeq_p64_p64)
			FLAGS_CMP_IMM(fcmpNeq_p64_imm)
			FLAGS_CMP(fcmpGt_p64_p64)
			FLAGS_CMP_IMM(fcmpGt_p64_imm)
			FLAGS_CMP(fcmpGe_p64_p64)
			FLAGS_CMP_IMM(fcmpGe_p64_imm)
			FLAGS_CMP(fcmpLt_p64_p64)
			FLAGS_CMP_IMM(fcmpLt_p64_imm)
			FLAGS_CMP(fcmpLe_p64_p64)
			FLAGS_CMP_IMM(fcmpLe_p64_imm)
			FLAGS_CMP(fcmpEq_p32_p32)
			FLAGS_CMP_IMM(fcmpEq_p32_imm)
			FLAGS_CMP(fcmpNeq_p32_p32)
			FLAGS_CMP_IMM(fcmpNeq_p32_imm)
			FLAGS_CMP(fcmpGt_p32_p32)
			FLAGS_CMP_IMM(fcmpGt_p32_imm)
			FLAGS_CMP(fcmpGe_p32_p32)
			FLAGS_CMP_IMM(fcmpGe_p32_imm)
			FLAGS_CMP(fcmpLt_p32_p32)
			FLAGS_CMP_IMM(fcmpLt_p32_imm)
			FLAGS_CMP(fcmpLe_p32_p32)
			FLAGS_CMP_IMM(fcmpLe_p32_imm) instr_case(ins::Op_cmpNull_pptr, i) { rd(i.ptr); }

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
			FLAGS_W_R(sext_p16_p8)
			FLAGS_W_R(sext_p32_p8)
			FLAGS_W_R(sext_p64_p8)
			FLAGS_W_R(sext_p32_p16)
			FLAGS_W_R(sext_p64_p16)
			FLAGS_W_R(sext_p64_p32)
			FLAGS_W_R(zext_p16_p8)
			FLAGS_W_R(zext_p32_p8)
			FLAGS_W_R(zext_p64_p8)
			FLAGS_W_R(zext_p32_p16)
			FLAGS_W_R(zext_p64_p16)
			FLAGS_W_R(zext_p64_p32)

			// ===== Truncation =====
			FLAGS_W_R(trunc_p8_p16)
			FLAGS_W_R(trunc_p8_p32)
			FLAGS_W_R(trunc_p8_p64)
			FLAGS_W_R(trunc_p16_p32)
			FLAGS_W_R(trunc_p16_p64)
			FLAGS_W_R(trunc_p32_p64)

			// ===== Int/Float conversions =====
			FLAGS_W_R(sitofp_p32_p8)
			FLAGS_W_R(sitofp_p64_p8)
			FLAGS_W_R(uitofp_p32_p8)
			FLAGS_W_R(uitofp_p64_p8)
			FLAGS_W_R(sitofp_p32_p16)
			FLAGS_W_R(sitofp_p64_p16)
			FLAGS_W_R(uitofp_p32_p16)
			FLAGS_W_R(uitofp_p64_p16)
			FLAGS_W_R(sitofp_p32_p32)
			FLAGS_W_R(sitofp_p64_p32)
			FLAGS_W_R(uitofp_p32_p32)
			FLAGS_W_R(uitofp_p64_p32)
			FLAGS_W_R(sitofp_p32_p64)
			FLAGS_W_R(sitofp_p64_p64)
			FLAGS_W_R(uitofp_p32_p64)
			FLAGS_W_R(uitofp_p64_p64)
			FLAGS_W_R(fptosi_p8_p32)
			FLAGS_W_R(fptoui_p8_p32)
			FLAGS_W_R(fptosi_p16_p32)
			FLAGS_W_R(fptoui_p16_p32)
			FLAGS_W_R(fptosi_p32_p32)
			FLAGS_W_R(fptoui_p32_p32)
			FLAGS_W_R(fptosi_p64_p32)
			FLAGS_W_R(fptoui_p64_p32)
			FLAGS_W_R(fptosi_p8_p64)
			FLAGS_W_R(fptoui_p8_p64)
			FLAGS_W_R(fptosi_p16_p64)
			FLAGS_W_R(fptoui_p16_p64)
			FLAGS_W_R(fptosi_p32_p64)
			FLAGS_W_R(fptoui_p32_p64)
			FLAGS_W_R(fptosi_p64_p64)
			FLAGS_W_R(fptoui_p64_p64)
			FLAGS_W_R(fptrunc_p32_p64)
			FLAGS_W_R(fpext_p64_p32)

			// ===== Misc =====
			instr_case(ins::Op_nop, i) {}
			instr_case(ins::Op_exit, i) { flags |= ControlFlowModifying; }
			instr_case(ins::Op_initFromVmValue, i) {}
			instr_case(ins::Comment, i) {}
			instr_default { CORE_PANIC("Unhandled instruction: ", internal_value.name()); }
		}
		POP_DIAGNOSTIC

#undef FLAGS_W
#undef FLAGS_W_R
#undef FLAGS_RW
#undef FLAGS_RW_R
#undef FLAGS_CMP
#undef FLAGS_CMP_IMM

		return flags;
	}
}
