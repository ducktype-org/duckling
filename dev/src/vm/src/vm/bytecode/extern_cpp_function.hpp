#pragma once

#include <base/box.hpp>
#include <base/macros/for_each.hpp>

// #include <vm/bytecode/bytecode.hpp>
// @TODO: Fix includes

#define VM_EXT_CPP_INTO_VM_TYPE_NAME(Type, VmType, Name) VM_EXT_CPP_VM_TYPE_NAME(VmType),
#define VM_EXT_CPP_INTO_FIELDS(Type, VmType, Name)       Type Name;
#define VM_EXT_CPP_INTO_PARAMS(Type, VmType, Name)       , Type Name
#define VM_EXT_CPP_INTO_ARGS(Type, VmType, Name)         , func_args->Name
#define VM_EXT_CPP_PLACE_VALIDATION(Type, VmType, Name)                                     \
	if constexpr (!std::is_same_v<void, Type>) {                                            \
		auto tp_##Name = vm::api::getType(pid, VmType);                                     \
		if (!tp_##Name.has_value())                                                         \
			throw base::LogicError(#VmType " does not exist for " #Type ", " #Name);        \
		if (tp_##Name->type->getSize() != sizeof(Type))                                     \
			throw base::LogicError(                                                         \
				base::strConcat(#Type " does not match size: ", tp_##Name->type->getSize()) \
			);                                                                              \
	}

#define VM_EXT_CPP_PUT2(arg1, arg2) arg1 arg2

#define VM_EXT_CPP_VM_TYPE_NAME(Type) \
	[&]() -> vm::code::Identifier { return { base::StrID(Type) }; }()

// @TODO: Do not do packed, but do what the VM does with its stack.
// @TODO: Validate argument sizes
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
			VM_EXT_CPP_PLACE_VALIDATION(ResTp, ResVmType, result)                                           \
			FOR_EACH_ARG(VM_EXT_CPP_PUT2, VM_EXT_CPP_PLACE_VALIDATION, __VA_ARGS__);                        \
			vm::code::FuncSignature signature;                                                              \
			signature.result_type = VM_EXT_CPP_VM_TYPE_NAME(ResVmType);                                     \
			signature.parameters                                                                            \
				= { FOR_EACH_ARG(VM_EXT_CPP_PUT2, VM_EXT_CPP_INTO_VM_TYPE_NAME, __VA_ARGS__) };             \
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
