#pragma once

#include <base/pointers/box.hpp>
#include <base/preproc/for_each.hpp>

#include <utility>

#define VM_EXT_C_INTO_VM_TYPE_NAME(Type, VmType, Name) VM_EXT_C_VM_TYPE_NAME(VmType),
#define VM_EXT_C_INTO_FIELDS(Type, VmType, Name)       Type Name;
#define VM_EXT_C_INTO_PARAMS(Type, VmType, Name)       , Type Name
#define VM_EXT_C_INTO_ARGS(Type, VmType, Name)         , func_args->Name
#define VM_EXT_C_PLACE_VALIDATION(Type, VmType, Name)                   \
	auto tp_##Name = vm::api::getType(pid, VmType);                     \
	if (!tp_##Name.has_value()) throw vm::ExtCVmTypeNotExists(#VmType); \
	if (tp_##Name->type->getSize() != sizeof(Type))                     \
		throw vm::ExtCArgumentSizeMismatch(                             \
			#Type, sizeof(Type), #VmType, tp_##Name->type->getSize()    \
		);                                                              \
	vm_arg_type_size_sum += tp_##Name->type->getSize();

#define VM_EXT_C_PUT2(arg1, arg2) arg1 arg2

#define VM_EXT_C_VM_TYPE_NAME(Type) \
	[&]() -> vm::code::Identifier { return { base::StrID(Type) }; }()

#define DEF_VM_EXT_C_FUNC(ResTp, ResVmType, FuncName, ...)                                              \
	struct FuncName {                                                                                   \
		using Result = ResTp;                                                                           \
		struct FunctionData {                                                                           \
			FOR_EACH_ARG(VM_EXT_C_PUT2, VM_EXT_C_INTO_FIELDS, __VA_ARGS__)                              \
		} __attribute__((packed));                                                                      \
		static_assert(                                                                                  \
			sizeof(FunctionData) != 0, "Cannot create extern functions without arguments"               \
		);                                                                                              \
		static ResTp          call([[maybe_unused]] u64 _                                               \
		                               FOR_EACH_ARG(VM_EXT_C_PUT2, VM_EXT_C_INTO_PARAMS, __VA_ARGS__)); \
		constexpr static void wrapper(std::byte* storage, std::byte* data) {                            \
			[[maybe_unused]] auto func_args = reinterpret_cast<FunctionData*>(data);                    \
			if constexpr (std::is_same_v<ResTp, void>)                                                  \
				FuncName::call(0ULL FOR_EACH_ARG(VM_EXT_C_PUT2, VM_EXT_C_INTO_ARGS, __VA_ARGS__));      \
			else                                                                                        \
				vm::safeWriteBytes(                                                                     \
					storage,                                                                            \
					FuncName::call(                                                                     \
						0ULL FOR_EACH_ARG(VM_EXT_C_PUT2, VM_EXT_C_INTO_ARGS, __VA_ARGS__)               \
					)                                                                                   \
				);                                                                                      \
		}                                                                                               \
		static vm::code::FuncSignature getSignature(vm::PID pid) {                                      \
			usize vm_arg_type_size_sum = 0;                                                             \
			VM_EXT_C_PLACE_VALIDATION(ResTp, ResVmType, result)                                         \
			vm_arg_type_size_sum                                                                        \
				-= tp_result->type->getSize(); /* undo what we've done to the sum */                    \
			FOR_EACH_ARG(VM_EXT_C_PUT2, VM_EXT_C_PLACE_VALIDATION, __VA_ARGS__);                        \
			vm::code::FuncSignature signature;                                                          \
			signature.result_type = VM_EXT_C_VM_TYPE_NAME(ResVmType);                                   \
			signature.parameters                                                                        \
				= { FOR_EACH_ARG(VM_EXT_C_PUT2, VM_EXT_C_INTO_VM_TYPE_NAME, __VA_ARGS__) };             \
			CORE_ASSERT(                                                                                \
				vm_arg_type_size_sum == sizeof(FunctionData)                                            \
					|| (vm_arg_type_size_sum == 0 && sizeof(FunctionData) == 1),                        \
				"FunctionData\'s fields alignment does not match stack structure in the VM: ",          \
				vm_arg_type_size_sum,                                                                   \
				"!=",                                                                                   \
				sizeof(FunctionData)                                                                    \
			);                                                                                          \
			return signature;                                                                           \
		}                                                                                               \
	};                                                                                                  \
	ResTp FuncName::call(                                                                               \
		[[maybe_unused]] u64 _ FOR_EACH_ARG(VM_EXT_C_PUT2, VM_EXT_C_INTO_PARAMS, __VA_ARGS__)           \
	)

#define VM_INSTANCE_EXT_C_FUNC(Name, Pid)                                                     \
	vm::code::ExternalCFunction {                                                             \
		.name = vm::code::Identifier(base::StrID(base::strSplit(#Name, "::").back().data())), \
		.function_pointer = Name::wrapper, .signature = Name::getSignature(Pid)               \
	}

namespace vm {
	class ExtCFuncError: public base::LogicError {
	public:
		ExtCFuncError(std::string reason): base::LogicError(std::move(reason)) {}
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
