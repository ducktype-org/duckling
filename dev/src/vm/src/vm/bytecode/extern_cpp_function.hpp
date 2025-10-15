#pragma once

#include <base/box.hpp>
#include <base/macros/for_each.hpp>

#include <utility>

#define VM_EXT_CPP_INTO_VM_TYPE_NAME(Type, VmType, Name) VM_EXT_CPP_VM_TYPE_NAME(VmType),
#define VM_EXT_CPP_INTO_FIELDS(Type, VmType, Name)       Type Name;
#define VM_EXT_CPP_INTO_PARAMS(Type, VmType, Name)       , Type Name
#define VM_EXT_CPP_INTO_ARGS(Type, VmType, Name)         , func_args->Name
#define VM_EXT_CPP_PLACE_VALIDATION(Type, VmType, Name)                       \
	if constexpr (!std::is_same_v<void, Type>) {                              \
		auto tp_##Name = vm::api::getType(pid, VmType);                       \
		if (!tp_##Name.has_value()) throw vm::ExtCppVmTypeNotExists(#VmType); \
		if (!tp_##Name->type->isTriviallyCopyable())                          \
			throw vm::ExtCppVmTypeNotTriviallyCopyable(#VmType);              \
		if (tp_##Name->type->getSize() != sizeof(Type))                       \
			throw vm::ExtCppArgumentSizeMismatch(                             \
				#Type, sizeof(Type), #VmType, tp_##Name->type->getSize()      \
			);                                                                \
		vm_arg_type_size_sum += tp_##Name->type->getSize();                   \
	}

#define VM_EXT_CPP_PUT2(arg1, arg2) arg1 arg2

#define VM_EXT_CPP_VM_TYPE_NAME(Type) \
	[&]() -> vm::code::Identifier { return { base::StrID(Type) }; }()

#define DEF_VM_EXT_CPP_FUNC(ResTp, ResVmType, FuncName, ...)                                                \
	struct FuncName {                                                                                       \
		using Result = ResTp;                                                                               \
		struct FunctionData {                                                                               \
			FOR_EACH_ARG(VM_EXT_CPP_PUT2, VM_EXT_CPP_INTO_FIELDS, __VA_ARGS__)                              \
		} __attribute__((packed));                                                                          \
		static_assert(                                                                                      \
			sizeof(FunctionData) != 0, "Cannot create extern functions without arguments"                   \
		);                                                                                                  \
		static ResTp          call([[maybe_unused]] u64 _                                                   \
		                               FOR_EACH_ARG(VM_EXT_CPP_PUT2, VM_EXT_CPP_INTO_PARAMS, __VA_ARGS__)); \
		constexpr static void wrapper(std::byte* storage, std::byte* data) {                                \
			[[maybe_unused]] auto func_args = reinterpret_cast<FunctionData*>(data);                        \
			if constexpr (std::is_same_v<ResTp, void>)                                                      \
				FuncName::call(                                                                             \
					0ULL FOR_EACH_ARG(VM_EXT_CPP_PUT2, VM_EXT_CPP_INTO_ARGS, __VA_ARGS__)                   \
				);                                                                                          \
			else                                                                                            \
				vm::safeWriteBytes(                                                                         \
					storage,                                                                                \
					FuncName::call(                                                                         \
						0ULL FOR_EACH_ARG(VM_EXT_CPP_PUT2, VM_EXT_CPP_INTO_ARGS, __VA_ARGS__)               \
					)                                                                                       \
				);                                                                                          \
		}                                                                                                   \
		static vm::code::FuncSignature getSignature(vm::PID pid) {                                          \
			usize vm_arg_type_size_sum = 0;                                                                 \
			VM_EXT_CPP_PLACE_VALIDATION(ResTp, ResVmType, result)                                           \
			FOR_EACH_ARG(VM_EXT_CPP_PUT2, VM_EXT_CPP_PLACE_VALIDATION, __VA_ARGS__);                        \
			vm::code::FuncSignature signature;                                                              \
			signature.result_type = VM_EXT_CPP_VM_TYPE_NAME(ResVmType);                                     \
			signature.parameters                                                                            \
				= { FOR_EACH_ARG(VM_EXT_CPP_PUT2, VM_EXT_CPP_INTO_VM_TYPE_NAME, __VA_ARGS__) };             \
			CORE_ASSERT(                                                                                    \
				vm_arg_type_size_sum == sizeof(FunctionData),                                               \
				"FunctionData\'s fields alignment does not match stack structure in the VM: ",              \
				vm_arg_type_size_sum,                                                                       \
				"!=",                                                                                       \
				sizeof(FunctionData)                                                                        \
			);                                                                                              \
			return signature;                                                                               \
		}                                                                                                   \
	};                                                                                                      \
	ResTp FuncName::call(                                                                                   \
		[[maybe_unused]] u64 _ FOR_EACH_ARG(VM_EXT_CPP_PUT2, VM_EXT_CPP_INTO_PARAMS, __VA_ARGS__)           \
	)

#define VM_INSTANCE_EXT_CPP_FUNC(Name, Pid)                                                   \
	vm::code::CppFunction {                                                                   \
		.name = vm::code::Identifier(base::StrID(base::strSplit(#Name, "::").back().data())), \
		.function_pointer = Name::wrapper, .signature = Name::getSignature(Pid)               \
	}

namespace vm {
	class ExtCppFuncError: public base::LogicError {
	public:
		ExtCppFuncError(std::string reason): base::LogicError(std::move(reason)) {}
	};

	class ExtCppArgumentSizeMismatch: public ExtCppFuncError {
	public:
		constexpr static std::string_view ERR_MSG = "C++ type and VM type have different sizes: ";

		ExtCppArgumentSizeMismatch(
			std::string cpp_type,
			const usize cpp_type_size,
			std::string vm_type,
			const usize vm_type_size
		):
			  ExtCppFuncError(
				  base::strConcat(
					  ERR_MSG,
					  cpp_type,
					  "(",
					  cpp_type_size,
					  ") vs. ",
					  vm_type,
					  "(",
					  vm_type_size,
					  ")"
				  )
			  ) {}
	};

	class ExtCppVmTypeNotExists: public ExtCppFuncError {
	public:
		constexpr static std::string_view ERR_MSG = "Given VM type does not exist: ";

		ExtCppVmTypeNotExists(std::string vm_type):
			  ExtCppFuncError(base::strConcat(ERR_MSG, vm_type)) {}
	};

	/**
	 * @note A type may be trivially copyable if its bits can be just copied and they
	 * value remains correct.
	 */
	class ExtCppVmTypeNotTriviallyCopyable: public ExtCppFuncError {
	public:
		constexpr static std::string_view ERR_MSG = "Given VM type is not trivially copyable: ";

		ExtCppVmTypeNotTriviallyCopyable(std::string vm_type):
			  ExtCppFuncError(base::strConcat(ERR_MSG, vm_type)) {}
	};
}
