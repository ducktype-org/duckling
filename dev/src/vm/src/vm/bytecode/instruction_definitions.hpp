
/**
 * @file instruction_definitions.hpp
 * @brief Contains definitions of all high bytecode instructions. Can be used for generating
 * repetitive code based on list of instructions.
 *
 * you can just define `HANDLE_INSTR` macro and include this
 * header like so:
 * ```cpp
 *  constexpr usize countHighInstructions() {
 *  	usize count = 0;
 *		#define HANDLE_INSTR(i) count++;
 * 		#include "instruction_definitions.hpp"
 * 		#undef HANDLE_INSTR
 * 		return count;
 * 	}
 * ```
 * This above function just counts the instructions, but the possibilities are endless.
 *
 * There are other macros for cases where you want to know
 * what arguments the instruction has - HANDLE_INSTR_#ARGS,
 * where # is the number of arguments.
 * You can define them similarly to the above example.
 *
 * You can also override the `DEF_INSTR` macro for
 * even higher control.
 */

#ifndef HANDLE_INSTR
#define DEFAULT_HANDLE_INSTR
#define HANDLE_INSTR(instr)
#endif

#ifndef HANDLE_INSTR_ARGS
#define DEFAULT_HANDLE_INSTR_ARGS
#define HANDLE_INSTR_ARGS(instr, ...) HANDLE_INSTR(instr)
#endif

#ifndef DEF_INSTR
#define DEFAULT_DEF_INSTR
#define DEF_INSTR(...) HANDLE_INSTR_ARGS(__VA_ARGS__)
#endif


// ========= MOV OPERATIONS ========

DEF_INSTR(mov_l8_imm, (vm::opargs::StackLocal8, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mov_l8_l8, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(cmov_l8_l8, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(cmov_l8_imm, (vm::opargs::StackLocal8, dst), (vm::opargs::Immediate, src))

DEF_INSTR(mov_l16_imm, (vm::opargs::StackLocal16, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mov_l16_l16, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(cmov_l16_l16, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(cmov_l16_imm, (vm::opargs::StackLocal16, dst), (vm::opargs::Immediate, src))

DEF_INSTR(mov_l32_imm, (vm::opargs::StackLocal32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mov_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(cmov_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(cmov_l32_imm, (vm::opargs::StackLocal32, dst), (vm::opargs::Immediate, src))

DEF_INSTR(mov_l64_imm, (vm::opargs::StackLocal64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mov_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(cmov_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(cmov_l64_imm, (vm::opargs::StackLocal64, dst), (vm::opargs::Immediate, src))


DEF_INSTR(mov_g64_g64, (vm::opargs::Global64, dst), (vm::opargs::Global64, src))
DEF_INSTR(mov_g64_l64, (vm::opargs::Global64, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(mov_g64_imm, (vm::opargs::Global64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mov_g32_g32, (vm::opargs::Global32, dst), (vm::opargs::Global32, src))
DEF_INSTR(mov_g32_l32, (vm::opargs::Global32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(mov_g32_imm, (vm::opargs::Global32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mov_g16_g16, (vm::opargs::Global16, dst), (vm::opargs::Global16, src))
DEF_INSTR(mov_g16_l16, (vm::opargs::Global16, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(mov_g16_imm, (vm::opargs::Global16, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mov_g8_g8, (vm::opargs::Global8, dst), (vm::opargs::Global8, src))
DEF_INSTR(mov_g8_l8, (vm::opargs::Global8, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(mov_g8_imm, (vm::opargs::Global8, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mov_gptr_lptr, (vm::opargs::GlobalPtr, dst), (vm::opargs::StackLocalPtr, src))
DEF_INSTR(mov_l64_g64, (vm::opargs::StackLocal64, dst), (vm::opargs::Global64, src))
DEF_INSTR(mov_l32_g32, (vm::opargs::StackLocal32, dst), (vm::opargs::Global32, src))
DEF_INSTR(mov_l16_g16, (vm::opargs::StackLocal16, dst), (vm::opargs::Global16, src))
DEF_INSTR(mov_l8_g8, (vm::opargs::StackLocal8, dst), (vm::opargs::Global8, src))
DEF_INSTR(mov_lptr_gptr, (vm::opargs::StackLocalPtr, dst), (vm::opargs::GlobalPtr, src))

DEF_INSTR(
	mov_lste_lste, (vm::opargs::StackLocalStructure, dst), (vm::opargs::StackLocalStructure, src)
)
DEF_INSTR(mov_gste_gste, (vm::opargs::GlobalStructure, dst), (vm::opargs::GlobalStructure, src))
DEF_INSTR(mov_gste_lste, (vm::opargs::GlobalStructure, dst), (vm::opargs::StackLocalStructure, src))
DEF_INSTR(mov_lste_gste, (vm::opargs::StackLocalStructure, dst), (vm::opargs::GlobalStructure, src))

// does a shallow pointer copy
DEF_INSTR(mov_lptr_lptr, (vm::opargs::StackLocalPtr, dst), (vm::opargs::StackLocalPtr, src))

// sets pointer to null
DEF_INSTR(setNull_lptr, (vm::opargs::StackLocalPtr, dst))

// Copies an opaque value
DEF_INSTR(mov_lopq_lopq, (vm::opargs::StackLocalOpq, dst), (vm::opargs::StackLocalOpq, src))

// Copies an opaque value between globals and locals
DEF_INSTR(mov_gopq_lopq, (vm::opargs::GlobalOpq, dst), (vm::opargs::StackLocalOpq, src))
DEF_INSTR(mov_lopq_gopq, (vm::opargs::StackLocalOpq, dst), (vm::opargs::GlobalOpq, src))
DEF_INSTR(mov_lopq_imm, (vm::opargs::StackLocalOpq, dst), (vm::opargs::Immediate, src))


// ========= SIGNED INTEGER ARITHMETIC OPERATIONS ========

DEF_INSTR(add_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(add_l64_imm, (vm::opargs::StackLocal64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(sub_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(sub_l64_imm, (vm::opargs::StackLocal64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mul_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(mul_l64_imm, (vm::opargs::StackLocal64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(div_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(div_l64_imm, (vm::opargs::StackLocal64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mod_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(mod_l64_imm, (vm::opargs::StackLocal64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(neg_l64, (vm::opargs::StackLocal64, dst))

DEF_INSTR(add_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(add_l32_imm, (vm::opargs::StackLocal32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(sub_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(sub_l32_imm, (vm::opargs::StackLocal32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mul_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(mul_l32_imm, (vm::opargs::StackLocal32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(div_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(div_l32_imm, (vm::opargs::StackLocal32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mod_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(mod_l32_imm, (vm::opargs::StackLocal32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(neg_l32, (vm::opargs::StackLocal32, dst))

DEF_INSTR(add_l16_l16, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(add_l16_imm, (vm::opargs::StackLocal16, dst), (vm::opargs::Immediate, src))
DEF_INSTR(sub_l16_l16, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(sub_l16_imm, (vm::opargs::StackLocal16, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mul_l16_l16, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(mul_l16_imm, (vm::opargs::StackLocal16, dst), (vm::opargs::Immediate, src))
DEF_INSTR(div_l16_l16, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(div_l16_imm, (vm::opargs::StackLocal16, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mod_l16_l16, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(mod_l16_imm, (vm::opargs::StackLocal16, dst), (vm::opargs::Immediate, src))
DEF_INSTR(neg_l16, (vm::opargs::StackLocal16, dst))

DEF_INSTR(add_l8_l8, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(add_l8_imm, (vm::opargs::StackLocal8, dst), (vm::opargs::Immediate, src))
DEF_INSTR(sub_l8_l8, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(sub_l8_imm, (vm::opargs::StackLocal8, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mul_l8_l8, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(mul_l8_imm, (vm::opargs::StackLocal8, dst), (vm::opargs::Immediate, src))
DEF_INSTR(div_l8_l8, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(div_l8_imm, (vm::opargs::StackLocal8, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mod_l8_l8, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(mod_l8_imm, (vm::opargs::StackLocal8, dst), (vm::opargs::Immediate, src))
DEF_INSTR(neg_l8, (vm::opargs::StackLocal8, dst))

// ========= UNSIGNED INTEGER ARITHMETIC OPERATIONS ========
DEF_INSTR(umul_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(umul_l64_imm, (vm::opargs::StackLocal64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(umod_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(umod_l64_imm, (vm::opargs::StackLocal64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(udiv_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(udiv_l64_imm, (vm::opargs::StackLocal64, dst), (vm::opargs::Immediate, src))

DEF_INSTR(umul_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(umul_l32_imm, (vm::opargs::StackLocal32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(umod_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(umod_l32_imm, (vm::opargs::StackLocal32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(udiv_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(udiv_l32_imm, (vm::opargs::StackLocal32, dst), (vm::opargs::Immediate, src))

DEF_INSTR(umul_l16_l16, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(umul_l16_imm, (vm::opargs::StackLocal16, dst), (vm::opargs::Immediate, src))
DEF_INSTR(umod_l16_l16, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(umod_l16_imm, (vm::opargs::StackLocal16, dst), (vm::opargs::Immediate, src))
DEF_INSTR(udiv_l16_l16, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(udiv_l16_imm, (vm::opargs::StackLocal16, dst), (vm::opargs::Immediate, src))

DEF_INSTR(umul_l8_l8, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(umul_l8_imm, (vm::opargs::StackLocal8, dst), (vm::opargs::Immediate, src))
DEF_INSTR(umod_l8_l8, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(umod_l8_imm, (vm::opargs::StackLocal8, dst), (vm::opargs::Immediate, src))
DEF_INSTR(udiv_l8_l8, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(udiv_l8_imm, (vm::opargs::StackLocal8, dst), (vm::opargs::Immediate, src))

// ========= FLOATING POINT OPERATIONS ========
DEF_INSTR(fadd_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(fadd_l64_imm, (vm::opargs::StackLocal64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(fsub_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(fsub_l64_imm, (vm::opargs::StackLocal64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(fmul_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(fmul_l64_imm, (vm::opargs::StackLocal64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(fdiv_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(fdiv_l64_imm, (vm::opargs::StackLocal64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(fneg_l64, (vm::opargs::StackLocal64, dst))

DEF_INSTR(fadd_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(fadd_l32_imm, (vm::opargs::StackLocal32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(fsub_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(fsub_l32_imm, (vm::opargs::StackLocal32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(fmul_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(fmul_l32_imm, (vm::opargs::StackLocal32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(fdiv_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(fdiv_l32_imm, (vm::opargs::StackLocal32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(fneg_l32, (vm::opargs::StackLocal32, dst))

// ========= BOOLEAN OPERATIONS ========

// Evaluate logical operations (AND, OR, etc.) on operands as booleans (non-zero = true)
// Result is 0 or 1 stored in the first argument

DEF_INSTR(log_and_l8_l8, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(log_and_l8_imm, (vm::opargs::StackLocal8, dst), (vm::opargs::Immediate, src))

DEF_INSTR(log_or_l8_l8, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(log_or_l8_imm, (vm::opargs::StackLocal8, dst), (vm::opargs::Immediate, src))

DEF_INSTR(log_xor_l8_l8, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(log_xor_l8_imm, (vm::opargs::StackLocal8, dst), (vm::opargs::Immediate, src))

DEF_INSTR(log_not_l8, (vm::opargs::StackLocal8, dst))

// ========= LOGICAL OPERATIONS ========

// --- 64-bit Integer Comparisons ---
DEF_INSTR(cmpEq_l64_l64, (vm::opargs::StackLocal64, lhs), (vm::opargs::StackLocal64, rhs))
DEF_INSTR(cmpEq_l64_imm, (vm::opargs::StackLocal64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpNeq_l64_l64, (vm::opargs::StackLocal64, lhs), (vm::opargs::StackLocal64, rhs))
DEF_INSTR(cmpNeq_l64_imm, (vm::opargs::StackLocal64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpGt_l64_l64, (vm::opargs::StackLocal64, lhs), (vm::opargs::StackLocal64, rhs))
DEF_INSTR(cmpGt_l64_imm, (vm::opargs::StackLocal64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpGe_l64_l64, (vm::opargs::StackLocal64, lhs), (vm::opargs::StackLocal64, rhs))
DEF_INSTR(cmpGe_l64_imm, (vm::opargs::StackLocal64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpGt_l64_l64, (vm::opargs::StackLocal64, lhs), (vm::opargs::StackLocal64, rhs))
DEF_INSTR(ucmpGt_l64_imm, (vm::opargs::StackLocal64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpGe_l64_l64, (vm::opargs::StackLocal64, lhs), (vm::opargs::StackLocal64, rhs))
DEF_INSTR(ucmpGe_l64_imm, (vm::opargs::StackLocal64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpLt_l64_l64, (vm::opargs::StackLocal64, lhs), (vm::opargs::StackLocal64, rhs))
DEF_INSTR(cmpLt_l64_imm, (vm::opargs::StackLocal64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpLe_l64_l64, (vm::opargs::StackLocal64, lhs), (vm::opargs::StackLocal64, rhs))
DEF_INSTR(cmpLe_l64_imm, (vm::opargs::StackLocal64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpLt_l64_l64, (vm::opargs::StackLocal64, lhs), (vm::opargs::StackLocal64, rhs))
DEF_INSTR(ucmpLt_l64_imm, (vm::opargs::StackLocal64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpLe_l64_l64, (vm::opargs::StackLocal64, lhs), (vm::opargs::StackLocal64, rhs))
DEF_INSTR(ucmpLe_l64_imm, (vm::opargs::StackLocal64, lhs), (vm::opargs::Immediate, rhs))

// --- 32-bit Integer Comparisons ---
DEF_INSTR(cmpEq_l32_l32, (vm::opargs::StackLocal32, lhs), (vm::opargs::StackLocal32, rhs))
DEF_INSTR(cmpEq_l32_imm, (vm::opargs::StackLocal32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpNeq_l32_l32, (vm::opargs::StackLocal32, lhs), (vm::opargs::StackLocal32, rhs))
DEF_INSTR(cmpNeq_l32_imm, (vm::opargs::StackLocal32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpGt_l32_l32, (vm::opargs::StackLocal32, lhs), (vm::opargs::StackLocal32, rhs))
DEF_INSTR(cmpGt_l32_imm, (vm::opargs::StackLocal32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpGe_l32_l32, (vm::opargs::StackLocal32, lhs), (vm::opargs::StackLocal32, rhs))
DEF_INSTR(cmpGe_l32_imm, (vm::opargs::StackLocal32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpGt_l32_l32, (vm::opargs::StackLocal32, lhs), (vm::opargs::StackLocal32, rhs))
DEF_INSTR(ucmpGt_l32_imm, (vm::opargs::StackLocal32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpGe_l32_l32, (vm::opargs::StackLocal32, lhs), (vm::opargs::StackLocal32, rhs))
DEF_INSTR(ucmpGe_l32_imm, (vm::opargs::StackLocal32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpLt_l32_l32, (vm::opargs::StackLocal32, lhs), (vm::opargs::StackLocal32, rhs))
DEF_INSTR(cmpLt_l32_imm, (vm::opargs::StackLocal32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpLe_l32_l32, (vm::opargs::StackLocal32, lhs), (vm::opargs::StackLocal32, rhs))
DEF_INSTR(cmpLe_l32_imm, (vm::opargs::StackLocal32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpLt_l32_l32, (vm::opargs::StackLocal32, lhs), (vm::opargs::StackLocal32, rhs))
DEF_INSTR(ucmpLt_l32_imm, (vm::opargs::StackLocal32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpLe_l32_l32, (vm::opargs::StackLocal32, lhs), (vm::opargs::StackLocal32, rhs))
DEF_INSTR(ucmpLe_l32_imm, (vm::opargs::StackLocal32, lhs), (vm::opargs::Immediate, rhs))

// --- 16-bit Integer Comparisons ---
DEF_INSTR(cmpEq_l16_l16, (vm::opargs::StackLocal16, lhs), (vm::opargs::StackLocal16, rhs))
DEF_INSTR(cmpEq_l16_imm, (vm::opargs::StackLocal16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpNeq_l16_l16, (vm::opargs::StackLocal16, lhs), (vm::opargs::StackLocal16, rhs))
DEF_INSTR(cmpNeq_l16_imm, (vm::opargs::StackLocal16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpGt_l16_l16, (vm::opargs::StackLocal16, lhs), (vm::opargs::StackLocal16, rhs))
DEF_INSTR(cmpGt_l16_imm, (vm::opargs::StackLocal16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpGe_l16_l16, (vm::opargs::StackLocal16, lhs), (vm::opargs::StackLocal16, rhs))
DEF_INSTR(cmpGe_l16_imm, (vm::opargs::StackLocal16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpGt_l16_l16, (vm::opargs::StackLocal16, lhs), (vm::opargs::StackLocal16, rhs))
DEF_INSTR(ucmpGt_l16_imm, (vm::opargs::StackLocal16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpGe_l16_l16, (vm::opargs::StackLocal16, lhs), (vm::opargs::StackLocal16, rhs))
DEF_INSTR(ucmpGe_l16_imm, (vm::opargs::StackLocal16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpLt_l16_l16, (vm::opargs::StackLocal16, lhs), (vm::opargs::StackLocal16, rhs))
DEF_INSTR(cmpLt_l16_imm, (vm::opargs::StackLocal16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpLe_l16_l16, (vm::opargs::StackLocal16, lhs), (vm::opargs::StackLocal16, rhs))
DEF_INSTR(cmpLe_l16_imm, (vm::opargs::StackLocal16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpLt_l16_l16, (vm::opargs::StackLocal16, lhs), (vm::opargs::StackLocal16, rhs))
DEF_INSTR(ucmpLt_l16_imm, (vm::opargs::StackLocal16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpLe_l16_l16, (vm::opargs::StackLocal16, lhs), (vm::opargs::StackLocal16, rhs))
DEF_INSTR(ucmpLe_l16_imm, (vm::opargs::StackLocal16, lhs), (vm::opargs::Immediate, rhs))

// --- 8-bit Integer Comparisons ---
DEF_INSTR(cmpEq_l8_l8, (vm::opargs::StackLocal8, lhs), (vm::opargs::StackLocal8, rhs))
DEF_INSTR(cmpEq_l8_imm, (vm::opargs::StackLocal8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpNeq_l8_l8, (vm::opargs::StackLocal8, lhs), (vm::opargs::StackLocal8, rhs))
DEF_INSTR(cmpNeq_l8_imm, (vm::opargs::StackLocal8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpGt_l8_l8, (vm::opargs::StackLocal8, lhs), (vm::opargs::StackLocal8, rhs))
DEF_INSTR(cmpGt_l8_imm, (vm::opargs::StackLocal8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpGe_l8_l8, (vm::opargs::StackLocal8, lhs), (vm::opargs::StackLocal8, rhs))
DEF_INSTR(cmpGe_l8_imm, (vm::opargs::StackLocal8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpGt_l8_l8, (vm::opargs::StackLocal8, lhs), (vm::opargs::StackLocal8, rhs))
DEF_INSTR(ucmpGt_l8_imm, (vm::opargs::StackLocal8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpGe_l8_l8, (vm::opargs::StackLocal8, lhs), (vm::opargs::StackLocal8, rhs))
DEF_INSTR(ucmpGe_l8_imm, (vm::opargs::StackLocal8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpLt_l8_l8, (vm::opargs::StackLocal8, lhs), (vm::opargs::StackLocal8, rhs))
DEF_INSTR(cmpLt_l8_imm, (vm::opargs::StackLocal8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpLe_l8_l8, (vm::opargs::StackLocal8, lhs), (vm::opargs::StackLocal8, rhs))
DEF_INSTR(cmpLe_l8_imm, (vm::opargs::StackLocal8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpLt_l8_l8, (vm::opargs::StackLocal8, lhs), (vm::opargs::StackLocal8, rhs))
DEF_INSTR(ucmpLt_l8_imm, (vm::opargs::StackLocal8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpLe_l8_l8, (vm::opargs::StackLocal8, lhs), (vm::opargs::StackLocal8, rhs))
DEF_INSTR(ucmpLe_l8_imm, (vm::opargs::StackLocal8, lhs), (vm::opargs::Immediate, rhs))

// --- 64-bit Floating Point Comparisons ---
DEF_INSTR(fcmpEq_l64_l64, (vm::opargs::StackLocal64, lhs), (vm::opargs::StackLocal64, rhs))
DEF_INSTR(fcmpEq_l64_imm, (vm::opargs::StackLocal64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpNeq_l64_l64, (vm::opargs::StackLocal64, lhs), (vm::opargs::StackLocal64, rhs))
DEF_INSTR(fcmpNeq_l64_imm, (vm::opargs::StackLocal64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpGt_l64_l64, (vm::opargs::StackLocal64, lhs), (vm::opargs::StackLocal64, rhs))
DEF_INSTR(fcmpGt_l64_imm, (vm::opargs::StackLocal64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpGe_l64_l64, (vm::opargs::StackLocal64, lhs), (vm::opargs::StackLocal64, rhs))
DEF_INSTR(fcmpGe_l64_imm, (vm::opargs::StackLocal64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpLt_l64_l64, (vm::opargs::StackLocal64, lhs), (vm::opargs::StackLocal64, rhs))
DEF_INSTR(fcmpLt_l64_imm, (vm::opargs::StackLocal64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpLe_l64_l64, (vm::opargs::StackLocal64, lhs), (vm::opargs::StackLocal64, rhs))
DEF_INSTR(fcmpLe_l64_imm, (vm::opargs::StackLocal64, lhs), (vm::opargs::Immediate, rhs))

// --- 32-bit Floating Point Comparisons ---
DEF_INSTR(fcmpEq_l32_l32, (vm::opargs::StackLocal32, lhs), (vm::opargs::StackLocal32, rhs))
DEF_INSTR(fcmpEq_l32_imm, (vm::opargs::StackLocal32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpNeq_l32_l32, (vm::opargs::StackLocal32, lhs), (vm::opargs::StackLocal32, rhs))
DEF_INSTR(fcmpNeq_l32_imm, (vm::opargs::StackLocal32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpGt_l32_l32, (vm::opargs::StackLocal32, lhs), (vm::opargs::StackLocal32, rhs))
DEF_INSTR(fcmpGt_l32_imm, (vm::opargs::StackLocal32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpGe_l32_l32, (vm::opargs::StackLocal32, lhs), (vm::opargs::StackLocal32, rhs))
DEF_INSTR(fcmpGe_l32_imm, (vm::opargs::StackLocal32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpLt_l32_l32, (vm::opargs::StackLocal32, lhs), (vm::opargs::StackLocal32, rhs))
DEF_INSTR(fcmpLt_l32_imm, (vm::opargs::StackLocal32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpLe_l32_l32, (vm::opargs::StackLocal32, lhs), (vm::opargs::StackLocal32, rhs))
DEF_INSTR(fcmpLe_l32_imm, (vm::opargs::StackLocal32, lhs), (vm::opargs::Immediate, rhs))

// sets the flag if pointer is null
DEF_INSTR(cmpNull_lptr, (vm::opargs::StackLocalPtr, ptr))

// ========= VARIANT OPERATIONS ========


// Sets `variant`'s inner type to `inner_type`. It also invalidates pointers to its data.
DEF_INSTR(
	variantSetInner_lvnt_type, (vm::opargs::StackLocalVnt, variant), (vm::opargs::Type, inner_type)
)
/**
 * @brief Sets `dst_ptr` to point at `variant`'s data. Expects `variant` to has `expected_type`
 * set, and if it's not, `destination` becomes nullptr.
 * @note `expected_type` required to know which type is to be expected. There is no other way to
 * obtain type information in the implementation.
 */
DEF_INSTR(
	variantGetInner_lptr_lvnt_type,
	(vm::opargs::StackLocalPtr, dst_ptr),
	(vm::opargs::StackLocalVnt, variant),
	(vm::opargs::Type, expected_type)
)

/**
 * @brief Sets inner type of variant under `variant_ptr` to `inner_type`. It also invalidates
 * pointers to its data.
 */
DEF_INSTR(
	variantSetInner_lptr_type,
	(vm::opargs::StackLocalPtr, variant_ptr),
	(vm::opargs::Type, inner_type)
)

/**
 * @brief Sets `dst_ptr` to point at data of variant under `variant_ptr`. Expects the variant to
 * have `expected_type` set, and if it's not, `destination` becomes nullptr.
 * @note `expected_type` required to know which type is to be expected. There is no other way to
 * obtain type information in the implementation.
 */
DEF_INSTR(
	variantGetInner_lptr_lptr_type,
	(vm::opargs::StackLocalPtr, dst_ptr),
	(vm::opargs::StackLocalPtr, variant_ptr),
	(vm::opargs::Type, expected_type)
)

// ========= LABELS AND JUMPS ========

DEF_INSTR(label, (vm::opargs::Label, label))

DEF_INSTR(jmp_label, (vm::opargs::Label, label))
DEF_INSTR(jmpIf_label, (vm::opargs::Label, label))
DEF_INSTR(jmpIfNot_label, (vm::opargs::Label, label))

// ========= FUNCTION OPERATIONS ========

DEF_INSTR(call_func, (vm::opargs::FunctionName, function))
DEF_INSTR(call_builtinfunc, (vm::opargs::BuiltinFunctionName, function))
DEF_INSTR(call_cfunc, (vm::opargs::ExtCFunctionName, function))

DEF_INSTR(set_threadctx, (vm::opargs::FunctionName, function))

// return while performing a tail call
DEF_INSTR(ret_tailcall_func, (vm::opargs::FunctionName, function))
// return
DEF_INSTR(ret)

// ========= STACK OPERATIONS ========

// initialize local variable on local stack with given type
DEF_INSTR(init_lany_type, (vm::opargs::StackLocalAny, var), (vm::opargs::Type, type))
// pop variable from local stack
DEF_INSTR(deinit)

// ========= IO OPERATIONS ========

DEF_INSTR(input_l64, (vm::opargs::StackLocal64, dst))
DEF_INSTR(output_l64, (vm::opargs::StackLocal64, src))

DEF_INSTR(input_l32, (vm::opargs::StackLocal32, dst))
DEF_INSTR(output_l32, (vm::opargs::StackLocal32, src))


// ========= CLASS OPERATIONS ========

// initialises vtable pointer
DEF_INSTR(setVTable_lptr_type, (vm::opargs::StackLocalPtr, object_ptr), (vm::opargs::Type, type))
// deinitialises vtable pointer
DEF_INSTR(resetVTable_lptr, (vm::opargs::StackLocalPtr, object_ptr))
// casts pointed object to its superclass
DEF_INSTR(upcast_lptr_lptr, (vm::opargs::StackLocalPtr, dst), (vm::opargs::StackLocalPtr, src))
// tries to cast pointed object to its subclass
DEF_INSTR(downcast_lptr_lptr, (vm::opargs::StackLocalPtr, dst), (vm::opargs::StackLocalPtr, src), )
// calls a method of specified name on an a pointer. Performs the dynamic dispatch.
DEF_INSTR(
	virtual_call_lptr_method,
	(vm::opargs::StackLocalPtr, object_ptr),
	(vm::opargs::MethodName, method)
)

// ========= GENERAL POINTER OPERATIONS ========

// allocates given type, stores pointer
DEF_INSTR(alloc_lptr_type, (vm::opargs::StackLocalPtr, ptr), (vm::opargs::Type, type))
// frees block under pointer
DEF_INSTR(free_lptr, (vm::opargs::StackLocalPtr, ptr))


// stores local data at pointer
DEF_INSTR(store_lptr_lany, (vm::opargs::StackLocalPtr, dst_ptr), (vm::opargs::StackLocalAny, src))
// dereferences pointer and stores into local
DEF_INSTR(load_lany_lptr, (vm::opargs::StackLocalAny, dst), (vm::opargs::StackLocalPtr, src_ptr))

// stores reference to local object of any type T in pointer<T>
DEF_INSTR(ref_lptr_lany, (vm::opargs::StackLocalPtr, dst_ptr), (vm::opargs::StackLocalAny, src))
// stores reference to global object of any type T in pointer<T>
DEF_INSTR(ref_lptr_gany, (vm::opargs::StackLocalPtr, dst_ptr), (vm::opargs::GlobalAny, src))

// ========= STRUCTURE OPERATIONS ========

// loads effective address of struct field
DEF_INSTR(
	structLea_lptr_lptr_field,
	(vm::opargs::StackLocalPtr, dst_ptr),
	(vm::opargs::StackLocalPtr, src_data_ptr),
	(vm::opargs::Field, field)
)
DEF_INSTR(
	structLoad_lany_lptr_field,
	(vm::opargs::StackLocalAny, dst),
	(vm::opargs::StackLocalPtr, src_data_ptr),
	(vm::opargs::Field, field)
)
DEF_INSTR(
	structStore_lptr_lany_field,
	(vm::opargs::StackLocalPtr, dst_data_ptr),
	(vm::opargs::StackLocalAny, src),
	(vm::opargs::Field, field)
)

// These are the same as above, but for structs referenced via local stack

DEF_INSTR(
	structLea_lptr_lste_field,
	(vm::opargs::StackLocalPtr, dst_ptr),
	(vm::opargs::StackLocalStructure, src_data_struct),
	(vm::opargs::Field, field)
)
DEF_INSTR(
	structLoad_lany_lste_field,
	(vm::opargs::StackLocalAny, dst),
	(vm::opargs::StackLocalStructure, src_data_struct),
	(vm::opargs::Field, field)
)
DEF_INSTR(
	structStore_lste_lany_field,
	(vm::opargs::StackLocalStructure, dst_data_struct),
	(vm::opargs::StackLocalAny, src),
	(vm::opargs::Field, field)
)

// ========= TABLE OPERATIONS ========

DEF_INSTR(
	fixedSizeTableLea_lptr_lptr_l64,
	(vm::opargs::StackLocalPtr, dst_ptr),
	(vm::opargs::StackLocalPtr, src_table_ptr),
	(vm::opargs::StackLocal64, index)
)
DEF_INSTR(
	fixedSizeTableLoad_lany_lptr_l64,
	(vm::opargs::StackLocalAny, dst),
	(vm::opargs::StackLocalPtr, src_table_ptr),
	(vm::opargs::StackLocal64, index)
)

DEF_INSTR(
	fixedSizeTableStore_lptr_lany_l64,
	(vm::opargs::StackLocalPtr, dst_table_ptr),
	(vm::opargs::StackLocalAny, src),
	(vm::opargs::StackLocal64, index)
)

DEF_INSTR(
	dynTableLea_lptr_lptr_l64,
	(vm::opargs::StackLocalPtr, dst_ptr),
	(vm::opargs::StackLocalPtr, src_table_ptr),
	(vm::opargs::StackLocal64, index)
)
DEF_INSTR(
	dynTableLoad_lany_lptr_l64,
	(vm::opargs::StackLocalAny, dst),
	(vm::opargs::StackLocalPtr, src_table_ptr),
	(vm::opargs::StackLocal64, index)
)

DEF_INSTR(
	dynTableStore_lptr_lany_l64,
	(vm::opargs::StackLocalPtr, dst_table_ptr),
	(vm::opargs::StackLocalAny, src),
	(vm::opargs::StackLocal64, index)
)

/**
 * @brief Re-allocates dynamic table under `table_ptr` with
 * `new_elem_count` elements. If given nullptr, then it will allocate
 * a new array.
 * `table_type` is type of the dynamic table itself, not the element type.
 * @note It's counter-intuitive, but if a reallocation has happened, this
 *  instruction will not modify pointer data (unlike in C).
 */
DEF_INSTR(
	dynTableReAlloc_lptr_type_l64,
	(vm::opargs::StackLocalPtr, dst_table_ptr),
	(vm::opargs::Type, table_type),
	(vm::opargs::StackLocal64, new_elem_count)
)

/**
 * @brief Outputs a dynamic table of bytes as a string.
 */
DEF_INSTR(strOutput_lptr, (vm::opargs::StackLocalPtr, string_ptr))

// ========= TYPE OPERATIONS ========
// Casts a primitive type in-place. This does nothing at runtime, but is needed
// for type checking.
DEF_INSTR(cast_l8_type, (vm::opargs::StackLocal8, value), (vm::opargs::Type, target_type))
DEF_INSTR(cast_l16_type, (vm::opargs::StackLocal16, value), (vm::opargs::Type, target_type))
DEF_INSTR(cast_l32_type, (vm::opargs::StackLocal32, value), (vm::opargs::Type, target_type))
DEF_INSTR(cast_l64_type, (vm::opargs::StackLocal64, value), (vm::opargs::Type, target_type))

// ========= CONVERSION OPERATIONS ========

// Sign Extension
DEF_INSTR(sext_l16_l8, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(sext_l32_l8, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(sext_l64_l8, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(sext_l32_l16, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(sext_l64_l16, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(sext_l64_l32, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal32, src))

// Zero Extension
DEF_INSTR(zext_l16_l8, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(zext_l32_l8, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(zext_l64_l8, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(zext_l32_l16, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(zext_l64_l16, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(zext_l64_l32, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal32, src))

// Truncation
DEF_INSTR(trunc_l8_l16, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(trunc_l8_l32, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(trunc_l8_l64, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(trunc_l16_l32, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(trunc_l16_l64, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(trunc_l32_l64, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal64, src))

// Int to Float
DEF_INSTR(sitofp_l32_l8, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(sitofp_l64_l8, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(uitofp_l32_l8, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal8, src))
DEF_INSTR(uitofp_l64_l8, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal8, src))

DEF_INSTR(sitofp_l32_l16, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(sitofp_l64_l16, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(uitofp_l32_l16, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal16, src))
DEF_INSTR(uitofp_l64_l16, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal16, src))

DEF_INSTR(sitofp_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(sitofp_l64_l32, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(uitofp_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(uitofp_l64_l32, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal32, src))

DEF_INSTR(sitofp_l32_l64, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(sitofp_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(uitofp_l32_l64, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(uitofp_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))

// Float to Int
DEF_INSTR(fptosi_l8_l32, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(fptoui_l8_l32, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(fptosi_l16_l32, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(fptoui_l16_l32, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(fptosi_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(fptoui_l32_l32, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(fptosi_l64_l32, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal32, src))
DEF_INSTR(fptoui_l64_l32, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal32, src))

DEF_INSTR(fptosi_l8_l64, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(fptoui_l8_l64, (vm::opargs::StackLocal8, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(fptosi_l16_l64, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(fptoui_l16_l64, (vm::opargs::StackLocal16, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(fptosi_l32_l64, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(fptoui_l32_l64, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(fptosi_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(fptoui_l64_l64, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal64, src))

// Float to float
DEF_INSTR(fptrunc_l32_l64, (vm::opargs::StackLocal32, dst), (vm::opargs::StackLocal64, src))
DEF_INSTR(fpext_l64_l32, (vm::opargs::StackLocal64, dst), (vm::opargs::StackLocal32, src))

// ========= MISC ========


DEF_INSTR(nop)

// terminates execution
DEF_INSTR(exit)

DEF_INSTR(breakpoint)

/**
 * @brief This is a very internal instruction, that should not be used in regular bytecode.
 * It is a helper for start functions.
 * @arg0 - pointer to a VmValue.
 * @arg1 - n/a.
 */
DEF_INSTR(initFromVmValue)

#ifdef DEFAULT_HANDLE_INSTR
#undef DEFAULT_HANDLE_INSTR
#undef HANDLE_INSTR
#endif

#ifdef DEFAULT_HANDLE_INSTR_ARGS
#undef DEFAULT_HANDLE_INSTR_ARGS
#undef HANDLE_INSTR_ARGS
#endif

#ifdef DEFAULT_DEF_INSTR
#undef DEFAULT_DEF_INSTR
#undef DEF_INSTR
#endif
