/**
 * @file extern_c_function.hpp
 * @author Mateusz Kołpa
 *
 * Extern C functions from VM perspective are C/C++ function pointers with VM signatures. Anybody
 * using a VM as a library can write their own Extern C Function, pass it to the VM and call it from
 * their bytecode.
 *
 * This is very useful in case of compiler's CTV evaluations. In some cases we need the ability to
 * call the compiler. Now it's possible.
 *
 * For convenience, there were very handy macros created. In order to define a C extern function and
 * pass it to the VM we would write:
 *
 * ```cpp
 * namespace simple {
 * 	DEF_VM_EXT_C_FUNC(i64, "i64", add, (i64, "i64", a), (i64, "i64", b)) { return a + b; }
 * }
 * ```
 *
 * To define a function we need to tell the macro a few things:
 * * C/C++ return type
 * * VM type corresponding to the returned value
 * * name of the function
 * * triplets, that correspond to arguments - (C/C++ type, corresponding VM type, name)
 *
 * All the types have to be trivially copyable, but we can pass the opaque ptr type[^1].
 *
 * Then we just write the function body. Under the hood there is a lot of boilerplate code
 * generated, including the translation of the interfaces, type check functions, etc. This allows us
 * to add the function to the vm like so:
 *
 * ```cpp
 * vm::api::loadCode(
 *     pid,
 *     { .functions            = {},
 *       .types                = {},
 *       .global_data          = {},
 *       .external_c_functions = { VM_INSTANCE_EXT_C_FUNC(simple::add, pid) }
 *     }
 * );
 * ```
 *
 * Note, that we have defined a function in a namespace just for the showcase.
 *
 * Now we can use it in the bytecode - note omitted namespace:
 *
 * ```
 * ...
 * init_pany_type res, i64;
 * init_pany_type a,   i64;
 * init_pany_type b,   i64;
 * input_p64 a;
 * input_p64 b;
 * call_cfunc add;
 * output_p64 res;
 * ...
 * ```
 *
 * Further more, these functions are really cheap to call, because we are literally passing VM's
 * stack pointers to the called function.
 *
 * [^1]: Opaque ptr type - `opaque_ptr` - is an opaque builtin, meaning VM can store its value but
 * cannot modify it and its size is 8 bytes. This is useful if we want to store a pointer in the VM
 * of an outside object for later use. We can write a function in C++ that allocates a new object of
 * any type and passes the pointer to the VM. Then, we can write functions that take that pointer
 * and operate on the object behind the pointer and return values to the VM based on that object.
 * This is showcased in the test - `cppVectorInVm`.
 */
#pragma once

#include <base/pointers/box.hpp>
#include <base/preproc/for_each.hpp>

#include <vm/api/vm.hpp>
#include <vm/core/safe/type_metadata/type.hpp>
#include <vm/utils/interpret.hpp>

namespace vm::detail {
	// Helper trait to safely get size of types
	// @TODO: #656 Change this when we figure out how to handle C voids in the VM
	template<typename T>
	struct safe_sizeof {
		static constexpr usize VALUE = sizeof(T);
	};

	template<>
	struct safe_sizeof<void> {
		static constexpr usize VALUE = 1;
	};

}

#define VM_EXT_C_INTO_VM_TYPE_NAME(Type, VmType, Name) VM_EXT_C_VM_TYPE_NAME(VmType),
#define VM_EXT_C_INTO_FIELDS(Type, VmType, Name)       Type Name;
#define VM_EXT_C_INTO_PARAMS(Type, VmType, Name)       , Type Name
#define VM_EXT_C_INTO_ARGS(Type, VmType, Name)         , func_args->Name

#define VM_EXT_C_PLACE_VALIDATION(Type, VmType, Name)                               \
	auto tp_##Name = vm::api::getType(pid, VmType);                                 \
	if (!tp_##Name.has_value()) throw vm::ExtCVmTypeNotExists(#VmType);             \
	if (tp_##Name->type->getSize().asInt() != vm::detail::safe_sizeof<Type>::VALUE) \
		throw vm::ExtCArgumentSizeMismatch(                                         \
			#Type,                                                                  \
			vm::detail::safe_sizeof<Type>::VALUE,                                   \
			#VmType,                                                                \
			tp_##Name->type->getSize().asInt()                                      \
		);                                                                          \
	vm_arg_type_size_sum += tp_##Name->type->getSize();

#define VM_EXT_C_PUT2(arg1, arg2) arg1 arg2

#define VM_EXT_C_VM_TYPE_NAME(Type) \
	[&]() -> vm::code::Identifier { return { base::StrID(Type) }; }()


/**
 * @brief Creates a wrapper for a C++ function, making it callable from the VM.
 * @details This function is only relevant when using the VM as a library. It creates a
 * wrapper for a C++ function that allows it to be called from the VM when
 * passed by a function pointer.
 *
 * @param ResCType The C++ result type of the function (can be void).
 * @param ResVMType The corresponding VM type for the result, passed as a string.
 * @param FuncName The identifier that will be used in C++ to identify the new external function.
 * @param ... A variable-length list of arguments, where each argument is a parenthesized triplet:
 *            (C++ arg type, VM arg type as a string, C++ arg name).
 *
 * Example:
 *  EF_VM_EXT_C_FUNC(i64, "i64", add, (i64, "i64", a), (i64, "i64", b)) { return a + b; }
 */
#define DEF_VM_EXT_C_FUNC(ResCType, ResVmType, FuncName, ...)                                           \
	struct FuncName {                                                                                   \
		struct FunctionData {                                                                           \
			FOR_EACH_ARG(VM_EXT_C_PUT2, VM_EXT_C_INTO_FIELDS, __VA_ARGS__)                              \
		} __attribute__((packed));                                                                      \
		static_assert(                                                                                  \
			sizeof(FunctionData) != 0, "Cannot create extern functions without arguments"               \
		);                                                                                              \
		static ResCType       call([[maybe_unused]] u64 _                                               \
		                               FOR_EACH_ARG(VM_EXT_C_PUT2, VM_EXT_C_INTO_PARAMS, __VA_ARGS__)); \
		constexpr static void wrapper(byte* storage, byte* data) {                                      \
			/* A templated helper, that calls the function and type checks correctly */                 \
			[&](auto f) {                                                                               \
				if constexpr (std::is_void_v<ResCType>) {                                               \
					f();                                                                                \
				} else {                                                                                \
					/*                                                                                  \
					 * Because f() is independent from FuncName::call, this branch is not               \
					 * semantically checked if ResCType is void, preventing the "passing void to        \
					 * function" error.                                                                 \
					 */                                                                                 \
					vm::safeWriteBytes(storage, f());                                                   \
				}                                                                                       \
			}([&] {                                                                                     \
				[[maybe_unused]] auto func_args = reinterpret_cast<FunctionData*>(data);                \
				return FuncName::call(                                                                  \
					0ULL FOR_EACH_ARG(VM_EXT_C_PUT2, VM_EXT_C_INTO_ARGS, __VA_ARGS__)                   \
				);                                                                                      \
			});                                                                                         \
		}                                                                                               \
		static vm::code::FuncSignature getSignature(vm::PID pid) {                                      \
			Bytes vm_arg_type_size_sum(0);                                                              \
			if (base::StrID(VM_EXT_C_VM_TYPE_NAME(ResVmType)) != "void") {                              \
				VM_EXT_C_PLACE_VALIDATION(ResCType, ResVmType, result)                                  \
				vm_arg_type_size_sum                                                                    \
					-= tp_result->type->getSize(); /* undo what we've done to the sum */                \
			}                                                                                           \
			FOR_EACH_ARG(VM_EXT_C_PUT2, VM_EXT_C_PLACE_VALIDATION, __VA_ARGS__);                        \
			vm::code::FuncSignature signature;                                                          \
			signature.result_types = {};                                                                \
			if (base::StrID(VM_EXT_C_VM_TYPE_NAME(ResVmType)) != "void")                                \
				signature.result_types.emplace_back(VM_EXT_C_VM_TYPE_NAME(ResVmType));                  \
			signature.parameters                                                                        \
				= { FOR_EACH_ARG(VM_EXT_C_PUT2, VM_EXT_C_INTO_VM_TYPE_NAME, __VA_ARGS__) };             \
			CORE_ASSERT(                                                                                \
				vm_arg_type_size_sum.asInt() == sizeof(FunctionData)                                    \
					|| (vm_arg_type_size_sum.asInt() == 0 && sizeof(FunctionData) == 1),                \
				"FunctionData\'s fields alignment does not match stack structure in the VM: ",          \
				vm_arg_type_size_sum.asInt(),                                                           \
				"!=",                                                                                   \
				sizeof(FunctionData)                                                                    \
			);                                                                                          \
			return signature;                                                                           \
		}                                                                                               \
	};                                                                                                  \
	ResCType FuncName::call(                                                                            \
		[[maybe_unused]] u64 _ FOR_EACH_ARG(VM_EXT_C_PUT2, VM_EXT_C_INTO_PARAMS, __VA_ARGS__)           \
	)


/**
 * @brief Helper macro to create a vm::code::ExternalCFunction instance for registering a C/C++
 *        function with the VM.
 *
 * This macro packs the necessary metadata for an extern C function (its VM-visible name,
 * the function pointer the VM should call, and the function signature) into a
 * vm::code::ExternalCFunction structure so it can be supplied to the VM loader APIs.
 *
 * Usage:
 * - VmName:    A C++ identifier (optionally namespaced) whose final component is used as the
 *              function name that will be visible inside the VM. The macro stringifies VmName
 *              and extracts the last segment after '::' to form the VM identifier.
 * - CFunc:     The C++ wrapper type generated by DEF_VM_EXT_C_FUNC. The macro reads
 *              CFunc::wrapper for the function pointer and CFunc::getSignature(Pid) for the
 *              VM signature.
 * - Pid:       The vm::PID value used when computing the VM-visible signature (passed to
 *              CFunc::getSignature).
 *
 * Example:
 *   // Given a function `simple::add` generated by DEF_VM_EXT_C_FUNC
 *   vm::api::loadCode(pid, {
 *       .functions            = {},
 *       .types                = {},
 *       .global_data          = {},
 *       .external_c_functions = {
 *           VM_INSTANCE_EXT_C_FUNC(add, simple::add, pid)
 *       }
 *   });
 *
 * See also:
 * - DEF_VM_EXT_C_FUNC: generates the wrapper type and getSignature implementation used here.
 */
#define VM_INSTANCE_EXT_C_FUNC(VmName, CFunc, Pid)                                              \
	vm::code::ExternalCFunction {                                                               \
		.name = vm::code::Identifier(base::StrID(#VmName)), .function_pointer = CFunc::wrapper, \
		.signature = CFunc::getSignature(Pid)                                                   \
	}

namespace vm {
	class ExtCFuncError: public base::LogicError {
	public:
		ExtCFuncError(const std::string& reason): base::LogicError(std::move(reason)) {}
	};

	class ExtCArgumentSizeMismatch: public ExtCFuncError {
	public:
		constexpr static std::string_view ERR_MSG = "C++ type and VM type have different sizes: ";

		ExtCArgumentSizeMismatch(
			const std::string& cpp_type,
			const usize        cpp_type_size,
			const std::string& vm_type,
			const usize        vm_type_size
		):
			  ExtCFuncError(base::strConcat(
				  ERR_MSG, cpp_type, "(", cpp_type_size, ") vs. ", vm_type, "(", vm_type_size, ")"
			  )) {}
	};

	class ExtCVmTypeNotExists: public ExtCFuncError {
	public:
		constexpr static std::string_view ERR_MSG = "Given VM type does not exist: ";

		ExtCVmTypeNotExists(const std::string& vm_type):
			  ExtCFuncError(base::strConcat(ERR_MSG, vm_type)) {}
	};
}
