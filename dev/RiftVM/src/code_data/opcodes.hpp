#pragma once

#include <base/ints.hpp>
#include <base/stringifyable_enum.hpp>

/**
 * Opcodes names conventions:
 *
 * Name is: name_[first arg description]_[optional second arg description]
 * Each name is:
 * imm      - immediate value
 * l[size]  - position of primitive local with given size
 * lptr     - position of local pointer
 * func     - function id
 * r[nr]    - primitive register with number [nr]
 * rprt[nr] - pointer register with number [nr]
 * type     - type name
 * label    - label name
 *
 * Most two argument operation store result in first argument
 *
 * Opcodes not following this convention have additional description
 */

MAKE_STRINGFYABLE_ENUM(
	vm,
	u16,
	OpcodeFix8,

	mov_l64_imm,

	mov_l64_l64,
	cmov_l64_l64,

	mov_l64_r0,
	mov_r0_l64,

	mov_l64_pFuncArg,     // moves primitive function arg to local variable
	mov_lptr_ptrFuncArg,  // moves pointer function arg to local variable

	add_l64_l64,
	add_l64_imm,

	// sub_l64_l64,
	sub_l64_l64,
	sub_l64_imm,

	// mul_l64_l64,
	mul_l64_imm,

	mod_l64_l64,
	mod_l64_imm,

	// div_l64_l64,
	div_l64_imm,

	cmpEq_l64_l64,
	cmpEq_l64_imm,
	cmpG_l64_l64,
	cmpG_l64_imm,

	jmpRel_label,
	jmpRelIf_label,
	jmpRelNotIf_label,

	setPArg_l64,     // set primitive argument
	setPtrArg_lptr,  // set pointer argument
	call_func,

	ret_l64,
	ret_imm,

	init_type,  // initialize local variable on local stack with given type
	deinit,     // pops variable from local stack

	input_l64,
	output_l64,

	nop,

	alloc_lptr_type,     // allocates given type, stores pointer
	free_lptr,           // frees block under pointer
	load_l64_lptr_ofs,   // load 64-bit primitive value from lptr + ofs
	store_lptr_l64_ofs,  // stores 64-bit primitive value under lptr + ofs

	ext_l64              // passes additional argument to preceding opcode
);
