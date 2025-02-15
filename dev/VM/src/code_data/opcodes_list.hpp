/**
 * @file opcodes_list.hpp
 * @brief Contains a list of all DuckBC opcodes. Can be used for generating
 * repetitive code based on list of opcodes.
 *
 * Suppose you want to automatically generate a whole VM
 * switch-case loop. Instead of listing all cases by hand and worrying
 * about their ordering, like this:
 * ```cpp
 * switch (opcode) {
 *   OP_CASE(foo)
 *   OP_CASE(bar)
 *   ...
 * }
 * ```
 * you can just define macros #DEF_OPCODE and include this
 * header like so:
 * ```cpp
 * switch (opcode) {
 *   #define DEF_OPCODE(opcode)     OP_CASE(opcode)
 *   #include <code_data/opcodes_list.hpp>
 *   #undef DEF_OPCODE
 * }
 * ```
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

DEF_OPCODE(mov_l64_imm)

DEF_OPCODE(mov_l64_l64)
DEF_OPCODE(cmov_l64_l64)

DEF_OPCODE(mov_l64_r0)
DEF_OPCODE(mov_r0_l64)


DEF_OPCODE(add_l64_l64)
DEF_OPCODE(add_l64_imm)

// DEF_OPCODE(sub_l64_l64)
DEF_OPCODE(sub_l64_l64)
DEF_OPCODE(sub_l64_imm)

// DEF_OPCODE(mul_l64_l64)
DEF_OPCODE(mul_l64_imm)

DEF_OPCODE(mod_l64_l64)
DEF_OPCODE(mod_l64_imm)

// DEF_OPCODE(div_l64_l64)
DEF_OPCODE(div_l64_imm)

DEF_OPCODE(cmpEq_l64_l64)
DEF_OPCODE(cmpEq_l64_imm)
DEF_OPCODE(cmpG_l64_l64)
DEF_OPCODE(cmpG_l64_imm)

DEF_OPCODE(jmpRel_label)
DEF_OPCODE(jmpRelIf_label)
DEF_OPCODE(jmpRelNotIf_label)

DEF_OPCODE(getFstArg_l64)
DEF_OPCODE(getFstArg_lptr)

DEF_OPCODE(mov_l64_arg64)
DEF_OPCODE(mov_lptr_argptr)

DEF_OPCODE(setFstArg_l64)
DEF_OPCODE(setFstArg_lptr)

DEF_OPCODE(mov_arg64_l64)
DEF_OPCODE(mov_argptr_lptr)


DEF_OPCODE(call_func)

// return while performing a tail call
DEF_OPCODE(ret_tailcall)
// return 64-bit primitive value
DEF_OPCODE(ret_l64)
// return immediate value
DEF_OPCODE(ret_imm)

// initialize local variable on local stack with given type
DEF_OPCODE(init_type)
// pop variable from local stack
DEF_OPCODE(deinit)

DEF_OPCODE(input_l64)
DEF_OPCODE(output_l64)

DEF_OPCODE(nop)

// allocates given type, stores pointer
DEF_OPCODE(alloc_lptr_type)
// frees block under pointer
DEF_OPCODE(free_lptr)
// load 64-bit primitive value from `lptr + ofs`
// expects `ext_l64` to be the next instruction
DEF_OPCODE(load_l64_lptr_ofs)
// stores 64-bit primitive value under `lptr + ofs`
// expects `ext_l64` to be the next instruction
DEF_OPCODE(store_lptr_l64_ofs)
// passes additional argument to preceding opcode
DEF_OPCODE(ext_l64)

// terminates execution
DEF_OPCODE(exit)

DEF_OPCODE(breakpoint)

#ifdef HANDLE_OPCODE
#undef DEFAULT_HANDLE_OPCODE
#undef HANDLE_OPCODE
#endif

#ifdef HANDLE_OPCODE_0ARGS
#undef DEFAULT_HANDLE_OPCODE_0ARGS
#undef HANDLE_OPCODE_0ARGS
#endif

#ifdef HANDLE_OPCODE_1ARGS
#undef DEFAULT_HANDLE_OPCODE_1ARGS
#undef HANDLE_OPCODE_1ARGS
#endif

#ifdef HANDLE_OPCODE_2ARGS
#undef DEFAULT_HANDLE_OPCODE_2ARGS
#undef HANDLE_OPCODE_2ARGS
#endif

#ifdef DEF_OPCODE
#undef DEFAULT_DEF_OPCODE
#undef DEF_OPCODE
#endif
