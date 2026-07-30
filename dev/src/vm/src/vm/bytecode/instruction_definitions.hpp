
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

DEF_INSTR(mov_p8_imm, (vm::opargs::Place8, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mov_p8_p8, (vm::opargs::Place8, dst), (vm::opargs::Place8, src))
DEF_INSTR(cmov_p8_p8, (vm::opargs::Place8, dst), (vm::opargs::Place8, src))
DEF_INSTR(cmov_p8_imm, (vm::opargs::Place8, dst), (vm::opargs::Immediate, src))

DEF_INSTR(mov_p16_imm, (vm::opargs::Place16, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mov_p16_p16, (vm::opargs::Place16, dst), (vm::opargs::Place16, src))
DEF_INSTR(cmov_p16_p16, (vm::opargs::Place16, dst), (vm::opargs::Place16, src))
DEF_INSTR(cmov_p16_imm, (vm::opargs::Place16, dst), (vm::opargs::Immediate, src))

DEF_INSTR(mov_p32_imm, (vm::opargs::Place32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mov_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(cmov_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(cmov_p32_imm, (vm::opargs::Place32, dst), (vm::opargs::Immediate, src))

DEF_INSTR(mov_p64_imm, (vm::opargs::Place64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mov_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))
DEF_INSTR(cmov_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))
DEF_INSTR(cmov_p64_imm, (vm::opargs::Place64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mov_pptr_pptr, (vm::opargs::PlacePtr, dst), (vm::opargs::PlacePtr, src))
DEF_INSTR(mov_pste_pste, (vm::opargs::PlaceStructure, dst), (vm::opargs::PlaceStructure, src))
DEF_INSTR(mov_pfst_pfst, (vm::opargs::PlaceFSTable, dst), (vm::opargs::PlaceFSTable, src))

// does a shallow pointer copy

// sets pointer to null
DEF_INSTR(setNull_pptr, (vm::opargs::PlacePtr, dst))

// Copies an opaque value
DEF_INSTR(mov_popq_popq, (vm::opargs::PlaceOpq, dst), (vm::opargs::PlaceOpq, src))

// Copies a C pointer value; source and destination must have the identical cpointer type
DEF_INSTR(mov_pcpt_pcpt, (vm::opargs::PlaceCptr, dst), (vm::opargs::PlaceCptr, src))


// ========= SIGNED INTEGER ARITHMETIC OPERATIONS ========

DEF_INSTR(add_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))
DEF_INSTR(add_p64_imm, (vm::opargs::Place64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(sub_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))
DEF_INSTR(sub_p64_imm, (vm::opargs::Place64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mul_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))
DEF_INSTR(mul_p64_imm, (vm::opargs::Place64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(div_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))
DEF_INSTR(div_p64_imm, (vm::opargs::Place64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mod_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))
DEF_INSTR(mod_p64_imm, (vm::opargs::Place64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(neg_p64, (vm::opargs::Place64, dst))

DEF_INSTR(add_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(add_p32_imm, (vm::opargs::Place32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(sub_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(sub_p32_imm, (vm::opargs::Place32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mul_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(mul_p32_imm, (vm::opargs::Place32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(div_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(div_p32_imm, (vm::opargs::Place32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mod_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(mod_p32_imm, (vm::opargs::Place32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(neg_p32, (vm::opargs::Place32, dst))

DEF_INSTR(add_p16_p16, (vm::opargs::Place16, dst), (vm::opargs::Place16, src))
DEF_INSTR(add_p16_imm, (vm::opargs::Place16, dst), (vm::opargs::Immediate, src))
DEF_INSTR(sub_p16_p16, (vm::opargs::Place16, dst), (vm::opargs::Place16, src))
DEF_INSTR(sub_p16_imm, (vm::opargs::Place16, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mul_p16_p16, (vm::opargs::Place16, dst), (vm::opargs::Place16, src))
DEF_INSTR(mul_p16_imm, (vm::opargs::Place16, dst), (vm::opargs::Immediate, src))
DEF_INSTR(div_p16_p16, (vm::opargs::Place16, dst), (vm::opargs::Place16, src))
DEF_INSTR(div_p16_imm, (vm::opargs::Place16, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mod_p16_p16, (vm::opargs::Place16, dst), (vm::opargs::Place16, src))
DEF_INSTR(mod_p16_imm, (vm::opargs::Place16, dst), (vm::opargs::Immediate, src))
DEF_INSTR(neg_p16, (vm::opargs::Place16, dst))

DEF_INSTR(add_p8_p8, (vm::opargs::Place8, dst), (vm::opargs::Place8, src))
DEF_INSTR(add_p8_imm, (vm::opargs::Place8, dst), (vm::opargs::Immediate, src))
DEF_INSTR(sub_p8_p8, (vm::opargs::Place8, dst), (vm::opargs::Place8, src))
DEF_INSTR(sub_p8_imm, (vm::opargs::Place8, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mul_p8_p8, (vm::opargs::Place8, dst), (vm::opargs::Place8, src))
DEF_INSTR(mul_p8_imm, (vm::opargs::Place8, dst), (vm::opargs::Immediate, src))
DEF_INSTR(div_p8_p8, (vm::opargs::Place8, dst), (vm::opargs::Place8, src))
DEF_INSTR(div_p8_imm, (vm::opargs::Place8, dst), (vm::opargs::Immediate, src))
DEF_INSTR(mod_p8_p8, (vm::opargs::Place8, dst), (vm::opargs::Place8, src))
DEF_INSTR(mod_p8_imm, (vm::opargs::Place8, dst), (vm::opargs::Immediate, src))
DEF_INSTR(neg_p8, (vm::opargs::Place8, dst))

// ========= UNSIGNED INTEGER ARITHMETIC OPERATIONS ========
DEF_INSTR(umul_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))
DEF_INSTR(umul_p64_imm, (vm::opargs::Place64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(umod_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))
DEF_INSTR(umod_p64_imm, (vm::opargs::Place64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(udiv_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))
DEF_INSTR(udiv_p64_imm, (vm::opargs::Place64, dst), (vm::opargs::Immediate, src))

DEF_INSTR(umul_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(umul_p32_imm, (vm::opargs::Place32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(umod_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(umod_p32_imm, (vm::opargs::Place32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(udiv_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(udiv_p32_imm, (vm::opargs::Place32, dst), (vm::opargs::Immediate, src))

DEF_INSTR(umul_p16_p16, (vm::opargs::Place16, dst), (vm::opargs::Place16, src))
DEF_INSTR(umul_p16_imm, (vm::opargs::Place16, dst), (vm::opargs::Immediate, src))
DEF_INSTR(umod_p16_p16, (vm::opargs::Place16, dst), (vm::opargs::Place16, src))
DEF_INSTR(umod_p16_imm, (vm::opargs::Place16, dst), (vm::opargs::Immediate, src))
DEF_INSTR(udiv_p16_p16, (vm::opargs::Place16, dst), (vm::opargs::Place16, src))
DEF_INSTR(udiv_p16_imm, (vm::opargs::Place16, dst), (vm::opargs::Immediate, src))

DEF_INSTR(umul_p8_p8, (vm::opargs::Place8, dst), (vm::opargs::Place8, src))
DEF_INSTR(umul_p8_imm, (vm::opargs::Place8, dst), (vm::opargs::Immediate, src))
DEF_INSTR(umod_p8_p8, (vm::opargs::Place8, dst), (vm::opargs::Place8, src))
DEF_INSTR(umod_p8_imm, (vm::opargs::Place8, dst), (vm::opargs::Immediate, src))
DEF_INSTR(udiv_p8_p8, (vm::opargs::Place8, dst), (vm::opargs::Place8, src))
DEF_INSTR(udiv_p8_imm, (vm::opargs::Place8, dst), (vm::opargs::Immediate, src))

// ========= FLOATING POINT OPERATIONS ========
DEF_INSTR(fadd_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))
DEF_INSTR(fadd_p64_imm, (vm::opargs::Place64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(fsub_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))
DEF_INSTR(fsub_p64_imm, (vm::opargs::Place64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(fmul_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))
DEF_INSTR(fmul_p64_imm, (vm::opargs::Place64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(fdiv_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))
DEF_INSTR(fdiv_p64_imm, (vm::opargs::Place64, dst), (vm::opargs::Immediate, src))
DEF_INSTR(fneg_p64, (vm::opargs::Place64, dst))

DEF_INSTR(fadd_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(fadd_p32_imm, (vm::opargs::Place32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(fsub_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(fsub_p32_imm, (vm::opargs::Place32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(fmul_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(fmul_p32_imm, (vm::opargs::Place32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(fdiv_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(fdiv_p32_imm, (vm::opargs::Place32, dst), (vm::opargs::Immediate, src))
DEF_INSTR(fneg_p32, (vm::opargs::Place32, dst))

// ========= BOOLEAN OPERATIONS ========

// Evaluate logical operations (AND, OR, etc.) on operands as booleans (non-zero = true)
// Result is 0 or 1 stored in the first argument

DEF_INSTR(log_and_p8_p8, (vm::opargs::Place8, dst), (vm::opargs::Place8, src))
DEF_INSTR(log_and_p8_imm, (vm::opargs::Place8, dst), (vm::opargs::Immediate, src))

DEF_INSTR(log_or_p8_p8, (vm::opargs::Place8, dst), (vm::opargs::Place8, src))
DEF_INSTR(log_or_p8_imm, (vm::opargs::Place8, dst), (vm::opargs::Immediate, src))

DEF_INSTR(log_xor_p8_p8, (vm::opargs::Place8, dst), (vm::opargs::Place8, src))
DEF_INSTR(log_xor_p8_imm, (vm::opargs::Place8, dst), (vm::opargs::Immediate, src))

DEF_INSTR(log_not_p8, (vm::opargs::Place8, dst))

// ========= LOGICAL OPERATIONS ========

// --- 64-bit Integer Comparisons ---
DEF_INSTR(cmpEq_p64_p64, (vm::opargs::Place64, lhs), (vm::opargs::Place64, rhs))
DEF_INSTR(cmpEq_p64_imm, (vm::opargs::Place64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpNeq_p64_p64, (vm::opargs::Place64, lhs), (vm::opargs::Place64, rhs))
DEF_INSTR(cmpNeq_p64_imm, (vm::opargs::Place64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpGt_p64_p64, (vm::opargs::Place64, lhs), (vm::opargs::Place64, rhs))
DEF_INSTR(cmpGt_p64_imm, (vm::opargs::Place64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpGe_p64_p64, (vm::opargs::Place64, lhs), (vm::opargs::Place64, rhs))
DEF_INSTR(cmpGe_p64_imm, (vm::opargs::Place64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpGt_p64_p64, (vm::opargs::Place64, lhs), (vm::opargs::Place64, rhs))
DEF_INSTR(ucmpGt_p64_imm, (vm::opargs::Place64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpGe_p64_p64, (vm::opargs::Place64, lhs), (vm::opargs::Place64, rhs))
DEF_INSTR(ucmpGe_p64_imm, (vm::opargs::Place64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpLt_p64_p64, (vm::opargs::Place64, lhs), (vm::opargs::Place64, rhs))
DEF_INSTR(cmpLt_p64_imm, (vm::opargs::Place64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpLe_p64_p64, (vm::opargs::Place64, lhs), (vm::opargs::Place64, rhs))
DEF_INSTR(cmpLe_p64_imm, (vm::opargs::Place64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpLt_p64_p64, (vm::opargs::Place64, lhs), (vm::opargs::Place64, rhs))
DEF_INSTR(ucmpLt_p64_imm, (vm::opargs::Place64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpLe_p64_p64, (vm::opargs::Place64, lhs), (vm::opargs::Place64, rhs))
DEF_INSTR(ucmpLe_p64_imm, (vm::opargs::Place64, lhs), (vm::opargs::Immediate, rhs))

// --- 32-bit Integer Comparisons ---
DEF_INSTR(cmpEq_p32_p32, (vm::opargs::Place32, lhs), (vm::opargs::Place32, rhs))
DEF_INSTR(cmpEq_p32_imm, (vm::opargs::Place32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpNeq_p32_p32, (vm::opargs::Place32, lhs), (vm::opargs::Place32, rhs))
DEF_INSTR(cmpNeq_p32_imm, (vm::opargs::Place32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpGt_p32_p32, (vm::opargs::Place32, lhs), (vm::opargs::Place32, rhs))
DEF_INSTR(cmpGt_p32_imm, (vm::opargs::Place32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpGe_p32_p32, (vm::opargs::Place32, lhs), (vm::opargs::Place32, rhs))
DEF_INSTR(cmpGe_p32_imm, (vm::opargs::Place32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpGt_p32_p32, (vm::opargs::Place32, lhs), (vm::opargs::Place32, rhs))
DEF_INSTR(ucmpGt_p32_imm, (vm::opargs::Place32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpGe_p32_p32, (vm::opargs::Place32, lhs), (vm::opargs::Place32, rhs))
DEF_INSTR(ucmpGe_p32_imm, (vm::opargs::Place32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpLt_p32_p32, (vm::opargs::Place32, lhs), (vm::opargs::Place32, rhs))
DEF_INSTR(cmpLt_p32_imm, (vm::opargs::Place32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpLe_p32_p32, (vm::opargs::Place32, lhs), (vm::opargs::Place32, rhs))
DEF_INSTR(cmpLe_p32_imm, (vm::opargs::Place32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpLt_p32_p32, (vm::opargs::Place32, lhs), (vm::opargs::Place32, rhs))
DEF_INSTR(ucmpLt_p32_imm, (vm::opargs::Place32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpLe_p32_p32, (vm::opargs::Place32, lhs), (vm::opargs::Place32, rhs))
DEF_INSTR(ucmpLe_p32_imm, (vm::opargs::Place32, lhs), (vm::opargs::Immediate, rhs))

// --- 16-bit Integer Comparisons ---
DEF_INSTR(cmpEq_p16_p16, (vm::opargs::Place16, lhs), (vm::opargs::Place16, rhs))
DEF_INSTR(cmpEq_p16_imm, (vm::opargs::Place16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpNeq_p16_p16, (vm::opargs::Place16, lhs), (vm::opargs::Place16, rhs))
DEF_INSTR(cmpNeq_p16_imm, (vm::opargs::Place16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpGt_p16_p16, (vm::opargs::Place16, lhs), (vm::opargs::Place16, rhs))
DEF_INSTR(cmpGt_p16_imm, (vm::opargs::Place16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpGe_p16_p16, (vm::opargs::Place16, lhs), (vm::opargs::Place16, rhs))
DEF_INSTR(cmpGe_p16_imm, (vm::opargs::Place16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpGt_p16_p16, (vm::opargs::Place16, lhs), (vm::opargs::Place16, rhs))
DEF_INSTR(ucmpGt_p16_imm, (vm::opargs::Place16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpGe_p16_p16, (vm::opargs::Place16, lhs), (vm::opargs::Place16, rhs))
DEF_INSTR(ucmpGe_p16_imm, (vm::opargs::Place16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpLt_p16_p16, (vm::opargs::Place16, lhs), (vm::opargs::Place16, rhs))
DEF_INSTR(cmpLt_p16_imm, (vm::opargs::Place16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpLe_p16_p16, (vm::opargs::Place16, lhs), (vm::opargs::Place16, rhs))
DEF_INSTR(cmpLe_p16_imm, (vm::opargs::Place16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpLt_p16_p16, (vm::opargs::Place16, lhs), (vm::opargs::Place16, rhs))
DEF_INSTR(ucmpLt_p16_imm, (vm::opargs::Place16, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpLe_p16_p16, (vm::opargs::Place16, lhs), (vm::opargs::Place16, rhs))
DEF_INSTR(ucmpLe_p16_imm, (vm::opargs::Place16, lhs), (vm::opargs::Immediate, rhs))

// --- 8-bit Integer Comparisons ---
DEF_INSTR(cmpEq_p8_p8, (vm::opargs::Place8, lhs), (vm::opargs::Place8, rhs))
DEF_INSTR(cmpEq_p8_imm, (vm::opargs::Place8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpNeq_p8_p8, (vm::opargs::Place8, lhs), (vm::opargs::Place8, rhs))
DEF_INSTR(cmpNeq_p8_imm, (vm::opargs::Place8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpGt_p8_p8, (vm::opargs::Place8, lhs), (vm::opargs::Place8, rhs))
DEF_INSTR(cmpGt_p8_imm, (vm::opargs::Place8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpGe_p8_p8, (vm::opargs::Place8, lhs), (vm::opargs::Place8, rhs))
DEF_INSTR(cmpGe_p8_imm, (vm::opargs::Place8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpGt_p8_p8, (vm::opargs::Place8, lhs), (vm::opargs::Place8, rhs))
DEF_INSTR(ucmpGt_p8_imm, (vm::opargs::Place8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpGe_p8_p8, (vm::opargs::Place8, lhs), (vm::opargs::Place8, rhs))
DEF_INSTR(ucmpGe_p8_imm, (vm::opargs::Place8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpLt_p8_p8, (vm::opargs::Place8, lhs), (vm::opargs::Place8, rhs))
DEF_INSTR(cmpLt_p8_imm, (vm::opargs::Place8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(cmpLe_p8_p8, (vm::opargs::Place8, lhs), (vm::opargs::Place8, rhs))
DEF_INSTR(cmpLe_p8_imm, (vm::opargs::Place8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpLt_p8_p8, (vm::opargs::Place8, lhs), (vm::opargs::Place8, rhs))
DEF_INSTR(ucmpLt_p8_imm, (vm::opargs::Place8, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(ucmpLe_p8_p8, (vm::opargs::Place8, lhs), (vm::opargs::Place8, rhs))
DEF_INSTR(ucmpLe_p8_imm, (vm::opargs::Place8, lhs), (vm::opargs::Immediate, rhs))

// --- 64-bit Floating Point Comparisons ---
DEF_INSTR(fcmpEq_p64_p64, (vm::opargs::Place64, lhs), (vm::opargs::Place64, rhs))
DEF_INSTR(fcmpEq_p64_imm, (vm::opargs::Place64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpNeq_p64_p64, (vm::opargs::Place64, lhs), (vm::opargs::Place64, rhs))
DEF_INSTR(fcmpNeq_p64_imm, (vm::opargs::Place64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpGt_p64_p64, (vm::opargs::Place64, lhs), (vm::opargs::Place64, rhs))
DEF_INSTR(fcmpGt_p64_imm, (vm::opargs::Place64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpGe_p64_p64, (vm::opargs::Place64, lhs), (vm::opargs::Place64, rhs))
DEF_INSTR(fcmpGe_p64_imm, (vm::opargs::Place64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpLt_p64_p64, (vm::opargs::Place64, lhs), (vm::opargs::Place64, rhs))
DEF_INSTR(fcmpLt_p64_imm, (vm::opargs::Place64, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpLe_p64_p64, (vm::opargs::Place64, lhs), (vm::opargs::Place64, rhs))
DEF_INSTR(fcmpLe_p64_imm, (vm::opargs::Place64, lhs), (vm::opargs::Immediate, rhs))

// --- 32-bit Floating Point Comparisons ---
DEF_INSTR(fcmpEq_p32_p32, (vm::opargs::Place32, lhs), (vm::opargs::Place32, rhs))
DEF_INSTR(fcmpEq_p32_imm, (vm::opargs::Place32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpNeq_p32_p32, (vm::opargs::Place32, lhs), (vm::opargs::Place32, rhs))
DEF_INSTR(fcmpNeq_p32_imm, (vm::opargs::Place32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpGt_p32_p32, (vm::opargs::Place32, lhs), (vm::opargs::Place32, rhs))
DEF_INSTR(fcmpGt_p32_imm, (vm::opargs::Place32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpGe_p32_p32, (vm::opargs::Place32, lhs), (vm::opargs::Place32, rhs))
DEF_INSTR(fcmpGe_p32_imm, (vm::opargs::Place32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpLt_p32_p32, (vm::opargs::Place32, lhs), (vm::opargs::Place32, rhs))
DEF_INSTR(fcmpLt_p32_imm, (vm::opargs::Place32, lhs), (vm::opargs::Immediate, rhs))
DEF_INSTR(fcmpLe_p32_p32, (vm::opargs::Place32, lhs), (vm::opargs::Place32, rhs))
DEF_INSTR(fcmpLe_p32_imm, (vm::opargs::Place32, lhs), (vm::opargs::Immediate, rhs))

// sets the flag if pointer is null
DEF_INSTR(cmpNull_pptr, (vm::opargs::PlacePtr, ptr))

// ========= VARIANT OPERATIONS ========


// Sets `variant`'s inner type to `inner_type`. It also invalidates pointers to its data.
DEF_INSTR(variantSetInner_pvnt_type, (vm::opargs::PlaceVnt, variant), (vm::opargs::Type, inner_type))
/**
 * @brief Sets `dst_ptr` to point at `variant`'s data. Expects `variant` to has `expected_type`
 * set, and if it's not, `destination` becomes nullptr.
 * @note `expected_type` required to know which type is to be expected. There is no other way to
 * obtain type information in the implementation.
 */
DEF_INSTR(
	variantGetInner_pptr_pvnt_type,
	(vm::opargs::PlacePtr, dst_ptr),
	(vm::opargs::PlaceVnt, variant),
	(vm::opargs::Type, expected_type)
)

/**
 * @brief Sets inner type of variant under `variant_ptr` to `inner_type`. It also invalidates
 * pointers to its data.
 */
DEF_INSTR(
	variantSetInner_pptr_type, (vm::opargs::PlacePtr, variant_ptr), (vm::opargs::Type, inner_type)
)

/**
 * @brief Sets `dst_ptr` to point at data of variant under `variant_ptr`. Expects the variant to
 * have `expected_type` set, and if it's not, `destination` becomes nullptr.
 * @note `expected_type` required to know which type is to be expected. There is no other way to
 * obtain type information in the implementation.
 */
DEF_INSTR(
	variantGetInner_pptr_pptr_type,
	(vm::opargs::PlacePtr, dst_ptr),
	(vm::opargs::PlacePtr, variant_ptr),
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
DEF_INSTR(call_ffifunc, (vm::opargs::FFIFunctionName, function))

DEF_INSTR(set_threadctx, (vm::opargs::FunctionName, function))

// return while performing a tail call
DEF_INSTR(ret_tailcall_func, (vm::opargs::FunctionName, function))
// return
DEF_INSTR(ret)

// ========= STACK OPERATIONS ========

// initialize local variable on local stack with given type
DEF_INSTR(init_pany_type, (vm::opargs::PlaceAny, var), (vm::opargs::Type, type))
// pop variable from local stack
DEF_INSTR(deinit)

// ========= IO OPERATIONS ========

DEF_INSTR(input_p64, (vm::opargs::Place64, dst))
DEF_INSTR(output_p64, (vm::opargs::Place64, src))

DEF_INSTR(input_p32, (vm::opargs::Place32, dst))
DEF_INSTR(output_p32, (vm::opargs::Place32, src))


// ========= CLASS OPERATIONS ========

// initialises vtable pointer
DEF_INSTR(setVTable_pptr_type, (vm::opargs::PlacePtr, object_ptr), (vm::opargs::Type, type))
// deinitialises vtable pointer
DEF_INSTR(resetVTable_pptr, (vm::opargs::PlacePtr, object_ptr))
// casts pointed object to its superclass
DEF_INSTR(upcast_pptr_pptr, (vm::opargs::PlacePtr, dst), (vm::opargs::PlacePtr, src))
// tries to cast pointed object to its subclass
DEF_INSTR(downcast_pptr_pptr, (vm::opargs::PlacePtr, dst), (vm::opargs::PlacePtr, src))
// calls a method of specified name on an a pointer. Performs the dynamic dispatch.
DEF_INSTR(
	virtual_call_pptr_method, (vm::opargs::PlacePtr, object_ptr), (vm::opargs::MethodName, method)
)

// ========= GENERAL POINTER OPERATIONS ========

// allocates given type, stores pointer
DEF_INSTR(alloc_pptr_type, (vm::opargs::PlacePtr, ptr), (vm::opargs::Type, type))
// frees block under pointer
DEF_INSTR(free_pptr, (vm::opargs::PlacePtr, ptr))


// stores local data at pointer
DEF_INSTR(store_pptr_pany, (vm::opargs::PlacePtr, dst_ptr), (vm::opargs::PlaceAny, src))
// dereferences pointer and stores into local
DEF_INSTR(load_pany_pptr, (vm::opargs::PlaceAny, dst), (vm::opargs::PlacePtr, src_ptr))

// stores reference to local object of any type T in pointer<T>
DEF_INSTR(ref_pptr_pany, (vm::opargs::PlacePtr, dst_ptr), (vm::opargs::PlaceAny, src))
// stores reference to global object of any type T in pointer<T>
// stores reference to global variant object in pointer<Variant>
DEF_INSTR(ref_pptr_pvnt, (vm::opargs::PlacePtr, dst_ptr), (vm::opargs::PlaceVnt, src))

// ========= STRUCTURE OPERATIONS ========

// loads effective address of struct field
DEF_INSTR(
	structLea_pptr_pptr_field,
	(vm::opargs::PlacePtr, dst_ptr),
	(vm::opargs::PlacePtr, src_data_ptr),
	(vm::opargs::Field, field)
)
DEF_INSTR(
	structLoad_pany_pptr_field,
	(vm::opargs::PlaceAny, dst),
	(vm::opargs::PlacePtr, src_data_ptr),
	(vm::opargs::Field, field)
)
DEF_INSTR(
	structStore_pptr_pany_field,
	(vm::opargs::PlacePtr, dst_data_ptr),
	(vm::opargs::PlaceAny, src),
	(vm::opargs::Field, field)
)

// These are the same as above, but for structs referenced directly.

DEF_INSTR(
	structLea_pptr_pste_field,
	(vm::opargs::PlacePtr, dst_ptr),
	(vm::opargs::PlaceStructure, src_data_struct),
	(vm::opargs::Field, field)
)
DEF_INSTR(
	structLoad_pany_pste_field,
	(vm::opargs::PlaceAny, dst),
	(vm::opargs::PlaceStructure, src_data_struct),
	(vm::opargs::Field, field)
)
DEF_INSTR(
	structStore_pste_pany_field,
	(vm::opargs::PlaceStructure, dst_data_struct),
	(vm::opargs::PlaceAny, src),
	(vm::opargs::Field, field)
)

// ========= STATIC ARRAY OPERATIONS ========

DEF_INSTR(
	fixedSizeTableLea_pptr_pptr_p64,
	(vm::opargs::PlacePtr, dst_ptr),
	(vm::opargs::PlacePtr, src_table_ptr),
	(vm::opargs::Place64, index)
)
DEF_INSTR(
	fixedSizeTableLoad_pany_pptr_p64,
	(vm::opargs::PlaceAny, dst),
	(vm::opargs::PlacePtr, src_table_ptr),
	(vm::opargs::Place64, index)
)

DEF_INSTR(
	fixedSizeTableStore_pptr_pany_p64,
	(vm::opargs::PlacePtr, dst_table_ptr),
	(vm::opargs::PlaceAny, src),
	(vm::opargs::Place64, index)
)

// These are the same as above, but for arrays referenced directly.
DEF_INSTR(
	fixedSizeTableLea_pptr_pfst_p64,
	(vm::opargs::PlacePtr, dst_ptr),
	(vm::opargs::PlaceFSTable, src_table),
	(vm::opargs::Place64, index)
)
DEF_INSTR(
	fixedSizeTableLoad_pany_pfst_p64,
	(vm::opargs::PlaceAny, dst),
	(vm::opargs::PlaceFSTable, src_table),
	(vm::opargs::Place64, index)
)

DEF_INSTR(
	fixedSizeTableStore_pfst_pany_p64,
	(vm::opargs::PlaceFSTable, dst_table),
	(vm::opargs::PlaceAny, src),
	(vm::opargs::Place64, index)
)

// ========= DYNAMIC ARRAY OPERATIONS ========

DEF_INSTR(
	dynTableLea_pptr_pptr_p64,
	(vm::opargs::PlacePtr, dst_ptr),
	(vm::opargs::PlacePtr, src_table_ptr),
	(vm::opargs::Place64, index)
)
DEF_INSTR(
	dynTableLoad_pany_pptr_p64,
	(vm::opargs::PlaceAny, dst),
	(vm::opargs::PlacePtr, src_table_ptr),
	(vm::opargs::Place64, index)
)

DEF_INSTR(
	dynTableStore_pptr_pany_p64,
	(vm::opargs::PlacePtr, dst_table_ptr),
	(vm::opargs::PlaceAny, src),
	(vm::opargs::Place64, index)
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
	dynTableReAlloc_pptr_type_p64,
	(vm::opargs::PlacePtr, dst_table_ptr),
	(vm::opargs::Type, table_type),
	(vm::opargs::Place64, new_elem_count)
)

/**
 * @brief Outputs a dynamic table of bytes as a string.
 */
DEF_INSTR(strOutput_pptr, (vm::opargs::PlacePtr, string_ptr))

// ========= CPOINTER OPERATIONS ========
// Operations on raw C pointers (native addresses obtained via FFI). The native side of these
// copies is unchecked by design - the program is trusted for the native address. On the VM side
// every copy is bounded by the pointed-to type: the value copies by the pointee's size, and the
// array copies by the dynamic table's element count (checked at runtime).

/**
 * @brief Copies `sizeof(pointee)` bytes from the native memory addressed by `src_ptr` into
 * `dst`. The source cpointer must have an FFI-compliant pointee, and `dst` must be of exactly
 * the pointee type.
 */
DEF_INSTR(cptrLoad_pany_pcpt, (vm::opargs::PlaceAny, dst), (vm::opargs::PlaceCptr, src_ptr))

/**
 * @brief Copies `sizeof(pointee)` bytes from `src` to the native memory addressed by
 * `dst_ptr`. The destination cpointer must have an FFI-compliant pointee, and `src` must be of
 * exactly the pointee type.
 */
DEF_INSTR(cptrStore_pcpt_pany, (vm::opargs::PlaceCptr, dst_ptr), (vm::opargs::PlaceAny, src))

/**
 * @brief Copies `sizeof(dst_ptr's pointee)` raw bytes from the native memory addressed by
 * `src_ptr` to the VM memory pointed to by `dst_ptr`. Works with any cpointer; the byte count
 * is fixed by the VM pointer's pointee type, which must be trivially copyable, so the copy can
 * never run past the pointed-to value into neighboring VM data.
 */
DEF_INSTR(cptrRead_pptr_pcpt, (vm::opargs::PlacePtr, dst_ptr), (vm::opargs::PlaceCptr, src_ptr))

/**
 * @brief Copies `sizeof(src_ptr's pointee)` raw bytes from the VM memory pointed to by
 * `src_ptr` to the native memory addressed by `dst_ptr`. Works with any cpointer; the byte
 * count is fixed by the VM pointer's pointee type, which must be trivially copyable.
 */
DEF_INSTR(cptrWrite_pcpt_pptr, (vm::opargs::PlaceCptr, dst_ptr), (vm::opargs::PlacePtr, src_ptr))

/**
 * @brief Reinterprets a cpointer as another cpointer type (the analogue of a C cast). Any
 * cpointer type converts to any other; the copy itself is a plain 8-byte move.
 */
DEF_INSTR(cptrCast_pcpt_pcpt, (vm::opargs::PlaceCptr, dst), (vm::opargs::PlaceCptr, src))

/**
 * @brief Sets `dst` to `src + offset` (byte-wise pointer arithmetic). Both places must have
 * the identical cpointer type.
 */
// @TODO: Perhaps add a dynamic check for the cast???
DEF_INSTR(
	cptrAddOffset_pcpt_pcpt_p64,
	(vm::opargs::PlaceCptr, dst),
	(vm::opargs::PlaceCptr, src),
	(vm::opargs::Place64, offset)
)

// Sets the flag if the C pointer is null (the native address 0). Works with any cpointer type.
DEF_INSTR(cmpNull_pcpt, (vm::opargs::PlaceCptr, ptr))

// ========= TYPE OPERATIONS ========
// Casts a primitive type in-place. This does nothing at runtime, but is needed
// for type checking.
DEF_INSTR(cast_p8_type, (vm::opargs::Place8, value), (vm::opargs::Type, target_type))
DEF_INSTR(cast_p16_type, (vm::opargs::Place16, value), (vm::opargs::Type, target_type))
DEF_INSTR(cast_p32_type, (vm::opargs::Place32, value), (vm::opargs::Type, target_type))
DEF_INSTR(cast_p64_type, (vm::opargs::Place64, value), (vm::opargs::Type, target_type))

// Copies a pointer to a fixed-size table into a pointer to a dynamic table with the same
// element type. The pointer value is unchanged; this is a type-system-only reinterpretation.
// @note The resulting pointer must not be passed to dynTableReAlloc.
DEF_INSTR(
	fstToDynTable_pptr_pptr,
	(vm::opargs::PlacePtr, dst_table_ptr),
	(vm::opargs::PlacePtr, src_table_ptr)
)

// ========= CONVERSION OPERATIONS ========

// Sign Extension
DEF_INSTR(sext_p16_p8, (vm::opargs::Place16, dst), (vm::opargs::Place8, src))
DEF_INSTR(sext_p32_p8, (vm::opargs::Place32, dst), (vm::opargs::Place8, src))
DEF_INSTR(sext_p64_p8, (vm::opargs::Place64, dst), (vm::opargs::Place8, src))
DEF_INSTR(sext_p32_p16, (vm::opargs::Place32, dst), (vm::opargs::Place16, src))
DEF_INSTR(sext_p64_p16, (vm::opargs::Place64, dst), (vm::opargs::Place16, src))
DEF_INSTR(sext_p64_p32, (vm::opargs::Place64, dst), (vm::opargs::Place32, src))

// Zero Extension
DEF_INSTR(zext_p16_p8, (vm::opargs::Place16, dst), (vm::opargs::Place8, src))
DEF_INSTR(zext_p32_p8, (vm::opargs::Place32, dst), (vm::opargs::Place8, src))
DEF_INSTR(zext_p64_p8, (vm::opargs::Place64, dst), (vm::opargs::Place8, src))
DEF_INSTR(zext_p32_p16, (vm::opargs::Place32, dst), (vm::opargs::Place16, src))
DEF_INSTR(zext_p64_p16, (vm::opargs::Place64, dst), (vm::opargs::Place16, src))
DEF_INSTR(zext_p64_p32, (vm::opargs::Place64, dst), (vm::opargs::Place32, src))

// Truncation
DEF_INSTR(trunc_p8_p16, (vm::opargs::Place8, dst), (vm::opargs::Place16, src))
DEF_INSTR(trunc_p8_p32, (vm::opargs::Place8, dst), (vm::opargs::Place32, src))
DEF_INSTR(trunc_p8_p64, (vm::opargs::Place8, dst), (vm::opargs::Place64, src))
DEF_INSTR(trunc_p16_p32, (vm::opargs::Place16, dst), (vm::opargs::Place32, src))
DEF_INSTR(trunc_p16_p64, (vm::opargs::Place16, dst), (vm::opargs::Place64, src))
DEF_INSTR(trunc_p32_p64, (vm::opargs::Place32, dst), (vm::opargs::Place64, src))

// Int to Float
DEF_INSTR(sitofp_p32_p8, (vm::opargs::Place32, dst), (vm::opargs::Place8, src))
DEF_INSTR(sitofp_p64_p8, (vm::opargs::Place64, dst), (vm::opargs::Place8, src))
DEF_INSTR(uitofp_p32_p8, (vm::opargs::Place32, dst), (vm::opargs::Place8, src))
DEF_INSTR(uitofp_p64_p8, (vm::opargs::Place64, dst), (vm::opargs::Place8, src))

DEF_INSTR(sitofp_p32_p16, (vm::opargs::Place32, dst), (vm::opargs::Place16, src))
DEF_INSTR(sitofp_p64_p16, (vm::opargs::Place64, dst), (vm::opargs::Place16, src))
DEF_INSTR(uitofp_p32_p16, (vm::opargs::Place32, dst), (vm::opargs::Place16, src))
DEF_INSTR(uitofp_p64_p16, (vm::opargs::Place64, dst), (vm::opargs::Place16, src))

DEF_INSTR(sitofp_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(sitofp_p64_p32, (vm::opargs::Place64, dst), (vm::opargs::Place32, src))
DEF_INSTR(uitofp_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(uitofp_p64_p32, (vm::opargs::Place64, dst), (vm::opargs::Place32, src))

DEF_INSTR(sitofp_p32_p64, (vm::opargs::Place32, dst), (vm::opargs::Place64, src))
DEF_INSTR(sitofp_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))
DEF_INSTR(uitofp_p32_p64, (vm::opargs::Place32, dst), (vm::opargs::Place64, src))
DEF_INSTR(uitofp_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))

// Float to Int
DEF_INSTR(fptosi_p8_p32, (vm::opargs::Place8, dst), (vm::opargs::Place32, src))
DEF_INSTR(fptoui_p8_p32, (vm::opargs::Place8, dst), (vm::opargs::Place32, src))
DEF_INSTR(fptosi_p16_p32, (vm::opargs::Place16, dst), (vm::opargs::Place32, src))
DEF_INSTR(fptoui_p16_p32, (vm::opargs::Place16, dst), (vm::opargs::Place32, src))
DEF_INSTR(fptosi_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(fptoui_p32_p32, (vm::opargs::Place32, dst), (vm::opargs::Place32, src))
DEF_INSTR(fptosi_p64_p32, (vm::opargs::Place64, dst), (vm::opargs::Place32, src))
DEF_INSTR(fptoui_p64_p32, (vm::opargs::Place64, dst), (vm::opargs::Place32, src))

DEF_INSTR(fptosi_p8_p64, (vm::opargs::Place8, dst), (vm::opargs::Place64, src))
DEF_INSTR(fptoui_p8_p64, (vm::opargs::Place8, dst), (vm::opargs::Place64, src))
DEF_INSTR(fptosi_p16_p64, (vm::opargs::Place16, dst), (vm::opargs::Place64, src))
DEF_INSTR(fptoui_p16_p64, (vm::opargs::Place16, dst), (vm::opargs::Place64, src))
DEF_INSTR(fptosi_p32_p64, (vm::opargs::Place32, dst), (vm::opargs::Place64, src))
DEF_INSTR(fptoui_p32_p64, (vm::opargs::Place32, dst), (vm::opargs::Place64, src))
DEF_INSTR(fptosi_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))
DEF_INSTR(fptoui_p64_p64, (vm::opargs::Place64, dst), (vm::opargs::Place64, src))

// Float to float
DEF_INSTR(fptrunc_p32_p64, (vm::opargs::Place32, dst), (vm::opargs::Place64, src))
DEF_INSTR(fpext_p64_p32, (vm::opargs::Place64, dst), (vm::opargs::Place32, src))

// ========= MISC ========


DEF_INSTR(nop)

// terminates execution
DEF_INSTR(exit)

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
