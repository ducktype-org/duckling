/**
 * @file opcode_definitions.hpp
 * @brief Contains definitions of all DuckBC opcodes. Can be used for generating
 * repetitive code based on list of opcodes.
 *
 * you can just define `HANDLE_OPCODE` macro and include this
 * header like so:
 * ```cpp
 *  constexpr u16 countOpCases() {
 *  	u16 count = 0;
 *		#define HANDLE_OPCODE(opcode) count++;
 * 		#include "opcode_definitions.hpp"
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


// ========= MOV OPERATIONS ========

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


DEF_OPCODE(mov_g64_g64, vm::opargs::GlobalI64, vm::opargs::GlobalI64)
DEF_OPCODE(mov_g64_l64, vm::opargs::GlobalI64, vm::opargs::StackLocalI64)
DEF_OPCODE(mov_g64_imm, vm::opargs::GlobalI64, vm::opargs::Immediate)
DEF_OPCODE(mov_g32_g32, vm::opargs::GlobalI32, vm::opargs::GlobalI32)
DEF_OPCODE(mov_g32_l32, vm::opargs::GlobalI32, vm::opargs::StackLocalI32)
DEF_OPCODE(mov_g32_imm, vm::opargs::GlobalI32, vm::opargs::Immediate)
DEF_OPCODE(mov_g16_g16, vm::opargs::GlobalI16, vm::opargs::GlobalI16)
DEF_OPCODE(mov_g16_l16, vm::opargs::GlobalI16, vm::opargs::StackLocalI16)
DEF_OPCODE(mov_g16_imm, vm::opargs::GlobalI16, vm::opargs::Immediate)
DEF_OPCODE(mov_g8_g8, vm::opargs::GlobalI8, vm::opargs::GlobalI8)
DEF_OPCODE(mov_g8_l8, vm::opargs::GlobalI8, vm::opargs::StackLocalI8)
DEF_OPCODE(mov_g8_imm, vm::opargs::GlobalI8, vm::opargs::Immediate)
DEF_OPCODE(mov_gptr_lptr, vm::opargs::GlobalPtr, vm::opargs::StackLocalPtr)
DEF_OPCODE(mov_l64_g64, vm::opargs::StackLocalI64, vm::opargs::GlobalI64)
DEF_OPCODE(mov_l32_g32, vm::opargs::StackLocalI32, vm::opargs::GlobalI32)
DEF_OPCODE(mov_l16_g16, vm::opargs::StackLocalI16, vm::opargs::GlobalI16)
DEF_OPCODE(mov_l8_g8, vm::opargs::StackLocalI8, vm::opargs::GlobalI8)
DEF_OPCODE(mov_lptr_gptr, vm::opargs::StackLocalPtr, vm::opargs::GlobalPtr)

// does a shallow pointer copy
DEF_OPCODE(mov_lptr_lptr, vm::opargs::StackLocalPtr, vm::opargs::StackLocalPtr)


// ========= ARITHMETIC OPERATIONS ========

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

// ========= LOGICAL OPERATIONS ========

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

// sets the flag if pointer is null
DEF_OPCODE(cmpNull_lptr, vm::opargs::StackLocalPtr)

// ========= VARIANT OPERATIONS ========


// Sets `variant`'s inner type to `inner_type`. It also invalidates pointers to it's data.
DEF_OPCODE(
	variantSetInner_lvnt_type,
	vm::opargs::StackLocalVnt /* variant */,
	vm::opargs::Type /* 			 inner_type */
)
/**
 * @brief Sets `destination` to point at `variant`'s data. Expects `variant` to has `expected_type`
 * set, and if it's not, `destination` becomes nullptr.
 * @note `ext_type` required to know which type is to be expected.
 */
DEF_OPCODE(
	variantGetInner_lptr_lvnt,
	vm::opargs::StackLocalPtr /* 	 destination */,
	vm::opargs::StackLocalVnt /* variant,
    vm::opargs::Type 				 expected_type*/
)

/**
 * @brief Sets inner type of variant under `variant_ptr` to `inner_type`. It also invalidates
 * pointers to it's data.
 */
DEF_OPCODE(
	variantSetInner_lptr_type,
	vm::opargs::StackLocalPtr /* variant_ptr */,
	vm::opargs::Type /* 		 inner_type */
)

/**
 * @brief Sets `destination` to point at data of variant under `variant_ptr`. Expects the variant to
 * have `expected_type` set, and if it's not, `destination` becomes nullptr.
 * @note `ext_type` required to know which type is to be expected.
 */
DEF_OPCODE(
	variantGetInner_lptr_lptr,
	vm::opargs::StackLocalPtr /* destination */,
	vm::opargs::StackLocalPtr /* variant_ptr,
    vm::opargs::Type 			 expected_type */
)

// ========= LABELS AND JUMPS ========

DEF_OPCODE(label, vm::opargs::Label)

DEF_OPCODE(jmp_label, vm::opargs::Label)
DEF_OPCODE(jmpIf_label, vm::opargs::Label)
DEF_OPCODE(jmpIfNot_label, vm::opargs::Label)

// ========= FUNCTION OPERATIONS ========

DEF_OPCODE(call_func, vm::opargs::FunctionName)
DEF_OPCODE(call_builtin_func, vm::opargs::BuiltinFunctionName)

// return while performing a tail call
DEF_OPCODE(ret_tailcall_func, vm::opargs::FunctionName)
// return
DEF_OPCODE(ret)

// ========= STACK OPERATIONS ========

// initialize local variable on local stack with given type
DEF_OPCODE(init_lany_type, vm::opargs::StackLocalAny, vm::opargs::Type)
// pop variable from local stack
DEF_OPCODE(deinit)

// ========= IO OPERATIONS ========

DEF_OPCODE(input_l64, vm::opargs::StackLocalI64)
DEF_OPCODE(output_l64, vm::opargs::StackLocalI64)

DEF_OPCODE(input_l32, vm::opargs::StackLocalI32)
DEF_OPCODE(output_l32, vm::opargs::StackLocalI32)


// ========= CLASS OPERATIONS ========

// initialises vtable pointer
DEF_OPCODE(setVTable_lptr_type, vm::opargs::StackLocalPtr, vm::opargs::Type)
// casts pointed object to its superclass
DEF_OPCODE(upcast_lptr_lptr, vm::opargs::StackLocalPtr, vm::opargs::StackLocalPtr)
// tries to cast pointed object to its subclass, requires that ext_64 is next
DEF_OPCODE(downcast_lptr_lptr, vm::opargs::StackLocalPtr, vm::opargs::StackLocalPtr)
// calls a method of specified name on an a pointer of specified class. Performs the dynamic dispatch.
DEF_OPCODE(virtual_call_lptr_func, vm::opargs::StackLocalPtr, vm::opargs::FunctionName)

// ========= GENERAL POINTER OPERATIONS ========

// allocates given type, stores pointer
DEF_OPCODE(alloc_lptr_type, vm::opargs::StackLocalPtr, vm::opargs::Type)
// frees block under pointer
DEF_OPCODE(free_lptr, vm::opargs::StackLocalPtr)

// /**
//  * @brief Allocates new dynamic table and stores pointer to it under `destination`.
//  * `element_type` is type of each element in the array, not the dynamic table itself.
//  * @note `ext_l64` is required to tell the count of elements
//  */
// DEF_OPCODE(
// 	dynTableAlloc_lptr_type,
// 	vm::opargs::StackLocalPtr /* destination */,
// 	vm::opargs::Type /* 		 element_type,
//     vm::opargs::StackLocalI64 	 element_count*/
// )

// /**
//  * @brief Re-allocates dynamic table from under `source` by changing its element count to
//  `element_count`.
//  * `element_type` is type of each element in the array, not the dynamic table itself.
//  * @note It's counter-intuitive, but this instruction does not modify pointer data. (unline in C)
//  * @note `ext_l64` is required to tell the count of elements
//  */
// DEF_OPCODE(
// 	dynTableReAlloc_lptr_type,
// 	vm::opargs::StackLocalPtr /* source */,
// 	vm::opargs::Type /* 		 element_type,
//     vm::opargs::StackLocalI64 	 element_count*/
// )


// stores local data at pointer
DEF_OPCODE(store_lptr_lany, vm::opargs::StackLocalPtr, vm::opargs::StackLocalAny)
// dereferences pointer and stores into local
DEF_OPCODE(load_lany_lptr, vm::opargs::StackLocalAny, vm::opargs::StackLocalPtr)
// loads effective address of struct field

// stores reference to local object of any type T in pointer<T>
DEF_OPCODE(ref_lptr_lany, vm::opargs::StackLocalPtr, vm::opargs::StackLocalAny)

// ========= STRUCTURE OPERATIONS ========

// expects `ext_field` to be the next instruction
DEF_OPCODE(
	structLea_lptr_lptr,
	vm::opargs::StackLocalPtr /* destination */,
	vm::opargs::StackLocalPtr /* source,
    vm::opargs::Field 			 field */
)
// expects `ext_field` to be the next instruction
DEF_OPCODE(
	structLoad_lany_lptr,
	vm::opargs::StackLocalAny /* destination */,
	vm::opargs::StackLocalPtr /* data_ptr,
    vm::opargs::Field 			 field */
)
// expects `ext_field` to be the next instruction
DEF_OPCODE(
	structStore_lptr_lany,
	vm::opargs::StackLocalPtr /* data_ptr */,
	vm::opargs::StackLocalAny /* source ,
    vm::opargs::Field 			 field */
)

// ========= TABLE OPERATIONS ========

// expects `ext_l64` to be the next instruction
DEF_OPCODE(
	staticTableLea_lptr_lptr,
	vm::opargs::StackLocalPtr /* destination */,
	vm::opargs::StackLocalPtr /* table_ptr,
    vm::opargs::StackLocalI64 	 index */
)
// expects `ext_l64` to be the next instruction
DEF_OPCODE(
	staticTableLoad_lany_lptr,
	vm::opargs::StackLocalAny /* destination */,
	vm::opargs::StackLocalPtr /* table_ptr,
    vm::opargs::StackLocalI64 	 index */
)

// expects `ext_l64` to be the next instruction
DEF_OPCODE(
	staticTableStore_lptr_lany,
	vm::opargs::StackLocalPtr /* table_ptr */,
	vm::opargs::StackLocalAny /* source,
    vm::opargs::StackLocalI64 	 index */
)

// expects `ext_l64` to be the next instruction
// DEF_OPCODE(pointerTableLea_lptr_lptr, vm::opargs::StackLocalPtr, vm::opargs::StackLocalPtr)
// expects `ext_type_l64` to be the next instruction
// DEF_OPCODE(pointerTableLoad_lany_lptr, vm::opargs::StackLocalAny, vm::opargs::StackLocalPtr)
// expects `ext_type_l64` to be the next instruction
// DEF_OPCODE(pointerTableStore_lptr_lany, vm::opargs::StackLocalPtr, vm::opargs::StackLocalAny)

// ========= EXT DEFINITIONS ========

// passes additional argument to preceding opcode
DEF_OPCODE(ext_l64, vm::opargs::StackLocalI64)
DEF_OPCODE(ext_type, vm::opargs::Type)
DEF_OPCODE(ext_field, vm::opargs::Field)
DEF_OPCODE(ext_type_field, vm::opargs::Type, vm::opargs::Field)
DEF_OPCODE(ext_type_l64, vm::opargs::Type, vm::opargs::StackLocalI64)

// ========= MISC ========


DEF_OPCODE(nop)

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
