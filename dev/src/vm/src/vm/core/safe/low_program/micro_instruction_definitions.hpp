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

DEF_MICRO_INSTR(mov_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mov_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(cmov_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(cmov_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(mov_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mov_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(cmov_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(cmov_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(mov_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mov_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(cmov_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(cmov_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(mov_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mov_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(cmov_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(cmov_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)


DEF_MICRO_INSTR(
	mov_bste_bste, vm::low::opargs::PlaceBlockStructure, vm::low::opargs::PlaceBlockStructure
)
DEF_MICRO_INSTR(mov_bfst_bfst, vm::low::opargs::PlaceBlockFSTable, vm::low::opargs::PlaceBlockFSTable)
DEF_MICRO_INSTR(mov_bvnt_bvnt, vm::low::opargs::PlaceBlockVariant, vm::low::opargs::PlaceBlockVariant)
// does a shallow pointer copy
DEF_MICRO_INSTR(mov_pptr_pptr, vm::low::opargs::PlacePtr, vm::low::opargs::PlacePtr)

// sets pointer to null
DEF_MICRO_INSTR(setNull_pptr, vm::low::opargs::PlacePtr)

// It requires a `ext_imm` after this instruction as third argument, defining the size of the opaque
// type in bytes.
DEF_MICRO_INSTR(mov_popq_popq, vm::low::opargs::PlaceOpq, vm::low::opargs::PlaceOpq)

// ========= SIGNED INTEGER ARITHMETIC OPERATIONS ========
DEF_MICRO_INSTR(add_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(add_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(sub_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(sub_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mul_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(mul_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(div_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(div_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mod_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(mod_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(neg_p64, vm::low::opargs::Place64)

DEF_MICRO_INSTR(add_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(add_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(sub_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(sub_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mul_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(mul_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(div_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(div_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mod_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(mod_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(neg_p32, vm::low::opargs::Place32)

DEF_MICRO_INSTR(add_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(add_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(sub_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(sub_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mul_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(mul_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(div_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(div_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mod_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(mod_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(neg_p16, vm::low::opargs::Place16)

DEF_MICRO_INSTR(add_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(add_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(sub_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(sub_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mul_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(mul_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(div_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(div_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(mod_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(mod_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(neg_p8, vm::low::opargs::Place8)

// ========= UNSIGNED INTEGER ARITHMETIC OPERATIONS ========
DEF_MICRO_INSTR(umul_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(umul_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(umod_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(umod_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(udiv_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(udiv_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(umul_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(umul_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(umod_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(umod_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(udiv_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(udiv_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(umul_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(umul_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(umod_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(umod_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(udiv_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(udiv_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(umul_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(umul_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(umod_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(umod_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(udiv_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(udiv_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)

// ========= FLOATING POINT OPERATIONS ========
DEF_MICRO_INSTR(fadd_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fadd_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fsub_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fsub_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fmul_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fmul_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fdiv_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fdiv_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fneg_p64, vm::low::opargs::Place64)

DEF_MICRO_INSTR(fadd_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(fadd_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fsub_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(fsub_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fmul_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(fmul_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fdiv_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(fdiv_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fneg_p32, vm::low::opargs::Place32)

// ========= BOOLEAN OPERATIONS ========

// Evaluate logical operations (AND, OR, etc.) on operands as booleans (non-zero = true)
// Result is 0 or 1 stored in the first argument

DEF_MICRO_INSTR(log_and_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(log_and_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(log_or_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(log_or_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(log_xor_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(log_xor_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)

DEF_MICRO_INSTR(log_not_p8, vm::low::opargs::Place8)

// ======== BITWISE OPERATIONS ========

// 64-bit
DEF_MICRO_INSTR(bit_and_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(bit_and_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(bit_or_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(bit_or_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(bit_xor_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(bit_xor_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(shl_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(shl_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(shr_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(shr_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(bit_not_p64, vm::low::opargs::Place64)

// 32-bit
DEF_MICRO_INSTR(bit_and_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(bit_and_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(bit_or_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(bit_or_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(bit_xor_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(bit_xor_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(shl_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(shl_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(shr_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(shr_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(bit_not_p32, vm::low::opargs::Place32)

// 16-bit
DEF_MICRO_INSTR(bit_and_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(bit_and_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(bit_or_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(bit_or_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(bit_xor_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(bit_xor_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(shl_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(shl_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(shr_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(shr_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(bit_not_p16, vm::low::opargs::Place16)

// 8-bit
DEF_MICRO_INSTR(bit_and_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(bit_and_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(bit_or_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(bit_or_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(bit_xor_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(bit_xor_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(shl_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(shl_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(shr_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(shr_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(bit_not_p8, vm::low::opargs::Place8)

// ========= LOGICAL OPERATIONS ========

// --- 64-bit Integer Comparisons ---
DEF_MICRO_INSTR(cmpEq_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(cmpEq_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpNeq_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(cmpNeq_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpGt_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(cmpGt_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpGe_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(cmpGe_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGt_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(ucmpGt_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGe_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(ucmpGe_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpLt_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(cmpLt_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpLe_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(cmpLe_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLt_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(ucmpLt_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLe_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(ucmpLe_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)

// --- 32-bit Integer Comparisons ---
DEF_MICRO_INSTR(cmpEq_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(cmpEq_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpNeq_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(cmpNeq_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpGt_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(cmpGt_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpGe_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(cmpGe_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGt_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(ucmpGt_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGe_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(ucmpGe_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpLt_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(cmpLt_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpLe_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(cmpLe_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLt_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(ucmpLt_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLe_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(ucmpLe_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)

// --- 16-bit Integer Comparisons ---
DEF_MICRO_INSTR(cmpEq_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(cmpEq_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpNeq_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(cmpNeq_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpGt_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(cmpGt_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpGe_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(cmpGe_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGt_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(ucmpGt_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGe_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(ucmpGe_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpLt_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(cmpLt_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpLe_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(cmpLe_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLt_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(ucmpLt_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLe_p16_p16, vm::low::opargs::Place16, vm::low::opargs::Place16)
DEF_MICRO_INSTR(ucmpLe_p16_imm, vm::low::opargs::Place16, vm::low::opargs::Immediate)

// --- 8-bit Integer Comparisons ---
DEF_MICRO_INSTR(cmpEq_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(cmpEq_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpNeq_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(cmpNeq_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpGt_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(cmpGt_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpGe_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(cmpGe_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGt_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(ucmpGt_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpGe_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(ucmpGe_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpLt_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(cmpLt_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(cmpLe_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(cmpLe_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLt_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(ucmpLt_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ucmpLe_p8_p8, vm::low::opargs::Place8, vm::low::opargs::Place8)
DEF_MICRO_INSTR(ucmpLe_p8_imm, vm::low::opargs::Place8, vm::low::opargs::Immediate)

// --- 64-bit Floating Point Comparisons ---
DEF_MICRO_INSTR(fcmpEq_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fcmpEq_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpNeq_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fcmpNeq_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpGt_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fcmpGt_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpGe_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fcmpGe_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpLt_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fcmpLt_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpLe_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fcmpLe_p64_imm, vm::low::opargs::Place64, vm::low::opargs::Immediate)

// --- 32-bit Floating Point Comparisons ---
DEF_MICRO_INSTR(fcmpEq_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(fcmpEq_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpNeq_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(fcmpNeq_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpGt_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(fcmpGt_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpGe_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(fcmpGe_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpLt_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(fcmpLt_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(fcmpLe_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(fcmpLe_p32_imm, vm::low::opargs::Place32, vm::low::opargs::Immediate)

// sets the flag if pointer is null
DEF_MICRO_INSTR(cmpNull_pptr, vm::low::opargs::PlacePtr)

// ========= VARIANT OPERATIONS ========


/**
 * Sets `variant`'s inner type to `inner_type`. It also invalidates pointers to its data.
 * @note `ext_type` required to know the variant type quickly at runtime.
 */
DEF_MICRO_INSTR(
	variantSetInner_bvnt_type,
	vm::low::opargs::PlaceBlockVariant /* variant */,
	vm::low::opargs::Type /* 		 inner_type
    vm::low::opargs::Type 			 variant_type */
)
/**
 * @brief Sets `destination` to point at `variant`'s data. Expects `variant` to has `expected_type`
 * set, and if it's not, `destination` becomes nullptr.
 * @note `ext_type_type` required to know the expected alternative and the variant type.
 */
DEF_MICRO_INSTR(
	variantGetInner_pptr_bvnt,
	vm::low::opargs::PlacePtr /* destination */,
	vm::low::opargs::PlaceBlockVariant /* variant,
    vm::low::opargs::Type 			 expected_type
    vm::low::opargs::Type 			 variant_type */
)

/**
 * @brief Sets inner type of variant under `variant_ptr` to `inner_type`. It also invalidates
 * pointers to its data.
 * @note `ext_type` required to know the variant type quickly at runtime.
 */
DEF_MICRO_INSTR(
	variantSetInner_pptr_type,
	vm::low::opargs::PlacePtr /* variant_ptr */,
	vm::low::opargs::Type /* 		 inner_type
    vm::low::opargs::Type 			 variant_type */
)

/**
 * @brief Sets `destination` to point at data of variant under `variant_ptr`. Expects the variant to
 * have `expected_type` set, and if it's not, `destination` becomes nullptr.
 * @note `ext_type_type` required to know the expected alternative and the variant type.
 */
DEF_MICRO_INSTR(
	variantGetInner_pptr_pptr,
	vm::low::opargs::PlacePtr /* destination */,
	vm::low::opargs::PlacePtr /* variant_ptr,
    vm::low::opargs::Type 			 expected_type
    vm::low::opargs::Type 			 variant_type */
)

// ========= JUMPS ========

DEF_MICRO_INSTR(jmp_label, vm::low::opargs::Label)
DEF_MICRO_INSTR(jmpIf_label, vm::low::opargs::Label)
DEF_MICRO_INSTR(jmpIfNot_label, vm::low::opargs::Label)

// ========= FUNCTION OPERATIONS ========

/**
 * @brief Calls a function.
 * The second argument is the distance between the caller's local stack base and the callee's
 * one, i.e. the caller's stack size at this point minus the space shared with the callee.
 */
DEF_MICRO_INSTR(call_func, vm::low::opargs::FunctionID, vm::low::opargs::Immediate)
#ifdef ENABLE_JIT
// function prologue, potentially compiles the current function and executes the native version
// mentioned in dev/scripts/jit/jitable_interface.py
/**
 * @brief Function prologue, potentially compiles the function in which it is situated and executes
 * the native version.
 * @note Unoptimizable by JIT, listed in dev/scripts/py/jit/jitable_interface.py. */
DEF_MICRO_INSTR(jitEntrypoint)
#endif

/**
 * @note Unoptimizable by JIT, listed in dev/scripts/py/jit/jitable_interface.py.
 */
DEF_MICRO_INSTR(call_builtinfunc, vm::low::opargs::BuiltinFunctionID)

DEF_MICRO_INSTR(call_cfunc, vm::low::opargs::ExtCFunction)
DEF_MICRO_INSTR(call_ffifunc, vm::low::opargs::FFIFunction)

DEF_MICRO_INSTR(set_threadctx, vm::low::opargs::FunctionID)

// return while performing a tail call
DEF_MICRO_INSTR(ret_tailcall_func, vm::low::opargs::FunctionID)
/**
 * @brief Returns from the function.
 * The argument is the number of the function's return values, which are the only entries
 * left on the block reference stack once the function's own `deinit`s ran.
 */
DEF_MICRO_INSTR(ret_imm, vm::low::opargs::Immediate)

// ========= STACK OPERATIONS ========

/**
 * @brief Initializes a local variable at a statically known offset on the local stack,
 * creating a block for it.
 * The first argument is the byte offset of the variable in the frame's local stack.
 */
DEF_MICRO_INSTR(initBlock_imm_type, vm::low::opargs::Immediate, vm::low::opargs::Type)

/**
 * @brief Initializes a local variable without creating a block for it: zeroes the variable's
 * bytes and reserves its slot.
 *
 * A block is only needed once something refers to the variable through it, and it is created
 * on demand by the instruction that needs it, out of `LowFuncData::local_slot_descs`.
 *
 * @arg0 - byte offset of the variable in the frame's local stack.
 * @arg1 - size of the variable in bytes.
 */
DEF_MICRO_INSTR(simpleInit_imm_imm, vm::low::opargs::Immediate, vm::low::opargs::Immediate)

/**
 * @brief Pops the topmost variable from the local stack, freeing its block if one was created.
 */
DEF_MICRO_INSTR(deinit)

/**
 * @brief Like `deinit`, for a variable whose type needs destructors run - i.e. one that holds
 * pointers, whose pointed-to blocks must be dereferenced. Needed separately because with no
 * block around there is nothing to take the type from.
 *
 * @arg0 - byte offset of the variable in the frame's local stack.
 * @arg1 - type of the variable.
 */
DEF_MICRO_INSTR(deinit_dtor_imm_type, vm::low::opargs::Immediate, vm::low::opargs::Type)

// ========= IO OPERATIONS ========

DEF_MICRO_INSTR(input_p64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(output_p64, vm::low::opargs::Place64)

DEF_MICRO_INSTR(input_p32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(output_p32, vm::low::opargs::Place32)


// ========= CLASS OPERATIONS ========

// initialises vtable pointer
DEF_MICRO_INSTR(setVTable_pptr_type, vm::low::opargs::PlacePtr, vm::low::opargs::Type)
// deinitialises vtable pointer
DEF_MICRO_INSTR(resetVTable_pptr, vm::low::opargs::PlacePtr)
// tries to cast pointed object to its subclass, requires that ext_64 is next
DEF_MICRO_INSTR(downcast_pptr_pptr, vm::low::opargs::PlacePtr, vm::low::opargs::PlacePtr)
// calls a method of specified name on an a pointer. Performs the dynamic dispatch.
// Requires `ext_imm` holding the local stack distance, see `call_func`
DEF_MICRO_INSTR(virtual_call_pptr_method, vm::low::opargs::PlacePtr, vm::low::opargs::MethodName)

// ========= GENERAL POINTER OPERATIONS ========

// allocates given type, stores pointer
DEF_MICRO_INSTR(alloc_pptr_type, vm::low::opargs::PlacePtr, vm::low::opargs::Type)
// frees block under pointer
DEF_MICRO_INSTR(free_pptr, vm::low::opargs::PlacePtr)


// stores local data at pointer
DEF_MICRO_INSTR(store_pptr_bany, vm::low::opargs::PlacePtr, vm::low::opargs::PlaceBlockAny)
// dereferences pointer and stores into local
DEF_MICRO_INSTR(load_bany_pptr, vm::low::opargs::PlaceBlockAny, vm::low::opargs::PlacePtr)

// stores reference to local object of any type T in pointer<T>
DEF_MICRO_INSTR(ref_pptr_bany, vm::low::opargs::PlacePtr, vm::low::opargs::PlaceBlockAny)

// ========= STRUCTURE OPERATIONS ========

// expects `ext_field` to be the next instruction
// loads effective address of struct field
DEF_MICRO_INSTR(
	structLea_pptr_pptr,
	vm::low::opargs::PlacePtr /* destination */,
	vm::low::opargs::PlacePtr /* source,
    vm::low::opargs::Field 			 field */
)
// expects `ext_field` to be the next instruction
DEF_MICRO_INSTR(
	structLoad_bany_pptr,
	vm::low::opargs::PlaceBlockAny /* destination */,
	vm::low::opargs::PlacePtr /* data_ptr,
    vm::low::opargs::Field 			 field */
)
// expects `ext_field` to be the next instruction
DEF_MICRO_INSTR(
	structStore_pptr_bany,
	vm::low::opargs::PlacePtr /* data_ptr */,
	vm::low::opargs::PlaceBlockAny /* source ,
    vm::low::opargs::Field 			 field */
)

// Same as above, but using a struct directly.

// expects `ext_field` to be the next instruction
DEF_MICRO_INSTR(
	structLea_pptr_bste,
	vm::low::opargs::PlacePtr /* destination */,
	vm::low::opargs::PlaceBlockStructure /* source,
    vm::low::opargs::Field 			 field */
)

// expects `ext_field` to be the next instruction
DEF_MICRO_INSTR(
	structLoad_bany_bste,
	vm::low::opargs::PlaceBlockAny /* destination */,
	vm::low::opargs::PlaceBlockStructure /* data_struct,
    vm::low::opargs::Field 			 field */
)

// expects `ext_field` to be the next instruction
DEF_MICRO_INSTR(
	structStore_bste_bany,
	vm::low::opargs::PlaceBlockStructure /* data_struct */,
	vm::low::opargs::PlaceBlockAny /* source ,
    vm::low::opargs::Field 			 field */
)

// ========= ARRAY OPERATIONS ========

// generic `lea` for both fixed size and dynamic tables.
// expects `ext_p64_type` (index, element type) to be the next instruction
DEF_MICRO_INSTR(
	anyArrayLea_pptr_pptr,
	vm::low::opargs::PlacePtr /* destination */,
	vm::low::opargs::PlacePtr /* table_ptr,
    vm::low::opargs::Place64 	 index
    vm::low::opargs::Type	 	 element_type */
)

// generic `load` for both fixed size and dynamic tables.
// expects `ext_p64_type` (index, element type) to be the next instruction
DEF_MICRO_INSTR(
	anyArrayLoad_bany_pptr,
	vm::low::opargs::PlaceBlockAny /* destination */,
	vm::low::opargs::PlacePtr /* table_ptr,
    vm::low::opargs::Place64 	 index
    vm::low::opargs::Type	     element_type */
)

// generic `store` for both fixed size and dynamic tables.
// expects `ext_p64_type` (index, element type) to be the next instruction
DEF_MICRO_INSTR(
	anyArrayStore_pptr_bany,
	vm::low::opargs::PlacePtr /* table_ptr */,
	vm::low::opargs::PlaceBlockAny /* source,
    vm::low::opargs::Place64 	 index
    vm::low::opargs::Type	     element_type */
)

// expects `ext_p64_type` (index, element type) to be the next instruction
DEF_MICRO_INSTR(
	fixedSizeTableLea_pptr_bfst,
	vm::low::opargs::PlacePtr /* destination */,
	vm::low::opargs::PlaceBlockFSTable /* table_ptr,
    vm::low::opargs::Place64 	 index
    vm::low::opargs::Type	     element_type */
)

// expects `ext_p64_type` (index, element type) to be the next instruction
DEF_MICRO_INSTR(
	fixedSizeTableLoad_bany_bfst,
	vm::low::opargs::PlaceBlockAny /* destination */,
	vm::low::opargs::PlaceBlockFSTable /* table_ptr,
    vm::low::opargs::Place64 	 index
    vm::low::opargs::Type	     element_type */
)

// expects `ext_p64_type` (index, element type) to be the next instruction
DEF_MICRO_INSTR(
	fixedSizeTableStore_bfst_bany,
	vm::low::opargs::PlaceBlockFSTable /* table_ptr */,
	vm::low::opargs::PlaceBlockAny /* source,
    vm::low::opargs::Place64 	 index
    vm::low::opargs::Type	     element_type */
)

/**
 * @brief Re-allocates dynamic table under `table_ptr` with
 * `new_elem_count` elements. If given nullptr, then it will allocate
 * a new array.
 * `table_type` is type of the dynamic table itself, not the element type.
 * @note It's counter-intuitive, but if a reallocation has happened, this
 *  instruction will not modify pointer data (unlike in C).
 * @note `ext_p64` is required to tell the count of elements
 */
DEF_MICRO_INSTR(
	dynTableReAlloc_pptr_type,
	vm::low::opargs::PlacePtr /* table_ptr */,
	vm::low::opargs::Type /* table_type ,
    vm::low::opargs::Place64     new_elem_count */
)

/**
 * @brief Outputs a dynamic table of bytes as a string.
 */
DEF_MICRO_INSTR(
	strOutput_pptr, vm::low::opargs::PlacePtr /* string_ptr */
)

// ========= CONVERSION OPERATIONS ========
// Sign Extension
DEF_MICRO_INSTR(sext_p16_p8, vm::low::opargs::Place16, vm::low::opargs::Place8)
DEF_MICRO_INSTR(sext_p32_p8, vm::low::opargs::Place32, vm::low::opargs::Place8)
DEF_MICRO_INSTR(sext_p64_p8, vm::low::opargs::Place64, vm::low::opargs::Place8)
DEF_MICRO_INSTR(sext_p32_p16, vm::low::opargs::Place32, vm::low::opargs::Place16)
DEF_MICRO_INSTR(sext_p64_p16, vm::low::opargs::Place64, vm::low::opargs::Place16)
DEF_MICRO_INSTR(sext_p64_p32, vm::low::opargs::Place64, vm::low::opargs::Place32)

// Zero Extension
DEF_MICRO_INSTR(zext_p16_p8, vm::low::opargs::Place16, vm::low::opargs::Place8)
DEF_MICRO_INSTR(zext_p32_p8, vm::low::opargs::Place32, vm::low::opargs::Place8)
DEF_MICRO_INSTR(zext_p64_p8, vm::low::opargs::Place64, vm::low::opargs::Place8)
DEF_MICRO_INSTR(zext_p32_p16, vm::low::opargs::Place32, vm::low::opargs::Place16)
DEF_MICRO_INSTR(zext_p64_p16, vm::low::opargs::Place64, vm::low::opargs::Place16)
DEF_MICRO_INSTR(zext_p64_p32, vm::low::opargs::Place64, vm::low::opargs::Place32)

// Truncation
DEF_MICRO_INSTR(trunc_p8_p16, vm::low::opargs::Place8, vm::low::opargs::Place16)
DEF_MICRO_INSTR(trunc_p8_p32, vm::low::opargs::Place8, vm::low::opargs::Place32)
DEF_MICRO_INSTR(trunc_p8_p64, vm::low::opargs::Place8, vm::low::opargs::Place64)
DEF_MICRO_INSTR(trunc_p16_p32, vm::low::opargs::Place16, vm::low::opargs::Place32)
DEF_MICRO_INSTR(trunc_p16_p64, vm::low::opargs::Place16, vm::low::opargs::Place64)
DEF_MICRO_INSTR(trunc_p32_p64, vm::low::opargs::Place32, vm::low::opargs::Place64)

// Int to Float
DEF_MICRO_INSTR(sitofp_p32_p8, vm::low::opargs::Place32, vm::low::opargs::Place8)
DEF_MICRO_INSTR(uitofp_p32_p8, vm::low::opargs::Place32, vm::low::opargs::Place8)
DEF_MICRO_INSTR(sitofp_p32_p16, vm::low::opargs::Place32, vm::low::opargs::Place16)
DEF_MICRO_INSTR(uitofp_p32_p16, vm::low::opargs::Place32, vm::low::opargs::Place16)
DEF_MICRO_INSTR(sitofp_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(uitofp_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(sitofp_p32_p64, vm::low::opargs::Place32, vm::low::opargs::Place64)
DEF_MICRO_INSTR(uitofp_p32_p64, vm::low::opargs::Place32, vm::low::opargs::Place64)

DEF_MICRO_INSTR(sitofp_p64_p8, vm::low::opargs::Place64, vm::low::opargs::Place8)
DEF_MICRO_INSTR(uitofp_p64_p8, vm::low::opargs::Place64, vm::low::opargs::Place8)
DEF_MICRO_INSTR(sitofp_p64_p16, vm::low::opargs::Place64, vm::low::opargs::Place16)
DEF_MICRO_INSTR(uitofp_p64_p16, vm::low::opargs::Place64, vm::low::opargs::Place16)
DEF_MICRO_INSTR(sitofp_p64_p32, vm::low::opargs::Place64, vm::low::opargs::Place32)
DEF_MICRO_INSTR(uitofp_p64_p32, vm::low::opargs::Place64, vm::low::opargs::Place32)
DEF_MICRO_INSTR(sitofp_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(uitofp_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)

// Float to Int (Saturating)
DEF_MICRO_INSTR(fptosi_p8_p32, vm::low::opargs::Place8, vm::low::opargs::Place32)
DEF_MICRO_INSTR(fptoui_p8_p32, vm::low::opargs::Place8, vm::low::opargs::Place32)
DEF_MICRO_INSTR(fptosi_p16_p32, vm::low::opargs::Place16, vm::low::opargs::Place32)
DEF_MICRO_INSTR(fptoui_p16_p32, vm::low::opargs::Place16, vm::low::opargs::Place32)
DEF_MICRO_INSTR(fptosi_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(fptoui_p32_p32, vm::low::opargs::Place32, vm::low::opargs::Place32)
DEF_MICRO_INSTR(fptosi_p64_p32, vm::low::opargs::Place64, vm::low::opargs::Place32)
DEF_MICRO_INSTR(fptoui_p64_p32, vm::low::opargs::Place64, vm::low::opargs::Place32)

DEF_MICRO_INSTR(fptosi_p8_p64, vm::low::opargs::Place8, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fptoui_p8_p64, vm::low::opargs::Place8, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fptosi_p16_p64, vm::low::opargs::Place16, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fptoui_p16_p64, vm::low::opargs::Place16, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fptosi_p32_p64, vm::low::opargs::Place32, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fptoui_p32_p64, vm::low::opargs::Place32, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fptosi_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fptoui_p64_p64, vm::low::opargs::Place64, vm::low::opargs::Place64)

DEF_MICRO_INSTR(fptrunc_p32_p64, vm::low::opargs::Place32, vm::low::opargs::Place64)
DEF_MICRO_INSTR(fpext_p64_p32, vm::low::opargs::Place64, vm::low::opargs::Place32)

// ========= CPOINTER OPERATIONS ========
// Copies through raw C pointers (native addresses). The cpointer operand is a plain 8-byte
// value, a Place64.

DEF_MICRO_INSTR(cptrLoad_bany_p64, vm::low::opargs::PlaceBlockAny, vm::low::opargs::Place64)
DEF_MICRO_INSTR(cptrStore_p64_bany, vm::low::opargs::Place64, vm::low::opargs::PlaceBlockAny)

// Requires `ext_imm`
DEF_MICRO_INSTR(cptrRead_pptr_p64, vm::low::opargs::PlacePtr, vm::low::opargs::Place64)
// Requires `ext_imm`
DEF_MICRO_INSTR(cptrWrite_p64_pptr, vm::low::opargs::Place64, vm::low::opargs::PlacePtr)

DEF_MICRO_INSTR(cptrCast_p64_pptr, vm::low::opargs::Place64, vm::low::opargs::PlacePtr)

// ========= POINTER DECOMPOSITION ========

DEF_MICRO_INSTR(
	ptrParts_p64_p64_pptr,
	vm::low::opargs::Place64 /* dst_id */,
	vm::low::opargs::Place64 /* dst_offset,
    vm::low::opargs::PlacePtr	 src_ptr */
)

// ========= EXT DEFINITIONS ========

// passes additional argument to preceding instruction
DEF_MICRO_INSTR(ext_p64, vm::low::opargs::Place64)
DEF_MICRO_INSTR(ext_pptr, vm::low::opargs::PlacePtr)
DEF_MICRO_INSTR(ext_imm, vm::low::opargs::Immediate)
DEF_MICRO_INSTR(ext_type, vm::low::opargs::Type)
DEF_MICRO_INSTR(ext_field, vm::low::opargs::Field)
DEF_MICRO_INSTR(ext_p64_type, vm::low::opargs::Place64, vm::low::opargs::Type)
DEF_MICRO_INSTR(ext_type_field, vm::low::opargs::Type, vm::low::opargs::Field)
DEF_MICRO_INSTR(ext_type_p64, vm::low::opargs::Type, vm::low::opargs::Place64)
DEF_MICRO_INSTR(ext_type_type, vm::low::opargs::Type, vm::low::opargs::Type)

// ========= MISC ========

DEF_MICRO_INSTR(check_strategy)
DEF_MICRO_INSTR(nop)

// terminates execution
DEF_MICRO_INSTR(exit)

/**
 * @note Unoptimizable by JIT, listed in dev/scripts/py/jit/jitable_interface.py.
 */
DEF_MICRO_INSTR(breakpoint)

DEF_MICRO_INSTR(stepGil)

/**
 * @brief This is a very internal instruction, that should not be used in regular bytecode.
 * It is a helper for start functions.
 * @arg0 - pointer to a VMValue.
 * @arg1 - byte offset of the initialized variable in the frame's local stack.
 */
DEF_MICRO_INSTR(initFromVMValue)

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
