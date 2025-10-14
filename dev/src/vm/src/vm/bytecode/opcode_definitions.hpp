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

DEF_OPCODE(mov_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_OPCODE(mov_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_OPCODE(cmov_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_OPCODE(cmov_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)

DEF_OPCODE(mov_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_OPCODE(mov_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_OPCODE(cmov_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_OPCODE(cmov_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)

DEF_OPCODE(mov_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_OPCODE(mov_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(cmov_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(cmov_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_OPCODE(mov_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_OPCODE(mov_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(cmov_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(cmov_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)


DEF_OPCODE(mov_g64_g64, vm::opargs::Global64, vm::opargs::Global64)
DEF_OPCODE(mov_g64_l64, vm::opargs::Global64, vm::opargs::StackLocal64)
DEF_OPCODE(mov_g64_imm, vm::opargs::Global64, vm::opargs::Immediate)
DEF_OPCODE(mov_g32_g32, vm::opargs::Global32, vm::opargs::Global32)
DEF_OPCODE(mov_g32_l32, vm::opargs::Global32, vm::opargs::StackLocal32)
DEF_OPCODE(mov_g32_imm, vm::opargs::Global32, vm::opargs::Immediate)
DEF_OPCODE(mov_g16_g16, vm::opargs::Global16, vm::opargs::Global16)
DEF_OPCODE(mov_g16_l16, vm::opargs::Global16, vm::opargs::StackLocal16)
DEF_OPCODE(mov_g16_imm, vm::opargs::Global16, vm::opargs::Immediate)
DEF_OPCODE(mov_g8_g8, vm::opargs::Global8, vm::opargs::Global8)
DEF_OPCODE(mov_g8_l8, vm::opargs::Global8, vm::opargs::StackLocal8)
DEF_OPCODE(mov_g8_imm, vm::opargs::Global8, vm::opargs::Immediate)
DEF_OPCODE(mov_gptr_lptr, vm::opargs::GlobalPtr, vm::opargs::StackLocalPtr)
DEF_OPCODE(mov_l64_g64, vm::opargs::StackLocal64, vm::opargs::Global64)
DEF_OPCODE(mov_l32_g32, vm::opargs::StackLocal32, vm::opargs::Global32)
DEF_OPCODE(mov_l16_g16, vm::opargs::StackLocal16, vm::opargs::Global16)
DEF_OPCODE(mov_l8_g8, vm::opargs::StackLocal8, vm::opargs::Global8)
DEF_OPCODE(mov_lptr_gptr, vm::opargs::StackLocalPtr, vm::opargs::GlobalPtr)

// does a shallow pointer copy
DEF_OPCODE(mov_lptr_lptr, vm::opargs::StackLocalPtr, vm::opargs::StackLocalPtr)

// Copies an opaque value
DEF_OPCODE(mov_lopq_lopq, vm::opargs::StackLocalOpq, vm::opargs::StackLocalOpq)

// sets pointer to null
DEF_OPCODE(setNull_lptr, vm::opargs::StackLocalPtr)


// ========= ARITHMETIC OPERATIONS ========

DEF_OPCODE(add_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(add_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_OPCODE(add_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(add_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)


DEF_OPCODE(sub_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(sub_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_OPCODE(sub_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(sub_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)


DEF_OPCODE(mul_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(mul_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_OPCODE(mul_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(mul_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_OPCODE(mod_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(mod_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_OPCODE(mod_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(mod_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_OPCODE(div_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(div_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_OPCODE(div_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(div_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_OPCODE(neg_l64, vm::opargs::StackLocal64)
DEF_OPCODE(neg_l32, vm::opargs::StackLocal32)

// ========= FLOATING POINT OPERATIONS ========
DEF_OPCODE(fadd_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(fadd_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_OPCODE(fadd_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(fadd_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_OPCODE(fsub_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(fsub_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_OPCODE(fsub_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(fsub_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_OPCODE(fmul_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(fmul_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_OPCODE(fmul_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(fmul_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_OPCODE(fdiv_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(fdiv_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_OPCODE(fdiv_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(fdiv_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_OPCODE(fneg_l64, vm::opargs::StackLocal64)
DEF_OPCODE(fneg_l32, vm::opargs::StackLocal32)

DEF_OPCODE(umul_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(umul_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_OPCODE(umul_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(umul_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_OPCODE(umod_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(umod_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_OPCODE(umod_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(umod_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_OPCODE(udiv_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(udiv_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_OPCODE(udiv_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(udiv_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

// ========= BOOLEAN OPERATIONS ========

// Evaluate logical operations (AND, OR, etc.) on operands as booleans (non-zero = true)
// Result is 0 or 1 stored in the first argument

DEF_OPCODE(log_and_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_OPCODE(log_and_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)

DEF_OPCODE(log_or_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_OPCODE(log_or_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)

DEF_OPCODE(log_xor_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_OPCODE(log_xor_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)

DEF_OPCODE(log_not_l8, vm::opargs::StackLocal8)

// ========= LOGICAL OPERATIONS ========

DEF_OPCODE(cmpEq_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(cmpEq_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_OPCODE(cmpG_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(cmpG_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_OPCODE(ucmpG_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(ucmpG_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_OPCODE(cmpL_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(cmpL_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_OPCODE(ucmpL_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_OPCODE(ucmpL_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_OPCODE(cmpEq_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(cmpEq_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_OPCODE(cmpG_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(cmpG_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_OPCODE(ucmpG_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(ucmpG_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_OPCODE(cmpL_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(cmpL_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_OPCODE(ucmpL_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_OPCODE(ucmpL_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_OPCODE(cmpEq_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_OPCODE(cmpEq_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_OPCODE(cmpG_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_OPCODE(cmpG_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_OPCODE(ucmpG_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_OPCODE(ucmpG_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_OPCODE(cmpL_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_OPCODE(cmpL_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_OPCODE(ucmpL_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_OPCODE(ucmpL_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)

// sets the flag if pointer is null
DEF_OPCODE(cmpNull_lptr, vm::opargs::StackLocalPtr)

// ========= VARIANT OPERATIONS ========


// Sets `variant`'s inner type to `inner_type`. It also invalidates pointers to its data.
DEF_OPCODE(
	variantSetInner_lvnt_type,
	vm::opargs::StackLocalVnt /* variant */,
	vm::opargs::Type /* 		 inner_type */
)
/**
 * @brief Sets `destination` to point at `variant`'s data. Expects `variant` to has `expected_type`
 * set, and if it's not, `destination` becomes nullptr.
 * @note `ext_type` required to know which type is to be expected. There is no other way to obtain
 * type information in the implementation.
 */
DEF_OPCODE(
	variantGetInner_lptr_lvnt,
	vm::opargs::StackLocalPtr /* destination */,
	vm::opargs::StackLocalVnt /* variant,
    vm::opargs::Type 			 expected_type */
)

/**
 * @brief Sets inner type of variant under `variant_ptr` to `inner_type`. It also invalidates
 * pointers to its data.
 */
DEF_OPCODE(
	variantSetInner_lptr_type,
	vm::opargs::StackLocalPtr /* variant_ptr */,
	vm::opargs::Type /* 		 inner_type */
)

/**
 * @brief Sets `destination` to point at data of variant under `variant_ptr`. Expects the variant to
 * have `expected_type` set, and if it's not, `destination` becomes nullptr.
 * @note `ext_type` required to know which type is to be expected. There is no other way to obtain
 * type information in the implementation.
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
DEF_OPCODE(call_builtinfunc, vm::opargs::BuiltinFunctionName)
DEF_OPCODE(call_cppfunc, vm::opargs::CppFunctionName)

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

DEF_OPCODE(input_l64, vm::opargs::StackLocal64)
DEF_OPCODE(output_l64, vm::opargs::StackLocal64)

DEF_OPCODE(input_l32, vm::opargs::StackLocal32)
DEF_OPCODE(output_l32, vm::opargs::StackLocal32)


// ========= CLASS OPERATIONS ========

// initialises vtable pointer
DEF_OPCODE(setVTable_lptr_type, vm::opargs::StackLocalPtr, vm::opargs::Type)
// casts pointed object to its superclass
DEF_OPCODE(upcast_lptr_lptr, vm::opargs::StackLocalPtr, vm::opargs::StackLocalPtr)
// tries to cast pointed object to its subclass, requires that ext_64 is next
DEF_OPCODE(downcast_lptr_lptr, vm::opargs::StackLocalPtr, vm::opargs::StackLocalPtr)
// calls a method of specified name on an a pointer. Performs the dynamic dispatch.
DEF_OPCODE(virtual_call_lptr_method, vm::opargs::StackLocalPtr, vm::opargs::MethodName)

// ========= GENERAL POINTER OPERATIONS ========

// allocates given type, stores pointer
DEF_OPCODE(alloc_lptr_type, vm::opargs::StackLocalPtr, vm::opargs::Type)
// frees block under pointer
DEF_OPCODE(free_lptr, vm::opargs::StackLocalPtr)


// stores local data at pointer
DEF_OPCODE(store_lptr_lany, vm::opargs::StackLocalPtr, vm::opargs::StackLocalAny)
// dereferences pointer and stores into local
DEF_OPCODE(load_lany_lptr, vm::opargs::StackLocalAny, vm::opargs::StackLocalPtr)

// stores reference to local object of any type T in pointer<T>
DEF_OPCODE(ref_lptr_lany, vm::opargs::StackLocalPtr, vm::opargs::StackLocalAny)

// ========= STRUCTURE OPERATIONS ========

// expects `ext_field` to be the next instruction
// loads effective address of struct field
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
	fixedSizeTableLea_lptr_lptr,
	vm::opargs::StackLocalPtr /* destination */,
	vm::opargs::StackLocalPtr /* table_ptr,
    vm::opargs::StackLocal64 	 index */
)
// expects `ext_l64` to be the next instruction
DEF_OPCODE(
	fixedSizeTableLoad_lany_lptr,
	vm::opargs::StackLocalAny /* destination */,
	vm::opargs::StackLocalPtr /* table_ptr,
    vm::opargs::StackLocal64 	 index */
)

// expects `ext_l64` to be the next instruction
DEF_OPCODE(
	fixedSizeTableStore_lptr_lany,
	vm::opargs::StackLocalPtr /* table_ptr */,
	vm::opargs::StackLocalAny /* source,
    vm::opargs::StackLocal64 	 index */
)

// expects `ext_l64` to be the next instruction
DEF_OPCODE(
	dynTableLea_lptr_lptr,
	vm::opargs::StackLocalPtr /* destination */,
	vm::opargs::StackLocalPtr /* table_ptr,
    vm::opargs::StackLocal64 	 index */
)
// expects `ext_l64` to be the next instruction
DEF_OPCODE(
	dynTableLoad_lany_lptr,
	vm::opargs::StackLocalAny /* destination */,
	vm::opargs::StackLocalPtr /* table_ptr,
    vm::opargs::StackLocal64 	 index */
)

// expects `ext_l64` to be the next instruction
DEF_OPCODE(
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
DEF_OPCODE(
	dynTableReAlloc_lptr_type,
	vm::opargs::StackLocalPtr /* table_ptr */,
	vm::opargs::Type /* table_type ,
vm::opargs::StackLocal64     new_elem_count */
)

/**
 * @brief Outputs a dynamic table of bytes as a string.
 */
DEF_OPCODE(
	strOutput_lptr, vm::opargs::StackLocalPtr /* string_ptr */
)

// ========= TYPE OPERATIONS ========

// Casts a primitive type in-place. This does nothing at runtime, but is needed
// for type checking.
DEF_OPCODE(cast_l8_type, vm::opargs::StackLocal8, vm::opargs::Type)
DEF_OPCODE(cast_l16_type, vm::opargs::StackLocal16, vm::opargs::Type)
DEF_OPCODE(cast_l32_type, vm::opargs::StackLocal32, vm::opargs::Type)
DEF_OPCODE(cast_l64_type, vm::opargs::StackLocal64, vm::opargs::Type)

// ========= EXT DEFINITIONS ========

// passes additional argument to preceding opcode
DEF_OPCODE(ext_l64, vm::opargs::StackLocal64)
DEF_OPCODE(ext_type, vm::opargs::Type)
DEF_OPCODE(ext_field, vm::opargs::Field)
DEF_OPCODE(ext_type_field, vm::opargs::Type, vm::opargs::Field)
DEF_OPCODE(ext_type_l64, vm::opargs::Type, vm::opargs::StackLocal64)

// ========= MISC ========


DEF_OPCODE(nop)

// terminates execution
DEF_OPCODE(exit)

DEF_OPCODE(breakpoint)

/**
 * @brief This is a very internal instruction, that should not be used in regular bytecode.
 * It is a helper for start functions.
 * @arg0 - pointer to a VmValue.
 * @arg1 - n/a.
 */
DEF_OPCODE(initFromVmValue)

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
