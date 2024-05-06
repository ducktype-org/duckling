/**
 * @file opcodes_list.hpp
 * @brief Contains a list of all RiftBC opcodes. Can be used for generating
 * repetetive code based on list of opcodes, #DEF_OPCODE and
 * #DEF_OPCODE_END macros.
 *
 * Use #DEF_OPCODE_END if the opcode is supposed to terminate the executor.
 *
 * Suppose you want to automatically generate a whole VM
 * switch-case loop. Instead of listing all cases by hand and worrying
 * about their ordering, like this:
 * ```cpp
 * switch (opcode) {
 *   OP_CASE(foo)
 *   OP_CASE(bar)
 *   ...
 *   OP_CASE_END(baz)
 * }
 * ```
 * you can just define macros #DEF_OPCODE, #DEF_OPCODE_END and include this
 * header like so:
 * ```cpp
 * switch (opcode) {
 *   #define DEF_OPCODE(opcode)     OP_CASE(opcode)
 *   #define DEF_OPCODE_END(opcode) OP_CASE_END(opcode)
 *   #include <code_data/opcodes_list.hpp>
 *   #undef DEF_OPCODE
 *   #undef DEF_OPCODE_END
 * }
 * ```
 */

#ifndef DEF_OPCODE
#define DEFAULT_DEF_OPCODE
#define DEF_OPCODE(opcode)
#endif

#ifndef DEF_OPCODE_END
#define DEFAULT_DEF_OPCODE_END
#define DEF_OPCODE_END(opcode) DEF_OPCODE(opcode)
#endif

DEF_OPCODE(mov_l64_imm)

DEF_OPCODE(mov_l64_l64)
DEF_OPCODE(cmov_l64_l64)

DEF_OPCODE(mov_l64_r0)
DEF_OPCODE(mov_r0_l64)

DEF_OPCODE(mov_l64_pFuncArg)     // moves primitive function arg to local variable
DEF_OPCODE(mov_lptr_ptrFuncArg)  // moves pointer function arg to local variable

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

DEF_OPCODE(setPArg_l64)     // set primitive argument
DEF_OPCODE(setPtrArg_lptr)  // set pointer argument
DEF_OPCODE(call_func)

DEF_OPCODE(ret_tailcall)
DEF_OPCODE(ret_l64)
DEF_OPCODE(ret_imm)

DEF_OPCODE(init_type)  // initialize local variable on local stack with given type
DEF_OPCODE(deinit)     // pops variable from local stack

DEF_OPCODE(input_l64)
DEF_OPCODE(output_l64)

DEF_OPCODE(nop)

DEF_OPCODE(alloc_lptr_type)     // allocates given type, stores pointer
DEF_OPCODE(free_lptr)           // frees block under pointer
DEF_OPCODE(load_l64_lptr_ofs)   // load 64-bit primitive value from lptr + ofs
DEF_OPCODE(store_lptr_l64_ofs)  // stores 64-bit primitive value under lptr + ofs

DEF_OPCODE(ext_l64)             // passes additional argument to preceding opcode
DEF_OPCODE_END(exit)

DEF_OPCODE(handle_strategy)

#ifdef DEFAULT_DEF_OPCODE
#undef DEFAULT_DEF_OPCODE
#undef DEF_OPCODE
#endif

#ifdef DEFAULT_DEF_OPCODE_END
#undef DEFAULT_DEF_OPCODE_END
#undef DEF_OPCODE_END
#endif
