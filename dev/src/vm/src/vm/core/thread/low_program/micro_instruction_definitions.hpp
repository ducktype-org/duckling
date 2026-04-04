/**
 * @file micro_instruction_definitions.hpp
 * @brief Contains definitions of all micro bytecode instructions. Can be used for generating
 * repetitive code based on list of instructions.
 *
 * you can just define `HANDLE_MICRO_INSTR` macro and include this
 * header like so:
 * ```cpp
 *  constexpr usize countMicroInstructions() {
 *  	usize count = 0;
 *		#define HANDLE_MICRO_INSTR(i) count++;
 * 		#include "micro_instruction_definitions.hpp"
 * 		#undef HANDLE_MICRO_INSTR
 * 		return count;
 * 	}
 * ```
 * This above function just counts the instructions, but the possibilities are endless.
 *
 * There are other macros for cases where you want to know
 * what arguments the instruction has - HANDLE_MICRO_INSTR_#ARGS,
 * where # is the number of arguments.
 * You can define them similarly to the above example.
 *
 * You can also override the `DEF_MICRO_INSTR` macro for
 * even higher control.
 */

// @TODO: #1189 This is where you define the new micro instructions.
// For now this file is almost exactly the same as its high bytecode equivalent,
// except for the lack of Op_label and Comment, since they do not make sense as a runtime instructions.

#ifndef HANDLE_MICRO_INSTR
#define DEFAULT_HANDLE_MICRO_INSTR
#define HANDLE_MICRO_INSTR(instr)
#endif

#ifndef HANDLE_MICRO_INSTR_0ARGS
#define DEFAULT_HANDLE_MICRO_INSTR_0ARGS
#define HANDLE_MICRO_INSTR_0ARGS(instr) HANDLE_MICRO_INSTR(instr)
#endif

#ifndef HANDLE_MICRO_INSTR_1ARGS
#define DEFAULT_HANDLE_MICRO_INSTR_1ARGS
#define HANDLE_MICRO_INSTR_1ARGS(instr, arg0_type) HANDLE_MICRO_INSTR(instr)
#endif

#ifndef HANDLE_MICRO_INSTR_2ARGS
#define DEFAULT_HANDLE_MICRO_INSTR_2ARGS
#define HANDLE_MICRO_INSTR_2ARGS(instr, arg0_type, arg1_type) HANDLE_MICRO_INSTR(instr)
#endif

#ifndef DEF_MICRO_INSTR
#define DEFAULT_DEF_MICRO_INSTR
#define GET_MACRO(_instr, _1, _2, NAME, ...) NAME
#define DEF_MICRO_INSTR(...)                                                                      \
	GET_MACRO(                                                                                    \
		__VA_ARGS__, HANDLE_MICRO_INSTR_2ARGS, HANDLE_MICRO_INSTR_1ARGS, HANDLE_MICRO_INSTR_0ARGS \
	)                                                                                             \
	(__VA_ARGS__)
#endif


// ========= MOV OPERATIONS ========

DEF_MICRO_INSTR(mov_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mov_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(cmov_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(cmov_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(mov_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mov_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(cmov_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(cmov_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(mov_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mov_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(cmov_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(cmov_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(mov_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mov_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(cmov_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(cmov_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)


DEF_MICRO_INSTR(mov_g64_g64, vm::low::opargs::Global64, vm::low::opargs::Global64)
DEF_MICRO_INSTR(mov_g64_l64, vm::low::opargs::Global64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(mov_g64_imm, vm::low::opargs::Global64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mov_g32_g32, vm::low::opargs::Global32, vm::low::opargs::Global32)
DEF_MICRO_INSTR(mov_g32_l32, vm::low::opargs::Global32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(mov_g32_imm, vm::low::opargs::Global32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mov_g16_g16, vm::low::opargs::Global16, vm::low::opargs::Global16)
DEF_MICRO_INSTR(mov_g16_l16, vm::low::opargs::Global16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(mov_g16_imm, vm::low::opargs::Global16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mov_g8_g8, vm::low::opargs::Global8, vm::low::opargs::Global8)
DEF_MICRO_INSTR(mov_g8_l8, vm::low::opargs::Global8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(mov_g8_imm, vm::low::opargs::Global8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mov_gptr_lptr, vm::low::opargs::GlobalPtr, vm::low::opargs::StackLocalPtr)
DEF_MICRO_INSTR(mov_l64_g64, vm::low::opargs::StackLocal64, vm::low::opargs::Global64)
DEF_MICRO_INSTR(mov_l32_g32, vm::low::opargs::StackLocal32, vm::low::opargs::Global32)
DEF_MICRO_INSTR(mov_l16_g16, vm::low::opargs::StackLocal16, vm::low::opargs::Global16)
DEF_MICRO_INSTR(mov_l8_g8, vm::low::opargs::StackLocal8, vm::low::opargs::Global8)
DEF_MICRO_INSTR(mov_lptr_gptr, vm::low::opargs::StackLocalPtr, vm::low::opargs::GlobalPtr)

DEF_MICRO_INSTR(
	mov_blste_blste,
	vm::low::opargs::BlockStackLocalStructure,
	vm::low::opargs::BlockStackLocalStructure
)
DEF_MICRO_INSTR(
	mov_blste_gste, vm::low::opargs::BlockStackLocalStructure, vm::low::opargs::GlobalStructure
)
DEF_MICRO_INSTR(
	mov_gste_blste, vm::low::opargs::GlobalStructure, vm::low::opargs::BlockStackLocalStructure
)
DEF_MICRO_INSTR(mov_gste_gste, vm::low::opargs::GlobalStructure, vm::low::opargs::GlobalStructure)

// does a shallow pointer copy
DEF_MICRO_INSTR(mov_lptr_lptr, vm::low::opargs::StackLocalPtr, vm::low::opargs::StackLocalPtr)

// sets pointer to null
DEF_MICRO_INSTR(setNull_lptr, vm::low::opargs::StackLocalPtr)

// It requires a `ext_imm` after this instruction as third argument, defining the size of the opaque
// type in bytes.
DEF_MICRO_INSTR(mov_lopq_lopq, vm::low::opargs::StackLocalOpq, vm::low::opargs::StackLocalOpq)
DEF_MICRO_INSTR(mov_gopq_lopq, vm::low::opargs::GlobalOpq, vm::low::opargs::StackLocalOpq)
DEF_MICRO_INSTR(mov_lopq_gopq, vm::low::opargs::StackLocalOpq, vm::low::opargs::GlobalOpq)
DEF_MICRO_INSTR(mov_lopq_imm, vm::low::opargs::StackLocalOpq, vm::low::opargs::Immediate)

// ========= SIGNED INTEGER ARITHMETIC OPERATIONS ========
DEF_MICRO_INSTR(add_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(add_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(sub_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(sub_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mul_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(mul_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(div_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(div_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mod_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(mod_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(neg_l64, vm::low::opargs::StackLocal64)

DEF_MICRO_INSTR(add_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(add_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(sub_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(sub_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mul_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(mul_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(div_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(div_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mod_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(mod_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(neg_l32, vm::low::opargs::StackLocal32)

DEF_MICRO_INSTR(add_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(add_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(sub_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(sub_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mul_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(mul_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(div_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(div_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mod_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(mod_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(neg_l16, vm::low::opargs::StackLocal16)

DEF_MICRO_INSTR(add_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(add_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(sub_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(sub_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mul_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(mul_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(div_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(div_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mod_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(mod_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(neg_l8, vm::low::opargs::StackLocal8)

// ========= UNSIGNED INTEGER ARITHMETIC OPERATIONS ========
DEF_MICRO_INSTR(umul_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(umul_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(umod_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(umod_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(udiv_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(udiv_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(umul_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(umul_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(umod_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(umod_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(udiv_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(udiv_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(umul_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(umul_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(umod_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(umod_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(udiv_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(udiv_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(umul_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(umul_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(umod_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(umod_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(udiv_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(udiv_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)

// ========= FLOATING POINT OPERATIONS ========
DEF_MICRO_INSTR(fadd_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fadd_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fsub_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fsub_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fmul_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fmul_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fdiv_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fdiv_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fneg_l64, vm::low::opargs::StackLocal64)

DEF_MICRO_INSTR(fadd_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(fadd_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fsub_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(fsub_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fmul_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(fmul_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fdiv_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(fdiv_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fneg_l32, vm::low::opargs::StackLocal32)

// ========= BOOLEAN OPERATIONS ========

// Evaluate logical operations (AND, OR, etc.) on operands as booleans (non-zero = true)
// Result is 0 or 1 stored in the first argument

DEF_MICRO_INSTR(log_and_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(log_and_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(log_or_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(log_or_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(log_xor_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(log_xor_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(log_not_l8, vm::low::opargs::StackLocal8)

// ========= LOGICAL OPERATIONS ========

// --- 64-bit Integer Comparisons ---
DEF_MICRO_INSTR(cmpEq_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(cmpEq_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpNeq_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(cmpNeq_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpGt_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(cmpGt_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpGe_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(cmpGe_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGt_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(ucmpGt_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGe_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(ucmpGe_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpLt_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(cmpLt_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpLe_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(cmpLe_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLt_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(ucmpLt_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLe_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(ucmpLe_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)

// --- 32-bit Integer Comparisons ---
DEF_MICRO_INSTR(cmpEq_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(cmpEq_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpNeq_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(cmpNeq_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpGt_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(cmpGt_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpGe_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(cmpGe_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGt_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(ucmpGt_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGe_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(ucmpGe_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpLt_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(cmpLt_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpLe_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(cmpLe_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLt_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(ucmpLt_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLe_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(ucmpLe_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)

// --- 16-bit Integer Comparisons ---
DEF_MICRO_INSTR(cmpEq_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(cmpEq_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpNeq_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(cmpNeq_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpGt_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(cmpGt_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpGe_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(cmpGe_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGt_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(ucmpGt_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGe_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(ucmpGe_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpLt_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(cmpLt_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpLe_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(cmpLe_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLt_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(ucmpLt_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLe_l16_l16, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(ucmpLe_l16_imm, vm::low::opargs::StackLocal16, vm::low::opargs::Immediate)

// --- 8-bit Integer Comparisons ---
DEF_MICRO_INSTR(cmpEq_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(cmpEq_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpNeq_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(cmpNeq_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpGt_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(cmpGt_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpGe_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(cmpGe_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGt_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(ucmpGt_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGe_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(ucmpGe_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpLt_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(cmpLt_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpLe_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(cmpLe_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLt_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(ucmpLt_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLe_l8_l8, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(ucmpLe_l8_imm, vm::low::opargs::StackLocal8, vm::low::opargs::Immediate)

// --- 64-bit Floating Point Comparisons ---
DEF_MICRO_INSTR(fcmpEq_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fcmpEq_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpNeq_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fcmpNeq_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpGt_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fcmpGt_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpGe_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fcmpGe_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpLt_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fcmpLt_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpLe_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fcmpLe_l64_imm, vm::low::opargs::StackLocal64, vm::low::opargs::Immediate)

// --- 32-bit Floating Point Comparisons ---
DEF_MICRO_INSTR(fcmpEq_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(fcmpEq_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpNeq_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(fcmpNeq_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpGt_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(fcmpGt_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpGe_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(fcmpGe_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpLt_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(fcmpLt_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpLe_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(fcmpLe_l32_imm, vm::low::opargs::StackLocal32, vm::low::opargs::Immediate)

// sets the flag if pointer is null
DEF_MICRO_INSTR(cmpNull_lptr, vm::low::opargs::StackLocalPtr)

// ========= VARIANT OPERATIONS ========


/**
 * Sets `variant`'s inner type to `inner_type`. It also invalidates pointers to its data.
 * @note `ext_type` required to know the variant type quickly at runtime.
 */
DEF_MICRO_INSTR(
	variantSetInner_blvnt_type,
	vm::low::opargs::BlockStackLocalVariant /* variant */,
	vm::low::opargs::Type /* 		 inner_type
    vm::low::opargs::Type 			 variant_type */
)
/**
 * @brief Sets `destination` to point at `variant`'s data. Expects `variant` to has `expected_type`
 * set, and if it's not, `destination` becomes nullptr.
 * @note `ext_type_type` required to know the expected alternative and the variant type.
 */
DEF_MICRO_INSTR(
	variantGetInner_lptr_blvnt,
	vm::low::opargs::StackLocalPtr /* destination */,
	vm::low::opargs::BlockStackLocalVariant /* variant,
    vm::low::opargs::Type 			 expected_type
    vm::low::opargs::Type 			 variant_type */
)

/**
 * @brief Sets inner type of variant under `variant_ptr` to `inner_type`. It also invalidates
 * pointers to its data.
 * @note `ext_type` required to know the variant type quickly at runtime.
 */
DEF_MICRO_INSTR(
	variantSetInner_lptr_type,
	vm::low::opargs::StackLocalPtr /* variant_ptr */,
	vm::low::opargs::Type /* 		 inner_type
    vm::low::opargs::Type 			 variant_type */
)

/**
 * @brief Sets `destination` to point at data of variant under `variant_ptr`. Expects the variant to
 * have `expected_type` set, and if it's not, `destination` becomes nullptr.
 * @note `ext_type_type` required to know the expected alternative and the variant type.
 */
DEF_MICRO_INSTR(
	variantGetInner_lptr_lptr,
	vm::low::opargs::StackLocalPtr /* destination */,
	vm::low::opargs::StackLocalPtr /* variant_ptr,
    vm::low::opargs::Type 			 expected_type
    vm::low::opargs::Type 			 variant_type */
)

// ========= JUMPS ========

DEF_MICRO_INSTR(jmp_label, vm::low::opargs::Label)
DEF_MICRO_INSTR(jmpIf_label, vm::low::opargs::Label)
DEF_MICRO_INSTR(jmpIfNot_label, vm::low::opargs::Label)

// ========= FUNCTION OPERATIONS ========

DEF_MICRO_INSTR(call_func, vm::low::opargs::Function)
#ifdef ENABLE_JIT
// call a function, with the possibility to compile it later
DEF_MICRO_INSTR(jit_call_entrypoint, vm::low::opargs::Function)
#endif
DEF_MICRO_INSTR(call_builtinfunc, vm::low::opargs::BuiltinFunctionID)
DEF_MICRO_INSTR(call_cfunc, vm::low::opargs::ExtCFunction)

DEF_MICRO_INSTR(set_threadctx, vm::low::opargs::Function)

// return while performing a tail call
DEF_MICRO_INSTR(ret_tailcall_func, vm::low::opargs::Function)
// return
DEF_MICRO_INSTR(ret)

// ========= STACK OPERATIONS ========

// initialize local variable on local stack with given type
DEF_MICRO_INSTR(init_blany_type, vm::low::opargs::BlockStackLocalAny, vm::low::opargs::Type)
// pop variable from local stack
DEF_MICRO_INSTR(deinit)

// ========= IO OPERATIONS ========

DEF_MICRO_INSTR(input_l64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(output_l64, vm::low::opargs::StackLocal64)

DEF_MICRO_INSTR(input_l32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(output_l32, vm::low::opargs::StackLocal32)


// ========= CLASS OPERATIONS ========

// initialises vtable pointer
DEF_MICRO_INSTR(setVTable_lptr_type, vm::low::opargs::StackLocalPtr, vm::low::opargs::Type)
// deinitialises vtable pointer
DEF_MICRO_INSTR(resetVTable_lptr, vm::low::opargs::StackLocalPtr)
// casts pointed object to its superclass
DEF_MICRO_INSTR(upcast_lptr_lptr, vm::low::opargs::StackLocalPtr, vm::low::opargs::StackLocalPtr)
// tries to cast pointed object to its subclass, requires that ext_64 is next
DEF_MICRO_INSTR(downcast_lptr_lptr, vm::low::opargs::StackLocalPtr, vm::low::opargs::StackLocalPtr)
// calls a method of specified name on an a pointer. Performs the dynamic dispatch.
DEF_MICRO_INSTR(virtual_call_lptr_method, vm::low::opargs::StackLocalPtr, vm::low::opargs::MethodName)

// ========= GENERAL POINTER OPERATIONS ========

// allocates given type, stores pointer
DEF_MICRO_INSTR(alloc_lptr_type, vm::low::opargs::StackLocalPtr, vm::low::opargs::Type)
// frees block under pointer
DEF_MICRO_INSTR(free_lptr, vm::low::opargs::StackLocalPtr)


// stores local data at pointer
DEF_MICRO_INSTR(store_lptr_blany, vm::low::opargs::StackLocalPtr, vm::low::opargs::BlockStackLocalAny)
// dereferences pointer and stores into local
DEF_MICRO_INSTR(load_blany_lptr, vm::low::opargs::BlockStackLocalAny, vm::low::opargs::StackLocalPtr)

// stores reference to local object of any type T in pointer<T>
DEF_MICRO_INSTR(ref_lptr_blany, vm::low::opargs::StackLocalPtr, vm::low::opargs::BlockStackLocalAny)
// stores reference to global object of any type T in pointer<T>
DEF_MICRO_INSTR(ref_lptr_gany, vm::low::opargs::StackLocalPtr, vm::low::opargs::GlobalAny)

// ========= STRUCTURE OPERATIONS ========

// expects `ext_field` to be the next instruction
// loads effective address of struct field
DEF_MICRO_INSTR(
	structLea_lptr_lptr,
	vm::low::opargs::StackLocalPtr /* destination */,
	vm::low::opargs::StackLocalPtr /* source,
    vm::low::opargs::Field 			 field */
)
// expects `ext_field` to be the next instruction
DEF_MICRO_INSTR(
	structLoad_blany_lptr,
	vm::low::opargs::BlockStackLocalAny /* destination */,
	vm::low::opargs::StackLocalPtr /* data_ptr,
    vm::low::opargs::Field 			 field */
)
// expects `ext_field` to be the next instruction
DEF_MICRO_INSTR(
	structStore_lptr_blany,
	vm::low::opargs::StackLocalPtr /* data_ptr */,
	vm::low::opargs::BlockStackLocalAny /* source ,
    vm::low::opargs::Field 			 field */
)

// Same as above, but using struct from local stack

// expects `ext_field` to be the next instruction
DEF_MICRO_INSTR(
	structLea_lptr_blste,
	vm::low::opargs::StackLocalPtr /* destination */,
	vm::low::opargs::BlockStackLocalStructure /* source,
    vm::low::opargs::Field 			 field */
)

// expects `ext_field` to be the next instruction
DEF_MICRO_INSTR(
	structLoad_blany_blste,
	vm::low::opargs::BlockStackLocalAny /* destination */,
	vm::low::opargs::BlockStackLocalStructure /* data_struct,
    vm::low::opargs::Field 			 field */
)

// expects `ext_field` to be the next instruction
DEF_MICRO_INSTR(
	structStore_blste_blany,
	vm::low::opargs::BlockStackLocalStructure /* data_struct */,
	vm::low::opargs::BlockStackLocalAny /* source ,
    vm::low::opargs::Field 			 field */
)

// ========= TABLE OPERATIONS ========

// expects `ext_l64` to be the next instruction
DEF_MICRO_INSTR(
	fixedSizeTableLea_lptr_lptr,
	vm::low::opargs::StackLocalPtr /* destination */,
	vm::low::opargs::StackLocalPtr /* table_ptr,
    vm::low::opargs::StackLocal64 	 index */
)
// expects `ext_l64` to be the next instruction
DEF_MICRO_INSTR(
	fixedSizeTableLoad_blany_lptr,
	vm::low::opargs::BlockStackLocalAny /* destination */,
	vm::low::opargs::StackLocalPtr /* table_ptr,
    vm::low::opargs::StackLocal64 	 index */
)

// expects `ext_l64` to be the next instruction
DEF_MICRO_INSTR(
	fixedSizeTableStore_lptr_blany,
	vm::low::opargs::StackLocalPtr /* table_ptr */,
	vm::low::opargs::BlockStackLocalAny /* source,
    vm::low::opargs::StackLocal64 	 index */
)

// expects `ext_l64` to be the next instruction
DEF_MICRO_INSTR(
	dynTableLea_lptr_lptr,
	vm::low::opargs::StackLocalPtr /* destination */,
	vm::low::opargs::StackLocalPtr /* table_ptr,
    vm::low::opargs::StackLocal64 	 index */
)
// expects `ext_l64` to be the next instruction
DEF_MICRO_INSTR(
	dynTableLoad_blany_lptr,
	vm::low::opargs::BlockStackLocalAny /* destination */,
	vm::low::opargs::StackLocalPtr /* table_ptr,
    vm::low::opargs::StackLocal64 	 index */
)

// expects `ext_l64` to be the next instruction
DEF_MICRO_INSTR(
	dynTableStore_lptr_blany,
	vm::low::opargs::StackLocalPtr /* table_ptr */,
	vm::low::opargs::BlockStackLocalAny /* source,
    vm::low::opargs::StackLocal64 	 index */
)

/**
 * @brief Re-allocates dynamic table under `table_ptr` with
 * `new_elem_count` elements. If given nullptr, then it will allocate
 * a new array.
 * `table_type` is type of the dynamic table itself, not the element type.
 * @note It's counter-intuitive, but if a reallocation has happened, this
 *  instruction will not modify pointer data (unlike in C).
 * @note `ext_l64` is required to tell the count of elements
 */
DEF_MICRO_INSTR(
	dynTableReAlloc_lptr_type,
	vm::low::opargs::StackLocalPtr /* table_ptr */,
	vm::low::opargs::Type /* table_type ,
vm::low::opargs::StackLocal64     new_elem_count */
)

/**
 * @brief Outputs a dynamic table of bytes as a string.
 */
DEF_MICRO_INSTR(
	strOutput_lptr, vm::low::opargs::StackLocalPtr /* string_ptr */
)

// ========= CONVERSION OPERATIONS ========
// Sign Extension
DEF_MICRO_INSTR(sext_l16_l8, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(sext_l32_l8, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(sext_l64_l8, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(sext_l32_l16, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(sext_l64_l16, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(sext_l64_l32, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal32)

// Zero Extension
DEF_MICRO_INSTR(zext_l16_l8, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(zext_l32_l8, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(zext_l64_l8, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(zext_l32_l16, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(zext_l64_l16, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(zext_l64_l32, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal32)

// Truncation
DEF_MICRO_INSTR(trunc_l8_l16, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(trunc_l8_l32, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(trunc_l8_l64, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(trunc_l16_l32, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(trunc_l16_l64, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(trunc_l32_l64, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal64)

// Int to Float
DEF_MICRO_INSTR(sitofp_l32_l8, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(uitofp_l32_l8, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(sitofp_l32_l16, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(uitofp_l32_l16, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(sitofp_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(uitofp_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(sitofp_l32_l64, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(uitofp_l32_l64, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal64)

DEF_MICRO_INSTR(sitofp_l64_l8, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(uitofp_l64_l8, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal8)
DEF_MICRO_INSTR(sitofp_l64_l16, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(uitofp_l64_l16, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal16)
DEF_MICRO_INSTR(sitofp_l64_l32, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(uitofp_l64_l32, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(sitofp_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(uitofp_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)

// Float to Int (Saturating)
DEF_MICRO_INSTR(fptosi_l8_l32, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(fptoui_l8_l32, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(fptosi_l16_l32, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(fptoui_l16_l32, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(fptosi_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(fptoui_l32_l32, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(fptosi_l64_l32, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal32)
DEF_MICRO_INSTR(fptoui_l64_l32, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal32)

DEF_MICRO_INSTR(fptosi_l8_l64, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fptoui_l8_l64, vm::low::opargs::StackLocal8, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fptosi_l16_l64, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fptoui_l16_l64, vm::low::opargs::StackLocal16, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fptosi_l32_l64, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fptoui_l32_l64, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fptosi_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fptoui_l64_l64, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal64)

DEF_MICRO_INSTR(fptrunc_l32_l64, vm::low::opargs::StackLocal32, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(fpext_l64_l32, vm::low::opargs::StackLocal64, vm::low::opargs::StackLocal32)

// ========= EXT DEFINITIONS ========

// passes additional argument to preceding instruction
DEF_MICRO_INSTR(ext_l64, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(ext_imm, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ext_type, vm::low::opargs::Type)
DEF_MICRO_INSTR(ext_field, vm::low::opargs::Field)
DEF_MICRO_INSTR(ext_type_field, vm::low::opargs::Type, vm::low::opargs::Field)
DEF_MICRO_INSTR(ext_type_l64, vm::low::opargs::Type, vm::low::opargs::StackLocal64)
DEF_MICRO_INSTR(ext_type_type, vm::low::opargs::Type, vm::low::opargs::Type)

// ========= MISC ========


DEF_MICRO_INSTR(nop)

// terminates execution
DEF_MICRO_INSTR(exit)

DEF_MICRO_INSTR(breakpoint)

DEF_MICRO_INSTR(stepGil)

/**
 * @brief This is a very internal instruction, that should not be used in regular bytecode.
 * It is a helper for start functions.
 * @arg0 - pointer to a VmValue.
 * @arg1 - n/a.
 */
DEF_MICRO_INSTR(initFromVmValue)

#ifdef DEFAULT_HANDLE_MICRO_INSTR
#undef DEFAULT_HANDLE_MICRO_INSTR
#undef HANDLE_MICRO_INSTR
#endif

#ifdef DEFAULT_HANDLE_MICRO_INSTR_0ARGS
#undef DEFAULT_HANDLE_MICRO_INSTR_0ARGS
#undef HANDLE_MICRO_INSTR_0ARGS
#endif

#ifdef DEFAULT_HANDLE_MICRO_INSTR_1ARGS
#undef DEFAULT_HANDLE_MICRO_INSTR_1ARGS
#undef HANDLE_MICRO_INSTR_1ARGS
#endif

#ifdef DEFAULT_HANDLE_MICRO_INSTR_2ARGS
#undef DEFAULT_HANDLE_MICRO_INSTR_2ARGS
#undef HANDLE_MICRO_INSTR_2ARGS
#endif

#ifdef DEFAULT_DEF_MICRO_INSTR
#undef DEFAULT_DEF_MICRO_INSTR
#undef DEF_MICRO_INSTR
#undef GET_MACRO
#endif
