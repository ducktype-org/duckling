/**
 * @file opcodes_list.hpp
 * @brief Contains a list of all DuckBC opcodes. Can be used for generating
 * repetitive code based on list of opcodes.
 *
 * you can just define `HANDLE_OPCODE` macro and include this
 * header like so:
 * ```cpp
 *  constexpr u16 countOpCases() {
 *  	u16 count = 0;
 *		#define HANDLE_OPCODE(opcode) count++;
 * 		#include "opcodes_list.hpp"
 * 		#undef HANDLE_OPCODE
 * 		return count;
 * 	}
 * ```
 * This above function just counts the opcodes, but the possibilities are endless.
 *
 * There are other macros for cases where you want to know
 * what arguments the opcode has - HANDLE_OPCODE_#ARGS,
 * where # is the number of arguments.
 * You can define them similarly to the above example.
 *
 * You can also override the `DEF_OPCODE` macro for
 * even higher control.
 */

#ifndef HANDLE_OPCODE
#define DEFAULT_HANDLE_OPCODE
#define HANDLE_OPCODE(opcode)
#endif

#ifndef HANDLE_OPCODE_0ARGS
#define DEFAULT_HANDLE_OPCODE_0ARGS
#define HANDLE_OPCODE_0ARGS(opcode) HANDLE_OPCODE(opcode)
#endif

#ifndef HANDLE_OPCODE_1ARGS
#define DEFAULT_HANDLE_OPCODE_1ARGS
#define HANDLE_OPCODE_1ARGS(opcode, arg0_type) HANDLE_OPCODE(opcode)
#endif

#ifndef HANDLE_OPCODE_2ARGS
#define DEFAULT_HANDLE_OPCODE_2ARGS
#define HANDLE_OPCODE_2ARGS(opcode, arg0_type, arg1_type) HANDLE_OPCODE(opcode)
#endif

#ifndef DEF_OPCODE
#define DEFAULT_DEF_OPCODE
#define GET_MACRO(_opcode, _1, _2, NAME, ...) NAME
#define DEF_OPCODE(...)                                                                   \
	GET_MACRO(__VA_ARGS__, HANDLE_OPCODE_2ARGS, HANDLE_OPCODE_1ARGS, HANDLE_OPCODE_0ARGS) \
	(__VA_ARGS__)
#endif

DEF_OPCODE(mov_l64_imm, vm::opargs::StackOffset, vm::opargs::ImmediateI64)

DEF_OPCODE(mov_l64_l64, vm::opargs::StackOffset, vm::opargs::StackOffset)
DEF_OPCODE(cmov_l64_l64, vm::opargs::StackOffset, vm::opargs::StackOffset)

DEF_OPCODE(mov_l64_r0, vm::opargs::StackOffset)
DEF_OPCODE(mov_r0_l64, vm::opargs::StackOffset)

DEF_OPCODE(add_l64_l64, vm::opargs::StackOffset, vm::opargs::StackOffset)
DEF_OPCODE(add_l64_imm, vm::opargs::StackOffset, vm::opargs::ImmediateI64)

// DEF_OPCODE(sub_l64_l64, vm::opargs::StackOffset, vm::opargs::StackOffset)
DEF_OPCODE(sub_l64_l64, vm::opargs::StackOffset, vm::opargs::StackOffset)
DEF_OPCODE(sub_l64_imm, vm::opargs::StackOffset, vm::opargs::ImmediateI64)

// DEF_OPCODE(mul_l64_l64, vm::opargs::StackOffset, vm::opargs::StackOffset)
DEF_OPCODE(mul_l64_imm, vm::opargs::StackOffset, vm::opargs::ImmediateI64)

DEF_OPCODE(mod_l64_l64, vm::opargs::StackOffset, vm::opargs::StackOffset)
DEF_OPCODE(mod_l64_imm, vm::opargs::StackOffset, vm::opargs::ImmediateI64)

// DEF_OPCODE(div_l64_l64, vm::opargs::StackOffset, vm::opargs::StackOffset)
DEF_OPCODE(div_l64_imm, vm::opargs::StackOffset, vm::opargs::ImmediateI64)

DEF_OPCODE(cmpEq_l64_l64, vm::opargs::StackOffset, vm::opargs::StackOffset)
DEF_OPCODE(cmpEq_l64_imm, vm::opargs::StackOffset, vm::opargs::ImmediateI64)
DEF_OPCODE(cmpG_l64_l64, vm::opargs::StackOffset, vm::opargs::StackOffset)
DEF_OPCODE(cmpG_l64_imm, vm::opargs::StackOffset, vm::opargs::ImmediateI64)

DEF_OPCODE(label, vm::opargs::Label)

DEF_OPCODE(jmpRel_label, vm::opargs::Label)
DEF_OPCODE(jmpRelIf_label, vm::opargs::Label)
DEF_OPCODE(jmpRelNotIf_label, vm::opargs::Label)

DEF_OPCODE(getFstArg_l64, vm::opargs::StackOffset)
DEF_OPCODE(getFstArg_lptr, vm::opargs::StackOffset)

DEF_OPCODE(mov_l64_arg64, vm::opargs::StackOffset, vm::opargs::ArgsOffset)
DEF_OPCODE(mov_lptr_argptr, vm::opargs::StackOffset, vm::opargs::ArgsOffset)

DEF_OPCODE(setFstArg_l64, vm::opargs::StackOffset)
DEF_OPCODE(setFstArg_lptr, vm::opargs::StackOffset)

DEF_OPCODE(mov_arg64_l64, vm::opargs::ArgsOffset, vm::opargs::StackOffset)
DEF_OPCODE(mov_argptr_lptr, vm::opargs::ArgsOffset, vm::opargs::StackOffset)

DEF_OPCODE(call_func, vm::opargs::FunctionName)

// return while performing a tail call
DEF_OPCODE(ret_tailcall, vm::opargs::FunctionName)
// return 64-bit primitive value
DEF_OPCODE(ret_l64, vm::opargs::StackOffset)
// return immediate value
DEF_OPCODE(ret_imm, vm::opargs::ImmediateI64)

// initialize local variable on local stack with given type
DEF_OPCODE(init_type, vm::opargs::Type)
// pop variable from local stack
DEF_OPCODE(deinit)

DEF_OPCODE(input_l64, vm::opargs::StackOffset)
DEF_OPCODE(output_l64, vm::opargs::StackOffset)

DEF_OPCODE(nop)

// allocates given type, stores pointer
DEF_OPCODE(alloc_lptr_type, vm::opargs::StackOffset, vm::opargs::Type)
// frees block under pointer
DEF_OPCODE(free_lptr, vm::opargs::StackOffset)
// load 64-bit primitive value from `lptr + ofs`
// expects `ext_l64` to be the next instruction
DEF_OPCODE(load_l64_lptr_ofs, vm::opargs::StackOffset, vm::opargs::StackOffset)
// stores 64-bit primitive value under `lptr + ofs`
// expects `ext_l64` to be the next instruction
DEF_OPCODE(store_lptr_l64_ofs, vm::opargs::StackOffset, vm::opargs::StackOffset)
// passes additional argument to preceding opcode
DEF_OPCODE(ext_l64, vm::opargs::StackOffset)

// terminates execution
DEF_OPCODE(exit)

DEF_OPCODE(breakpoint)

#ifdef DEFAULT_HANDLE_OPCODE
#undef DEFAULT_HANDLE_OPCODE
#undef HANDLE_OPCODE
#endif

#ifdef DEFAULT_HANDLE_OPCODE_0ARGS
#undef DEFAULT_HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_0ARGS
#endif

#ifdef DEFAULT_HANDLE_OPCODE_1ARGS
#undef DEFAULT_HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_1ARGS
#endif

#ifdef DEFAULT_HANDLE_OPCODE_2ARGS
#undef DEFAULT_HANDLE_OPCODE_2ARGS
#undef HANDLE_OPCODE_2ARGS
#endif

#ifdef DEFAULT_DEF_OPCODE
#undef DEFAULT_DEF_OPCODE
#undef DEF_OPCODE
#undef GET_MACRO
#endif
