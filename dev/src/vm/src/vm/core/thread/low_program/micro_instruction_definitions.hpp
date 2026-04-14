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

DEF_MICRO_INSTR(mov_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_MICRO_INSTR(mov_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(cmov_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(cmov_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)

DEF_MICRO_INSTR(mov_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_MICRO_INSTR(mov_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(cmov_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(cmov_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)

DEF_MICRO_INSTR(mov_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(mov_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(cmov_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(cmov_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_MICRO_INSTR(mov_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(mov_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(cmov_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(cmov_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)


DEF_MICRO_INSTR(mov_g64_g64, vm::opargs::Global64, vm::opargs::Global64)
DEF_MICRO_INSTR(mov_g64_l64, vm::opargs::Global64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(mov_g64_imm, vm::opargs::Global64, vm::opargs::Immediate)
DEF_MICRO_INSTR(mov_g32_g32, vm::opargs::Global32, vm::opargs::Global32)
DEF_MICRO_INSTR(mov_g32_l32, vm::opargs::Global32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(mov_g32_imm, vm::opargs::Global32, vm::opargs::Immediate)
DEF_MICRO_INSTR(mov_g16_g16, vm::opargs::Global16, vm::opargs::Global16)
DEF_MICRO_INSTR(mov_g16_l16, vm::opargs::Global16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(mov_g16_imm, vm::opargs::Global16, vm::opargs::Immediate)
DEF_MICRO_INSTR(mov_g8_g8, vm::opargs::Global8, vm::opargs::Global8)
DEF_MICRO_INSTR(mov_g8_l8, vm::opargs::Global8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(mov_g8_imm, vm::opargs::Global8, vm::opargs::Immediate)
DEF_MICRO_INSTR(mov_gptr_lptr, vm::opargs::GlobalPtr, vm::opargs::StackLocalPtr)
DEF_MICRO_INSTR(mov_l64_g64, vm::opargs::StackLocal64, vm::opargs::Global64)
DEF_MICRO_INSTR(mov_l32_g32, vm::opargs::StackLocal32, vm::opargs::Global32)
DEF_MICRO_INSTR(mov_l16_g16, vm::opargs::StackLocal16, vm::opargs::Global16)
DEF_MICRO_INSTR(mov_l8_g8, vm::opargs::StackLocal8, vm::opargs::Global8)
DEF_MICRO_INSTR(mov_lptr_gptr, vm::opargs::StackLocalPtr, vm::opargs::GlobalPtr)

DEF_MICRO_INSTR(mov_lste_lste, vm::opargs::StackLocalStructure, vm::opargs::StackLocalStructure)
DEF_MICRO_INSTR(mov_lste_gste, vm::opargs::StackLocalStructure, vm::opargs::GlobalStructure)
DEF_MICRO_INSTR(mov_gste_lste, vm::opargs::GlobalStructure, vm::opargs::StackLocalStructure)
DEF_MICRO_INSTR(mov_gste_gste, vm::opargs::GlobalStructure, vm::opargs::GlobalStructure)

// does a shallow pointer copy
DEF_MICRO_INSTR(mov_lptr_lptr, vm::opargs::StackLocalPtr, vm::opargs::StackLocalPtr)

// sets pointer to null
DEF_MICRO_INSTR(setNull_lptr, vm::opargs::StackLocalPtr)

DEF_MICRO_INSTR(mov_lopq_lopq, vm::opargs::StackLocalOpq, vm::opargs::StackLocalOpq)
DEF_MICRO_INSTR(mov_gopq_lopq, vm::opargs::GlobalOpq, vm::opargs::StackLocalOpq)
DEF_MICRO_INSTR(mov_lopq_gopq, vm::opargs::StackLocalOpq, vm::opargs::GlobalOpq)
DEF_MICRO_INSTR(mov_lopq_imm, vm::opargs::StackLocalOpq, vm::opargs::Immediate)

// ========= SIGNED INTEGER ARITHMETIC OPERATIONS ========
DEF_MICRO_INSTR(add_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(add_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(sub_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(sub_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(mul_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(mul_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(div_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(div_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(mod_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(mod_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(neg_l64, vm::opargs::StackLocal64)

DEF_MICRO_INSTR(add_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(add_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(sub_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(sub_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(mul_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(mul_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(div_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(div_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(mod_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(mod_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(neg_l32, vm::opargs::StackLocal32)

DEF_MICRO_INSTR(add_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(add_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_MICRO_INSTR(sub_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(sub_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_MICRO_INSTR(mul_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(mul_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_MICRO_INSTR(div_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(div_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_MICRO_INSTR(mod_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(mod_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_MICRO_INSTR(neg_l16, vm::opargs::StackLocal16)

DEF_MICRO_INSTR(add_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(add_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_MICRO_INSTR(sub_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(sub_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_MICRO_INSTR(mul_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(mul_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_MICRO_INSTR(div_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(div_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_MICRO_INSTR(mod_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(mod_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_MICRO_INSTR(neg_l8, vm::opargs::StackLocal8)

// ========= UNSIGNED INTEGER ARITHMETIC OPERATIONS ========
DEF_MICRO_INSTR(umul_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(umul_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(umod_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(umod_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(udiv_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(udiv_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_MICRO_INSTR(umul_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(umul_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(umod_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(umod_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(udiv_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(udiv_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_MICRO_INSTR(umul_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(umul_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_MICRO_INSTR(umod_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(umod_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_MICRO_INSTR(udiv_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(udiv_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)

DEF_MICRO_INSTR(umul_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(umul_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_MICRO_INSTR(umod_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(umod_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_MICRO_INSTR(udiv_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(udiv_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)

// ========= FLOATING POINT OPERATIONS ========
DEF_MICRO_INSTR(fadd_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fadd_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(fsub_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fsub_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(fmul_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fmul_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(fdiv_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fdiv_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(fneg_l64, vm::opargs::StackLocal64)

DEF_MICRO_INSTR(fadd_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(fadd_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(fsub_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(fsub_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(fmul_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(fmul_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(fdiv_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(fdiv_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(fneg_l32, vm::opargs::StackLocal32)

// ========= BOOLEAN OPERATIONS ========

// Evaluate logical operations (AND, OR, etc.) on operands as booleans (non-zero = true)
// Result is 0 or 1 stored in the first argument

DEF_MICRO_INSTR(log_and_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(log_and_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)

DEF_MICRO_INSTR(log_or_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(log_or_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)

DEF_MICRO_INSTR(log_xor_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(log_xor_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)

DEF_MICRO_INSTR(log_not_l8, vm::opargs::StackLocal8)

// ========= LOGICAL OPERATIONS ========

// --- 64-bit Integer Comparisons ---
DEF_MICRO_INSTR(cmpEq_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(cmpEq_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpNeq_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(cmpNeq_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpGt_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(cmpGt_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpGe_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(cmpGe_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGt_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(ucmpGt_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGe_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(ucmpGe_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpLt_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(cmpLt_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpLe_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(cmpLe_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLt_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(ucmpLt_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLe_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(ucmpLe_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

// --- 32-bit Integer Comparisons ---
DEF_MICRO_INSTR(cmpEq_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(cmpEq_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpNeq_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(cmpNeq_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpGt_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(cmpGt_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpGe_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(cmpGe_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGt_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(ucmpGt_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGe_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(ucmpGe_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpLt_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(cmpLt_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpLe_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(cmpLe_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLt_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(ucmpLt_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLe_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(ucmpLe_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

// --- 16-bit Integer Comparisons ---
DEF_MICRO_INSTR(cmpEq_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(cmpEq_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpNeq_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(cmpNeq_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpGt_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(cmpGt_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpGe_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(cmpGe_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGt_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(ucmpGt_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGe_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(ucmpGe_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpLt_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(cmpLt_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpLe_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(cmpLe_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLt_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(ucmpLt_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLe_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(ucmpLe_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)

// --- 8-bit Integer Comparisons ---
DEF_MICRO_INSTR(cmpEq_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(cmpEq_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpNeq_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(cmpNeq_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpGt_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(cmpGt_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpGe_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(cmpGe_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGt_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(ucmpGt_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGe_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(ucmpGe_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpLt_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(cmpLt_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_MICRO_INSTR(cmpLe_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(cmpLe_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLt_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(ucmpLt_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLe_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(ucmpLe_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)

// --- 64-bit Floating Point Comparisons ---
DEF_MICRO_INSTR(fcmpEq_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fcmpEq_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(fcmpNeq_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fcmpNeq_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(fcmpGt_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fcmpGt_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(fcmpGe_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fcmpGe_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(fcmpLt_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fcmpLt_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_MICRO_INSTR(fcmpLe_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fcmpLe_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

// --- 32-bit Floating Point Comparisons ---
DEF_MICRO_INSTR(fcmpEq_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(fcmpEq_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(fcmpNeq_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(fcmpNeq_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(fcmpGt_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(fcmpGt_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(fcmpGe_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(fcmpGe_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(fcmpLt_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(fcmpLt_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_MICRO_INSTR(fcmpLe_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(fcmpLe_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

// sets the flag if pointer is null
DEF_MICRO_INSTR(cmpNull_lptr, vm::opargs::StackLocalPtr)

// ========= VARIANT OPERATIONS ========


/**
 * Sets `variant`'s inner type to `inner_type`. It also invalidates pointers to its data.
 * @note `ext_type` required to know the variant type quickly at runtime.
 */
DEF_MICRO_INSTR(
	variantSetInner_lvnt_type,
	vm::opargs::StackLocalVnt /* variant */,
	vm::opargs::Type /* 		 inner_type
    vm::opargs::Type 			 variant_type */
)
/**
 * @brief Sets `destination` to point at `variant`'s data. Expects `variant` to has `expected_type`
 * set, and if it's not, `destination` becomes nullptr.
 * @note `ext_type_type` required to know the expected alternative and the variant type.
 */
DEF_MICRO_INSTR(
	variantGetInner_lptr_lvnt,
	vm::opargs::StackLocalPtr /* destination */,
	vm::opargs::StackLocalVnt /* variant,
    vm::opargs::Type 			 expected_type
    vm::opargs::Type 			 variant_type */
)

/**
 * @brief Sets inner type of variant under `variant_ptr` to `inner_type`. It also invalidates
 * pointers to its data.
 * @note `ext_type` required to know the variant type quickly at runtime.
 */
DEF_MICRO_INSTR(
	variantSetInner_lptr_type,
	vm::opargs::StackLocalPtr /* variant_ptr */,
	vm::opargs::Type /* 		 inner_type
    vm::opargs::Type 			 variant_type */
)

/**
 * @brief Sets `destination` to point at data of variant under `variant_ptr`. Expects the variant to
 * have `expected_type` set, and if it's not, `destination` becomes nullptr.
 * @note `ext_type_type` required to know the expected alternative and the variant type.
 */
DEF_MICRO_INSTR(
	variantGetInner_lptr_lptr,
	vm::opargs::StackLocalPtr /* destination */,
	vm::opargs::StackLocalPtr /* variant_ptr,
    vm::opargs::Type 			 expected_type
    vm::opargs::Type 			 variant_type */
)

// ========= JUMPS ========

DEF_MICRO_INSTR(jmp_label, vm::opargs::Label)
DEF_MICRO_INSTR(jmpIf_label, vm::opargs::Label)
DEF_MICRO_INSTR(jmpIfNot_label, vm::opargs::Label)

// ========= FUNCTION OPERATIONS ========

DEF_MICRO_INSTR(call_func, vm::opargs::FunctionName)
#ifdef ENABLE_JIT
// call a function, with the possibility to compile it later
DEF_MICRO_INSTR(jit_call_entrypoint, vm::opargs::FunctionName)
#endif
DEF_MICRO_INSTR(call_builtinfunc, vm::opargs::BuiltinFunctionName)
DEF_MICRO_INSTR(call_cfunc, vm::opargs::ExtCFunctionName)

DEF_MICRO_INSTR(set_threadctx, vm::opargs::FunctionName)

// return while performing a tail call
DEF_MICRO_INSTR(ret_tailcall_func, vm::opargs::FunctionName)
// return
DEF_MICRO_INSTR(ret)

// ========= STACK OPERATIONS ========

// initialize local variable on local stack with given type
DEF_MICRO_INSTR(init_lany_type, vm::opargs::StackLocalAny, vm::opargs::Type)
// pop variable from local stack
DEF_MICRO_INSTR(deinit)

// ========= IO OPERATIONS ========

DEF_MICRO_INSTR(input_l64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(output_l64, vm::opargs::StackLocal64)

DEF_MICRO_INSTR(input_l32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(output_l32, vm::opargs::StackLocal32)


// ========= CLASS OPERATIONS ========

// initialises vtable pointer
DEF_MICRO_INSTR(setVTable_lptr_type, vm::opargs::StackLocalPtr, vm::opargs::Type)
// deinitialises vtable pointer
DEF_MICRO_INSTR(resetVTable_lptr, vm::opargs::StackLocalPtr)
// casts pointed object to its superclass
DEF_MICRO_INSTR(upcast_lptr_lptr, vm::opargs::StackLocalPtr, vm::opargs::StackLocalPtr)
// tries to cast pointed object to its subclass, requires that ext_64 is next
DEF_MICRO_INSTR(downcast_lptr_lptr, vm::opargs::StackLocalPtr, vm::opargs::StackLocalPtr)
// calls a method of specified name on an a pointer. Performs the dynamic dispatch.
DEF_MICRO_INSTR(virtual_call_lptr_method, vm::opargs::StackLocalPtr, vm::opargs::MethodName)

// ========= GENERAL POINTER OPERATIONS ========

// allocates given type, stores pointer
DEF_MICRO_INSTR(alloc_lptr_type, vm::opargs::StackLocalPtr, vm::opargs::Type)
// frees block under pointer
DEF_MICRO_INSTR(free_lptr, vm::opargs::StackLocalPtr)


// stores local data at pointer
DEF_MICRO_INSTR(store_lptr_lany, vm::opargs::StackLocalPtr, vm::opargs::StackLocalAny)
// dereferences pointer and stores into local
DEF_MICRO_INSTR(load_lany_lptr, vm::opargs::StackLocalAny, vm::opargs::StackLocalPtr)

// stores reference to local object of any type T in pointer<T>
DEF_MICRO_INSTR(ref_lptr_lany, vm::opargs::StackLocalPtr, vm::opargs::StackLocalAny)

// ========= STRUCTURE OPERATIONS ========

// expects `ext_field` to be the next instruction
// loads effective address of struct field
DEF_MICRO_INSTR(
	structLea_lptr_lptr,
	vm::opargs::StackLocalPtr /* destination */,
	vm::opargs::StackLocalPtr /* source,
    vm::opargs::Field 			 field */
)
// expects `ext_field` to be the next instruction
DEF_MICRO_INSTR(
	structLoad_lany_lptr,
	vm::opargs::StackLocalAny /* destination */,
	vm::opargs::StackLocalPtr /* data_ptr,
    vm::opargs::Field 			 field */
)
// expects `ext_field` to be the next instruction
DEF_MICRO_INSTR(
	structStore_lptr_lany,
	vm::opargs::StackLocalPtr /* data_ptr */,
	vm::opargs::StackLocalAny /* source ,
    vm::opargs::Field 			 field */
)

// Same as above, but using struct from local stack

// expects `ext_field` to be the next instruction
DEF_MICRO_INSTR(
	structLea_lptr_lste,
	vm::opargs::StackLocalPtr /* destination */,
	vm::opargs::StackLocalStructure /* source,
    vm::opargs::Field 			 field */
)

// expects `ext_field` to be the next instruction
DEF_MICRO_INSTR(
	structLoad_lany_lste,
	vm::opargs::StackLocalAny /* destination */,
	vm::opargs::StackLocalStructure /* data_struct,
    vm::opargs::Field 			 field */
)

// expects `ext_field` to be the next instruction
DEF_MICRO_INSTR(
	structStore_lste_lany,
	vm::opargs::StackLocalStructure /* data_struct */,
	vm::opargs::StackLocalAny /* source ,
    vm::opargs::Field 			 field */
)

// ========= TABLE OPERATIONS ========

// expects `ext_l64` to be the next instruction
DEF_MICRO_INSTR(
	fixedSizeTableLea_lptr_lptr,
	vm::opargs::StackLocalPtr /* destination */,
	vm::opargs::StackLocalPtr /* table_ptr,
    vm::opargs::StackLocal64 	 index */
)
// expects `ext_l64` to be the next instruction
DEF_MICRO_INSTR(
	fixedSizeTableLoad_lany_lptr,
	vm::opargs::StackLocalAny /* destination */,
	vm::opargs::StackLocalPtr /* table_ptr,
    vm::opargs::StackLocal64 	 index */
)

// expects `ext_l64` to be the next instruction
DEF_MICRO_INSTR(
	fixedSizeTableStore_lptr_lany,
	vm::opargs::StackLocalPtr /* table_ptr */,
	vm::opargs::StackLocalAny /* source,
    vm::opargs::StackLocal64 	 index */
)

// expects `ext_l64` to be the next instruction
DEF_MICRO_INSTR(
	dynTableLea_lptr_lptr,
	vm::opargs::StackLocalPtr /* destination */,
	vm::opargs::StackLocalPtr /* table_ptr,
    vm::opargs::StackLocal64 	 index */
)
// expects `ext_l64` to be the next instruction
DEF_MICRO_INSTR(
	dynTableLoad_lany_lptr,
	vm::opargs::StackLocalAny /* destination */,
	vm::opargs::StackLocalPtr /* table_ptr,
    vm::opargs::StackLocal64 	 index */
)

// expects `ext_l64` to be the next instruction
DEF_MICRO_INSTR(
	dynTableStore_lptr_lany,
	vm::opargs::StackLocalPtr /* table_ptr */,
	vm::opargs::StackLocalAny /* source,
    vm::opargs::StackLocal64 	 index */
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
	vm::opargs::StackLocalPtr /* table_ptr */,
	vm::opargs::Type /* table_type ,
vm::opargs::StackLocal64     new_elem_count */
)

/**
 * @brief Outputs a dynamic table of bytes as a string.
 */
DEF_MICRO_INSTR(
	strOutput_lptr, vm::opargs::StackLocalPtr /* string_ptr */
)

// ========= TYPE OPERATIONS ========

// Casts a primitive type in-place. This does nothing at runtime, but is needed
// for type checking.
DEF_MICRO_INSTR(cast_l8_type, vm::opargs::StackLocal8, vm::opargs::Type)
DEF_MICRO_INSTR(cast_l16_type, vm::opargs::StackLocal16, vm::opargs::Type)
DEF_MICRO_INSTR(cast_l32_type, vm::opargs::StackLocal32, vm::opargs::Type)
DEF_MICRO_INSTR(cast_l64_type, vm::opargs::StackLocal64, vm::opargs::Type)

// ========= CONVERSION OPERATIONS ========
// Sign Extension
DEF_MICRO_INSTR(sext_l16_l8, vm::opargs::StackLocal16, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(sext_l32_l8, vm::opargs::StackLocal32, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(sext_l64_l8, vm::opargs::StackLocal64, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(sext_l32_l16, vm::opargs::StackLocal32, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(sext_l64_l16, vm::opargs::StackLocal64, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(sext_l64_l32, vm::opargs::StackLocal64, vm::opargs::StackLocal32)

// Zero Extension
DEF_MICRO_INSTR(zext_l16_l8, vm::opargs::StackLocal16, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(zext_l32_l8, vm::opargs::StackLocal32, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(zext_l64_l8, vm::opargs::StackLocal64, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(zext_l32_l16, vm::opargs::StackLocal32, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(zext_l64_l16, vm::opargs::StackLocal64, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(zext_l64_l32, vm::opargs::StackLocal64, vm::opargs::StackLocal32)

// Truncation
DEF_MICRO_INSTR(trunc_l8_l16, vm::opargs::StackLocal8, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(trunc_l8_l32, vm::opargs::StackLocal8, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(trunc_l8_l64, vm::opargs::StackLocal8, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(trunc_l16_l32, vm::opargs::StackLocal16, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(trunc_l16_l64, vm::opargs::StackLocal16, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(trunc_l32_l64, vm::opargs::StackLocal32, vm::opargs::StackLocal64)

// Int to Float
DEF_MICRO_INSTR(sitofp_l32_l8, vm::opargs::StackLocal32, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(uitofp_l32_l8, vm::opargs::StackLocal32, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(sitofp_l32_l16, vm::opargs::StackLocal32, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(uitofp_l32_l16, vm::opargs::StackLocal32, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(sitofp_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(uitofp_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(sitofp_l32_l64, vm::opargs::StackLocal32, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(uitofp_l32_l64, vm::opargs::StackLocal32, vm::opargs::StackLocal64)

DEF_MICRO_INSTR(sitofp_l64_l8, vm::opargs::StackLocal64, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(uitofp_l64_l8, vm::opargs::StackLocal64, vm::opargs::StackLocal8)
DEF_MICRO_INSTR(sitofp_l64_l16, vm::opargs::StackLocal64, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(uitofp_l64_l16, vm::opargs::StackLocal64, vm::opargs::StackLocal16)
DEF_MICRO_INSTR(sitofp_l64_l32, vm::opargs::StackLocal64, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(uitofp_l64_l32, vm::opargs::StackLocal64, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(sitofp_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(uitofp_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)

// Float to Int (Saturating)
DEF_MICRO_INSTR(fptosi_l8_l32, vm::opargs::StackLocal8, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(fptoui_l8_l32, vm::opargs::StackLocal8, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(fptosi_l16_l32, vm::opargs::StackLocal16, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(fptoui_l16_l32, vm::opargs::StackLocal16, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(fptosi_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(fptoui_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(fptosi_l64_l32, vm::opargs::StackLocal64, vm::opargs::StackLocal32)
DEF_MICRO_INSTR(fptoui_l64_l32, vm::opargs::StackLocal64, vm::opargs::StackLocal32)

DEF_MICRO_INSTR(fptosi_l8_l64, vm::opargs::StackLocal8, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fptoui_l8_l64, vm::opargs::StackLocal8, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fptosi_l16_l64, vm::opargs::StackLocal16, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fptoui_l16_l64, vm::opargs::StackLocal16, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fptosi_l32_l64, vm::opargs::StackLocal32, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fptoui_l32_l64, vm::opargs::StackLocal32, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fptosi_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fptoui_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)

DEF_MICRO_INSTR(fptrunc_l32_l64, vm::opargs::StackLocal32, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(fpext_l64_l32, vm::opargs::StackLocal64, vm::opargs::StackLocal32)

// ========= EXT DEFINITIONS ========

// passes additional argument to preceding instruction
DEF_MICRO_INSTR(ext_l64, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(ext_type, vm::opargs::Type)
DEF_MICRO_INSTR(ext_field, vm::opargs::Field)
DEF_MICRO_INSTR(ext_type_field, vm::opargs::Type, vm::opargs::Field)
DEF_MICRO_INSTR(ext_type_l64, vm::opargs::Type, vm::opargs::StackLocal64)
DEF_MICRO_INSTR(ext_type_type, vm::opargs::Type, vm::opargs::Type)

// ========= MISC ========


DEF_MICRO_INSTR(nop)

// terminates execution
DEF_MICRO_INSTR(exit)

DEF_MICRO_INSTR(breakpoint)

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
