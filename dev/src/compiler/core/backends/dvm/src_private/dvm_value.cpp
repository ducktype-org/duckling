#include "dvm_value.hpp"

#include <vm/bytecode/opcode_args.hpp>
#include <vm/core/process/builtin_functions.hpp>
#include <vm/utils/interpret.hpp>

using namespace compiler::backend_vm::internal;

[[nodiscard]] vm::opargs::OpCodeArg compiler::backend_vm::internal::DVMLocal::asArgument() const {
	variant_match(type) {
		variant_case(vm::code::PrimitiveType, primitive) {
			if (primitive.size == 8) return vm::opargs::StackLocal64{ name };
			if (primitive.size == 4) return vm::opargs::StackLocal32{ name };
			if (primitive.size == 2) return vm::opargs::StackLocal16{ name };
			if (primitive.size == 1) return vm::opargs::StackLocal8{ name };
		}
		variant_case(vm::code::PointerType, pointer) { return vm::opargs::StackLocalPtr(name); }
		variant_default {
			CORE_PANIC("DVMLocal type not supported for argument: ", typeName(type));
		}
	}
	CORE_UNREACHABLE();
}

[[nodiscard]] vm::opargs::OpCodeArg compiler::backend_vm::internal::DVMGlobal::asArgument() const {
	variant_match(type) {
		variant_case(vm::code::PrimitiveType, primitive) {
			if (primitive.size == 8) return vm::opargs::Global64{ name };
			if (primitive.size == 4) return vm::opargs::Global32{ name };
			if (primitive.size == 2) return vm::opargs::Global16{ name };
			if (primitive.size == 1) return vm::opargs::Global8{ name };
		}
		variant_case(vm::code::PointerType, pointer) { return vm::opargs::GlobalPtr(name); }
		variant_default {
			CORE_PANIC("DVMGlobal type not supported for argument: ", typeName(type));
		}
	}
	CORE_UNREACHABLE();
}

[[nodiscard]] vm::opargs::OpCodeArg compiler::backend_vm::internal::DVMValue::asArgument() const {
	return VISIT(stored_value, value, return value.asArgument());
}

[[nodiscard]] vm::opargs::OpCodeArg compiler::backend_vm::internal::DVMImmediate::asArgument() const {
	return vm::opargs::Immediate{ value };
}

namespace {
	template<class T>
	u64 translateToU64(T value) {
		if constexpr (sizeof(T) == 8)
			return vm::safeReadBytes<u64>(value);
		else if constexpr (sizeof(T) == 4)
			return vm::safeReadBytes<u32>(value);
		else if constexpr (sizeof(T) == 2)
			return vm::safeReadBytes<u16>(value);
		else if constexpr (sizeof(T) == 1)
			// @TODO: #1652 Use `u8` here when casting is possible.
			return vm::safeReadBytes<std::uint8_t>(value);
		else
			CORE_PANIC("Unsupported immediate size: ", sizeof(T));
	}
}

DVMImmediate::DVMImmediate(u64 value): value(translateToU64(value)) {}

DVMImmediate::DVMImmediate(i64 value): value(translateToU64(value)) {}

DVMImmediate::DVMImmediate(bool value): value(translateToU64(value)) {}

DVMImmediate::DVMImmediate(float value): value(translateToU64(value)) {}

DVMImmediate::DVMImmediate(double value): value(translateToU64(value)) {}

DVMImmediate::DVMImmediate(i32 value): value(translateToU64(value)) {}

DVMImmediate::DVMImmediate(u32 value): value(translateToU64(value)) {}

[[nodiscard]] vm::opargs::OpCodeArg DVMLabel::asArgument() const {
	return vm::opargs::Label{ name };
}

[[nodiscard]] vm::opargs::OpCodeArg DVMFunctionName::asArgument() const {
	if (vm::builtins::isBuiltinFunction(name))
		return vm::opargs::BuiltinFunctionName{ name };
	else
		return vm::opargs::FunctionName{ name };
}

DVMValue::operator vm::opargs::OpCodeArg() const { return asArgument(); }
