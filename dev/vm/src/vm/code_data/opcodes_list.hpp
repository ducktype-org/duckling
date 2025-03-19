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


DEF_OPCODE(mov_l8_imm, vm::opargs::StackLocalI8, vm::opargs::Immediate)
DEF_OPCODE(mov_l8_l8, vm::opargs::StackLocalI8, vm::opargs::StackLocalI8)
DEF_OPCODE(cmov_l8_l8, vm::opargs::StackLocalI8, vm::opargs::StackLocalI8)

DEF_OPCODE(mov_l16_imm, vm::opargs::StackLocalI16, vm::opargs::Immediate)
DEF_OPCODE(mov_l16_l16, vm::opargs::StackLocalI16, vm::opargs::StackLocalI16)
DEF_OPCODE(cmov_l16_l16, vm::opargs::StackLocalI16, vm::opargs::StackLocalI16)

DEF_OPCODE(mov_l32_imm, vm::opargs::StackLocalI32, vm::opargs::Immediate)
DEF_OPCODE(mov_l32_l32, vm::opargs::StackLocalI32, vm::opargs::StackLocalI32)
DEF_OPCODE(cmov_l32_l32, vm::opargs::StackLocalI32, vm::opargs::StackLocalI32)

DEF_OPCODE(mov_l64_imm, vm::opargs::StackLocalI64, vm::opargs::Immediate)
DEF_OPCODE(mov_l64_l64, vm::opargs::StackLocalI64, vm::opargs::StackLocalI64)
DEF_OPCODE(cmov_l64_l64, vm::opargs::StackLocalI64, vm::opargs::StackLocalI64)


DEF_OPCODE(mov_l64_r0, vm::opargs::StackLocalI64)
DEF_OPCODE(mov_r0_l64, vm::opargs::StackLocalI64)


DEF_OPCODE(add_l64_l64, vm::opargs::StackLocalI64, vm::opargs::StackLocalI64)
DEF_OPCODE(add_l64_imm, vm::opargs::StackLocalI64, vm::opargs::Immediate)

DEF_OPCODE(add_l32_l32, vm::opargs::StackLocalI32, vm::opargs::StackLocalI32)
DEF_OPCODE(add_l32_imm, vm::opargs::StackLocalI32, vm::opargs::Immediate)


DEF_OPCODE(sub_l64_l64, vm::opargs::StackLocalI64, vm::opargs::StackLocalI64)
DEF_OPCODE(sub_l64_imm, vm::opargs::StackLocalI64, vm::opargs::Immediate)

DEF_OPCODE(sub_l32_l32, vm::opargs::StackLocalI32, vm::opargs::StackLocalI32)
DEF_OPCODE(sub_l32_imm, vm::opargs::StackLocalI32, vm::opargs::Immediate)


DEF_OPCODE(mul_l64_l64, vm::opargs::StackLocalI64, vm::opargs::StackLocalI64)
DEF_OPCODE(mul_l64_imm, vm::opargs::StackLocalI64, vm::opargs::Immediate)

DEF_OPCODE(mul_l32_l32, vm::opargs::StackLocalI32, vm::opargs::StackLocalI32)
DEF_OPCODE(mul_l32_imm, vm::opargs::StackLocalI32, vm::opargs::Immediate)


DEF_OPCODE(mod_l64_l64, vm::opargs::StackLocalI64, vm::opargs::StackLocalI64)
DEF_OPCODE(mod_l64_imm, vm::opargs::StackLocalI64, vm::opargs::Immediate)

DEF_OPCODE(mod_l32_l32, vm::opargs::StackLocalI32, vm::opargs::StackLocalI32)
DEF_OPCODE(mod_l32_imm, vm::opargs::StackLocalI32, vm::opargs::Immediate)


DEF_OPCODE(div_l64_l64, vm::opargs::StackLocalI64, vm::opargs::StackLocalI64)
DEF_OPCODE(div_l64_imm, vm::opargs::StackLocalI64, vm::opargs::Immediate)

DEF_OPCODE(div_l32_l32, vm::opargs::StackLocalI32, vm::opargs::StackLocalI32)
DEF_OPCODE(div_l32_imm, vm::opargs::StackLocalI32, vm::opargs::Immediate)

DEF_OPCODE(neg_l64, vm::opargs::StackLocalI64)
DEF_OPCODE(neg_l32, vm::opargs::StackLocalI32)


DEF_OPCODE(cmpEq_l64_l64, vm::opargs::StackLocalI64, vm::opargs::StackLocalI64)
DEF_OPCODE(cmpEq_l64_imm, vm::opargs::StackLocalI64, vm::opargs::Immediate)
DEF_OPCODE(cmpG_l64_l64, vm::opargs::StackLocalI64, vm::opargs::StackLocalI64)
DEF_OPCODE(cmpG_l64_imm, vm::opargs::StackLocalI64, vm::opargs::Immediate)

DEF_OPCODE(cmpEq_l32_l32, vm::opargs::StackLocalI32, vm::opargs::StackLocalI32)
DEF_OPCODE(cmpEq_l32_imm, vm::opargs::StackLocalI32, vm::opargs::Immediate)
DEF_OPCODE(cmpG_l32_l32, vm::opargs::StackLocalI32, vm::opargs::StackLocalI32)
DEF_OPCODE(cmpG_l32_imm, vm::opargs::StackLocalI32, vm::opargs::Immediate)

DEF_OPCODE(cmpEq_l8_l8, vm::opargs::StackLocalI8, vm::opargs::StackLocalI8)
DEF_OPCODE(cmpEq_l8_imm, vm::opargs::StackLocalI8, vm::opargs::Immediate)
DEF_OPCODE(cmpG_l8_l8, vm::opargs::StackLocalI8, vm::opargs::StackLocalI8)
DEF_OPCODE(cmpG_l8_imm, vm::opargs::StackLocalI8, vm::opargs::Immediate)


DEF_OPCODE(label, vm::opargs::Label)

DEF_OPCODE(jmpRel_label, vm::opargs::Label)
DEF_OPCODE(jmpRelIf_label, vm::opargs::Label)
DEF_OPCODE(jmpRelNotIf_label, vm::opargs::Label)

DEF_OPCODE(getFstArg_l64, vm::opargs::StackLocalI64)
DEF_OPCODE(getFstArg_lptr, vm::opargs::StackLocalPtr)

DEF_OPCODE(mov_l64_arg64, vm::opargs::StackLocalI64, vm::opargs::ArgsOffset)
DEF_OPCODE(mov_lptr_argptr, vm::opargs::StackLocalPtr, vm::opargs::ArgsOffset)

DEF_OPCODE(setFstArg_l64, vm::opargs::StackLocalI64)
DEF_OPCODE(setFstArg_lptr, vm::opargs::StackLocalPtr)

DEF_OPCODE(mov_arg64_l64, vm::opargs::ArgsOffset, vm::opargs::StackLocalI64)
DEF_OPCODE(mov_argptr_lptr, vm::opargs::ArgsOffset, vm::opargs::StackLocalPtr)

DEF_OPCODE(call_func, vm::opargs::FunctionName)

// return while performing a tail call
DEF_OPCODE(ret_tailcall, vm::opargs::FunctionName)
// return value on the stack
DEF_OPCODE(ret_l64, vm::opargs::StackLocalI64)
DEF_OPCODE(ret_l32, vm::opargs::StackLocalI32)
// return immediate value
DEF_OPCODE(ret_imm, vm::opargs::Immediate)
// void return
DEF_OPCODE(ret)

// initialize local variable on local stack with given type
DEF_OPCODE(init_type, vm::opargs::Type)
// pop variable from local stack
DEF_OPCODE(deinit)

DEF_OPCODE(input_l64, vm::opargs::StackLocalI64)
DEF_OPCODE(output_l64, vm::opargs::StackLocalI64)

DEF_OPCODE(nop)

// allocates given type, stores pointer
DEF_OPCODE(alloc_lptr_type, vm::opargs::StackLocalPtr, vm::opargs::Type)
// frees block under pointer
DEF_OPCODE(free_lptr, vm::opargs::StackLocalPtr)
// load 64-bit primitive value from `lptr + ofs`
// expects `ext_l64` to be the next instruction
DEF_OPCODE(load_l64_lptr_ofs, vm::opargs::StackLocalI64, vm::opargs::StackLocalPtr)
// stores 64-bit primitive value under `lptr + ofs`
// expects `ext_l64` to be the next instruction
DEF_OPCODE(store_lptr_l64_ofs, vm::opargs::StackLocalPtr, vm::opargs::StackLocalI64)
// passes additional argument to preceding opcode
DEF_OPCODE(ext_l64, vm::opargs::StackLocalI64)
// stores reference to local object of any type T in pointer<T>
DEF_OPCODE(ref_lptr_any, vm::opargs::StackLocalPtr, vm::opargs::StackLocalAny)
// does a shallow pointer copy
DEF_OPCODE(mov_lptr_lptr, vm::opargs::StackLocalPtr, vm::opargs::StackLocalPtr)

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
