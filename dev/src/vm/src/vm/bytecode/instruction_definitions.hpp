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

#ifndef HANDLE_INSTR_0ARGS
#define DEFAULT_HANDLE_INSTR_0ARGS
#define HANDLE_INSTR_0ARGS(instr) HANDLE_INSTR(instr)
#endif

#ifndef HANDLE_INSTR_1ARGS
#define DEFAULT_HANDLE_INSTR_1ARGS
#define HANDLE_INSTR_1ARGS(instr, arg0_type) HANDLE_INSTR(instr)
#endif

#ifndef HANDLE_INSTR_2ARGS
#define DEFAULT_HANDLE_INSTR_2ARGS
#define HANDLE_INSTR_2ARGS(instr, arg0_type, arg1_type) HANDLE_INSTR(instr)
#endif

#ifndef DEF_INSTR
#define DEFAULT_DEF_INSTR
#define GET_MACRO(_instr, _1, _2, NAME, ...) NAME
#define DEF_INSTR(...)                                                                 \
	GET_MACRO(__VA_ARGS__, HANDLE_INSTR_2ARGS, HANDLE_INSTR_1ARGS, HANDLE_INSTR_0ARGS) \
	(__VA_ARGS__)
#endif


// ========= MOV OPERATIONS ========

DEF_INSTR(mov_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_INSTR(mov_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_INSTR(cmov_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_INSTR(cmov_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)

DEF_INSTR(mov_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)
DEF_INSTR(mov_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_INSTR(cmov_l16_l16, vm::opargs::StackLocal16, vm::opargs::StackLocal16)
DEF_INSTR(cmov_l16_imm, vm::opargs::StackLocal16, vm::opargs::Immediate)

DEF_INSTR(mov_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_INSTR(mov_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(cmov_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(cmov_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_INSTR(mov_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_INSTR(mov_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(cmov_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(cmov_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)


DEF_INSTR(mov_g64_g64, vm::opargs::Global64, vm::opargs::Global64)
DEF_INSTR(mov_g64_l64, vm::opargs::Global64, vm::opargs::StackLocal64)
DEF_INSTR(mov_g64_imm, vm::opargs::Global64, vm::opargs::Immediate)
DEF_INSTR(mov_g32_g32, vm::opargs::Global32, vm::opargs::Global32)
DEF_INSTR(mov_g32_l32, vm::opargs::Global32, vm::opargs::StackLocal32)
DEF_INSTR(mov_g32_imm, vm::opargs::Global32, vm::opargs::Immediate)
DEF_INSTR(mov_g16_g16, vm::opargs::Global16, vm::opargs::Global16)
DEF_INSTR(mov_g16_l16, vm::opargs::Global16, vm::opargs::StackLocal16)
DEF_INSTR(mov_g16_imm, vm::opargs::Global16, vm::opargs::Immediate)
DEF_INSTR(mov_g8_g8, vm::opargs::Global8, vm::opargs::Global8)
DEF_INSTR(mov_g8_l8, vm::opargs::Global8, vm::opargs::StackLocal8)
DEF_INSTR(mov_g8_imm, vm::opargs::Global8, vm::opargs::Immediate)
DEF_INSTR(mov_gptr_lptr, vm::opargs::GlobalPtr, vm::opargs::StackLocalPtr)
DEF_INSTR(mov_l64_g64, vm::opargs::StackLocal64, vm::opargs::Global64)
DEF_INSTR(mov_l32_g32, vm::opargs::StackLocal32, vm::opargs::Global32)
DEF_INSTR(mov_l16_g16, vm::opargs::StackLocal16, vm::opargs::Global16)
DEF_INSTR(mov_l8_g8, vm::opargs::StackLocal8, vm::opargs::Global8)
DEF_INSTR(mov_lptr_gptr, vm::opargs::StackLocalPtr, vm::opargs::GlobalPtr)

// does a shallow pointer copy
DEF_INSTR(mov_lptr_lptr, vm::opargs::StackLocalPtr, vm::opargs::StackLocalPtr)

// sets pointer to null
DEF_INSTR(setNull_lptr, vm::opargs::StackLocalPtr)


// ========= ARITHMETIC OPERATIONS ========

DEF_INSTR(add_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(add_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_INSTR(add_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(add_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)


DEF_INSTR(sub_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(sub_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_INSTR(sub_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(sub_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)


DEF_INSTR(mul_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(mul_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_INSTR(mul_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(mul_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_INSTR(mod_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(mod_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_INSTR(mod_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(mod_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_INSTR(div_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(div_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_INSTR(div_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(div_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_INSTR(neg_l64, vm::opargs::StackLocal64)
DEF_INSTR(neg_l32, vm::opargs::StackLocal32)

// ========= FLOATING POINT OPERATIONS ========
DEF_INSTR(fadd_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(fadd_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_INSTR(fadd_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(fadd_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_INSTR(fsub_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(fsub_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_INSTR(fsub_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(fsub_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_INSTR(fmul_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(fmul_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_INSTR(fmul_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(fmul_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_INSTR(fdiv_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(fdiv_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_INSTR(fdiv_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(fdiv_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_INSTR(fneg_l64, vm::opargs::StackLocal64)
DEF_INSTR(fneg_l32, vm::opargs::StackLocal32)

DEF_INSTR(umul_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(umul_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_INSTR(umul_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(umul_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_INSTR(umod_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(umod_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_INSTR(umod_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(umod_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_INSTR(udiv_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(udiv_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_INSTR(udiv_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(udiv_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

// ========= BOOLEAN OPERATIONS ========

// Evaluate logical operations (AND, OR, etc.) on operands as booleans (non-zero = true)
// Result is 0 or 1 stored in the first argument

DEF_INSTR(log_and_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_INSTR(log_and_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)

DEF_INSTR(log_or_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_INSTR(log_or_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)

DEF_INSTR(log_xor_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_INSTR(log_xor_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)

DEF_INSTR(log_not_l8, vm::opargs::StackLocal8)

// ========= LOGICAL OPERATIONS ========

DEF_INSTR(cmpEq_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(cmpEq_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_INSTR(cmpG_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(cmpG_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_INSTR(ucmpG_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(ucmpG_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_INSTR(cmpL_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(cmpL_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)
DEF_INSTR(ucmpL_l64_l64, vm::opargs::StackLocal64, vm::opargs::StackLocal64)
DEF_INSTR(ucmpL_l64_imm, vm::opargs::StackLocal64, vm::opargs::Immediate)

DEF_INSTR(cmpEq_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(cmpEq_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_INSTR(cmpG_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(cmpG_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_INSTR(ucmpG_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(ucmpG_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_INSTR(cmpL_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(cmpL_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)
DEF_INSTR(ucmpL_l32_l32, vm::opargs::StackLocal32, vm::opargs::StackLocal32)
DEF_INSTR(ucmpL_l32_imm, vm::opargs::StackLocal32, vm::opargs::Immediate)

DEF_INSTR(cmpEq_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_INSTR(cmpEq_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_INSTR(cmpG_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_INSTR(cmpG_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_INSTR(ucmpG_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_INSTR(ucmpG_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_INSTR(cmpL_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_INSTR(cmpL_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)
DEF_INSTR(ucmpL_l8_l8, vm::opargs::StackLocal8, vm::opargs::StackLocal8)
DEF_INSTR(ucmpL_l8_imm, vm::opargs::StackLocal8, vm::opargs::Immediate)

// sets the flag if pointer is null
DEF_INSTR(cmpNull_lptr, vm::opargs::StackLocalPtr)

// ========= VARIANT OPERATIONS ========


// Sets `variant`'s inner type to `inner_type`. It also invalidates pointers to its data.
DEF_INSTR(
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
DEF_INSTR(
	variantGetInner_lptr_lvnt,
	vm::opargs::StackLocalPtr /* destination */,
	vm::opargs::StackLocalVnt /* variant,
    vm::opargs::Type 			 expected_type */
)

/**
 * @brief Sets inner type of variant under `variant_ptr` to `inner_type`. It also invalidates
 * pointers to its data.
 */
DEF_INSTR(
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
DEF_INSTR(
	variantGetInner_lptr_lptr,
	vm::opargs::StackLocalPtr /* destination */,
	vm::opargs::StackLocalPtr /* variant_ptr,
    vm::opargs::Type 			 expected_type */
)

// ========= LABELS AND JUMPS ========

DEF_INSTR(label, vm::opargs::Label)

DEF_INSTR(jmp_label, vm::opargs::Label)
DEF_INSTR(jmpIf_label, vm::opargs::Label)
DEF_INSTR(jmpIfNot_label, vm::opargs::Label)

// ========= FUNCTION OPERATIONS ========

DEF_INSTR(call_func, vm::opargs::FunctionName)
DEF_INSTR(call_builtin_func, vm::opargs::BuiltinFunctionName)

// return while performing a tail call
DEF_INSTR(ret_tailcall_func, vm::opargs::FunctionName)
// return
DEF_INSTR(ret)

// ========= STACK OPERATIONS ========

// initialize local variable on local stack with given type
DEF_INSTR(init_lany_type, vm::opargs::StackLocalAny, vm::opargs::Type)
// pop variable from local stack
DEF_INSTR(deinit)

// ========= IO OPERATIONS ========

DEF_INSTR(input_l64, vm::opargs::StackLocal64)
DEF_INSTR(output_l64, vm::opargs::StackLocal64)

DEF_INSTR(input_l32, vm::opargs::StackLocal32)
DEF_INSTR(output_l32, vm::opargs::StackLocal32)


// ========= CLASS OPERATIONS ========

// initialises vtable pointer
DEF_INSTR(setVTable_lptr_type, vm::opargs::StackLocalPtr, vm::opargs::Type)
// casts pointed object to its superclass
DEF_INSTR(upcast_lptr_lptr, vm::opargs::StackLocalPtr, vm::opargs::StackLocalPtr)
// tries to cast pointed object to its subclass, requires that ext_64 is next
DEF_INSTR(downcast_lptr_lptr, vm::opargs::StackLocalPtr, vm::opargs::StackLocalPtr)
// calls a method of specified name on an a pointer. Performs the dynamic dispatch.
DEF_INSTR(virtual_call_lptr_method, vm::opargs::StackLocalPtr, vm::opargs::MethodName)

// ========= GENERAL POINTER OPERATIONS ========

// allocates given type, stores pointer
DEF_INSTR(alloc_lptr_type, vm::opargs::StackLocalPtr, vm::opargs::Type)
// frees block under pointer
DEF_INSTR(free_lptr, vm::opargs::StackLocalPtr)


// stores local data at pointer
DEF_INSTR(store_lptr_lany, vm::opargs::StackLocalPtr, vm::opargs::StackLocalAny)
// dereferences pointer and stores into local
DEF_INSTR(load_lany_lptr, vm::opargs::StackLocalAny, vm::opargs::StackLocalPtr)

// stores reference to local object of any type T in pointer<T>
DEF_INSTR(ref_lptr_lany, vm::opargs::StackLocalPtr, vm::opargs::StackLocalAny)

// ========= STRUCTURE OPERATIONS ========

// expects `ext_field` to be the next instruction
// loads effective address of struct field
DEF_INSTR(
	structLea_lptr_lptr,
	vm::opargs::StackLocalPtr /* destination */,
	vm::opargs::StackLocalPtr /* source,
    vm::opargs::Field 			 field */
)
// expects `ext_field` to be the next instruction
DEF_INSTR(
	structLoad_lany_lptr,
	vm::opargs::StackLocalAny /* destination */,
	vm::opargs::StackLocalPtr /* data_ptr,
    vm::opargs::Field 			 field */
)
// expects `ext_field` to be the next instruction
DEF_INSTR(
	structStore_lptr_lany,
	vm::opargs::StackLocalPtr /* data_ptr */,
	vm::opargs::StackLocalAny /* source ,
    vm::opargs::Field 			 field */
)

// ========= TABLE OPERATIONS ========

// expects `ext_l64` to be the next instruction
DEF_INSTR(
	fixedSizeTableLea_lptr_lptr,
	vm::opargs::StackLocalPtr /* destination */,
	vm::opargs::StackLocalPtr /* table_ptr,
    vm::opargs::StackLocal64 	 index */
)
// expects `ext_l64` to be the next instruction
DEF_INSTR(
	fixedSizeTableLoad_lany_lptr,
	vm::opargs::StackLocalAny /* destination */,
	vm::opargs::StackLocalPtr /* table_ptr,
    vm::opargs::StackLocal64 	 index */
)

// expects `ext_l64` to be the next instruction
DEF_INSTR(
	fixedSizeTableStore_lptr_lany,
	vm::opargs::StackLocalPtr /* table_ptr */,
	vm::opargs::StackLocalAny /* source,
    vm::opargs::StackLocal64 	 index */
)

// expects `ext_l64` to be the next instruction
DEF_INSTR(
	dynTableLea_lptr_lptr,
	vm::opargs::StackLocalPtr /* destination */,
	vm::opargs::StackLocalPtr /* table_ptr,
    vm::opargs::StackLocal64 	 index */
)
// expects `ext_l64` to be the next instruction
DEF_INSTR(
	dynTableLoad_lany_lptr,
	vm::opargs::StackLocalAny /* destination */,
	vm::opargs::StackLocalPtr /* table_ptr,
    vm::opargs::StackLocal64 	 index */
)

// expects `ext_l64` to be the next instruction
DEF_INSTR(
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
DEF_INSTR(
	dynTableReAlloc_lptr_type,
	vm::opargs::StackLocalPtr /* table_ptr */,
	vm::opargs::Type /* table_type ,
vm::opargs::StackLocal64     new_elem_count */
)

/**
 * @brief Outputs a dynamic table of bytes as a string.
 */
DEF_INSTR(
	strOutput_lptr, vm::opargs::StackLocalPtr /* string_ptr */
)

// ========= TYPE OPERATIONS ========

// Casts a primitive type in-place. This does nothing at runtime, but is needed
// for type checking.
DEF_INSTR(cast_l8_type, vm::opargs::StackLocal8, vm::opargs::Type)
DEF_INSTR(cast_l16_type, vm::opargs::StackLocal16, vm::opargs::Type)
DEF_INSTR(cast_l32_type, vm::opargs::StackLocal32, vm::opargs::Type)
DEF_INSTR(cast_l64_type, vm::opargs::StackLocal64, vm::opargs::Type)

// ========= EXT DEFINITIONS ========

// passes additional argument to preceding instruction
DEF_INSTR(ext_l64, vm::opargs::StackLocal64)
DEF_INSTR(ext_type, vm::opargs::Type)
DEF_INSTR(ext_field, vm::opargs::Field)
DEF_INSTR(ext_type_field, vm::opargs::Type, vm::opargs::Field)
DEF_INSTR(ext_type_l64, vm::opargs::Type, vm::opargs::StackLocal64)

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

#ifdef DEFAULT_HANDLE_INSTR_0ARGS
#undef DEFAULT_HANDLE_INSTR_0ARGS
#undef HANDLE_INSTR_0ARGS
#endif

#ifdef DEFAULT_HANDLE_INSTR_1ARGS
#undef DEFAULT_HANDLE_INSTR_1ARGS
#undef HANDLE_INSTR_1ARGS
#endif

#ifdef DEFAULT_HANDLE_INSTR_2ARGS
#undef DEFAULT_HANDLE_INSTR_2ARGS
#undef HANDLE_INSTR_2ARGS
#endif

#ifdef DEFAULT_DEF_INSTR
#undef DEFAULT_DEF_INSTR
#undef DEF_INSTR
#undef GET_MACRO
#endif
